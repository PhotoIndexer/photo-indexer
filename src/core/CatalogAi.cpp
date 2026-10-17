// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"
#include "core/Perf.h"

#include "core/Taxonomy.h"

#include "imaging/Hashes.h"

#include <QLoggingCategory>
#include <QSqlQuery>

#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(lcCatalog)

namespace pix {

QString Catalog::builtinId(const char *key) const
{
    return m_builtinIds.value(QString::fromLatin1(key));
}

QList<Catalog::Classifiable> Catalog::classifiableCategories() const
{
    PIX_PERF("Catalog::classifiableCategories");
    // Folhas com ``auto`` ligado, menos as de regra (sem categoria, borrada,
    // duplicata): o CLIP só escolhe entre as que descrevem o conteúdo.
    const QSet<QString> rules{builtinId(builtin::kUncategorized), builtinId(builtin::kBlurry), builtinId(builtin::kDuplicate)};
    QList<Classifiable> out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT c.id, c.prompt, c.name, coalesce(c.parent_id, '') FROM category c "
                                           "WHERE c.auto = 1 AND NOT EXISTS "
                                           "(SELECT 1 FROM category k WHERE k.parent_id = c.id) ORDER BY c.position, c.name"));
    while (q.next()) {
        const QString id = q.value(0).toString();
        if (rules.contains(id))
            continue;
        const QStringList prompts = clipPrompts(q.value(1).toString(), q.value(2).toString());
        if (!prompts.isEmpty())
            out.append({id, q.value(3).toString(), prompts});
    }
    return out;
}

void Catalog::applyCategorize(const CategorizeResult &r)
{
    PIX_PERF("Catalog::applyCategorize");
    if (!m_db.scalar(QStringLiteral("SELECT 1 FROM photo WHERE id = ?"), {r.photoId}).isValid())
        return;
    Transaction tx(m_db);
    if (!r.ok) {
        m_db.exec(QStringLiteral("UPDATE photo SET ai_state = 'error', error = ? WHERE id = ?"),
                  {QStringLiteral("AI: ") + r.error.left(400), r.photoId});
        tx.commit();
        return;
    }
    if (!r.embedding.empty())
        m_db.exec(QStringLiteral("INSERT INTO photo_embedding(photo_id, model, vector) VALUES(?, ?, ?) "
                                 "ON CONFLICT(photo_id) DO UPDATE SET model = excluded.model, vector = excluded.vector"),
                  {r.photoId, QStringLiteral("clip-vit-base-patch32"), imaging::toBlob(r.embedding)});
    // As da IA são refeitas; as da pessoa (user) e as que ela tirou (rejected) ficam.
    m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND source = 'ai'"), {r.photoId});
    for (const auto &[categoryId, score] : r.categories)
        m_db.exec(QStringLiteral("INSERT OR IGNORE INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, ?, 'ai')"),
                  {r.photoId, categoryId, double(score)});
    applyRuleCategories(r.photoId);
    m_db.exec(QStringLiteral("UPDATE photo SET ai_state = 'done', categorized_at = ? WHERE id = ?"),
              {Database::timestamp(), r.photoId});
    tx.commit();

    if (m_embeddingsLoaded && !r.embedding.empty()) {
        const qsizetype row = m_embeddingIds.indexOf(r.photoId);
        if (row >= 0)
            std::copy(r.embedding.begin(), r.embedding.end(), m_embeddings.begin() + row * qsizetype(r.embedding.size()));
        else if (m_embeddings.empty() || m_embeddings.size() % r.embedding.size() == 0) {
            m_embeddingIds << r.photoId;
            m_embeddings.insert(m_embeddings.end(), r.embedding.begin(), r.embedding.end());
        }
    }
}

void Catalog::applyRuleCategories(const QString &photoId)
{
    PIX_PERF("Catalog::applyRuleCategories");
    const QString blurry = builtinId(builtin::kBlurry);
    const QString uncategorized = builtinId(builtin::kUncategorized);
    const QString duplicate = builtinId(builtin::kDuplicate);
    // Borrada: variância do Laplaciano abaixo do limiar (preferência).
    if (!blurry.isEmpty()) {
        const double threshold = setting(QStringLiteral("ai/blurThreshold"), 40).toDouble();
        const QVariant sharp = m_db.scalar(QStringLiteral("SELECT sharpness FROM photo WHERE id = ?"), {photoId});
        if (!sharp.isNull() && sharp.toDouble() >= 0 && sharp.toDouble() < threshold)
            m_db.exec(QStringLiteral("INSERT OR IGNORE INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, 1, 'rule')"),
                      {photoId, blurry});
        else
            m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND category_id = ? AND source = 'rule'"),
                      {photoId, blurry});
    }
    // Sem categoria: nenhuma de conteúdo (as de regra não contam).
    if (!uncategorized.isEmpty()) {
        const int content = m_db.scalar(QStringLiteral("SELECT count(*) FROM photo_category WHERE photo_id = ? "
                                                       "AND source IN ('ai', 'user') AND category_id NOT IN (?, ?, ?)"),
                                        {photoId, uncategorized, blurry, duplicate})
                                .toInt();
        if (content == 0)
            m_db.exec(QStringLiteral("INSERT OR IGNORE INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, 1, 'rule')"),
                      {photoId, uncategorized});
        else
            m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND category_id = ? AND source = 'rule'"),
                      {photoId, uncategorized});
    }
}

namespace {

/// Quanto cada nível de tolerância aceita. Medido num catálogo real (3.728
/// pares da mesma foto com e sem a marca d'água): o hash perceptual muda no
/// máximo 5 bits (99% até 3) e o CLIP fica acima de 0,89. Fotos diferentes
/// até 8 bits são raras e, quase sempre, gráficos chapados (fundo de
/// certificado, ícone) — que o CLIP separa.
struct CopyLevel
{
    int bits;                   ///< distância de Hamming máxima do hash perceptual
    float minCosine;            ///< e o CLIP, quando as duas fotos o têm
    int bitsWithoutClip;        ///< sem o CLIP de uma delas, só até aqui
    float clipOnly;             ///< só pelo CLIP (corte, reenquadre): > 1 = não
};
constexpr CopyLevel kCopyLevels[] = {
    {0, 0.0f, 0, 2.0f},         // idêntica: mesmos pixels ou mesmo hash
    {6, 0.88f, 6, 2.0f},        // parecida: tamanho, compressão, marca d'água
    {12, 0.88f, 8, 0.96f},      // ampla: e cortes, ajuste de cor
};
constexpr int kMaxCopies = 2000;

} // namespace

QList<QPair<QString, float>> Catalog::findCopies(const QString &photoId, int tolerance)
{
    PIX_PERF("Catalog::findCopies");
    QList<QPair<QString, float>> out;
    QSqlQuery me = m_db.exec(QStringLiteral("SELECT picture_hash, phash, private FROM photo WHERE id = ?"), {photoId});
    if (!me.next() || me.value(2).toBool())
        return out;
    const CopyLevel &level = kCopyLevels[std::clamp(tolerance, 0, 2)];
    QHash<QString, float> found;
    const QString pictureHash = me.value(0).toString();
    if (!pictureHash.isEmpty()) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM photo WHERE picture_hash = ? AND id != ? AND private = 0"),
                                {pictureHash, photoId});
        while (q.next())
            found.insert(q.value(0).toString(), 1.0f);
    }
    const imaging::Embedding mine =
        imaging::fromBlob(m_db.scalar(QStringLiteral("SELECT vector FROM photo_embedding WHERE photo_id = ?"), {photoId}).toByteArray());
    bool ok = false;
    const QString hex = me.value(1).toString();
    const quint64 phash = imaging::hashFromHex(hex, &ok);
    if (ok && !hex.isEmpty()) {
        QHash<QString, int> candidates;
        for (const HashTree::Match &m : m_hashes.query(phash, level.bits))
            if (m.id != photoId && !found.contains(m.id))
                candidates.insert(m.id, m.distance);
        // O CLIP dos candidatos (em lotes), para separar o que só tem o hash parecido.
        QHash<QString, float> cosine;
        if (!mine.empty() && level.minCosine > 0 && !candidates.isEmpty()) {
            const QStringList ids = candidates.keys();
            for (qsizetype start = 0; start < ids.size(); start += 500) {
                const QStringList chunk = ids.mid(start, 500);
                QVariantList params;
                for (const QString &id : chunk)
                    params << id;
                QSqlQuery q = m_db.exec(QStringLiteral("SELECT photo_id, vector FROM photo_embedding WHERE photo_id IN (%1)")
                                            .arg(QStringList(chunk.size(), QStringLiteral("?")).join(u',')),
                                        params);
                while (q.next()) {
                    const imaging::Embedding v = imaging::fromBlob(q.value(1).toByteArray());
                    if (v.size() == mine.size())
                        cosine.insert(q.value(0).toString(), imaging::dot(mine.data(), v.data(), int(v.size())));
                }
            }
        }
        for (auto it = candidates.constBegin(); it != candidates.constEnd(); ++it) {
            const auto c = cosine.constFind(it.key());
            if (c != cosine.constEnd() ? *c < level.minCosine : it.value() > level.bitsWithoutClip)
                continue;
            found.insert(it.key(), 1.0f - float(it.value()) / 64.0f);
        }
    }
    // Só pelo CLIP: a mesma cena cortada ou com outra cor (o hash muda muito).
    if (!mine.empty() && level.clipOnly <= 1.0f)
        for (const auto &[id, s] : rankByEmbedding(mine, level.clipOnly, kMaxCopies))
            if (id != photoId && !found.contains(id))
                found.insert(id, s);
    for (auto it = found.constBegin(); it != found.constEnd(); ++it)
        out.append({it.key(), it.value()});
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    if (out.size() > kMaxCopies)
        out.resize(kMaxCopies);
    out.prepend({photoId, 1.0f});
    return out;
}

QList<QPair<QString, float>> Catalog::rankByEmbedding(const imaging::Embedding &text, float minScore, int limit)
{
    PIX_PERF("Catalog::rankByEmbedding");
    QList<QPair<QString, float>> out;
    if (text.empty())
        return out;
    if (!m_embeddingsLoaded) {
        m_embeddingIds.clear();
        m_embeddings.clear();
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT e.photo_id, e.vector FROM photo_embedding e "
                                               "JOIN photo p ON p.id = e.photo_id WHERE p.state != 'missing' AND p.private = 0"));
        while (q.next()) {
            const imaging::Embedding v = imaging::fromBlob(q.value(1).toByteArray());
            if (v.size() != text.size())
                continue;
            m_embeddingIds << q.value(0).toString();
            m_embeddings.insert(m_embeddings.end(), v.begin(), v.end());
        }
        m_embeddingsLoaded = true;
        qCInfo(lcCatalog) << "AI search index:" << m_embeddingIds.size() << "photos";
    }
    const int dims = int(text.size());
    for (qsizetype i = 0; i < m_embeddingIds.size(); ++i) {
        const float s = imaging::dot(text.data(), m_embeddings.data() + i * dims, dims);
        if (s >= minScore)
            out.append({m_embeddingIds[i], s});
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    if (limit > 0 && out.size() > limit)
        out.resize(limit);
    return out;
}

} // namespace pix

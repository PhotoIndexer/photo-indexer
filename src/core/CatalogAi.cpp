// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include "core/Taxonomy.h"

#include <QLoggingCategory>
#include <QSqlQuery>

#include <algorithm>

Q_DECLARE_LOGGING_CATEGORY(lcCatalog)

namespace pix {

QString Catalog::builtinId(const char *key) const
{
    return m_builtinIds.value(QString::fromLatin1(key));
}

QList<QPair<QString, QStringList>> Catalog::classifiableCategories() const
{
    // Folhas com ``auto`` ligado, menos as de regra (sem categoria, borrada,
    // duplicata): o CLIP só escolhe entre as que descrevem o conteúdo.
    const QSet<QString> rules{builtinId(builtin::kUncategorized), builtinId(builtin::kBlurry), builtinId(builtin::kDuplicate)};
    QList<QPair<QString, QStringList>> out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT c.id, c.prompt, c.name FROM category c WHERE c.auto = 1 AND NOT EXISTS "
                                           "(SELECT 1 FROM category k WHERE k.parent_id = c.id) ORDER BY c.position, c.name"));
    while (q.next()) {
        const QString id = q.value(0).toString();
        if (rules.contains(id))
            continue;
        const QStringList prompts = clipPrompts(q.value(1).toString(), q.value(2).toString());
        if (!prompts.isEmpty())
            out.append({id, prompts});
    }
    return out;
}

void Catalog::applyCategorize(const CategorizeResult &r)
{
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

QList<QPair<QString, float>> Catalog::rankByEmbedding(const imaging::Embedding &text, float minScore, int limit)
{
    QList<QPair<QString, float>> out;
    if (text.empty())
        return out;
    if (!m_embeddingsLoaded) {
        m_embeddingIds.clear();
        m_embeddings.clear();
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT e.photo_id, e.vector FROM photo_embedding e "
                                               "JOIN photo p ON p.id = e.photo_id WHERE p.state != 'missing'"));
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

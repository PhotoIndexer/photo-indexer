// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"
#include "core/Perf.h"

#include "imaging/Faces.h"

#include <QDate>
#include <QFile>
#include <QLoggingCategory>
#include <QSqlQuery>

#include <algorithm>
#include <cmath>

Q_DECLARE_LOGGING_CATEGORY(lcCatalog)

namespace pix {

namespace {

float similarityToSum(const imaging::Embedding &sum, const imaging::Embedding &e)
{
    if (sum.size() != e.size() || sum.empty())
        return 0.0f;
    double dot = 0.0, norm = 0.0;
    for (size_t i = 0; i < sum.size(); ++i) {
        dot += double(sum[i]) * e[i];
        norm += double(sum[i]) * sum[i];
    }
    return norm > 0 ? float(dot / std::sqrt(norm)) : 0.0f;
}

} // namespace

// --------------------------------------------------------------------------- //
// Estado em memória
// --------------------------------------------------------------------------- //

void Catalog::loadPeople()
{
    PIX_PERF("Catalog::loadPeople");
    m_people.clear();
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, name FROM person"));
    while (q.next())
        m_people.insert(q.value(0).toString(), PersonState{{}, 0, q.value(1).toString()});
    QSqlQuery f = m_db.exec(QStringLiteral("SELECT id, person_id FROM face WHERE person_id IS NOT NULL"));
    while (f.next()) {
        auto it = m_people.find(f.value(1).toString());
        if (it == m_people.end())
            continue;
        const imaging::Embedding e = m_faces.embeddingOf(f.value(0).toString());
        if (e.empty())
            continue;
        if (it->sum.empty())
            it->sum.assign(e.size(), 0.0f);
        for (size_t i = 0; i < e.size(); ++i)
            it->sum[i] += e[i];
        ++it->faces;
    }
}

imaging::Embedding Catalog::centroidOf(const QString &personId) const
{
    const auto it = m_people.constFind(personId);
    if (it == m_people.constEnd() || it->sum.empty())
        return {};
    imaging::Embedding c = it->sum;
    imaging::normalize(c);
    return c;
}

ClusterParams Catalog::clusterParams() const
{
    // Padrões do core/FaceClusterer.h (calibrados num catálogo real).
    ClusterParams p;
    p.join = setting(QStringLiteral("faces/matchSimilarity"), p.join).toFloat();
    p.merge = setting(QStringLiteral("faces/mergeSimilarity"), p.merge).toFloat();
    p.mergeMinFaces = setting(QStringLiteral("faces/mergeMinFaces"), p.mergeMinFaces).toInt();
    return p;
}

float Catalog::minPersonScore() const
{
    return setting(QStringLiteral("faces/minPersonScore"), 0.65).toFloat();
}

QString Catalog::clusterFace(const QString &faceId, const imaging::Embedding &embedding, float *similarity,
                             const QSet<QString> &exclude)
{
    // A pessoa mais parecida (cosseno com o centroide) a partir do limiar;
    // senão, uma pessoa nova. Não contam: as de que esta face foi tirada
    // (face_rejection) e as que já têm um rosto nesta foto (core/FaceClusterer.h).
    const float threshold = clusterParams().join;
    QSet<QString> rejected;
    QSqlQuery r = m_db.exec(QStringLiteral("SELECT person_id FROM face_rejection WHERE face_id = ?"), {faceId});
    while (r.next())
        rejected.insert(r.value(0).toString());
    QString best;
    float bestSim = 0.0f;
    for (auto it = m_people.constBegin(); it != m_people.constEnd(); ++it) {
        if (it->faces == 0 || rejected.contains(it.key()) || exclude.contains(it.key()))
            continue;
        const float s = similarityToSum(it->sum, embedding);
        if (s > bestSim) {
            bestSim = s;
            best = it.key();
        }
    }
    const QString now = Database::timestamp();
    if (best.isEmpty() || bestSim < threshold) {
        best = Database::newId();
        m_db.exec(QStringLiteral("INSERT INTO person(id, name, created_at, updated_at) VALUES(?, '', ?, ?)"), {best, now, now});
        m_people.insert(best, PersonState{{}, 0, {}});
        bestSim = 1.0f;
    }
    PersonState &p = m_people[best];
    if (p.sum.empty())
        p.sum.assign(embedding.size(), 0.0f);
    for (size_t i = 0; i < embedding.size() && i < p.sum.size(); ++i)
        p.sum[i] += embedding[i];
    ++p.faces;
    *similarity = bestSim;
    return best;
}

void Catalog::refreshPerson(const QString &personId)
{
    PIX_PERF("Catalog::refreshPerson");
    if (personId.isEmpty())
        return;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, photo_id, score FROM face WHERE person_id = ? ORDER BY score DESC"), {personId});
    std::vector<imaging::Embedding> vectors;
    QSet<QString> photos;
    QString cover;
    int count = 0;
    while (q.next()) {
        if (cover.isEmpty())
            cover = q.value(0).toString();
        photos.insert(q.value(1).toString());
        ++count;
        imaging::Embedding e = m_faces.embeddingOf(q.value(0).toString());
        if (!e.empty())
            vectors.push_back(std::move(e));
    }
    const QString name = m_db.scalar(QStringLiteral("SELECT name FROM person WHERE id = ?"), {personId}).toString();
    if (count == 0 && name.isEmpty()) {
        // Pessoa sem nome e sem face: não existe mais.
        m_db.exec(QStringLiteral("DELETE FROM person WHERE id = ?"), {personId});
        m_people.remove(personId);
        return;
    }
    PersonState state{{}, int(vectors.size()), name};
    if (!vectors.empty()) {
        state.sum.assign(vectors.front().size(), 0.0f);
        for (const auto &v : vectors)
            for (size_t i = 0; i < v.size(); ++i)
                state.sum[i] += v[i];
    }
    m_people.insert(personId, state);
    const imaging::Embedding centroid = imaging::mean(vectors);
    m_db.exec(QStringLiteral("UPDATE person SET face_count = ?, photo_count = ?, cover_face_id = ?, centroid = ?, "
                             "updated_at = ? WHERE id = ?"),
              {count, int(photos.size()), cover.isEmpty() ? QVariant() : QVariant(cover),
               centroid.empty() ? QVariant() : QVariant(imaging::toBlob(centroid)), Database::timestamp(), personId});
}

// --------------------------------------------------------------------------- //
// Faces -> catálogo
// --------------------------------------------------------------------------- //

void Catalog::applyFaces(const FaceResult &r)
{
    PIX_PERF("Catalog::applyFaces");
    if (!m_db.scalar(QStringLiteral("SELECT 1 FROM photo WHERE id = ?"), {r.photoId}).isValid()) {
        for (const FoundFace &f : r.faces)
            QFile::remove(f.cropPath);
        return;
    }
    if (!r.ok) {
        m_db.exec(QStringLiteral("UPDATE photo SET faces_state = 'error', error = ? WHERE id = ?"),
                  {QStringLiteral("Faces: ") + r.error.left(400), r.photoId});
        emit facesDone(r.photoId);
        return;
    }
    QSet<QString> touched;
    Transaction tx(m_db);
    // Reprocessar substitui as faces anteriores da foto.
    QSqlQuery old = m_db.exec(QStringLiteral("SELECT id, person_id, crop_path FROM face WHERE photo_id = ?"), {r.photoId});
    while (old.next()) {
        if (!old.value(1).isNull())
            touched.insert(old.value(1).toString());
        QFile::remove(old.value(2).toString());
        m_faces.remove(old.value(0).toString());
    }
    m_db.exec(QStringLiteral("DELETE FROM face WHERE photo_id = ?"), {r.photoId});
    // Sem esses, a soma em memória das pessoas tocadas ficaria com as faces velhas.
    for (const QString &p : touched)
        refreshPerson(p);

    const QString now = Database::timestamp();
    // Detecção fraca (animais, objetos, rostos minúsculos no fundo): a face
    // fica na foto e na busca por rosto, mas não forma nem engorda pessoas.
    const float minScore = minPersonScore();
    QSet<QString> inThisPhoto;  // uma pessoa por foto
    for (const FoundFace &f : r.faces) {
        m_db.exec(QStringLiteral("INSERT INTO face(id, photo_id, x, y, w, h, score, embedding, model, crop_path, created_at) "
                                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
                  {f.faceId, r.photoId, f.x, f.y, f.w, f.h, f.score, imaging::toBlob(f.embedding),
                   QString::fromLatin1(imaging::FaceEngine::kName), f.cropPath, now});
        m_faces.insert({f.faceId, r.photoId, QString()}, f.embedding);
        if (f.score < minScore)
            continue;
        float similarity = 0.0f;
        const QString person = clusterFace(f.faceId, f.embedding, &similarity, inThisPhoto);
        inThisPhoto.insert(person);
        m_db.exec(QStringLiteral("UPDATE face SET person_id = ?, similarity = ? WHERE id = ?"), {person, similarity, f.faceId});
        m_faces.setPerson(f.faceId, person);
        touched.insert(person);
    }
    for (const QString &p : touched)
        refreshPerson(p);
    m_db.exec(QStringLiteral("UPDATE photo SET faces_state = 'done', face_count = ?, faces_at = ? WHERE id = ?"),
              {int(r.faces.size()), now, r.photoId});
    tx.commit();
    if (!r.faces.isEmpty() || !touched.isEmpty())
        emit peopleChanged();
    emit facesDone(r.photoId);
}

int Catalog::autoMergePeople()
{
    PIX_PERF("Catalog::autoMergePeople");
    return applyMergePairs(mergePairs(mergeSnapshot()));
}

// Junta duas pessoas quando a MÉDIA das similaridades entre as faces de uma
// e as da outra passa do limiar, recalculada depois de cada junção (sem
// cadeia), as duas são confiáveis (faces suficientes, confiança média alta)
// e nunca aparecem juntas numa foto. Nomes diferentes nunca se juntam. Ver
// core/FaceClusterer.h.
Catalog::MergeSnapshot Catalog::mergeSnapshot() const
{
    PIX_PERF("Catalog::mergeSnapshot");
    const ClusterParams params = clusterParams();
    MergeSnapshot snap;
    snap.threshold = params.merge;
    const double minScore = setting(QStringLiteral("faces/mergeMinScore"), 0.6).toDouble();
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT person_id FROM face WHERE person_id IS NOT NULL "
                                           "GROUP BY person_id HAVING count(*) >= ? AND avg(score) >= ?"),
                            {params.mergeMinFaces, minScore});
    while (q.next()) {
        const auto it = m_people.constFind(q.value(0).toString());
        if (it == m_people.constEnd() || it->faces == 0 || it->sum.empty())
            continue;
        snap.ids << it.key();
        snap.sums.push_back(it->sum);
        snap.counts.push_back(it->faces);
    }
    return snap;
}

std::vector<Catalog::MergePair> Catalog::mergePairs(const MergeSnapshot &snap)
{
    PIX_PERF("Catalog::mergePairs");
    std::vector<MergePair> pairs;
    const int n = int(snap.ids.size());
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j) {
            const auto &x = snap.sums[size_t(i)], &y = snap.sums[size_t(j)];
            if (x.size() != y.size())
                continue;
            double d = 0;
            for (size_t k = 0; k < x.size(); ++k)
                d += double(x[k]) * y[k];
            const float s = float(d / (double(snap.counts[size_t(i)]) * snap.counts[size_t(j)]));
            if (s >= snap.threshold)
                pairs.push_back({snap.ids[i], snap.ids[j], s});
        }
    std::sort(pairs.begin(), pairs.end(), [](const MergePair &x, const MergePair &y) { return x.sim > y.sim; });
    return pairs;
}

int Catalog::applyMergePairs(const std::vector<MergePair> &pairs)
{
    PIX_PERF("Catalog::applyMergePairs");
    const float threshold = clusterParams().merge;
    auto link = [this](const QString &a, const QString &b) {
        const PersonState &x = m_people[a], &y = m_people[b];
        if (x.sum.size() != y.sum.size() || x.faces == 0 || y.faces == 0)
            return 0.0f;
        double d = 0;
        for (size_t i = 0; i < x.sum.size(); ++i)
            d += double(x.sum[i]) * y.sum[i];
        return float(d / (double(x.faces) * y.faces));
    };
    QHash<QString, QString> into;  // absorvida -> quem absorveu
    auto find = [&into](QString id) {
        while (into.contains(id))
            id = into.value(id);
        return id;
    };
    int merged = 0;
    for (const MergePair &p : pairs) {
        QString a = find(p.a), b = find(p.b);
        if (a == b || !m_people.contains(a) || !m_people.contains(b))
            continue;
        const QString na = m_people.value(a).name.trimmed(), nb = m_people.value(b).name.trimmed();
        if (!na.isEmpty() && !nb.isEmpty() && na.compare(nb, Qt::CaseInsensitive) != 0)
            continue;
        const float sim = link(a, b);  // com as pessoas de agora (sem cadeia)
        if (sim < threshold)
            continue;
        if (m_db.scalar(QStringLiteral("SELECT 1 FROM face x JOIN face y ON y.photo_id = x.photo_id "
                                       "WHERE x.person_id = ? AND y.person_id = ? LIMIT 1"),
                        {a, b})
                .isValid())
            continue;  // aparecem juntas numa foto: são duas pessoas
        // Fica a nomeada; entre duas sem nome, a maior.
        if (!nb.isEmpty() || (na.isEmpty() && m_people.value(b).faces > m_people.value(a).faces))
            std::swap(a, b);
        qCInfo(lcCatalog) << "Auto-merging person" << b << "into" << a << "average similarity" << sim;
        mergePeople(a, b);
        into.insert(b, a);
        ++merged;
    }
    return merged;
}

void Catalog::reclusterPeople()
{
    PIX_PERF("Catalog::reclusterPeople");
    const ReclusterPlan plan = prepareRecluster();
    applyRecluster(plan, clusterFaces(plan.faces, int(plan.fixedPersons.size()), plan.params));
}

Catalog::ReclusterPlan Catalog::prepareRecluster() const
{
    PIX_PERF("Catalog::prepareRecluster");
    ReclusterPlan plan;
    plan.params = clusterParams();
    // Ficam como estão (grupos fixos): as pessoas com nome e as que têm
    // alguma face confirmada pela pessoa.
    QHash<QString, int> fixed;
    QSet<QString> named;
    // Num grupo da pessoa conta como com nome: ela já organizou.
    QSqlQuery p = m_db.exec(QStringLiteral("SELECT id, name != '' OR group_id IS NOT NULL FROM person "
                                           "WHERE name != '' OR group_id IS NOT NULL OR id IN "
                                           "(SELECT person_id FROM face WHERE assignment = 'user' AND person_id IS NOT NULL)"));
    while (p.next()) {
        fixed.insert(p.value(0).toString(), int(plan.fixedPersons.size()));
        plan.fixedPersons << p.value(0).toString();
        if (p.value(1).toBool())
            named.insert(p.value(0).toString());
    }
    QHash<QString, std::vector<int>> rejected;
    QSqlQuery r = m_db.exec(QStringLiteral("SELECT face_id, person_id FROM face_rejection"));
    while (r.next())
        if (fixed.contains(r.value(1).toString()))
            rejected[r.value(0).toString()].push_back(fixed.value(r.value(1).toString()));
    QHash<QString, int> photos;
    const float minScore = minPersonScore();
    QSqlQuery f = m_db.exec(QStringLiteral("SELECT id, photo_id, person_id, assignment, score FROM face ORDER BY created_at"));
    while (f.next()) {
        const QString faceId = f.value(0).toString();
        const QString person = f.value(2).toString();
        // Ficam onde estão: as faces das pessoas com nome e as confirmadas.
        const bool keep = fixed.contains(person) && (named.contains(person) || f.value(3).toString() == QLatin1String("user"));
        if (!keep && f.value(4).toFloat() < minScore)
            continue;  // detecção fraca (nuca, perfil, borrão): não forma pessoa
        imaging::Embedding e = m_faces.embeddingOf(faceId);
        if (e.empty())
            continue;
        ClusterFace c;
        c.photo = photos.value(f.value(1).toString(), int(photos.size()));
        photos.insert(f.value(1).toString(), c.photo);
        c.embedding = std::move(e);
        c.fixed = keep ? fixed.value(person) : -1;
        c.notIn = rejected.value(faceId);
        plan.faces.push_back(std::move(c));
        plan.faceIds << faceId;
    }
    return plan;
}

int Catalog::applyRecluster(const ReclusterPlan &plan, const std::vector<int> &labels)
{
    PIX_PERF("Catalog::applyRecluster");
    const int fixedCount = int(plan.fixedPersons.size());
    int groups = fixedCount;
    for (int l : labels)
        groups = std::max(groups, l + 1);
    // Soma de cada grupo: o centroide (similaridade de cada face e o da pessoa).
    const size_t dims = plan.faces.empty() ? 0 : plan.faces.front().embedding.size();
    std::vector<double> sums(size_t(groups) * dims, 0.0);
    for (size_t i = 0; i < labels.size(); ++i)
        if (labels[i] >= 0)
            for (size_t k = 0; k < dims; ++k)
                sums[size_t(labels[i]) * dims + k] += plan.faces[i].embedding[k];
    std::vector<double> norms(size_t(groups), 0.0);
    for (int g = 0; g < groups; ++g) {
        double n = 0;
        for (size_t k = 0; k < dims; ++k)
            n += sums[size_t(g) * dims + k] * sums[size_t(g) * dims + k];
        norms[size_t(g)] = std::sqrt(n);
    }
    QStringList personOf = plan.fixedPersons;
    const QString now = Database::timestamp();
    Transaction tx(m_db);
    // As pessoas automáticas (sem nome, sem face confirmada) são refeitas.
    QVariantList keep;
    for (const QString &id : plan.fixedPersons)
        keep << id;
    m_db.exec(QStringLiteral("UPDATE face SET person_id = NULL WHERE assignment = 'auto'"));
    if (keep.isEmpty())
        m_db.exec(QStringLiteral("DELETE FROM person WHERE name = ''"));
    else
        m_db.exec(QStringLiteral("DELETE FROM person WHERE name = '' AND id NOT IN (%1)")
                      .arg(QStringList(keep.size(), QStringLiteral("?")).join(u',')),
                  keep);
    for (int g = fixedCount; g < groups; ++g) {
        const QString id = Database::newId();
        m_db.exec(QStringLiteral("INSERT INTO person(id, name, created_at, updated_at) VALUES(?, '', ?, ?)"), {id, now, now});
        personOf << id;
    }
    QSqlQuery update(m_db.handle());
    update.prepare(QStringLiteral("UPDATE face SET person_id = ?, similarity = ? WHERE id = ?"));
    for (size_t i = 0; i < labels.size(); ++i) {
        const int g = labels[i];
        if (g < 0)
            continue;
        double d = 0;
        for (size_t k = 0; k < dims; ++k)
            d += sums[size_t(g) * dims + k] * plan.faces[i].embedding[k];
        update.addBindValue(personOf.at(g));
        update.addBindValue(norms[size_t(g)] > 0 ? d / norms[size_t(g)] : 0.0);
        update.addBindValue(plan.faceIds.at(qsizetype(i)));
        update.exec();
    }
    // Contagens, capa (a face de maior confiança) e centroide, em lote.
    m_db.exec(QStringLiteral("UPDATE person SET "
                             "face_count = (SELECT count(*) FROM face f WHERE f.person_id = person.id), "
                             "photo_count = (SELECT count(DISTINCT photo_id) FROM face f WHERE f.person_id = person.id), "
                             "cover_face_id = (SELECT id FROM face f WHERE f.person_id = person.id ORDER BY score DESC LIMIT 1), "
                             "updated_at = ?"),
              {now});
    QSqlQuery centroid(m_db.handle());
    centroid.prepare(QStringLiteral("UPDATE person SET centroid = ? WHERE id = ?"));
    for (int g = 0; g < groups; ++g) {
        if (norms[size_t(g)] <= 0)
            continue;
        imaging::Embedding c(dims);
        for (size_t k = 0; k < dims; ++k)
            c[k] = float(sums[size_t(g) * dims + k] / norms[size_t(g)]);
        centroid.addBindValue(imaging::toBlob(c));
        centroid.addBindValue(personOf.at(g));
        centroid.exec();
    }
    tx.commit();
    QSqlQuery all = m_db.exec(QStringLiteral("SELECT id, person_id FROM face"));
    while (all.next())
        m_faces.setPerson(all.value(0).toString(), all.value(1).toString());
    loadPeople();
    const int people = m_db.scalar(QStringLiteral("SELECT count(*) FROM person WHERE face_count > 0")).toInt();
    qCInfo(lcCatalog) << "People regrouped:" << plan.faces.size() << "faces," << people << "people";
    emit peopleChanged();
    emit treeChanged(QStringLiteral("people"));
    return people;
}

// --------------------------------------------------------------------------- //
// Pessoas: leitura
// --------------------------------------------------------------------------- //

QString Catalog::createPersonGroup(const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("group.emptyName"));
    const QString id = Database::newId();
    const int position = m_db.scalar(QStringLiteral("SELECT coalesce(max(position), 0) + 1 FROM person_group")).toInt();
    m_db.exec(QStringLiteral("INSERT INTO person_group(id, name, position, created_at) VALUES(?, ?, ?, ?)"),
              {id, clean, position, Database::timestamp()});
    emit treeChanged(QStringLiteral("people"));
    return id;
}

void Catalog::renamePersonGroup(const QString &groupId, const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("group.emptyName"));
    m_db.exec(QStringLiteral("UPDATE person_group SET name = ? WHERE id = ?"), {clean, groupId});
    emit treeChanged(QStringLiteral("people"));
}

void Catalog::deletePersonGroup(const QString &groupId)
{
    Transaction tx(m_db);
    m_db.exec(QStringLiteral("UPDATE person SET group_id = NULL WHERE group_id = ?"), {groupId});
    m_db.exec(QStringLiteral("DELETE FROM person_group WHERE id = ?"), {groupId});
    tx.commit();
    emit treeChanged(QStringLiteral("people"));
    emit peopleChanged();
}

void Catalog::setPersonGroup(const QString &personId, const QString &groupId)
{
    if (!groupId.isEmpty() && !m_db.scalar(QStringLiteral("SELECT 1 FROM person_group WHERE id = ?"), {groupId}).isValid())
        throw ActionError(QStringLiteral("group.notFound"));
    m_db.exec(QStringLiteral("UPDATE person SET group_id = ?, updated_at = ? WHERE id = ?"),
              {groupId.isEmpty() ? QVariant() : QVariant(groupId), Database::timestamp(), personId});
    emit treeChanged(QStringLiteral("people"));
    emit peopleChanged();
}

QVariantList Catalog::personGroups() const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, name FROM person_group ORDER BY position, name COLLATE NOCASE"));
    while (q.next())
        out << QVariantMap{{QStringLiteral("id"), q.value(0)}, {QStringLiteral("name"), q.value(1)}};
    return out;
}

QString Catalog::personGroupOf(const QString &personId) const
{
    return m_db.scalar(QStringLiteral("SELECT coalesce(group_id, '') FROM person WHERE id = ?"), {personId}).toString();
}

QVariantMap Catalog::photoPeople(const QString &photoId) const
{
    QVariantMap out;
    out.insert(QStringLiteral("facesState"), m_db.scalar(QStringLiteral("SELECT faces_state FROM photo WHERE id = ?"), {photoId}));
    QVariantList people;
    QSet<QString> seen;
    int faces = 0;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT f.person_id, coalesce(pe.name, ''), f.id FROM face f "
                                           "LEFT JOIN person pe ON pe.id = f.person_id WHERE f.photo_id = ? ORDER BY f.x"),
                            {photoId});
    QVariantList unknown;
    while (q.next()) {
        ++faces;
        const QString id = q.value(0).toString();
        // Sem pessoa (detecção fraca, tirada de alguém): ainda dá para buscar o rosto.
        if (id.isEmpty()) {
            unknown << QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), QString()},
                                   {QStringLiteral("faceId"), q.value(2)}};
            continue;
        }
        if (seen.contains(id))
            continue;
        seen.insert(id);
        people << QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), q.value(1)}, {QStringLiteral("faceId"), q.value(2)}};
    }
    people << unknown;
    out.insert(QStringLiteral("faces"), faces);
    out.insert(QStringLiteral("people"), people);
    return out;
}

int Catalog::minPersonPhotos() const
{
    return std::max(1, setting(QStringLiteral("people/minPhotos"), 20).toInt());
}

QVariantList Catalog::people() const
{
    PIX_PERF("Catalog::people");
    QVariantList out;
    // As com nome sempre, primeiro (em ordem alfabética); as sem nome só a
    // partir de people/minPhotos fotos (padrão 20), as com mais fotos antes.
    // As outras continuam existindo: aparecem na busca por rosto.
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT p.id, p.name, f.crop_path, p.face_count, p.photo_count FROM person p "
        "LEFT JOIN face f ON f.id = p.cover_face_id WHERE p.name != '' OR p.group_id IS NOT NULL OR (p.face_count > 0 AND p.photo_count >= ?) "
        "ORDER BY (p.name = ''), p.name COLLATE NOCASE, p.photo_count DESC, p.face_count DESC"),
        {minPersonPhotos()});
    while (q.next())
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},
                           {QStringLiteral("name"), q.value(1)},
                           {QStringLiteral("coverPath"), q.value(2)},
                           {QStringLiteral("faceCount"), q.value(3)},
                           {QStringLiteral("photoCount"), q.value(4)}};
    return out;
}

QVariantMap Catalog::person(const QString &personId) const
{
    PIX_PERF("Catalog::person");
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT p.id, p.name, f.crop_path, p.face_count, p.photo_count, p.created_at "
                                           "FROM person p LEFT JOIN face f ON f.id = p.cover_face_id WHERE p.id = ?"),
                            {personId});
    if (!q.next())
        return {};
    return {{QStringLiteral("id"), q.value(0)},
            {QStringLiteral("name"), q.value(1)},
            {QStringLiteral("coverPath"), q.value(2)},
            {QStringLiteral("faceCount"), q.value(3)},
            {QStringLiteral("photoCount"), q.value(4)},
            {QStringLiteral("createdAt"), Database::parseTimestamp(q.value(5)).toLocalTime()}};
}

QVariantList Catalog::personFaces(const QString &personId) const
{
    PIX_PERF("Catalog::personFaces");
    QVariantList out;
    const imaging::Embedding centroid = centroidOf(personId);
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT f.id, f.photo_id, f.crop_path, f.score, f.assignment, f.x, f.y, f.w, f.h, "
                                           "p.name FROM face f JOIN photo p ON p.id = f.photo_id WHERE f.person_id = ?"),
                            {personId});
    while (q.next()) {
        const QString faceId = q.value(0).toString();
        out << QVariantMap{{QStringLiteral("id"), faceId},
                           {QStringLiteral("photoId"), q.value(1)},
                           {QStringLiteral("cropPath"), q.value(2)},
                           {QStringLiteral("score"), q.value(3)},
                           {QStringLiteral("assignment"), q.value(4)},
                           {QStringLiteral("similarity"), imaging::dot(centroid, m_faces.embeddingOf(faceId))},
                           {QStringLiteral("photoName"), q.value(9)}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("similarity")).toFloat() > b.toMap().value(QStringLiteral("similarity")).toFloat();
    });
    return out;
}

QVariantList Catalog::unassignedFaces() const
{
    PIX_PERF("Catalog::unassignedFaces");
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT f.id, f.photo_id, f.crop_path, f.score, p.name FROM face f "
                                           "JOIN photo p ON p.id = f.photo_id WHERE f.person_id IS NULL AND f.assignment = 'user' "
                                           "ORDER BY f.score DESC"));
    while (q.next())
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},
                           {QStringLiteral("photoId"), q.value(1)},
                           {QStringLiteral("cropPath"), q.value(2)},
                           {QStringLiteral("score"), q.value(3)},
                           {QStringLiteral("assignment"), QStringLiteral("user")},
                           {QStringLiteral("similarity"), 0.0},
                           {QStringLiteral("photoName"), q.value(4)}};
    return out;
}

QList<FaceIndex::Match> Catalog::searchFace(const imaging::Embedding &embedding, float threshold, int limit) const
{
    return m_faces.search(embedding, threshold, limit);
}

// --------------------------------------------------------------------------- //
// Pessoas: correções da pessoa
// --------------------------------------------------------------------------- //

QString Catalog::createPerson(const QString &name, const QStringList &faceIds)
{
    const QString id = Database::newId();
    const QString now = Database::timestamp();
    m_db.exec(QStringLiteral("INSERT INTO person(id, name, created_at, updated_at) VALUES(?, ?, ?, ?)"),
              {id, name.trimmed(), now, now});
    m_people.insert(id, PersonState{{}, 0, name.trimmed()});
    if (!faceIds.isEmpty())
        assignFaces(faceIds, id);
    emit peopleChanged();
    return id;
}

void Catalog::renamePerson(const QString &personId, const QString &name)
{
    m_db.exec(QStringLiteral("UPDATE person SET name = ?, updated_at = ? WHERE id = ?"),
              {name.trimmed(), Database::timestamp(), personId});
    if (m_people.contains(personId))
        m_people[personId].name = name.trimmed();
    emit peopleChanged();
    emit treeChanged(QStringLiteral("people"));
}

void Catalog::deletePerson(const QString &personId)
{
    Transaction tx(m_db);
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM face WHERE person_id = ?"), {personId});
    while (q.next())
        m_faces.setPerson(q.value(0).toString(), QString());
    m_db.exec(QStringLiteral("UPDATE face SET person_id = NULL, assignment = 'auto' WHERE person_id = ?"), {personId});
    m_db.exec(QStringLiteral("DELETE FROM person WHERE id = ?"), {personId});
    tx.commit();
    m_people.remove(personId);
    emit peopleChanged();
}

void Catalog::mergePeople(const QString &targetId, const QString &sourceId)
{
    PIX_PERF("Catalog::mergePeople");
    if (targetId == sourceId || targetId.isEmpty() || sourceId.isEmpty())
        return;
    Transaction tx(m_db);
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM face WHERE person_id = ?"), {sourceId});
    while (q.next())
        m_faces.setPerson(q.value(0).toString(), targetId);
    m_db.exec(QStringLiteral("UPDATE face SET person_id = ? WHERE person_id = ?"), {targetId, sourceId});
    m_db.exec(QStringLiteral("INSERT OR IGNORE INTO face_rejection(face_id, person_id, created_at) "
                             "SELECT face_id, ?, created_at FROM face_rejection WHERE person_id = ?"),
              {targetId, sourceId});
    const QString sourceName = m_db.scalar(QStringLiteral("SELECT name FROM person WHERE id = ?"), {sourceId}).toString();
    m_db.exec(QStringLiteral("UPDATE person SET name = ? WHERE id = ? AND name = ''"), {sourceName, targetId});
    m_db.exec(QStringLiteral("DELETE FROM person WHERE id = ?"), {sourceId});
    m_people.remove(sourceId);
    refreshPerson(targetId);
    tx.commit();
    emit peopleChanged();
}

void Catalog::assignFaces(const QStringList &faceIds, const QString &personId)
{
    PIX_PERF("Catalog::assignFaces");
    QSet<QString> touched{personId};
    Transaction tx(m_db);
    for (const QString &faceId : faceIds) {
        const QString old = m_db.scalar(QStringLiteral("SELECT person_id FROM face WHERE id = ?"), {faceId}).toString();
        if (!old.isEmpty())
            touched.insert(old);
        m_db.exec(QStringLiteral("UPDATE face SET person_id = ?, assignment = 'user', similarity = 1 WHERE id = ?"),
                  {personId, faceId});
        m_db.exec(QStringLiteral("DELETE FROM face_rejection WHERE face_id = ? AND person_id = ?"), {faceId, personId});
        m_faces.setPerson(faceId, personId);
    }
    for (const QString &p : touched)
        refreshPerson(p);
    tx.commit();
    emit peopleChanged();
}

void Catalog::rejectFaces(const QStringList &faceIds)
{
    PIX_PERF("Catalog::rejectFaces");
    QSet<QString> touched;
    const QString now = Database::timestamp();
    Transaction tx(m_db);
    for (const QString &faceId : faceIds) {
        const QString person = m_db.scalar(QStringLiteral("SELECT person_id FROM face WHERE id = ?"), {faceId}).toString();
        if (person.isEmpty())
            continue;
        touched.insert(person);
        m_db.exec(QStringLiteral("INSERT OR IGNORE INTO face_rejection(face_id, person_id, created_at) VALUES(?, ?, ?)"),
                  {faceId, person, now});
        m_db.exec(QStringLiteral("UPDATE face SET person_id = NULL, assignment = 'user', similarity = 0 WHERE id = ?"), {faceId});
        m_faces.setPerson(faceId, QString());
    }
    for (const QString &p : touched)
        refreshPerson(p);
    tx.commit();
    emit peopleChanged();
}

int Catalog::rejectPersonInPhotos(const QString &personId, const QStringList &photoIds)
{
    if (personId.isEmpty())
        return 0;
    QStringList faceIds;
    for (const QString &photoId : photoIds) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM face WHERE photo_id = ? AND person_id = ?"), {photoId, personId});
        while (q.next())
            faceIds << q.value(0).toString();
    }
    if (!faceIds.isEmpty())
        rejectFaces(faceIds);
    return int(faceIds.size());
}

// --------------------------------------------------------------------------- //
// Ao longo dos anos
// --------------------------------------------------------------------------- //

QList<timeline::Face> Catalog::timelineFaces(const QString &personId) const
{
    PIX_PERF("Catalog::timelineFaces");
    QList<timeline::Face> out;
    // Datas impossíveis (relógio da câmera errado: 1970, 2076) ficam fora.
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT f.id, f.photo_id, p.path, p.thumb_path, f.crop_path, p.name, p.year, "
                                           "COALESCE(p.month, 0), f.x, f.y, f.w, f.h, p.width, p.height, f.score, "
                                           "COALESCE(f.similarity, 0.5) FROM face f JOIN photo p ON p.id = f.photo_id "
                                           "WHERE f.person_id = ? AND p.private = 0 AND p.year BETWEEN 1900 AND ?"),
                            {personId, QDate::currentDate().year()});
    while (q.next()) {
        timeline::Face f;
        f.faceId = q.value(0).toString();
        f.photoId = q.value(1).toString();
        f.path = q.value(2).toString();
        f.thumbPath = q.value(3).toString();
        f.cropPath = q.value(4).toString();
        f.photoName = q.value(5).toString();
        f.year = q.value(6).toInt();
        f.month = q.value(7).toInt();
        f.x = q.value(8).toDouble();
        f.y = q.value(9).toDouble();
        f.w = q.value(10).toDouble();
        f.h = q.value(11).toDouble();
        f.width = q.value(12).toInt();
        f.height = q.value(13).toInt();
        f.score = q.value(14).toDouble();
        f.similarity = q.value(15).toDouble();
        out.append(f);
    }
    return out;
}

QHash<QString, timeline::Pick> Catalog::timelinePicks(const QString &personId, const QString &frequency) const
{
    QHash<QString, timeline::Pick> out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT period, COALESCE(face_id, ''), chosen_by, hidden FROM person_timeline "
                                           "WHERE person_id = ? AND frequency = ?"),
                            {personId, frequency});
    while (q.next()) {
        timeline::Pick p;
        p.period = q.value(0).toString();
        p.faceId = q.value(1).toString();
        p.user = q.value(2).toString() == QLatin1String("user");
        p.hidden = q.value(3).toInt() != 0;
        out.insert(p.period, p);
    }
    return out;
}

void Catalog::saveTimelinePicks(const QString &personId, const QString &frequency, const QList<timeline::Pick> &picks)
{
    if (picks.isEmpty() || personId.isEmpty() || !timeline::validFrequency(frequency))
        return;
    Transaction tx(m_db);
    for (const timeline::Pick &p : picks)
        m_db.exec(QStringLiteral("INSERT INTO person_timeline(person_id, frequency, period, face_id, chosen_by, hidden) "
                                 "VALUES(?, ?, ?, ?, ?, ?) ON CONFLICT(person_id, frequency, period) DO UPDATE SET "
                                 "face_id = excluded.face_id, chosen_by = excluded.chosen_by, hidden = excluded.hidden"),
                  {personId, frequency, p.period, p.faceId.isEmpty() ? QVariant() : QVariant(p.faceId),
                   p.user ? QStringLiteral("user") : QStringLiteral("auto"), p.hidden ? 1 : 0});
    tx.commit();
}

void Catalog::resetTimeline(const QString &personId, const QString &frequency)
{
    m_db.exec(QStringLiteral("DELETE FROM person_timeline WHERE person_id = ? AND frequency = ?"), {personId, frequency});
}

} // namespace pix

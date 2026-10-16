// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include "imaging/Faces.h"

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

QString Catalog::clusterFace(const QString &faceId, const imaging::Embedding &embedding, float *similarity)
{
    // O _assign_person do photoe: a pessoa mais parecida (cosseno com o
    // centroide) a partir do limiar; senão, uma pessoa nova. Pessoas de que
    // esta face foi tirada (face_rejection) não contam.
    const float threshold = setting(QStringLiteral("faces/matchSimilarity"), imaging::kFaceMatchSimilarity).toFloat();
    QSet<QString> rejected;
    QSqlQuery r = m_db.exec(QStringLiteral("SELECT person_id FROM face_rejection WHERE face_id = ?"), {faceId});
    while (r.next())
        rejected.insert(r.value(0).toString());
    QString best;
    float bestSim = 0.0f;
    for (auto it = m_people.constBegin(); it != m_people.constEnd(); ++it) {
        if (it->faces == 0 || rejected.contains(it.key()))
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
    if (!m_db.scalar(QStringLiteral("SELECT 1 FROM photo WHERE id = ?"), {r.photoId}).isValid()) {
        for (const FoundFace &f : r.faces)
            QFile::remove(f.cropPath);
        return;
    }
    if (!r.ok) {
        m_db.exec(QStringLiteral("UPDATE photo SET faces_state = 'error', error = ? WHERE id = ?"),
                  {QStringLiteral("Faces: ") + r.error.left(400), r.photoId});
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
    const float minPersonScore = setting(QStringLiteral("faces/minPersonScore"), 0.5).toFloat();
    for (const FoundFace &f : r.faces) {
        m_db.exec(QStringLiteral("INSERT INTO face(id, photo_id, x, y, w, h, score, embedding, model, crop_path, created_at) "
                                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"),
                  {f.faceId, r.photoId, f.x, f.y, f.w, f.h, f.score, imaging::toBlob(f.embedding),
                   QString::fromLatin1(imaging::FaceEngine::kName), f.cropPath, now});
        m_faces.insert({f.faceId, r.photoId, QString()}, f.embedding);
        if (f.score < minPersonScore)
            continue;
        float similarity = 0.0f;
        const QString person = clusterFace(f.faceId, f.embedding, &similarity);
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
}

int Catalog::autoMergePeople()
{
    // Junta duas pessoas quando os centroides são parecidos o bastante
    // (similaridade) E as duas são confiáveis: faces suficientes e confiança
    // média de detecção alta. Nomes diferentes nunca se juntam.
    const float minSim = setting(QStringLiteral("faces/mergeSimilarity"), 0.55).toFloat();
    const int minFaces = setting(QStringLiteral("faces/mergeMinFaces"), 3).toInt();
    const double minScore = setting(QStringLiteral("faces/mergeMinScore"), 0.6).toDouble();

    struct Candidate
    {
        QString id;
        QString name;
        imaging::Embedding centroid;
        int faces;
    };
    QList<Candidate> candidates;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT person_id, count(*), avg(score) FROM face WHERE person_id IS NOT NULL "
                                           "GROUP BY person_id HAVING count(*) >= ? AND avg(score) >= ?"),
                            {minFaces, minScore});
    while (q.next()) {
        const QString id = q.value(0).toString();
        imaging::Embedding c = centroidOf(id);
        if (!c.empty())
            candidates.append({id, m_people.value(id).name.trimmed(), std::move(c), q.value(1).toInt()});
    }
    struct Pair
    {
        int a, b;
        float sim;
    };
    std::vector<Pair> pairs;
    for (int i = 0; i < candidates.size(); ++i)
        for (int j = i + 1; j < candidates.size(); ++j) {
            const float s = imaging::dot(candidates[i].centroid, candidates[j].centroid);
            if (s >= minSim)
                pairs.push_back({i, j, s});
        }
    std::sort(pairs.begin(), pairs.end(), [](const Pair &x, const Pair &y) { return x.sim > y.sim; });
    QHash<int, int> root;
    auto find = [&](int i) {
        while (root.contains(i))
            i = root.value(i);
        return i;
    };
    int merged = 0;
    for (const Pair &p : pairs) {
        const int a = find(p.a), b = find(p.b);
        if (a == b)
            continue;
        const QString na = candidates[a].name, nb = candidates[b].name;
        if (!na.isEmpty() && !nb.isEmpty() && na.compare(nb, Qt::CaseInsensitive) != 0)
            continue;
        // Fica a nomeada; entre duas sem nome, a maior.
        const bool keepA = !na.isEmpty() || (nb.isEmpty() && candidates[a].faces >= candidates[b].faces);
        const int target = keepA ? a : b, source = keepA ? b : a;
        qCInfo(lcCatalog) << "Auto-merging person" << candidates[source].id << "into" << candidates[target].id
                          << "similarity" << p.sim;
        mergePeople(candidates[target].id, candidates[source].id);
        candidates[target].faces += candidates[source].faces;
        if (candidates[target].name.isEmpty())
            candidates[target].name = candidates[source].name;
        root.insert(source, target);
        ++merged;
    }
    if (merged)
        emit peopleChanged();
    return merged;
}

void Catalog::reclusterPeople()
{
    Transaction tx(m_db);
    // Fica o que a pessoa confirmou e o que está em pessoas com nome; o
    // automático das pessoas sem nome (e as faces soltas) é refeito.
    m_db.exec(QStringLiteral("UPDATE face SET person_id = NULL WHERE assignment = 'auto' AND "
                             "person_id IN (SELECT id FROM person WHERE name = '')"));
    m_db.exec(QStringLiteral("DELETE FROM person WHERE name = '' AND id NOT IN "
                             "(SELECT person_id FROM face WHERE person_id IS NOT NULL)"));
    QSqlQuery all = m_db.exec(QStringLiteral("SELECT id, person_id FROM face"));
    while (all.next())
        m_faces.setPerson(all.value(0).toString(), all.value(1).toString());
    loadPeople();
    QStringList faceIds;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM face WHERE assignment = 'auto' AND person_id IS NULL "
                                           "AND score >= ? ORDER BY score DESC"),
                            {setting(QStringLiteral("faces/minPersonScore"), 0.5).toDouble()});
    while (q.next())
        faceIds << q.value(0).toString();
    QSet<QString> touched;
    for (const QString &faceId : faceIds) {
        float similarity = 0.0f;
        const QString person = clusterFace(faceId, m_faces.embeddingOf(faceId), &similarity);
        m_db.exec(QStringLiteral("UPDATE face SET person_id = ?, similarity = ? WHERE id = ?"), {person, similarity, faceId});
        m_faces.setPerson(faceId, person);
        touched.insert(person);
    }
    QSqlQuery people = m_db.exec(QStringLiteral("SELECT id FROM person"));
    QStringList ids;
    while (people.next())
        ids << people.value(0).toString();
    for (const QString &id : ids)
        refreshPerson(id);
    tx.commit();
    autoMergePeople();
    emit peopleChanged();
}

// --------------------------------------------------------------------------- //
// Pessoas: leitura
// --------------------------------------------------------------------------- //

QVariantList Catalog::people() const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT p.id, p.name, f.crop_path, p.face_count, p.photo_count FROM person p "
        "LEFT JOIN face f ON f.id = p.cover_face_id WHERE p.face_count > 0 OR p.name != '' "
        "ORDER BY (p.name = ''), p.name COLLATE NOCASE, p.face_count DESC"));
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

} // namespace pix

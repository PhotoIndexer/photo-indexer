// SPDX-License-Identifier: BSD-2-Clause
#include "PeopleController.h"

#include "ImageProviders.h"

#include "core/Catalog.h"
#include "core/CatalogReader.h"
#include "core/Pipeline.h"
#include "imaging/Faces.h"

#include <QImage>
#include <QSqlQuery>

#include <cmath>

using namespace pix;

namespace {

QString fileUrl(const QString &path)
{
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

} // namespace

PeopleController::PeopleController(Catalog *catalog, Pipeline *pipeline, CatalogReader *reader, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_pipeline(pipeline)
    , m_reader(reader)
    , m_threshold(catalog->setting(QStringLiteral("faces/matchSimilarity"), imaging::kFaceMatchSimilarity).toDouble())
{
    // Indexando, as pessoas mudam a cada foto com rosto: a lista é refeita no
    // leitor, no máximo a cada 5 s (parado, quase na hora).
    m_reloadTimer.setSingleShot(true);
    connect(&m_reloadTimer, &QTimer::timeout, this, [this]() {
        reload();
        // Busca por rosto aberta: os nomes e as junções aparecem na lista
        // de "quem pode ser" (a pessoa junta some, a nomeada muda de nome).
        if (!m_queryEmbeddings.empty())
            runSearch(true);
    });
    connect(catalog, &Catalog::peopleChanged, this, [this]() {
        if (!m_reloadTimer.isActive())
            m_reloadTimer.start(m_pipeline->busy() ? 5000 : 200);
    });
    reload();
}

bool PeopleController::facesAvailable() const
{
    return m_pipeline->faceEngine().available();
}

void PeopleController::reload()
{
    struct People
    {
        QVariantList people;
        int unassigned = 0;
    };
    const int serial = ++m_reloadSerial;
    m_reader->read<People>(
        this,
        [](const Catalog &c) { return People{c.people(), int(c.unassignedFaces().size())}; },
        [this, serial](People r) {
            if (serial != m_reloadSerial)
                return;
            QVariantList people;
            int unnamed = 0;
            for (const QVariant &v : r.people) {
                QVariantMap p = v.toMap();
                const QString name = p.value(QStringLiteral("name")).toString();
                p.insert(QStringLiteral("label"), name.isEmpty() ? tr("Person %1").arg(++unnamed) : name);
                p.insert(QStringLiteral("coverUrl"), fileUrl(p.value(QStringLiteral("coverPath")).toString()));
                people << p;
            }
            // Igual ao que já está: a tela não refaz a grade.
            if (people == m_people && r.unassigned == m_unassignedCount)
                return;
            m_people = people;
            m_list.setPeople(people);
            m_unassignedCount = r.unassigned;
            emit peopleChanged();
            if (!m_personId.isEmpty() || m_unassignedOpen)
                loadPerson();
        });
}

QString PeopleController::labelOf(const QString &personId) const
{
    for (const QVariant &v : m_people)
        if (v.toMap().value(QStringLiteral("id")).toString() == personId)
            return v.toMap().value(QStringLiteral("label")).toString();
    // Fora da lista (sem nome e com poucas fotos — aparece só na busca por
    // rosto): o nome, se tiver, ou um rótulo genérico.
    const QString name = m_catalog->person(personId).value(QStringLiteral("name")).toString();
    return name.isEmpty() ? tr("Unnamed person") : name;
}

void PeopleController::openPerson(const QString &personId)
{
    m_personId = personId;
    m_unassignedOpen = personId.isEmpty();
    loadPerson();
}

void PeopleController::closePerson()
{
    m_personId.clear();
    m_unassignedOpen = false;
    m_person.clear();
    m_faces.clear();
    emit personChanged();
}

void PeopleController::setLeastSimilarFirst(bool on)
{
    if (on == m_leastFirst)
        return;
    m_leastFirst = on;
    loadPerson();
}

void PeopleController::loadPerson()
{
    QVariantList faces;
    if (m_unassignedOpen) {
        m_person = {{QStringLiteral("id"), QString()}, {QStringLiteral("label"), tr("Faces without a person")}};
        faces = m_catalog->unassignedFaces();
    } else {
        m_person = m_catalog->person(m_personId);
        if (m_person.isEmpty()) {
            // Sumiu (juntada em outra, apagada): volta à lista.
            m_personId.clear();
            m_faces.clear();
            emit personChanged();
            return;
        }
        m_person.insert(QStringLiteral("label"), labelOf(m_personId));
        m_person.insert(QStringLiteral("coverUrl"), fileUrl(m_person.value(QStringLiteral("coverPath")).toString()));
        faces = m_catalog->personFaces(m_personId);
        if (m_leastFirst)
            std::reverse(faces.begin(), faces.end());
    }
    m_faces.clear();
    for (const QVariant &v : faces) {
        QVariantMap f = v.toMap();
        f.insert(QStringLiteral("cropUrl"), fileUrl(f.value(QStringLiteral("cropPath")).toString()));
        m_faces << f;
    }
    emit personChanged();
}

void PeopleController::rename(const QString &personId, const QString &name)
{
    m_catalog->renamePerson(personId, name);
}

void PeopleController::merge(const QString &targetId, const QString &sourceId)
{
    m_catalog->mergePeople(targetId, sourceId);
    if (m_personId == sourceId)
        openPerson(targetId);
}

void PeopleController::remove(const QString &personId)
{
    m_catalog->deletePerson(personId);
    if (m_personId == personId)
        m_personId.clear();
}

void PeopleController::rejectFaces(const QStringList &faceIds)
{
    m_catalog->rejectFaces(faceIds);
}

void PeopleController::assignFaces(const QStringList &faceIds, const QString &personId)
{
    m_catalog->assignFaces(faceIds, personId);
}

QString PeopleController::createPerson(const QString &name, const QStringList &faceIds)
{
    return m_catalog->createPerson(name, faceIds);
}

QString PeopleController::personNamed(const QString &name, const QString &exceptId) const
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        return {};
    for (const QVariant &v : m_people) {
        const QVariantMap p = v.toMap();
        if (p.value(QStringLiteral("id")).toString() != exceptId
            && p.value(QStringLiteral("name")).toString().compare(clean, Qt::CaseInsensitive) == 0)
            return p.value(QStringLiteral("id")).toString();
    }
    return {};
}

// --------------------------------------------------------------------------- //
// Busca por rosto
// --------------------------------------------------------------------------- //

void PeopleController::searchByPhoto(const QUrl &file)
{
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const int serial = ++m_searchSerial;
    m_detecting = true;
    m_queryPerson.clear();
    m_searchError.clear();
    m_queryFaces.clear();
    m_queryEmbeddings.clear();
    m_matched.clear();
    m_results.setRows({});
    m_queryUrl = QUrl::fromLocalFile(path).toString();
    emit searchChanged();
    m_pipeline->detectFaces(path, [this, serial](QList<imaging::DetectedFace> faces, QString error) {
        if (serial != m_searchSerial)
            return;
        m_detecting = false;
        m_searchError = error;
        // O maior rosto primeiro: numa selfie, é quem a pessoa quer achar.
        std::sort(faces.begin(), faces.end(), [](const auto &a, const auto &b) { return a.box.area() > b.box.area(); });
        for (int i = 0; i < faces.size(); ++i) {
            const QString key = QStringLiteral("query-%1-%2").arg(serial).arg(i);
            const QByteArray crop = faces[i].crop;
            ImageRegistry::instance().put(QStringLiteral("file"), key, [crop]() { return QImage::fromData(crop); });
            m_queryFaces << QVariantMap{{QStringLiteral("index"), i},
                                        {QStringLiteral("cropUrl"), QStringLiteral("image://photo/file/") + key},
                                        {QStringLiteral("score"), faces[i].score}};
            m_queryEmbeddings.push_back(faces[i].embedding);
        }
        if (m_searchError.isEmpty() && faces.isEmpty())
            m_searchError = QStringLiteral("faces.noneFound");
        m_queryFace = 0;
        runSearch();
    });
}

void PeopleController::searchByFace(const QString &faceId)
{
    ++m_searchSerial;
    m_detecting = false;
    m_queryPerson.clear();
    m_searchError.clear();
    m_queryFaces.clear();
    m_queryEmbeddings.clear();
    const imaging::Embedding e = m_catalog->faceIndex().embeddingOf(faceId);
    const QString crop = m_catalog->db().scalar(QStringLiteral("SELECT crop_path FROM face WHERE id = ?"), {faceId}).toString();
    m_queryUrl = fileUrl(crop);
    if (!e.empty()) {
        m_queryFaces << QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("cropUrl"), fileUrl(crop)}, {QStringLiteral("score"), 1.0}};
        m_queryEmbeddings.push_back(e);
    }
    m_queryFace = 0;
    runSearch();
}

void PeopleController::searchByPerson(const QString &personId)
{
    ++m_searchSerial;
    m_detecting = false;
    m_searchError.clear();
    m_queryFaces.clear();
    m_queryEmbeddings.clear();
    m_queryPerson = personId;
    const QVariantMap p = m_catalog->person(personId);
    m_queryUrl = fileUrl(p.value(QStringLiteral("coverPath")).toString());
    imaging::Embedding centroid =
        imaging::fromBlob(m_catalog->db().scalar(QStringLiteral("SELECT centroid FROM person WHERE id = ?"), {personId}).toByteArray());
    double norm = 0;
    for (float v : centroid)
        norm += double(v) * v;
    if (norm > 0) {
        for (float &v : centroid)
            v = float(v / std::sqrt(norm));
        m_queryEmbeddings.push_back(centroid);
    } else {
        m_searchError = QStringLiteral("faces.noneFound");
    }
    m_queryFace = 0;
    runSearch();
}

void PeopleController::setQueryFace(int index)
{
    if (index == m_queryFace || index < 0 || index >= int(m_queryEmbeddings.size()))
        return;
    m_queryFace = index;
    runSearch();
}

void PeopleController::setThreshold(double value)
{
    const double v = std::clamp(value, 0.2, 0.9);
    if (qFuzzyCompare(v, m_threshold))
        return;
    m_threshold = v;
    runSearch();
}

void PeopleController::runSearch(bool refresh)
{
    m_matched.clear();
    if (m_queryFace < 0 || m_queryFace >= int(m_queryEmbeddings.size())) {
        m_results.setRows({});
        emit searchChanged();
        return;
    }
    const auto matches = m_catalog->searchFace(m_queryEmbeddings[size_t(m_queryFace)], float(m_threshold));
    // Fotos na ordem da melhor face; pessoas pela melhor similaridade.
    QList<QPair<QString, float>> ranked;
    QHash<QString, QPair<float, int>> people;
    for (const FaceIndex::Match &m : matches) {
        if (!m_queryPerson.isEmpty() && m.entry.personId == m_queryPerson)
            continue;  // as dela mesma: o que interessa são as outras
        ranked.append({m.entry.photoId, m.similarity});
        if (!m.entry.personId.isEmpty()) {
            auto &p = people[m.entry.personId];
            p.first = std::max(p.first, m.similarity);
            ++p.second;
        }
    }
    m_results.setRows(m_catalog->photosByIds(ranked), refresh);
    for (auto it = people.constBegin(); it != people.constEnd(); ++it) {
        const QVariantMap p = m_catalog->person(it.key());
        m_matched << QVariantMap{{QStringLiteral("id"), it.key()},
                                 {QStringLiteral("label"), labelOf(it.key())},
                                 {QStringLiteral("coverUrl"), fileUrl(p.value(QStringLiteral("coverPath")).toString())},
                                 {QStringLiteral("similarity"), it.value().first},
                                 {QStringLiteral("faces"), it.value().second}};
    }
    std::sort(m_matched.begin(), m_matched.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("similarity")).toFloat() > b.toMap().value(QStringLiteral("similarity")).toFloat();
    });
    emit searchChanged();
}

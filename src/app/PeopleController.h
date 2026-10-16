// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O catálogo de pessoas (App.people no QML): a lista, a pessoa aberta e as
// faces dela, as correções (nome, juntar, tirar ou pôr faces) e a busca por
// rosto a partir de uma foto que a pessoa escolhe no disco.

#include "PhotoGridModel.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace pix {
class Catalog;
class Pipeline;
}

class PeopleController : public QObject
{
    Q_OBJECT
    /// [{id, name, coverUrl, faceCount, photoCount, label}]
    Q_PROPERTY(QVariantList people READ people NOTIFY peopleChanged)
    Q_PROPERTY(int unassignedCount READ unassignedCount NOTIFY peopleChanged)
    Q_PROPERTY(QString personId READ personId NOTIFY personChanged)
    Q_PROPERTY(QVariantMap person READ person NOTIFY personChanged)
    /// Faces da pessoa aberta (ou as sem pessoa): [{id, photoId, cropUrl, score,
    /// similarity, assignment, photoName}]
    Q_PROPERTY(QVariantList faces READ faces NOTIFY personChanged)
    /// Menos parecidas primeiro: os falsos positivos aparecem no topo.
    Q_PROPERTY(bool leastSimilarFirst READ leastSimilarFirst WRITE setLeastSimilarFirst NOTIFY personChanged)

    // Busca por rosto
    Q_PROPERTY(bool detecting READ detecting NOTIFY searchChanged)
    Q_PROPERTY(QString queryUrl READ queryUrl NOTIFY searchChanged)
    /// Rostos da foto enviada: [{index, cropUrl, score}]
    Q_PROPERTY(QVariantList queryFaces READ queryFaces NOTIFY searchChanged)
    Q_PROPERTY(int queryFace READ queryFace WRITE setQueryFace NOTIFY searchChanged)
    Q_PROPERTY(double threshold READ threshold WRITE setThreshold NOTIFY searchChanged)
    /// Pessoas do catálogo que casam: [{id, label, coverUrl, similarity, faces}]
    Q_PROPERTY(QVariantList matchedPeople READ matchedPeople NOTIFY searchChanged)
    Q_PROPERTY(PhotoGridModel *results READ results CONSTANT)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)
    Q_PROPERTY(bool facesAvailable READ facesAvailable CONSTANT)

public:
    PeopleController(pix::Catalog *catalog, pix::Pipeline *pipeline, QObject *parent = nullptr);

    QVariantList people() const { return m_people; }
    int unassignedCount() const { return m_unassignedCount; }
    QString personId() const { return m_personId; }
    QVariantMap person() const { return m_person; }
    QVariantList faces() const { return m_faces; }
    bool leastSimilarFirst() const { return m_leastFirst; }
    void setLeastSimilarFirst(bool on);

    bool detecting() const { return m_detecting; }
    QString queryUrl() const { return m_queryUrl; }
    QVariantList queryFaces() const { return m_queryFaces; }
    int queryFace() const { return m_queryFace; }
    void setQueryFace(int index);
    double threshold() const { return m_threshold; }
    void setThreshold(double value);
    QVariantList matchedPeople() const { return m_matched; }
    PhotoGridModel *results() { return &m_results; }
    QString searchError() const { return m_searchError; }
    bool facesAvailable() const;

    Q_INVOKABLE void reload();
    /// Abre uma pessoa ("" = as faces sem pessoa).
    Q_INVOKABLE void openPerson(const QString &personId);
    /// Volta para a lista.
    Q_INVOKABLE void closePerson();
    Q_INVOKABLE void rename(const QString &personId, const QString &name);
    Q_INVOKABLE void merge(const QString &targetId, const QString &sourceId);
    Q_INVOKABLE void remove(const QString &personId);
    Q_INVOKABLE void rejectFaces(const QStringList &faceIds);
    Q_INVOKABLE void assignFaces(const QStringList &faceIds, const QString &personId);
    Q_INVOKABLE QString createPerson(const QString &name, const QStringList &faceIds);
    /// Pessoa com o mesmo nome (sem diferenciar maiúsculas), para oferecer juntar.
    Q_INVOKABLE QString personNamed(const QString &name, const QString &exceptId) const;
    Q_INVOKABLE QString labelOf(const QString &personId) const;

    /// Busca as fotos de quem está na foto ``file`` (escolhida no disco).
    Q_INVOKABLE void searchByPhoto(const QUrl &file);
    /// Busca a partir de uma face já catalogada (o rosto de uma foto).
    Q_INVOKABLE void searchByFace(const QString &faceId);

signals:
    void peopleChanged();
    void personChanged();
    void searchChanged();
    void failed(const QString &key, const QVariantMap &params);

private:
    void loadPerson();
    void runSearch();

    pix::Catalog *m_catalog;
    pix::Pipeline *m_pipeline;
    QVariantList m_people;
    int m_unassignedCount = 0;
    QString m_personId;
    bool m_unassignedOpen = false;
    QVariantMap m_person;
    QVariantList m_faces;
    bool m_leastFirst = false;

    bool m_detecting = false;
    QString m_queryUrl;
    QVariantList m_queryFaces;
    std::vector<std::vector<float>> m_queryEmbeddings;
    int m_queryFace = 0;
    double m_threshold = 0.45;
    QVariantList m_matched;
    PhotoGridModel m_results;
    QString m_searchError;
    int m_searchSerial = 0;
};

// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// A foto aberta (App.viewer no QML) — detalhe é uma página, não um modal
// (regra 3): a imagem grande, os metadados, as categorias, os álbuns, as
// faces e o EXIF completo; anterior/próxima seguem a grade da biblioteca.

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

namespace pix {
class Catalog;
}
class PhotoGridModel;

class ViewerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString photoId READ photoId NOTIFY photoChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY photoChanged)
    Q_PROPERTY(QString imageUrl READ imageUrl NOTIFY photoChanged)
    /// Mesmos pixels (identical) e parecidas pela árvore de hashes.
    Q_PROPERTY(QVariantList similar READ similar NOTIFY photoChanged)
    Q_PROPERTY(int position READ position NOTIFY photoChanged)
    Q_PROPERTY(int total READ total NOTIFY photoChanged)

public:
    ViewerController(pix::Catalog *catalog, PhotoGridModel *grid, QObject *parent = nullptr);

    QString photoId() const { return m_photoId; }
    QVariantMap detail() const { return m_detail; }
    QString imageUrl() const { return m_imageUrl; }
    QVariantList similar() const { return m_similar; }
    int position() const;
    int total() const;

    Q_INVOKABLE void open(const QString &photoId);
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void reload();

    Q_INVOKABLE void addCategory(const QString &categoryId);
    Q_INVOKABLE void removeCategory(const QString &categoryId);
    Q_INVOKABLE void addAlbum(const QString &albumId);
    Q_INVOKABLE void removeAlbum(const QString &albumId);
    /// Esta foto: privada (cifra o original) / pública de novo.
    Q_INVOKABLE void makePrivate();
    Q_INVOKABLE void makePublic();
    /// A face é desta pessoa / NÃO é / é de uma pessoa nova com este nome.
    Q_INVOKABLE void assignFace(const QString &faceId, const QString &personId);
    Q_INVOKABLE void rejectFace(const QString &faceId);
    Q_INVOKABLE void newPersonFromFace(const QString &faceId, const QString &name);

private:
    template<typename F>
    void act(F &&fn);

signals:
    void photoChanged();
    void failed(const QString &key, const QVariantMap &params);

private:
    void load();

    pix::Catalog *m_catalog;
    PhotoGridModel *m_grid;
    QString m_photoId;
    QVariantMap m_detail;
    QString m_imageUrl;
    QVariantList m_similar;
    int m_serial = 0;
};

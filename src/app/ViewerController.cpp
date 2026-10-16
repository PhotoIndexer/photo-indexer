// SPDX-License-Identifier: BSD-2-Clause
#include "ViewerController.h"

#include "ImageProviders.h"
#include "PhotoGridModel.h"

#include "core/Catalog.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"

#include <QFileInfo>
#include <QImage>
#include <QUrl>

using namespace pix;

namespace {

/// Lado maior da foto no visualizador.
constexpr int kViewSide = 2560;
/// Distância de Hamming máxima para "parecida" na árvore de hashes.
constexpr int kSimilarDistance = 8;

} // namespace

ViewerController::ViewerController(Catalog *catalog, PhotoGridModel *grid, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_grid(grid)
{
    connect(catalog, &Catalog::peopleChanged, this, [this]() {
        if (!m_photoId.isEmpty())
            reload();
    });
}

int ViewerController::position() const
{
    return m_grid->indexOf(m_photoId);
}

int ViewerController::total() const
{
    return m_grid->count();
}

void ViewerController::open(const QString &photoId)
{
    m_photoId = photoId;
    load();
}

void ViewerController::load()
{
    m_detail = m_catalog->photoDetail(m_photoId);
    m_similar = m_catalog->similarPhotos(m_photoId, kSimilarDistance);
    const QString path = m_catalog->photoPath(m_photoId);
    const QString thumb = m_detail.value(QStringLiteral("thumbPath")).toString();
    // Disco desconectado (ou arquivo que sumiu): a miniatura do cache em
    // ~/.photo_indexer continua aí — mostra ela, avisando.
    const bool offline = !QFileInfo::exists(path);
    m_detail.insert(QStringLiteral("offline"), offline);
    // A chave muda a cada abertura: o QML não reaproveita a imagem velha se o
    // arquivo mudou no disco.
    const QString key = m_photoId + u'-' + QString::number(++m_serial);
    ImageRegistry::instance().put(QStringLiteral("view"), key, [path, thumb, offline]() {
        QImage image;
        if (!offline)
            image = imaging::toQImage(imaging::loadDisplayImage(path, kViewSide));
        if (image.isNull() && !thumb.isEmpty())
            image = QImage(thumb);
        return image;
    });
    m_imageUrl = QStringLiteral("image://photo/view/") + key;
    emit photoChanged();
}

void ViewerController::reload()
{
    // Só os dados: a imagem fica.
    const bool offline = m_detail.value(QStringLiteral("offline")).toBool();
    m_detail = m_catalog->photoDetail(m_photoId);
    m_detail.insert(QStringLiteral("offline"), offline);
    m_similar = m_catalog->similarPhotos(m_photoId, kSimilarDistance);
    emit photoChanged();
}

void ViewerController::next()
{
    const int i = position();
    if (i >= 0 && i + 1 < m_grid->count())
        open(m_grid->idAt(i + 1));
}

void ViewerController::previous()
{
    const int i = position();
    if (i > 0)
        open(m_grid->idAt(i - 1));
}

void ViewerController::addCategory(const QString &categoryId)
{
    m_catalog->addPhotosToCategory({m_photoId}, categoryId);
    reload();
}

void ViewerController::removeCategory(const QString &categoryId)
{
    m_catalog->removePhotosFromCategory({m_photoId}, categoryId);
    reload();
}

void ViewerController::addAlbum(const QString &albumId)
{
    m_catalog->addPhotosToAlbum({m_photoId}, albumId);
    reload();
}

void ViewerController::removeAlbum(const QString &albumId)
{
    m_catalog->removePhotosFromAlbum({m_photoId}, albumId);
    reload();
}

void ViewerController::assignFace(const QString &faceId, const QString &personId)
{
    m_catalog->assignFaces({faceId}, personId);
}

void ViewerController::rejectFace(const QString &faceId)
{
    m_catalog->rejectFaces({faceId});
}

void ViewerController::newPersonFromFace(const QString &faceId, const QString &name)
{
    m_catalog->createPerson(name, {faceId});
}

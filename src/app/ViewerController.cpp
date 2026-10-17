// SPDX-License-Identifier: BSD-2-Clause
#include "ViewerController.h"

#include "core/Perf.h"

#include "ImageProviders.h"
#include "PhotoGridModel.h"

#include "core/Catalog.h"
#include "core/Crypto.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUrl>

using namespace pix;

namespace {

/// Lado maior da foto no visualizador.
constexpr int kViewSide = 2560;
/// Distância de Hamming máxima para "parecida" na árvore de hashes.
constexpr int kSimilarDistance = 8;
/// As parecidas mais próximas que a tela mostra (cada uma é uma miniatura
/// carregada de uma vez — um Repeater, não uma grade preguiçosa).
constexpr int kMaxSimilar = 48;

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
    // A etapa de faces passou por esta foto (mesmo sem achar rosto): a
    // seção Pessoas sai de "na fila".
    connect(catalog, &Catalog::facesDone, this, [this](const QString &photoId) {
        if (photoId == m_photoId)
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
    PIX_PERF("ViewerController::load");
    m_detail = m_catalog->photoDetail(m_photoId);
    m_similar = m_catalog->similarPhotos(m_photoId, kSimilarDistance).mid(0, kMaxSimilar);
    const QString path = m_catalog->photoPath(m_photoId);
    const QString thumb = m_detail.value(QStringLiteral("thumbPath")).toString();
    const bool isPrivate = m_detail.value(QStringLiteral("private")).toInt() == 1;
    // "private" é palavra reservada no JS: o QML lê ``isPrivate``.
    m_detail.insert(QStringLiteral("isPrivate"), isPrivate);
    const KeySnapshot keys = isPrivate ? m_catalog->keySnapshot() : KeySnapshot();
    // Disco desconectado (ou arquivo que sumiu): a miniatura do cache em
    // ~/.photo_indexer continua aí — mostra ela, avisando.
    const bool offline = !QFileInfo::exists(path);
    m_detail.insert(QStringLiteral("offline"), offline);
    // A chave muda a cada abertura: o QML não reaproveita a imagem velha se o
    // arquivo mudou no disco.
    const QString key = m_photoId + u'-' + QString::number(++m_serial);
    ImageRegistry::instance().put(QStringLiteral("view"), key, [path, thumb, offline, keys]() {
        QImage image;
        const bool encrypted = path.endsWith(QStringLiteral(".") + QLatin1String(crypto::kEncryptedExtension));
        if (!offline && encrypted) {
            // Decifra na memória: o original em claro nunca vai para o disco.
            QString name;
            if (const auto plain = crypto::decryptFile(keys.keys, path, &name))
                image = imaging::toQImage(imaging::loadDisplayImageData(*plain, name, kViewSide));
        } else if (!offline) {
            image = imaging::toQImage(imaging::loadDisplayImage(path, kViewSide));
        }
        if (image.isNull() && thumb.endsWith(QLatin1String(".pidx"))) {
            QFile f(thumb);
            if (f.open(QIODevice::ReadOnly))
                if (const auto plain = crypto::decryptBytes(keys.keys, f.readAll()))
                    image = QImage::fromData(*plain);
        } else if (image.isNull() && !thumb.isEmpty()) {
            image = QImage(thumb);
        }
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
    m_detail.insert(QStringLiteral("isPrivate"), m_detail.value(QStringLiteral("private")).toInt() == 1);
    m_similar = m_catalog->similarPhotos(m_photoId, kSimilarDistance).mid(0, kMaxSimilar);
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
    act([&]() { m_catalog->addPhotosToAlbum({m_photoId}, albumId); });
    reload();
}

void ViewerController::removeAlbum(const QString &albumId)
{
    act([&]() { m_catalog->removePhotosFromAlbum({m_photoId}, albumId); });
    reload();
}

void ViewerController::makePrivate()
{
    act([&]() { m_catalog->makePrivate({m_photoId}); });
    reload();
}

void ViewerController::makePublic()
{
    act([&]() { m_catalog->makePublic({m_photoId}); });
    reload();
}

template<typename F>
void ViewerController::act(F &&fn)
{
    try {
        fn();
    } catch (const ActionError &e) {
        emit failed(e.key(), e.params());
    } catch (const std::exception &e) {
        emit failed(QStringLiteral("error.generic"), {{QStringLiteral("detail"), QString::fromUtf8(e.what())}});
    }
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

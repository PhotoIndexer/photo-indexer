// SPDX-License-Identifier: BSD-2-Clause
#include "ImageProviders.h"

#include <QDir>
#include <QFile>
#include <QPainter>
#include <QRunnable>
#include <QSvgRenderer>
#include <QThreadPool>
#include <QUrl>
#include <QUrlQuery>

namespace {

class Response : public QQuickImageResponse, public QRunnable
{
public:
    Response(ImageRegistry::Producer producer, const QSize &requested)
        : m_producer(std::move(producer))
        , m_requested(requested)
    {
        setAutoDelete(false);
    }

    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override
    {
        return m_image.isNull() ? QStringLiteral("image unavailable") : QString();
    }

    void run() override
    {
        try {
            if (m_producer)
                m_image = m_producer();
        } catch (const std::exception &e) {
            qWarning("image provider: %s", e.what());
        }
        if (!m_image.isNull() && m_requested.isValid() && m_requested.width() > 0 && m_requested.height() > 0
            && (m_image.width() > m_requested.width() || m_image.height() > m_requested.height()))
            m_image = m_image.scaled(m_requested, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        emit finished();
    }

private:
    ImageRegistry::Producer m_producer;
    QSize m_requested;
    QImage m_image;
};

QThreadPool &imagePool()
{
    // Pool próprio: a foto do visualizador não espera a fila das etapas.
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(2);
        return p;
    }();
    return *pool;
}

} // namespace

QString thumbnailUrl(const QString &path)
{
    if (path.isEmpty())
        return {};
    if (path.endsWith(QLatin1String(".") + QLatin1String(pix::crypto::kThumbnailExtension)))
        return QStringLiteral("image://private/")
            + QString::fromLatin1(QDir::fromNativeSeparators(path).toUtf8().toBase64(QByteArray::Base64UrlEncoding
                                                                                   | QByteArray::OmitTrailingEquals));
    return QUrl::fromLocalFile(QDir::fromNativeSeparators(path)).toString();
}

QQuickImageResponse *PrivateImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    const QString path = QString::fromUtf8(QByteArray::fromBase64(id.section(u'?', 0, 0).toLatin1(),
                                                                  QByteArray::Base64UrlEncoding));
    auto keys = m_keys;
    auto *response = new Response(
        [path, keys]() {
            // Só arquivos cifrados da pasta de dados, e só com o cofre aberto.
            if (!keys || !keys->isOpen() || !path.endsWith(QLatin1String(".") + QLatin1String(pix::crypto::kThumbnailExtension)))
                return QImage();
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly))
                return QImage();
            const auto plain = pix::crypto::decryptBytes(keys->keys(), f.readAll());
            return plain ? QImage::fromData(*plain) : QImage();
        },
        requestedSize);
    imagePool().start(response);
    return response;
}

ImageRegistry &ImageRegistry::instance()
{
    static ImageRegistry registry;
    return registry;
}

void ImageRegistry::put(const QString &kind, const QString &key, Producer producer)
{
    QMutexLocker locker(&m_lock);
    m_producers.insert(kind + u'/' + key, std::move(producer));
}

ImageRegistry::Producer ImageRegistry::take(const QString &kind, const QString &key) const
{
    QMutexLocker locker(&m_lock);
    return m_producers.value(kind + u'/' + key);
}

void ImageRegistry::clear()
{
    QMutexLocker locker(&m_lock);
    m_producers.clear();
}

QQuickImageResponse *PhotoImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    // id = "<tipo>/<chave>[?...]"; a query só serve para mudar a URL.
    const QString path = id.section(u'?', 0, 0);
    auto *response = new Response(ImageRegistry::instance().take(path.section(u'/', 0, 0), path.section(u'/', 1)),
                                  requestedSize);
    imagePool().start(response);
    return response;
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id = "<nome>?color=#rrggbb[&fill=1]": troca o currentColor do SVG do
    // Lucide; ``fill`` preenche o desenho na mesma cor.
    const QString name = id.section(u'?', 0, 0);
    const QUrlQuery query(id.section(u'?', 1));
    const QString color = query.queryItemValue(QStringLiteral("color"));
    QFile f(QStringLiteral(":/qt/qml/PhotoIDX/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray svg = f.readAll();
    if (query.queryItemValue(QStringLiteral("fill")) == QLatin1String("1"))
        svg.replace("fill=\"none\"", "fill=\"currentColor\"");
    if (!color.isEmpty())
        svg.replace("currentColor", color.toLatin1());
    QSvgRenderer renderer(svg);
    const QSize out = requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
    QImage image(out, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    if (size)
        *size = out;
    return image;
}

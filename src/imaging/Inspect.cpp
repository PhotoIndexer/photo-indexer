// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Inspect.h"

#include "imaging/Formats.h"
#include "imaging/Hashes.h"
#include "imaging/ImageIO.h"
#include "imaging/Memory.h"
#include "imaging/Raw.h"

#include <QBuffer>
#include <QFile>
#include <QImageReader>

#include <opencv2/imgproc.hpp>

namespace pix::imaging {

namespace {

bool isJpegExt(const QString &ext)
{
    return ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("jpe")
        || ext == QLatin1String("jfif");
}

/// EXIF guardado num chunk de PNG ("eXIf") ou de WebP ("EXIF").
QByteArray embeddedExif(const QByteArray &data, const QString &ext)
{
    if (ext == QLatin1String("png")) {
        qsizetype pos = 8;
        while (pos + 12 <= data.size()) {
            const auto *p = reinterpret_cast<const uchar *>(data.constData() + pos);
            const quint32 len = (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | p[3];
            const QByteArray type = data.mid(pos + 4, 4);
            if (type == "eXIf")
                return data.mid(pos + 8, len);
            if (type == "IDAT" || type == "IEND")
                break;
            pos += 12 + qsizetype(len);
        }
    } else if (ext == QLatin1String("webp") && data.startsWith("RIFF")) {
        qsizetype pos = 12;
        while (pos + 8 <= data.size()) {
            const auto *p = reinterpret_cast<const uchar *>(data.constData() + pos + 4);
            const quint32 len = quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
            if (data.mid(pos, 4) == "EXIF")
                return data.mid(pos + 8, len);
            pos += 8 + qsizetype(len) + (len & 1);
        }
    } else if ((ext == QLatin1String("tif") || ext == QLatin1String("tiff"))
               && (data.startsWith("II*") || data.startsWith("MM\0*"))) {
        return data;  // o próprio arquivo é o TIFF
    }
    return {};
}

/// Imagem pelo QImageReader (formatos que o OpenCV deste build não abre).
cv::Mat readWithQt(const QByteArray &data, int maxSide, int *fullWidth, int *fullHeight)
{
    QByteArray copy = data;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    QSize size = reader.size();
    if (size.isValid()) {
        if (reader.transformation() & QImageIOHandler::TransformationRotate90)
            size.transpose();
        *fullWidth = size.width();
        *fullHeight = size.height();
    }
    const QImage image = reader.read();
    if (image.isNull())
        return {};
    if (*fullWidth <= 0) {
        *fullWidth = image.width();
        *fullHeight = image.height();
    }
    return fitTo(fromQImage(image), maxSide);
}

/// Memória de uma decodificação completa: a imagem, a cópia ao orientar e a
/// conversão para RGB (``channels`` por ponto), mais o arquivo lido.
qint64 decodeBytes(qint64 width, qint64 height, int channels, qint64 fileBytes)
{
    if (width <= 0 || height <= 0)
        return fileBytes * 12;  // sem cabeçalho: estimativa pela compressão típica
    return width * height * channels * 3 + fileBytes;
}

/// Grande demais para decodificar inteira: fica catalogada, sem imagem.
bool tooLarge(Inspection &out, qint64 width, qint64 height)
{
    if (width * height <= kMaxDecodePixels)
        return false;
    out.ok = true;
    out.width = int(width);
    out.height = int(height);
    out.error = QStringLiteral("image too large to decode (%1 MP)").arg(width * height / 1000000);
    return true;
}

void finish(Inspection &out)
{
    if (out.image.empty())
        return;
    out.perceptualHash = perceptualHash(out.image);
    out.sharpness = sharpness(out.image);
}

Inspection inspectImpl(const QString &path, const QByteArray *input);

} // namespace

Inspection inspect(const QString &path)
{
    if (isRawFile(path))
        return inspectImpl(path, nullptr);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        Inspection out;
        out.format = formatLabel(extensionOf(path));
        out.error = f.errorString();
        return out;
    }
    const QByteArray data = f.readAll();
    return inspectImpl(path, &data);
}

Inspection inspectData(const QByteArray &data, const QString &fileName)
{
    return inspectImpl(fileName, &data);
}

namespace {

Inspection inspectImpl(const QString &path, const QByteArray *input)
{
    Inspection out;
    const QString ext = extensionOf(path);
    out.format = formatLabel(ext);
    out.isRaw = isRawFile(path);

    if (out.isRaw) {
        RawInfo raw = input ? readRawData(*input, true, true) : readRaw(path, true, true);
        if (!raw.ok) {
            out.error = raw.error;
            return out;
        }
        out.ok = true;
        out.tags = std::move(raw.tags);
        out.shot = raw.shot;
        out.width = raw.width;
        out.height = raw.height;
        out.image = fitTo(raw.preview, kWorkingSide);
        out.pictureHash = raw.pictureHash;
        if (out.pictureHash.isEmpty() && !raw.preview.empty())
            out.pictureHash = pictureHash(raw.preview);
        if (out.image.empty())
            out.error = QStringLiteral("no embedded preview");
        finish(out);
        return out;
    }

    const QByteArray &data = *input;

    if (isJpegExt(ext) || data.startsWith("\xFF\xD8")) {
        const auto header = parseJpegHeader(data);
        if (header) {
            out.tags = readExifTags(header->exif);
            out.shot = summarize(out.tags);
        }
        if (header && tooLarge(out, header->width, header->height))
            return out;
        // Pixels completos: o picture_hash é dos pixels como o Pillow os
        // decodifica (mesma libjpeg-turbo, mesmo IDCT), já orientados.
        const MemoryLease lease(header ? decodeBytes(header->width, header->height, 3, data.size())
                                       : decodeBytes(0, 0, 3, data.size()));
        cv::Mat full = decodeImage(data, 1, true);
        if (full.empty()) {
            out.error = QStringLiteral("unreadable JPEG");
            out.ok = header.has_value();
            return out;
        }
        full = applyOrientation(full, out.shot.orientation);
        out.ok = true;
        out.width = full.cols;
        out.height = full.rows;
        out.pictureHash = pictureHash(full);
        if (full.channels() == 1) {
            cv::Mat rgb;
            cv::cvtColor(full, rgb, cv::COLOR_GRAY2RGB);
            full = rgb;
        }
        out.image = fitTo(full, kWorkingSide);
        finish(out);
        return out;
    }

    out.tags = readExifTags(embeddedExif(data, ext));
    out.shot = summarize(out.tags);
    // Tamanho pelo cabeçalho (o QImageReader não decodifica para isso).
    QSize declared;
    {
        QByteArray probe = data;
        QBuffer buffer(&probe);
        buffer.open(QIODevice::ReadOnly);
        declared = QImageReader(&buffer).size();
    }
    if (declared.isValid() && tooLarge(out, declared.width(), declared.height()))
        return out;
    const MemoryLease lease(decodeBytes(declared.width(), declared.height(), 4, data.size()));
    if (ext == QLatin1String("png")) {
        // PNG pelo OpenCV: os pixels completos para o hash.
        cv::Mat full = decodeImage(data, 1, true);
        if (!full.empty()) {
            full = applyOrientation(full, out.shot.orientation);
            out.ok = true;
            out.width = full.cols;
            out.height = full.rows;
            out.pictureHash = pictureHash(full);
            if (full.channels() == 1) {
                cv::Mat rgb;
                cv::cvtColor(full, rgb, cv::COLOR_GRAY2RGB);
                full = rgb;
            }
            out.image = fitTo(full, kWorkingSide);
            finish(out);
            return out;
        }
    }
    int w = 0, h = 0;
    QByteArray copy = data;
    QBuffer buffer(&copy);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        out.ok = true;  // catalogado, sem imagem (formato sem plugin no Qt)
        out.error = reader.errorString();
        out.width = out.shot.width;
        out.height = out.shot.height;
        return out;
    }
    out.ok = true;
    const cv::Mat full = fromQImage(image);
    w = full.cols;
    h = full.rows;
    out.width = w;
    out.height = h;
    out.pictureHash = pictureHash(full);
    out.image = fitTo(full, kWorkingSide);
    finish(out);
    return out;
}

} // namespace

cv::Mat loadDisplayImage(const QString &path, int maxSide)
{
    if (isRawFile(path))
        return fitTo(readRaw(path, true, false).preview, maxSide);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return loadDisplayImageData(f.readAll(), path, maxSide);
}

cv::Mat loadDisplayImageData(const QByteArray &data, const QString &fileName, int maxSide)
{
    if (isRawFile(fileName))
        return fitTo(readRawData(data, true, false).preview, maxSide);
    if (const auto header = parseJpegHeader(data)) {
        const ShotInfo shot = summarize(readExifTags(header->exif));
        // Reduz na DCT: o maior fator que ainda deixa o lado pedido.
        int scale = 1;
        const int longest = std::max(header->width, header->height);
        for (int s : {8, 4, 2})
            if (maxSide > 0 && longest / s >= maxSide) {
                scale = s;
                break;
            }
        return fitTo(applyOrientation(decodeImage(data, scale), shot.orientation), maxSide);
    }
    QSize declared;
    {
        QByteArray probe = data;
        QBuffer buffer(&probe);
        buffer.open(QIODevice::ReadOnly);
        declared = QImageReader(&buffer).size();
    }
    if (declared.isValid() && qint64(declared.width()) * declared.height() > kMaxDecodePixels)
        return {};
    const MemoryLease lease(decodeBytes(declared.width(), declared.height(), 4, data.size()));
    int w = 0, h = 0;
    return readWithQt(data, maxSide, &w, &h);
}

} // namespace pix::imaging

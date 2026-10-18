// SPDX-License-Identifier: BSD-2-Clause
#include "Collage.h"

#include <QBuffer>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QFontMetricsF>
#include <QImageWriter>
#include <QPainter>
#include <QPainterPath>
#include <QSaveFile>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

namespace pix::imaging {

namespace {

/// Altura do rosto no recorte e onde fica o centro dele (fração da altura).
constexpr double kFaceHeight = 0.40;
constexpr double kFaceWidth = 0.55;
constexpr double kFaceCenterY = 0.45;

// ------------------------------------------------------------------ //
// EXIF (TIFF little-endian)
// ------------------------------------------------------------------ //

enum : quint16 { kShort = 3, kLong = 4, kAscii = 2, kUndefined = 7 };

struct Tag
{
    quint16 tag;
    quint16 type;
    quint32 count;
    QByteArray data;   ///< já em little-endian
};

void put16(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xff)).append(char(v >> 8));
}

void put32(QByteArray &b, quint32 v)
{
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

Tag ascii(quint16 tag, const QString &text)
{
    QByteArray data = text.toUtf8();
    data.append('\0');
    return {tag, kAscii, quint32(data.size()), data};
}

Tag short1(quint16 tag, quint16 value)
{
    QByteArray data;
    put16(data, value);
    return {tag, kShort, 1, data};
}

Tag long1(quint16 tag, quint32 value)
{
    QByteArray data;
    put32(data, value);
    return {tag, kLong, 1, data};
}

Tag undefined(quint16 tag, const QByteArray &data)
{
    return {tag, kUndefined, quint32(data.size()), data};
}

/// Bytes que o IFD ocupa (entradas + valores que não cabem em 4 bytes).
quint32 ifdSize(const QList<Tag> &tags)
{
    quint32 size = 2 + 12 * quint32(tags.size()) + 4;
    for (const Tag &t : tags)
        if (t.data.size() > 4)
            size += quint32(t.data.size() + (t.data.size() & 1));
    return size;
}

/// Acrescenta o IFD ao fim de ``tiff`` (que tem de terminar em ``tiff.size()``
/// = o deslocamento dele).
void appendIfd(QByteArray &tiff, QList<Tag> tags)
{
    std::sort(tags.begin(), tags.end(), [](const Tag &a, const Tag &b) { return a.tag < b.tag; });
    const quint32 start = quint32(tiff.size());
    quint32 dataOffset = start + 2 + 12 * quint32(tags.size()) + 4;
    QByteArray data;
    put16(tiff, quint16(tags.size()));
    for (const Tag &t : tags) {
        put16(tiff, t.tag);
        put16(tiff, t.type);
        put32(tiff, t.count);
        if (t.data.size() <= 4) {
            tiff.append(t.data);
            tiff.append(QByteArray(4 - t.data.size(), '\0'));
        } else {
            put32(tiff, dataOffset + quint32(data.size()));
            data.append(t.data);
            if (data.size() & 1)
                data.append('\0');
        }
    }
    put32(tiff, 0);   // sem próximo IFD
    tiff.append(data);
}

/// Insere o APP1 depois do SOI (e do APP0/JFIF, se houver).
QByteArray insertApp1(const QByteArray &jpeg, const QByteArray &exif)
{
    if (jpeg.size() < 4 || quint8(jpeg[0]) != 0xff || quint8(jpeg[1]) != 0xd8 || exif.size() + 2 > 0xffff)
        return jpeg;
    int at = 2;
    if (quint8(jpeg[2]) == 0xff && quint8(jpeg[3]) == 0xe0 && jpeg.size() > 6)
        at = 4 + ((quint8(jpeg[4]) << 8) | quint8(jpeg[5]));
    QByteArray segment;
    segment.append(char(0xff)).append(char(0xe1));
    const int length = int(exif.size()) + 2;
    segment.append(char(length >> 8)).append(char(length & 0xff));
    segment.append(exif);
    return jpeg.left(at) + segment + jpeg.mid(at);
}

} // namespace

cv::Mat cropPortrait(const cv::Mat &src, const QRectF &face, cv::Size out)
{
    if (src.empty() || out.width <= 0 || out.height <= 0)
        return {};
    const double W = src.cols, H = src.rows;
    const double aspect = double(out.width) / out.height;
    double cx = W / 2, cy = H / 2, ch = H;
    if (face.width() > 0 && face.height() > 0) {
        const double fw = face.width() * W, fh = face.height() * H;
        cx = (face.x() + face.width() / 2) * W;
        cy = (face.y() + face.height() / 2) * H;
        ch = std::max(fh / kFaceHeight, fw / kFaceWidth / aspect);
    }
    double cw = ch * aspect;
    // Maior que a foto: encolhe mantendo a proporção.
    const double fit = std::min({1.0, W / cw, H / ch});
    cw *= fit;
    ch *= fit;
    const double x0 = std::clamp(cx - cw / 2, 0.0, W - cw);
    const double y0 = std::clamp(cy - kFaceCenterY * ch, 0.0, H - ch);
    cv::Rect r(int(std::lround(x0)), int(std::lround(y0)), std::max(1, int(std::lround(cw))), std::max(1, int(std::lround(ch))));
    r &= cv::Rect(0, 0, src.cols, src.rows);
    if (r.empty())
        return {};
    cv::Mat dst;
    cv::resize(src(r), dst, out, 0, 0, r.width > out.width ? cv::INTER_AREA : cv::INTER_CUBIC);
    return dst;
}

QImage composeCollage(const QList<QImage> &tiles, const QStringList &labels, const CollageStyle &style)
{
    if (tiles.isEmpty())
        return {};
    QSize tile;
    for (const QImage &t : tiles)
        if (!t.isNull()) {
            tile = t.size();
            break;
        }
    if (tile.isEmpty())
        return {};
    const int n = int(tiles.size());
    const int cols = std::clamp(style.columns, 1, n);
    const int rows = (n + cols - 1) / cols;
    const int gap = std::max(0, style.gap);
    QImage canvas(cols * tile.width() + (cols + 1) * gap, rows * tile.height() + (rows + 1) * gap, QImage::Format_RGB32);
    canvas.fill(style.background);

    QPainter p(&canvas);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    QFont font(QStringLiteral("Inter"));
    font.setPixelSize(std::max(10, int(std::lround(tile.width() * 0.085))));
    font.setWeight(QFont::Bold);
    p.setFont(font);
    const QFontMetricsF metrics(font);

    for (int i = 0; i < n; ++i) {
        const int row = i / cols, col = i % cols;
        // Última linha incompleta: centralizada.
        const int inRow = std::min(cols, n - row * cols);
        const int shift = (cols - inRow) * (tile.width() + gap) / 2;
        const QRect cell(gap + col * (tile.width() + gap) + shift, gap + row * (tile.height() + gap), tile.width(), tile.height());
        if (!tiles[i].isNull())
            p.drawImage(cell, tiles[i]);
        if (!style.labels || i >= labels.size() || labels[i].isEmpty())
            continue;
        // A faixa branca com o período, embaixo e no centro.
        const double pad = font.pixelSize() * 0.55;
        const double h = font.pixelSize() * 1.5;
        const double w = metrics.horizontalAdvance(labels[i]) + 2 * pad;
        const QRectF pill(cell.center().x() - w / 2, cell.bottom() - h - tile.height() * 0.05, w, h);
        QPainterPath path;
        path.addRoundedRect(pill, h * 0.22, h * 0.22);
        p.fillPath(path, QColor(255, 255, 255));
        p.setPen(QColor(17, 17, 17));
        p.drawText(pill, Qt::AlignCenter, labels[i]);
    }
    p.end();
    return canvas;
}

QByteArray buildExif(const ExportInfo &info)
{
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy:MM:dd HH:mm:ss"));
    QString comment = info.software;
    if (!info.url.isEmpty())
        comment += (comment.isEmpty() ? QString() : QStringLiteral(" - ")) + info.url;
    QByteArray userComment("ASCII\0\0\0", 8);
    userComment.append(comment.toUtf8());

    QList<Tag> exifIfd{undefined(0x9000, QByteArrayLiteral("0232")),   // ExifVersion
                       ascii(0x9004, now),                               // DateTimeDigitized
                       undefined(0x9286, userComment)};                  // UserComment
    QList<Tag> ifd0{short1(0x0112, 1),                                   // Orientation
                    ascii(0x0131, info.software),                        // Software
                    ascii(0x0132, now),                                  // DateTime
                    long1(0x8769, 0)};                                   // Exif IFD (abaixo)
    if (!info.title.isEmpty())
        ifd0.append(ascii(0x010e, info.title));                          // ImageDescription
    if (!info.url.isEmpty())
        ifd0.append(ascii(0x013c, info.url));                            // HostComputer: de onde veio

    // O Exif IFD vem logo depois do IFD0 (cabeçalho de 8 bytes).
    for (Tag &t : ifd0)
        if (t.tag == 0x8769)
            t = long1(0x8769, 8 + ifdSize(ifd0));

    QByteArray tiff("II*\0", 4);
    put32(tiff, 8);
    appendIfd(tiff, ifd0);
    appendIfd(tiff, exifIfd);
    return QByteArray("Exif\0\0", 6) + tiff;
}

QString saveExport(const QImage &image, const QString &path, const ExportInfo &info)
{
    if (image.isNull())
        return QStringLiteral("empty image");
    const QString ext = QFileInfo(path).suffix().toLower();
    const bool png = ext == QLatin1String("png");
    QByteArray bytes;
    {
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, png ? "png" : "jpg");
        QImage out = image.convertToFormat(png ? QImage::Format_RGB32 : QImage::Format_RGB888);
        if (png) {
            // Palavras-chave padrão do PNG (tEXt).
            out.setText(QStringLiteral("Title"), info.title);
            out.setText(QStringLiteral("Software"), info.software);
            out.setText(QStringLiteral("Source"), info.url);
            out.setText(QStringLiteral("Creation Time"), QDateTime::currentDateTime().toString(Qt::RFC2822Date));
        } else {
            writer.setQuality(92);
        }
        if (!writer.write(out))
            return writer.errorString();
    }
    if (!png)
        bytes = insertApp1(bytes, buildExif(info));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return file.errorString();
    return {};
}

} // namespace pix::imaging

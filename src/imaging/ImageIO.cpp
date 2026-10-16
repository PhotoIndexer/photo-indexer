// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/ImageIO.h"

#include <QBuffer>
#include <QSaveFile>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace pix::imaging {

namespace {

const QByteArray kExifHeader("Exif\0\0", 6);

bool isSofMarker(uchar m)
{
    // SOF0..SOF15, menos DHT (C4), JPG (C8) e DAC (CC).
    return m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
}

/// Percorre os segmentos até o SOS, lendo só o que importa (SOF e APP1 Exif);
/// o resto é pulado com seek.
std::optional<JpegHeader> parseHeader(QIODevice &dev)
{
    const QByteArray soi = dev.read(2);
    if (soi.size() != 2 || uchar(soi[0]) != 0xFF || uchar(soi[1]) != 0xD8)
        return std::nullopt;

    JpegHeader header;
    bool haveSof = false;
    bool haveExif = false;
    for (;;) {
        char c = 0;
        if (!dev.getChar(&c))
            break;
        if (uchar(c) != 0xFF)
            break;  // fora de sincronia: para por aqui
        uchar marker = 0xFF;
        while (marker == 0xFF && dev.getChar(&c))  // bytes de enchimento
            marker = uchar(c);
        if (marker == 0xFF || marker == 0xDA || marker == 0xD9)
            break;
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01)
            continue;  // marcadores sem tamanho
        const QByteArray lenBytes = dev.read(2);
        if (lenBytes.size() != 2)
            break;
        const int len = (uchar(lenBytes[0]) << 8) | uchar(lenBytes[1]);
        if (len < 2)
            break;
        const qint64 payloadLen = len - 2;
        if (isSofMarker(marker) && !haveSof) {
            const QByteArray p = dev.read(payloadLen);
            if (p.size() < 6)
                break;
            header.height = (uchar(p[1]) << 8) | uchar(p[2]);
            header.width = (uchar(p[3]) << 8) | uchar(p[4]);
            header.components = uchar(p[5]);
            haveSof = header.width > 0 && header.height > 0;
            continue;
        }
        if (marker == 0xE1 && !haveExif) {
            const QByteArray p = dev.read(payloadLen);
            if (p.size() != payloadLen)
                break;
            if (p.startsWith(kExifHeader)) {
                header.exif = p;
                haveExif = true;
            }
            continue;
        }
        if (!dev.seek(dev.pos() + payloadLen))
            break;
    }
    if (!haveSof)
        return std::nullopt;
    return header;
}

} // namespace

std::optional<JpegHeader> parseJpegHeader(const QByteArray &data)
{
    QByteArray copy = data;  // QBuffer pede não-const; é só uma referência compartilhada
    QBuffer buf(&copy);
    if (!buf.open(QIODevice::ReadOnly))
        return std::nullopt;
    return parseHeader(buf);
}

cv::Mat applyOrientation(const cv::Mat &img, int orientation)
{
    cv::Mat out;
    switch (orientation) {
    case 2: cv::flip(img, out, 1); return out;                          // FLIP_LEFT_RIGHT
    case 3: cv::rotate(img, out, cv::ROTATE_180); return out;           // ROTATE_180
    case 4: cv::flip(img, out, 0); return out;                          // FLIP_TOP_BOTTOM
    case 5: cv::transpose(img, out); return out;                        // TRANSPOSE
    case 6: cv::rotate(img, out, cv::ROTATE_90_CLOCKWISE); return out;  // ROTATE_270
    case 7: {                                                           // TRANSVERSE
        cv::Mat t;
        cv::transpose(img, t);
        cv::flip(t, out, -1);
        return out;
    }
    case 8: cv::rotate(img, out, cv::ROTATE_90_COUNTERCLOCKWISE); return out;  // ROTATE_90
    default: return img;
    }
}

cv::Size fitSize(cv::Size size, int maxSide)
{
    if (maxSide <= 0 || size.width <= 0 || size.height <= 0
        || (size.width <= maxSide && size.height <= maxSide))
        return size;
    const double scale = double(maxSide) / std::max(size.width, size.height);
    return {std::max(1, int(std::lround(size.width * scale))), std::max(1, int(std::lround(size.height * scale)))};
}

cv::Mat resizeTo(const cv::Mat &img, cv::Size size)
{
    if (img.empty() || img.size() == size)
        return img;
    const bool shrink = size.width <= img.cols && size.height <= img.rows;
    cv::Mat out;
    cv::resize(img, out, size, 0, 0, shrink ? cv::INTER_AREA : cv::INTER_LANCZOS4);
    return out;
}

cv::Mat fitTo(const cv::Mat &img, int maxSide)
{
    return img.empty() ? img : resizeTo(img, fitSize(img.size(), maxSide));
}

cv::Mat decodeImage(const QByteArray &data, int scale, bool keepGray)
{
    if (data.isEmpty())
        return {};
    int flags = cv::IMREAD_COLOR;
    switch (scale) {
    case 2: flags = cv::IMREAD_REDUCED_COLOR_2; break;
    case 4: flags = cv::IMREAD_REDUCED_COLOR_4; break;
    case 8: flags = cv::IMREAD_REDUCED_COLOR_8; break;
    default: break;
    }
    // ANYCOLOR: cinza fica com 1 canal (o modo "L" do Pillow), cor com 3.
    if (keepGray && scale == 1)
        flags = cv::IMREAD_ANYCOLOR;
    const cv::Mat buf(1, int(data.size()), CV_8UC1, const_cast<char *>(data.constData()));
    cv::Mat img;
    try {
        img = cv::imdecode(buf, flags | cv::IMREAD_IGNORE_ORIENTATION);
    } catch (const cv::Exception &) {
        return {};
    }
    if (img.empty())
        return {};
    if (img.depth() != CV_8U) {
        cv::Mat eight;
        img.convertTo(eight, CV_8U, img.depth() == CV_16U ? 1.0 / 257.0 : 1.0);
        img = eight;
    }
    cv::Mat out;
    switch (img.channels()) {
    case 1:
        if (keepGray)
            return img;
        cv::cvtColor(img, out, cv::COLOR_GRAY2RGB);
        return out;
    case 3: cv::cvtColor(img, out, cv::COLOR_BGR2RGB); return out;
    case 4: cv::cvtColor(img, out, cv::COLOR_BGRA2RGB); return out;
    default: return {};
    }
}

QByteArray encodeJpeg(const cv::Mat &rgb, int quality)
{
    if (rgb.empty() || rgb.type() != CV_8UC3)
        return {};
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    const std::vector<int> params{cv::IMWRITE_JPEG_QUALITY, quality, cv::IMWRITE_JPEG_OPTIMIZE, 1};
    std::vector<uchar> out;
    try {
        if (!cv::imencode(".jpg", bgr, out, params))
            return {};
    } catch (const cv::Exception &) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char *>(out.data()), qsizetype(out.size()));
}

bool writeAtomic(const QString &path, const QByteArray &data)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    if (f.write(data) != data.size()) {
        f.cancelWriting();
        return false;
    }
    return f.commit();
}

QImage toQImage(const cv::Mat &mat)
{
    if (mat.empty())
        return {};
    if (mat.type() == CV_8UC3)
        return QImage(mat.data, mat.cols, mat.rows, qsizetype(mat.step), QImage::Format_RGB888).copy();
    if (mat.type() == CV_8UC4)
        return QImage(mat.data, mat.cols, mat.rows, qsizetype(mat.step), QImage::Format_RGBA8888).copy();
    if (mat.type() == CV_8UC1)
        return QImage(mat.data, mat.cols, mat.rows, qsizetype(mat.step), QImage::Format_Grayscale8).copy();
    return {};
}

cv::Mat fromQImage(const QImage &image)
{
    if (image.isNull())
        return {};
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    const cv::Mat view(rgb.height(), rgb.width(), CV_8UC3, const_cast<uchar *>(rgb.constBits()),
                       size_t(rgb.bytesPerLine()));
    return view.clone();
}

} // namespace pix::imaging

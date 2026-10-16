// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Raw.h"

#include "imaging/Hashes.h"
#include "imaging/ImageIO.h"

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QSet>
#include <QTimeZone>

#include <libraw/libraw.h>

#include <opencv2/imgproc.hpp>

#include <memory>

namespace pix::imaging {

namespace {

Q_LOGGING_CATEGORY(lcRaw, "photoidx.imaging.raw")

/// Coleta as tags que o parser do LibRaw vai encontrando.
struct TagCollector
{
    ExifTags tags;
    QSet<QString> seen;  ///< "<ifd>/<tag>": a mesma tag pode ser lida duas vezes
};

/// Grupo da tag pelo que o LibRaw codifica nos bits altos (ver
/// src/metadata/tiff.cpp e exif_gps.cpp do LibRaw).
QString groupOf(unsigned hi)
{
    if (hi >= 0x10)
        return QStringLiteral("IFD%1").arg((hi >> 4) - 1);
    switch (hi) {
    case 0x0: return QStringLiteral("Exif");
    case 0x2: return QStringLiteral("Kodak");
    case 0x3: return QStringLiteral("Panasonic");
    case 0x4: return QStringLiteral("Interop");
    case 0x5: return QStringLiteral("GPS");
    default: return QStringLiteral("Group%1").arg(hi);
    }
}

void exifCallback(void *context, int tag, int type, int len, unsigned int ord, void *ifp, INT64 /*base*/)
{
    auto *collector = static_cast<TagCollector *>(context);
    auto *stream = static_cast<LibRaw_abstract_datastream *>(ifp);
    const int size = exifTypeSize(type);
    if (!collector || !stream || size == 0 || len < 0)
        return;
    ExifTag t;
    t.ifd = groupOf(unsigned(tag) >> 16);
    t.tag = quint16(tag & 0xFFFF);
    t.type = quint16(type);
    t.count = quint32(len);
    t.little = ord == 0x4949;
    const QString key = t.ifd + u'/' + QString::number(t.tag);
    if (collector->seen.contains(key))
        return;
    const qint64 total = qint64(size) * len;
    const qint64 keep = std::min<qint64>(total, kMaxTagBytes);
    t.data.resize(keep);
    if (keep > 0 && stream->read(t.data.data(), 1, size_t(keep)) != int(keep))
        return;
    collector->seen.insert(key);
    collector->tags.append(t);
}

QString latin(const char *text)
{
    return QString::fromLatin1(text).trimmed();
}

/// O ``flip`` do LibRaw (0, 3, 5, 6) na Orientation do EXIF.
int orientationFromFlip(int flip)
{
    switch (flip) {
    case 3: return 3;
    case 5: return 8;
    case 6: return 6;
    default: return 1;
    }
}

int openRaw(LibRaw &raw, const QString &path)
{
#ifdef _WIN32
    const QString native = QDir::toNativeSeparators(path);
    return raw.open_file(reinterpret_cast<const wchar_t *>(native.utf16()));
#else
    return raw.open_file(QFile::encodeName(path).constData());
#endif
}

/// A maior prévia embutida, decodificada e na orientação certa.
cv::Mat largestPreview(LibRaw &raw, int orientation)
{
    const auto &list = raw.imgdata.thumbs_list;
    int best = -1;
    qint64 bestArea = 0;
    for (int i = 0; i < list.thumbcount && i < LIBRAW_THUMBNAIL_MAXCOUNT; ++i) {
        const auto &item = list.thumblist[i];
        const qint64 area = qint64(item.twidth) * item.theight;
        // JPEG primeiro; bitmap só se for o que há.
        const qint64 score = item.tformat == LIBRAW_INTERNAL_THUMBNAIL_JPEG ? area * 2 : area;
        if (score > bestArea || best < 0) {
            best = i;
            bestArea = score;
        }
    }
    int rc = best >= 0 ? raw.unpack_thumb_ex(best) : raw.unpack_thumb();
    if (rc != LIBRAW_SUCCESS && best >= 0)
        rc = raw.unpack_thumb();
    if (rc != LIBRAW_SUCCESS)
        return {};
    int err = 0;
    libraw_processed_image_t *img = raw.dcraw_make_mem_thumb(&err);
    if (!img)
        return {};
    std::unique_ptr<libraw_processed_image_t, void (*)(libraw_processed_image_t *)> guard(img, LibRaw::dcraw_clear_mem);
    cv::Mat rgb;
    if (img->type == LIBRAW_IMAGE_JPEG) {
        const QByteArray bytes(reinterpret_cast<const char *>(img->data), qsizetype(img->data_size));
        rgb = decodeImage(bytes);
    } else if (img->type == LIBRAW_IMAGE_BITMAP && img->bits == 8 && (img->colors == 3 || img->colors == 1)) {
        const cv::Mat view(img->height, img->width, img->colors == 3 ? CV_8UC3 : CV_8UC1, img->data);
        if (img->colors == 3)
            rgb = view.clone();
        else
            cv::cvtColor(view, rgb, cv::COLOR_GRAY2RGB);
    }
    if (rgb.empty())
        return {};
    // A prévia sai na orientação do sensor (o EXIF dela, quando há, é
    // ignorado na decodificação): gira pelo flip do RAW.
    return applyOrientation(rgb, orientation);
}

} // namespace

RawInfo readRaw(const QString &path, bool wantPreview, bool wantHash)
{
    RawInfo out;
    auto raw = std::make_unique<LibRaw>();
    TagCollector collector;
    raw->set_exifparser_handler(exifCallback, &collector);
    const int rc = openRaw(*raw, path);
    if (rc != LIBRAW_SUCCESS) {
        out.error = QString::fromLatin1(libraw_strerror(rc));
        return out;
    }
    out.ok = true;
    out.tags = collector.tags;
    out.shot = summarize(out.tags);

    const auto &d = raw->imgdata;
    // O que as tags não trouxeram, dos campos já interpretados pelo LibRaw.
    if (out.shot.make.isEmpty())
        out.shot.make = latin(d.idata.make);
    if (out.shot.model.isEmpty())
        out.shot.model = latin(d.idata.model);
    if (out.shot.lens.isEmpty())
        out.shot.lens = latin(d.lens.Lens);
    if (out.shot.iso <= 0 && d.other.iso_speed > 0)
        out.shot.iso = int(d.other.iso_speed);
    if (out.shot.exposureTime <= 0 && d.other.shutter > 0)
        out.shot.exposureTime = d.other.shutter;
    if (out.shot.fNumber <= 0 && d.other.aperture > 0)
        out.shot.fNumber = d.other.aperture;
    if (out.shot.focalLength <= 0 && d.other.focal_len > 0)
        out.shot.focalLength = d.other.focal_len;
    if (out.shot.artist.isEmpty())
        out.shot.artist = latin(d.other.artist);
    if (!out.shot.takenLocal.isValid() && d.other.timestamp > 0) {
        // O LibRaw converte a data da câmera com mktime (hora local): volta
        // para o relógio da câmera.
        const QDateTime local = QDateTime::fromSecsSinceEpoch(qint64(d.other.timestamp), QTimeZone::LocalTime);
        out.shot.takenLocal = QDateTime(local.date(), local.time(), QTimeZone::UTC);
    }
    if (!out.shot.hasGps && d.other.parsed_gps.gpsparsed) {
        const auto &g = d.other.parsed_gps;
        const double lat = g.latitude[0] + g.latitude[1] / 60.0 + g.latitude[2] / 3600.0;
        const double lon = g.longitude[0] + g.longitude[1] / 60.0 + g.longitude[2] / 3600.0;
        if (lat != 0.0 || lon != 0.0) {
            out.shot.hasGps = true;
            out.shot.latitude = g.latref == 'S' ? -lat : lat;
            out.shot.longitude = g.longref == 'W' ? -lon : lon;
            out.shot.altitude = g.altref == 1 ? -g.altitude : g.altitude;
        }
    }

    const int orientation = orientationFromFlip(d.sizes.flip);
    out.shot.orientation = orientation;
    out.width = d.sizes.width;
    out.height = d.sizes.height;
    if (orientation >= 5)
        std::swap(out.width, out.height);

    if (wantPreview)
        out.preview = largestPreview(*raw, orientation);

    if (wantHash && raw->unpack() == LIBRAW_SUCCESS) {
        const auto &r = raw->imgdata.rawdata;
        const int w = r.sizes.raw_width;
        const int h = r.sizes.raw_height;
        const int pitch = int(r.sizes.raw_pitch);
        if (r.raw_image)
            out.pictureHash = rawPictureHash(r.raw_image, w, h, pitch);
        else if (r.color4_image)
            out.pictureHash = rawPictureHash(&r.color4_image[0][0], w * 4, h, pitch);
        else if (r.color3_image)
            out.pictureHash = rawPictureHash(&r.color3_image[0][0], w * 3, h, pitch);
    }
    if (out.pictureHash.isEmpty() && wantHash)
        qCDebug(lcRaw) << "No sensor data to hash in" << path;
    return out;
}

} // namespace pix::imaging

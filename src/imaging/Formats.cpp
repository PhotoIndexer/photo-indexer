// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Formats.h"

#include <QFile>
#include <QMimeDatabase>
#include <QSet>

#include <algorithm>

namespace pix::imaging {

namespace {

const QSet<QString> &imageSet()
{
    static const QSet<QString> set(imageExtensions().cbegin(), imageExtensions().cend());
    return set;
}

const QSet<QString> &rawSet()
{
    static const QSet<QString> set(rawExtensions().cbegin(), rawExtensions().cend());
    return set;
}

} // namespace

const QStringList &imageExtensions()
{
    // Os que não são RAW: JPEG/PNG pelo OpenCV; o resto pelo QImageReader
    // (o que não tiver plugin no Qt fica catalogado, sem miniatura).
    static const QStringList list{
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("jpe"), QStringLiteral("jfif"),
        QStringLiteral("png"), QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("webp"),
        QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("heic"), QStringLiteral("heif"),
        QStringLiteral("avif"),
    };
    return list;
}

const QStringList &rawExtensions()
{
    static const QStringList list{
        QStringLiteral("3fr"), QStringLiteral("ari"), QStringLiteral("arw"), QStringLiteral("bay"),
        QStringLiteral("cap"), QStringLiteral("cr2"), QStringLiteral("cr3"), QStringLiteral("crw"),
        QStringLiteral("dcr"), QStringLiteral("dcs"), QStringLiteral("dng"), QStringLiteral("drf"),
        QStringLiteral("eip"), QStringLiteral("erf"), QStringLiteral("fff"), QStringLiteral("gpr"),
        QStringLiteral("iiq"), QStringLiteral("k25"), QStringLiteral("kdc"), QStringLiteral("mdc"),
        QStringLiteral("mef"), QStringLiteral("mos"), QStringLiteral("mrw"), QStringLiteral("nef"),
        QStringLiteral("nrw"), QStringLiteral("orf"), QStringLiteral("ori"), QStringLiteral("pef"),
        QStringLiteral("ptx"), QStringLiteral("pxn"), QStringLiteral("r3d"), QStringLiteral("raf"),
        QStringLiteral("raw"), QStringLiteral("rw2"), QStringLiteral("rwl"), QStringLiteral("rwz"),
        QStringLiteral("sr2"), QStringLiteral("srf"), QStringLiteral("srw"), QStringLiteral("sti"),
        QStringLiteral("x3f"),
    };
    return list;
}

QString extensionOf(const QString &path)
{
    const qsizetype dot = path.lastIndexOf(u'.');
    const qsizetype slash = std::max(path.lastIndexOf(u'/'), path.lastIndexOf(u'\\'));
    if (dot < 0 || dot < slash)
        return {};
    return path.mid(dot + 1).toLower();
}

bool isImageFile(const QString &path)
{
    const QString ext = extensionOf(path);
    return !ext.isEmpty() && (imageSet().contains(ext) || rawSet().contains(ext));
}

bool isRawFile(const QString &path)
{
    return rawSet().contains(extensionOf(path));
}

QString formatLabel(const QString &extension)
{
    const QString ext = extension.toLower();
    if (ext == QLatin1String("jpg") || ext == QLatin1String("jpeg") || ext == QLatin1String("jpe")
        || ext == QLatin1String("jfif"))
        return QStringLiteral("JPEG");
    if (ext == QLatin1String("tif"))
        return QStringLiteral("TIFF");
    return ext.toUpper();
}

QString sniffImageMime(const QByteArray &head)
{
    const auto at = [&head](qsizetype pos, const char *magic, qsizetype len) {
        return head.size() >= pos + len && head.mid(pos, len) == QByteArray::fromRawData(magic, len);
    };
    if (at(0, "\xFF\xD8\xFF", 3))
        return QStringLiteral("image/jpeg");
    if (at(0, "\x89PNG\r\n\x1A\n", 8))
        return QStringLiteral("image/png");
    if (at(0, "GIF87a", 6) || at(0, "GIF89a", 6))
        return QStringLiteral("image/gif");
    if (at(0, "RIFF", 4) && at(8, "WEBP", 4))
        return QStringLiteral("image/webp");
    // RAW com assinatura própria.
    if (at(0, "FUJIFILMCCD-RAW", 15))
        return QStringLiteral("image/x-fuji-raf");
    if (at(4, "ftypcrx ", 8))
        return QStringLiteral("image/x-canon-cr3");
    if (at(0, "II\x1A\x00\x00\x00HEAPCCDR", 14))
        return QStringLiteral("image/x-canon-crw");
    if (at(0, "\x00MRM", 4))
        return QStringLiteral("image/x-minolta-mrw");
    if (at(0, "FOVb", 4))
        return QStringLiteral("image/x-sigma-x3f");
    if (at(0, "IIRO", 4) || at(0, "IIRS", 4) || at(0, "MMOR", 4))
        return QStringLiteral("image/x-olympus-orf");
    if (at(0, "IIU\x00", 4))
        return QStringLiteral("image/x-panasonic-rw2");
    // TIFF e os RAW que são TIFF (NEF, CR2, ARW, DNG, PEF, SR2, IIQ, 3FR...).
    if (at(0, "II*\x00", 4) || at(0, "MM\x00*", 4) || at(0, "II+\x00", 4) || at(0, "MM\x00+", 4))
        return QStringLiteral("image/tiff");
    // HEIF/AVIF (ISO-BMFF): "ftyp" + uma marca de imagem.
    if (at(4, "ftyp", 4) && head.size() >= 16) {
        static const QList<QByteArray> brands{"heic", "heix", "hevc", "hevx", "heim", "heis", "mif1", "msf1", "avif", "avis"};
        for (const QByteArray &b : brands)
            if (head.mid(8, 4) == b || head.mid(16, qMin<qsizetype>(head.size() - 16, 32)).contains(b))
                return b.startsWith("avi") ? QStringLiteral("image/avif") : QStringLiteral("image/heif");
    }
    // O resto pelo banco de tipos do Qt (BMP e companhia), só pelo conteúdo.
    static const QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForData(head);
    if (mime.isValid() && mime.name().startsWith(QLatin1String("image/")))
        return mime.name();
    return {};
}

QString sniffImageFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return sniffImageMime(f.read(kSniffBytes));
}

} // namespace pix::imaging

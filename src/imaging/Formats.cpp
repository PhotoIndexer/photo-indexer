// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Formats.h"

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

} // namespace pix::imaging

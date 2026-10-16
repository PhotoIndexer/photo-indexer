// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Exif.h"

#include <QHash>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <set>

namespace pix::imaging {

namespace {

const QByteArray kExifHeader("Exif\0\0", 6);

constexpr quint16 kTiffMagic = 42;
constexpr int kIfdEntrySize = 12;
constexpr int kMaxIfdEntries = 1000;
constexpr int kMaxIfds = 16;
/// Vetores mais longos que isso aparecem cortados no texto.
constexpr int kMaxListedValues = 16;

// --------------------------------------------------------------------------- //
// Nomes das tags (EXIF 2.32 / TIFF 6.0 / DNG): os mais comuns
// --------------------------------------------------------------------------- //

struct TagName
{
    quint16 tag;
    const char *name;
};

// IFD0/IFD1/Exif/Interop compartilham o espaço de números do TIFF.
const TagName kTiffTags[] = {
    {0x0001, "InteropIndex"}, {0x0002, "InteropVersion"},
    {0x000B, "ProcessingSoftware"}, {0x00FE, "SubfileType"}, {0x00FF, "OldSubfileType"},
    {0x0100, "ImageWidth"}, {0x0101, "ImageHeight"}, {0x0102, "BitsPerSample"}, {0x0103, "Compression"},
    {0x0106, "PhotometricInterpretation"}, {0x0107, "Thresholding"}, {0x010A, "FillOrder"},
    {0x010D, "DocumentName"}, {0x010E, "ImageDescription"}, {0x010F, "Make"}, {0x0110, "Model"},
    {0x0111, "StripOffsets"}, {0x0112, "Orientation"}, {0x0115, "SamplesPerPixel"}, {0x0116, "RowsPerStrip"},
    {0x0117, "StripByteCounts"}, {0x011A, "XResolution"}, {0x011B, "YResolution"},
    {0x011C, "PlanarConfiguration"}, {0x0128, "ResolutionUnit"}, {0x012D, "TransferFunction"},
    {0x0131, "Software"}, {0x0132, "ModifyDate"}, {0x013B, "Artist"}, {0x013C, "HostComputer"},
    {0x013D, "Predictor"}, {0x013E, "WhitePoint"}, {0x013F, "PrimaryChromaticities"},
    {0x0142, "TileWidth"}, {0x0143, "TileLength"}, {0x0144, "TileOffsets"}, {0x0145, "TileByteCounts"},
    {0x014A, "SubIFDs"}, {0x0152, "ExtraSamples"}, {0x0153, "SampleFormat"},
    {0x0201, "ThumbnailOffset"}, {0x0202, "ThumbnailLength"}, {0x0211, "YCbCrCoefficients"},
    {0x0212, "YCbCrSubSampling"}, {0x0213, "YCbCrPositioning"}, {0x0214, "ReferenceBlackWhite"},
    {0x02BC, "ApplicationNotes"}, {0x4746, "Rating"}, {0x4749, "RatingPercent"},
    {0x828D, "CFARepeatPatternDim"}, {0x828E, "CFAPattern"}, {0x8298, "Copyright"},
    {0x829A, "ExposureTime"}, {0x829D, "FNumber"}, {0x83BB, "IPTC-NAA"}, {0x8649, "PhotoshopSettings"},
    {0x8769, "ExifOffset"}, {0x8773, "ICC_Profile"}, {0x8822, "ExposureProgram"},
    {0x8824, "SpectralSensitivity"}, {0x8825, "GPSInfo"}, {0x8827, "ISO"}, {0x8828, "OECF"},
    {0x8830, "SensitivityType"}, {0x8831, "StandardOutputSensitivity"},
    {0x8832, "RecommendedExposureIndex"}, {0x8833, "ISOSpeed"},
    {0x9000, "ExifVersion"}, {0x9003, "DateTimeOriginal"}, {0x9004, "CreateDate"},
    {0x9010, "OffsetTime"}, {0x9011, "OffsetTimeOriginal"}, {0x9012, "OffsetTimeDigitized"},
    {0x9101, "ComponentsConfiguration"}, {0x9102, "CompressedBitsPerPixel"},
    {0x9201, "ShutterSpeedValue"}, {0x9202, "ApertureValue"}, {0x9203, "BrightnessValue"},
    {0x9204, "ExposureCompensation"}, {0x9205, "MaxApertureValue"}, {0x9206, "SubjectDistance"},
    {0x9207, "MeteringMode"}, {0x9208, "LightSource"}, {0x9209, "Flash"}, {0x920A, "FocalLength"},
    {0x9214, "SubjectArea"}, {0x927C, "MakerNote"}, {0x9286, "UserComment"},
    {0x9290, "SubSecTime"}, {0x9291, "SubSecTimeOriginal"}, {0x9292, "SubSecTimeDigitized"},
    {0x9C9B, "XPTitle"}, {0x9C9C, "XPComment"}, {0x9C9D, "XPAuthor"}, {0x9C9E, "XPKeywords"},
    {0x9C9F, "XPSubject"}, {0xA000, "FlashpixVersion"}, {0xA001, "ColorSpace"},
    {0xA002, "ExifImageWidth"}, {0xA003, "ExifImageHeight"}, {0xA004, "RelatedSoundFile"},
    {0xA005, "InteropOffset"}, {0xA20B, "FlashEnergy"}, {0xA20E, "FocalPlaneXResolution"},
    {0xA20F, "FocalPlaneYResolution"}, {0xA210, "FocalPlaneResolutionUnit"},
    {0xA214, "SubjectLocation"}, {0xA215, "ExposureIndex"}, {0xA217, "SensingMethod"},
    {0xA300, "FileSource"}, {0xA301, "SceneType"}, {0xA302, "CFAPattern"},
    {0xA401, "CustomRendered"}, {0xA402, "ExposureMode"}, {0xA403, "WhiteBalance"},
    {0xA404, "DigitalZoomRatio"}, {0xA405, "FocalLengthIn35mmFormat"}, {0xA406, "SceneCaptureType"},
    {0xA407, "GainControl"}, {0xA408, "Contrast"}, {0xA409, "Saturation"}, {0xA40A, "Sharpness"},
    {0xA40C, "SubjectDistanceRange"}, {0xA420, "ImageUniqueID"}, {0xA430, "OwnerName"},
    {0xA431, "SerialNumber"}, {0xA432, "LensInfo"}, {0xA433, "LensMake"}, {0xA434, "LensModel"},
    {0xA435, "LensSerialNumber"}, {0xA460, "CompositeImage"}, {0xA500, "Gamma"},
    {0xC612, "DNGVersion"}, {0xC613, "DNGBackwardVersion"}, {0xC614, "UniqueCameraModel"},
    {0xC615, "LocalizedCameraModel"}, {0xC621, "ColorMatrix1"}, {0xC622, "ColorMatrix2"},
    {0xC627, "AnalogBalance"}, {0xC628, "AsShotNeutral"}, {0xC62F, "CameraSerialNumber"},
    {0xC630, "DNGLensInfo"}, {0xC65A, "CalibrationIlluminant1"}, {0xC65B, "CalibrationIlluminant2"},
    {0xC68B, "OriginalRawFileName"}, {0xC6F8, "ProfileName"},
};

const TagName kGpsTags[] = {
    {0x0000, "GPSVersionID"}, {0x0001, "GPSLatitudeRef"}, {0x0002, "GPSLatitude"},
    {0x0003, "GPSLongitudeRef"}, {0x0004, "GPSLongitude"}, {0x0005, "GPSAltitudeRef"},
    {0x0006, "GPSAltitude"}, {0x0007, "GPSTimeStamp"}, {0x0008, "GPSSatellites"},
    {0x0009, "GPSStatus"}, {0x000A, "GPSMeasureMode"}, {0x000B, "GPSDOP"}, {0x000C, "GPSSpeedRef"},
    {0x000D, "GPSSpeed"}, {0x000E, "GPSTrackRef"}, {0x000F, "GPSTrack"},
    {0x0010, "GPSImgDirectionRef"}, {0x0011, "GPSImgDirection"}, {0x0012, "GPSMapDatum"},
    {0x0013, "GPSDestLatitudeRef"}, {0x0014, "GPSDestLatitude"}, {0x0015, "GPSDestLongitudeRef"},
    {0x0016, "GPSDestLongitude"}, {0x0017, "GPSDestBearingRef"}, {0x0018, "GPSDestBearing"},
    {0x0019, "GPSDestDistanceRef"}, {0x001A, "GPSDestDistance"}, {0x001B, "GPSProcessingMethod"},
    {0x001C, "GPSAreaInformation"}, {0x001D, "GPSDateStamp"}, {0x001E, "GPSDifferential"},
    {0x001F, "GPSHPositioningError"},
};

const QHash<quint16, const char *> &tiffNames()
{
    static const QHash<quint16, const char *> map = [] {
        QHash<quint16, const char *> m;
        for (const TagName &t : kTiffTags)
            m.insert(t.tag, t.name);
        return m;
    }();
    return map;
}

const QHash<quint16, const char *> &gpsNames()
{
    static const QHash<quint16, const char *> map = [] {
        QHash<quint16, const char *> m;
        for (const TagName &t : kGpsTags)
            m.insert(t.tag, t.name);
        return m;
    }();
    return map;
}

// --------------------------------------------------------------------------- //
// Leitura de valores
// --------------------------------------------------------------------------- //

quint16 rd16(const uchar *p, bool little)
{
    return little ? quint16(p[0] | (p[1] << 8)) : quint16((p[0] << 8) | p[1]);
}

quint32 rd32(const uchar *p, bool little)
{
    return little ? (quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24))
                  : ((quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]));
}

quint64 rd64(const uchar *p, bool little)
{
    const quint64 a = rd32(p, little), b = rd32(p + 4, little);
    return little ? (a | (b << 32)) : ((a << 32) | b);
}

/// Quantos valores do tipo cabem nos bytes guardados.
int valueCount(const ExifTag &t)
{
    const int sz = exifTypeSize(t.type);
    if (sz == 0)
        return 0;
    return int(std::min<qint64>(t.count, t.data.size() / sz));
}

/// i-ésimo valor como double (racionais divididos); nullopt se ilegível.
std::optional<double> numberAt(const ExifTag &t, int i)
{
    if (i >= valueCount(t))
        return std::nullopt;
    const auto *p = reinterpret_cast<const uchar *>(t.data.constData()) + i * exifTypeSize(t.type);
    switch (t.type) {
    case 1: case 7: return double(p[0]);
    case 6: return double(qint8(p[0]));
    case 3: return double(rd16(p, t.little));
    case 8: return double(qint16(rd16(p, t.little)));
    case 4: case 13: return double(rd32(p, t.little));
    case 9: return double(qint32(rd32(p, t.little)));
    case 5: {
        const quint32 n = rd32(p, t.little), d = rd32(p + 4, t.little);
        if (d == 0)
            return std::nullopt;
        return double(n) / double(d);
    }
    case 10: {
        const qint32 n = qint32(rd32(p, t.little)), d = qint32(rd32(p + 4, t.little));
        if (d == 0)
            return std::nullopt;
        return double(n) / double(d);
    }
    case 11: {
        const quint32 bits = rd32(p, t.little);
        float f;
        std::memcpy(&f, &bits, 4);
        return double(f);
    }
    case 12: {
        const quint64 bits = rd64(p, t.little);
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }
    default: return std::nullopt;
    }
}

/// ``str(x).strip('\x00 ')``.
QString stripNulSpace(const QString &s)
{
    qsizetype b = 0;
    qsizetype e = s.size();
    auto junk = [](QChar c) { return c == QChar(0) || c.isSpace(); };
    while (b < e && junk(s.at(b)))
        ++b;
    while (e > b && junk(s.at(e - 1)))
        --e;
    return s.mid(b, e - b);
}

/// Texto ASCII da tag. Câmeras e programas gravam UTF-8 aqui com frequência:
/// se os bytes forem UTF-8 válido, vale o UTF-8; senão, Latin-1. Para no
/// primeiro NUL.
QString textValue(const ExifTag &t)
{
    QByteArray bytes = t.data;
    const qsizetype nul = bytes.indexOf('\0');
    if (nul >= 0)
        bytes.truncate(nul);
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString text = utf8.decode(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes);
    return stripNulSpace(text);
}

/// UNDEFINED que é texto (ExifVersion "0230", FileSource...).
bool printable(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return false;
    for (char c : bytes) {
        const uchar u = uchar(c);
        if (u != 0 && (u < 0x20 || u > 0x7E))
            return false;
    }
    return true;
}

/// "1/200", "2.8", "16" — o racional do jeito que se lê.
QString numberText(const ExifTag &t, int i)
{
    const auto v = numberAt(t, i);
    if (!v)
        return QStringLiteral("?");
    if (t.type == 5 || t.type == 10) {
        const auto *p = reinterpret_cast<const uchar *>(t.data.constData()) + i * 8;
        const qint64 n = t.type == 5 ? qint64(rd32(p, t.little)) : qint64(qint32(rd32(p, t.little)));
        const qint64 d = t.type == 5 ? qint64(rd32(p + 4, t.little)) : qint64(qint32(rd32(p + 4, t.little)));
        if (n == 1 && d > 1)
            return QStringLiteral("1/%1").arg(d);
    }
    if (std::floor(*v) == *v && std::abs(*v) < 1e15)
        return QString::number(qint64(*v));
    return QString::number(*v, 'g', 6);
}

/// Grau/minuto/segundo -> graus decimais.
std::optional<double> dmsDegrees(const ExifTag &t)
{
    if (valueCount(t) < 1)
        return std::nullopt;
    double deg = numberAt(t, 0).value_or(0.0);
    if (valueCount(t) >= 2)
        deg += numberAt(t, 1).value_or(0.0) / 60.0;
    if (valueCount(t) >= 3)
        deg += numberAt(t, 2).value_or(0.0) / 3600.0;
    if (!std::isfinite(deg))
        return std::nullopt;
    return deg;
}

/// Nome do valor das enumerações mais vistas (ExifTool usa os mesmos).
QString enumText(quint16 tag, int value)
{
    switch (tag) {
    case 0x0112: {
        static const char *const names[] = {"", "Horizontal (normal)", "Mirror horizontal", "Rotate 180",
                                            "Mirror vertical", "Mirror horizontal and rotate 270 CW",
                                            "Rotate 90 CW", "Mirror horizontal and rotate 90 CW", "Rotate 270 CW"};
        return value >= 1 && value <= 8 ? QString::fromLatin1(names[value]) : QString();
    }
    case 0x0128:
        return value == 2 ? QStringLiteral("inches") : value == 3 ? QStringLiteral("cm") : QString();
    case 0x8822: {
        static const char *const names[] = {"Not defined", "Manual", "Program AE", "Aperture-priority AE",
                                            "Shutter speed priority AE", "Creative (slow speed)",
                                            "Action (high speed)", "Portrait", "Landscape"};
        return value >= 0 && value <= 8 ? QString::fromLatin1(names[value]) : QString();
    }
    case 0x9207: {
        switch (value) {
        case 0: return QStringLiteral("Unknown");
        case 1: return QStringLiteral("Average");
        case 2: return QStringLiteral("Center-weighted average");
        case 3: return QStringLiteral("Spot");
        case 4: return QStringLiteral("Multi-spot");
        case 5: return QStringLiteral("Multi-segment");
        case 6: return QStringLiteral("Partial");
        }
        return {};
    }
    case 0x9209:
        return (value & 1) ? QStringLiteral("Fired") : QStringLiteral("No flash");
    case 0xA001:
        return value == 1 ? QStringLiteral("sRGB") : value == 0xFFFF ? QStringLiteral("Uncalibrated") : QString();
    case 0xA402:
        return value == 0 ? QStringLiteral("Auto") : value == 1 ? QStringLiteral("Manual")
             : value == 2 ? QStringLiteral("Auto bracket") : QString();
    case 0xA403:
        return value == 0 ? QStringLiteral("Auto") : value == 1 ? QStringLiteral("Manual") : QString();
    case 0xA406: {
        static const char *const names[] = {"Standard", "Landscape", "Portrait", "Night"};
        return value >= 0 && value <= 3 ? QString::fromLatin1(names[value]) : QString();
    }
    }
    return {};
}

// --------------------------------------------------------------------------- //
// Percurso do TIFF
// --------------------------------------------------------------------------- //

/// Leitor de um blob TIFF (sem o "Exif\0\0"), com checagem de limites.
class Tiff
{
public:
    explicit Tiff(const QByteArray &data)
        : m_data(data)
    {
        if (m_data.size() < 8)
            return;
        if (m_data.startsWith("II"))
            m_little = true;
        else if (!m_data.startsWith("MM"))
            return;
        if (u16(2) != kTiffMagic)
            return;
        m_ifd0 = u32(4);
        m_ok = true;
    }

    bool ok() const { return m_ok; }
    quint32 ifd0() const { return m_ifd0; }

    bool has(qint64 off, qint64 len) const
    {
        return off >= 0 && len >= 0 && off <= m_data.size() && len <= m_data.size() - off;
    }
    quint16 u16(qint64 off) const { return rd16(ptr(off), m_little); }
    quint32 u32(qint64 off) const { return rd32(ptr(off), m_little); }

    /// Lê o IFD em ``off`` para ``out`` (grupo ``ifd``) e devolve os ponteiros
    /// de sub-IFD que ele traz (tag -> offset) e o próximo IFD encadeado.
    bool readIfd(qint64 off, const QString &ifd, ExifTags &out, QList<std::pair<quint16, quint32>> &subs,
                 quint32 &next) const
    {
        next = 0;
        if (off < 8 || !has(off, 2))
            return false;
        const int n = u16(off);
        if (n == 0 || n > kMaxIfdEntries || !has(off + 2, qint64(n) * kIfdEntrySize))
            return false;
        for (int i = 0; i < n; ++i) {
            const qint64 e = off + 2 + qint64(i) * kIfdEntrySize;
            ExifTag tag;
            tag.ifd = ifd;
            tag.tag = u16(e);
            tag.type = u16(e + 2);
            tag.count = u32(e + 4);
            tag.little = m_little;
            const int sz = exifTypeSize(tag.type);
            if (sz == 0)
                continue;
            const qint64 total = qint64(sz) * tag.count;
            const qint64 valueOff = total <= 4 ? e + 8 : qint64(u32(e + 8));
            const qint64 keep = std::min<qint64>(total, kMaxTagBytes);
            if (!has(valueOff, keep))
                continue;
            tag.data = m_data.mid(valueOff, keep);
            if ((tag.tag == kExifIfdPointer || tag.tag == kGpsIfdPointer || tag.tag == kInteropIfdPointer)
                && (tag.type == 4 || tag.type == 13) && tag.count >= 1)
                subs.append({tag.tag, u32(e + 8)});
            out.append(tag);
        }
        if (has(off + 2 + qint64(n) * kIfdEntrySize, 4))
            next = u32(off + 2 + qint64(n) * kIfdEntrySize);
        return true;
    }

private:
    const uchar *ptr(qint64 off) const { return reinterpret_cast<const uchar *>(m_data.constData() + off); }

    const QByteArray &m_data;
    bool m_ok = false;
    bool m_little = false;
    quint32 m_ifd0 = 0;
};

const ExifTag *findTag(const ExifTags &tags, quint16 tag, std::initializer_list<const char *> ifds)
{
    for (const char *ifd : ifds)
        for (const ExifTag &t : tags)
            if (t.tag == tag && t.ifd == QLatin1String(ifd))
                return &t;
    return nullptr;
}

} // namespace

int exifTypeSize(int type)
{
    switch (type) {
    case 1: case 2: case 6: case 7: return 1;
    case 3: case 8: return 2;
    case 4: case 9: case 11: case 13: return 4;
    case 5: case 10: case 12: return 8;
    default: return 0;
    }
}

ExifTags readExifTags(const QByteArray &exif)
{
    ExifTags out;
    const QByteArray tiff = exif.startsWith(kExifHeader) ? exif.mid(kExifHeader.size()) : exif;
    const Tiff t(tiff);
    if (!t.ok())
        return out;

    // IFD0 -> IFD1 -> ... pela cadeia; Exif, GPS e Interop pelos ponteiros.
    std::set<quint32> seen;
    QList<std::pair<QString, quint32>> pending;
    quint32 chain = t.ifd0();
    for (int index = 0; chain != 0 && index < kMaxIfds; ++index) {
        if (!seen.insert(chain).second)
            break;
        QList<std::pair<quint16, quint32>> subs;
        quint32 next = 0;
        if (!t.readIfd(chain, QStringLiteral("IFD%1").arg(index), out, subs, next))
            break;
        for (const auto &[tag, off] : subs)
            pending.append({tag == kExifIfdPointer ? QStringLiteral("Exif")
                            : tag == kGpsIfdPointer ? QStringLiteral("GPS")
                                                    : QStringLiteral("Interop"),
                            off});
        chain = next;
    }
    while (!pending.isEmpty()) {
        const auto [ifd, off] = pending.takeFirst();
        if (!seen.insert(off).second)
            continue;
        QList<std::pair<quint16, quint32>> subs;
        quint32 next = 0;
        t.readIfd(off, ifd, out, subs, next);
        for (const auto &[tag, sub] : subs)
            if (tag == kInteropIfdPointer)
                pending.append({QStringLiteral("Interop"), sub});
    }
    return out;
}

QString exifTagName(const QString &ifd, quint16 tag)
{
    const auto &names = ifd == QLatin1String("GPS") ? gpsNames() : tiffNames();
    if (const char *name = names.value(tag, nullptr))
        return QString::fromLatin1(name);
    return QStringLiteral("0x%1").arg(tag, 4, 16, QLatin1Char('0')).toUpper().replace(QLatin1String("0X"), QLatin1String("0x"));
}

QString exifValueText(const ExifTag &t)
{
    if (t.tag == 0x927C)  // MakerNote: binário do fabricante
        return QStringLiteral("(%1 bytes)").arg(t.count);
    if (t.type == 2)
        return textValue(t);
    if (t.type == 1 && t.ifd != QLatin1String("GPS") && t.tag >= 0x9C9B && t.tag <= 0x9C9F) {
        // XPTitle, XPComment...: UTF-16LE do Windows.
        QByteArray bytes = t.data;
        if (bytes.size() % 2)
            bytes.chop(1);
        return stripNulSpace(QString::fromUtf16(reinterpret_cast<const char16_t *>(bytes.constData()), bytes.size() / 2));
    }
    if (t.type == 7) {
        if (t.tag == 0x9286 && t.data.size() > 8) {  // UserComment: 8 bytes de código + texto
            const QByteArray code = t.data.left(8);
            const QByteArray body = t.data.mid(8);
            if (code.startsWith("UNICODE")) {
                const bool little = t.little;
                QString s;
                for (qsizetype i = 0; i + 1 < body.size(); i += 2) {
                    const auto *p = reinterpret_cast<const uchar *>(body.constData() + i);
                    s.append(QChar(little ? (p[0] | (p[1] << 8)) : ((p[0] << 8) | p[1])));
                }
                return stripNulSpace(s);
            }
            return printable(body) ? stripNulSpace(QString::fromLatin1(body)) : QStringLiteral("(%1 bytes)").arg(t.count);
        }
        if (printable(t.data) && t.data.size() <= 256)
            return stripNulSpace(QString::fromLatin1(t.data));
        if (t.data.size() <= 16 && qint64(t.data.size()) == qint64(t.count))
            return QString::fromLatin1(t.data.toHex(' '));
        return QStringLiteral("(%1 bytes)").arg(t.count);
    }

    const int n = valueCount(t);
    if (n == 1) {
        const auto v = numberAt(t, 0);
        if (v && t.ifd != QLatin1String("GPS")) {
            const QString e = enumText(t.tag, int(*v));
            if (!e.isEmpty())
                return e;
        }
    }
    QStringList parts;
    for (int i = 0; i < std::min(n, kMaxListedValues); ++i)
        parts << numberText(t, i);
    QString text = parts.join(u' ');
    if (qint64(t.count) > parts.size())
        text += QStringLiteral(" … (%1 values)").arg(t.count);
    return text;
}

QDateTime parseExifDateTime(const QString &text, const QString &subsec)
{
    static const QRegularExpression kDateRe(
        QStringLiteral("^(\\d{4})[:\\-](\\d{1,2})[:\\-](\\d{1,2})[ T](\\d{1,2}):(\\d{1,2}):(\\d{1,2})"));
    const QString raw = stripNulSpace(text);
    const auto m = kDateRe.match(raw);
    if (!m.hasMatch())
        return {};
    const QDate date(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    const int h = m.captured(4).toInt();
    const int mi = m.captured(5).toInt();
    const int s = m.captured(6).toInt();
    // "0000:00:00 00:00:00" é o "sem data" de muitas câmeras.
    if (!date.isValid() || date.year() < 1800 || h > 23 || mi > 59 || s > 59)
        return {};
    int ms = 0;
    const QString sub = stripNulSpace(subsec);
    static const QRegularExpression kDigits(QStringLiteral("^\\d+$"));
    if (kDigits.match(sub).hasMatch())
        ms = (sub + QStringLiteral("000")).left(3).toInt();
    // Relógio da câmera, sem fuso: guardado como "hora local do evento".
    return QDateTime(date, QTime(h, mi, s, ms), QTimeZone::UTC);
}

ShotInfo summarize(const ExifTags &tags)
{
    ShotInfo info;
    auto text = [&](quint16 tag, std::initializer_list<const char *> ifds) {
        const ExifTag *t = findTag(tags, tag, ifds);
        return t ? exifValueText(*t) : QString();
    };
    auto number = [&](quint16 tag, std::initializer_list<const char *> ifds) {
        const ExifTag *t = findTag(tags, tag, ifds);
        const auto v = t ? numberAt(*t, 0) : std::nullopt;
        return v && std::isfinite(*v) ? *v : 0.0;
    };
    const auto main = {"IFD0", "Exif"};
    const auto exif = {"Exif", "IFD0"};

    if (const ExifTag *o = findTag(tags, kExifOrientation, {"IFD0"})) {
        const auto v = numberAt(*o, 0);
        if (v && *v >= 1 && *v <= 8)
            info.orientation = int(*v);
    }
    info.takenLocal = parseExifDateTime(text(0x9003, exif), text(0x9291, exif));
    if (!info.takenLocal.isValid())
        info.takenLocal = parseExifDateTime(text(0x9004, exif), text(0x9292, exif));
    info.takenOffset = text(0x9011, exif);
    info.modifiedLocal = parseExifDateTime(text(0x0132, {"IFD0"}), text(0x9290, exif));
    info.make = text(0x010F, main);
    info.model = text(0x0110, main);
    info.lens = text(0xA434, exif);
    if (info.lens.isEmpty())
        info.lens = text(0xC630, {"IFD0"}).isEmpty() ? QString() : QString();
    info.software = text(0x0131, main);
    info.artist = text(0x013B, main);
    info.copyright = text(0x8298, main);
    info.description = text(0x010E, main);
    info.exposureTime = std::max(0.0, number(0x829A, exif));
    info.fNumber = std::max(0.0, number(0x829D, exif));
    info.focalLength = std::max(0.0, number(0x920A, exif));
    info.focalLength35 = int(std::max(0.0, number(0xA405, exif)));
    info.exposureBias = number(0x9204, exif);
    info.iso = int(std::lround(std::clamp(number(0x8827, exif), 0.0, 1e7)));
    if (info.iso == 0)
        info.iso = int(std::lround(std::clamp(number(0x8833, exif), 0.0, 1e7)));
    if (findTag(tags, 0x9209, exif))
        info.flash = int(number(0x9209, exif));
    info.width = int(number(0xA002, {"Exif"}));
    info.height = int(number(0xA003, {"Exif"}));
    if (info.width <= 0 || info.height <= 0) {
        info.width = int(number(0x0100, {"IFD0"}));
        info.height = int(number(0x0101, {"IFD0"}));
    }

    // GPS: graus/minutos/segundos + referência (N/S, E/W, acima/abaixo do mar).
    const ExifTag *lat = findTag(tags, 0x0002, {"GPS"});
    const ExifTag *lon = findTag(tags, 0x0004, {"GPS"});
    if (lat && lon) {
        const auto la = dmsDegrees(*lat);
        const auto lo = dmsDegrees(*lon);
        if (la && lo && !(*la == 0.0 && *lo == 0.0) && std::abs(*la) <= 90 && std::abs(*lo) <= 180) {
            info.hasGps = true;
            info.latitude = text(0x0001, {"GPS"}).startsWith(u'S') ? -*la : *la;
            info.longitude = text(0x0003, {"GPS"}).startsWith(u'W') ? -*lo : *lo;
            info.altitude = number(0x0006, {"GPS"});
            if (number(0x0005, {"GPS"}) == 1.0)
                info.altitude = -info.altitude;
        }
    }
    return info;
}

QString formatShutter(double seconds)
{
    if (!(seconds > 0))
        return {};
    if (seconds < 0.5)
        return QStringLiteral("1/%1").arg(std::lround(1.0 / seconds));
    const double r = std::nearbyint(seconds * 10.0) / 10.0;
    return (r == std::floor(r) ? QString::number(qint64(r)) : QString::number(r, 'f', 1)) + u's';
}

QString formatAperture(double fNumber)
{
    if (!(fNumber > 0))
        return {};
    const double r = std::nearbyint(fNumber * 10.0) / 10.0;
    return QStringLiteral("f/") + (r == std::floor(r) ? QString::number(qint64(r)) : QString::number(r, 'f', 1));
}

QString cameraName(const QString &make, const QString &model)
{
    const QString mk = make.simplified(), md = model.simplified();
    if (mk.isEmpty())
        return md;
    if (md.isEmpty())
        return mk;
    // "NIKON CORPORATION" + "NIKON D850" -> "NIKON D850"
    const QString brand = mk.section(u' ', 0, 0);
    if (md.startsWith(brand, Qt::CaseInsensitive))
        return md;
    return mk + u' ' + md;
}

} // namespace pix::imaging

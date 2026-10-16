// SPDX-License-Identifier: BSD-2-Clause
#include "core/Disks.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QUrl>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <winnetwk.h>
#elif defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#endif

namespace pix {

namespace {

QString cleanMount(const QString &mount)
{
    QString m = QDir::fromNativeSeparators(mount);
    if (!m.endsWith(u'/'))
        m += u'/';
    return m;
}

#if defined(Q_OS_LINUX)

QString readSys(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

/// UUID do sistema de arquivos pelo /dev/disk/by-uuid (links para o dispositivo).
QString linuxUuid(const QString &device)
{
    const QString target = QFileInfo(device).canonicalFilePath();
    if (target.isEmpty())
        return {};
    for (const char *dir : {"/dev/disk/by-uuid", "/dev/disk/by-partuuid"}) {
        QDirIterator it(QString::fromLatin1(dir), QDir::System | QDir::Files | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            const QString link = it.next();
            if (QFileInfo(link).canonicalFilePath() == target)
                return (QLatin1String(dir).endsWith(QLatin1String("partuuid")) ? QStringLiteral("partuuid:")
                                                                                : QStringLiteral("uuid:"))
                    + QFileInfo(link).fileName().toLower();
        }
    }
    return {};
}

void linuxDevice(const QString &device, DiskInfo &info)
{
    const QString name = QFileInfo(QFileInfo(device).canonicalFilePath()).fileName();  // sda1, nvme0n1p2
    if (name.isEmpty())
        return;
    const QString block = QStringLiteral("/sys/class/block/") + name;
    // A partição aponta para o disco pai: é nele que estão removable e o modelo.
    QString parent = QFileInfo(block).canonicalFilePath();
    if (QFileInfo::exists(parent + QStringLiteral("/partition")))
        parent = QFileInfo(parent).absolutePath();
    if (readSys(parent + QStringLiteral("/removable")) == QLatin1String("1"))
        info.kind = QStringLiteral("removable");
    info.model = readSys(parent + QStringLiteral("/device/model"));
    if (info.serial.isEmpty())
        info.serial = readSys(parent + QStringLiteral("/device/serial"));
}

#elif defined(Q_OS_MACOS)

QString cfString(CFTypeRef value)
{
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
        return {};
    const auto str = static_cast<CFStringRef>(value);
    const CFIndex len = CFStringGetLength(str);
    QString out(len, Qt::Uninitialized);
    CFStringGetCharacters(str, CFRangeMake(0, len), reinterpret_cast<UniChar *>(out.data()));
    return out;
}

void macDevice(const QString &device, DiskInfo &info)
{
    QString bsd = device;
    if (bsd.startsWith(QLatin1String("/dev/")))
        bsd = bsd.mid(5);
    DASessionRef session = DASessionCreate(kCFAllocatorDefault);
    if (!session)
        return;
    const QByteArray name = bsd.toUtf8();
    DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, name.constData());
    if (disk) {
        if (CFDictionaryRef desc = DADiskCopyDescription(disk)) {
            if (const void *uuid = CFDictionaryGetValue(desc, kDADiskDescriptionVolumeUUIDKey)) {
                CFStringRef text = CFUUIDCreateString(kCFAllocatorDefault, static_cast<CFUUIDRef>(uuid));
                info.key = QStringLiteral("uuid:") + cfString(text).toLower();
                CFRelease(text);
            }
            const QString volumeName = cfString(CFDictionaryGetValue(desc, kDADiskDescriptionVolumeNameKey));
            if (!volumeName.isEmpty())
                info.label = volumeName;
            info.model = cfString(CFDictionaryGetValue(desc, kDADiskDescriptionDeviceModelKey)).trimmed();
            const void *internal = CFDictionaryGetValue(desc, kDADiskDescriptionDeviceInternalKey);
            const void *removable = CFDictionaryGetValue(desc, kDADiskDescriptionMediaRemovableKey);
            if ((removable && CFBooleanGetValue(static_cast<CFBooleanRef>(removable)))
                || (internal && !CFBooleanGetValue(static_cast<CFBooleanRef>(internal))))
                info.kind = QStringLiteral("removable");
            CFRelease(desc);
        }
        CFRelease(disk);
    }
    CFRelease(session);
}

#elif defined(Q_OS_WIN)

void windowsVolume(const QString &root, DiskInfo &info)
{
    QString mount = QDir::toNativeSeparators(root);
    if (!mount.endsWith(u'\\'))
        mount += u'\\';
    const std::wstring wroot = mount.toStdWString();
    const UINT type = GetDriveTypeW(wroot.c_str());
    if (type == DRIVE_REMOTE) {
        info.kind = QStringLiteral("network");
        // Unidade mapeada (Z:) -> \\servidor\compartilhamento.
        if (mount.size() >= 2 && mount[1] == u':') {
            wchar_t remote[1024];
            DWORD len = 1024;
            const std::wstring drive = mount.left(2).toStdWString();
            if (WNetGetConnectionW(drive.c_str(), remote, &len) == NO_ERROR)
                info.device = QString::fromWCharArray(remote);
        }
    } else if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM) {
        info.kind = QStringLiteral("removable");
    }
    wchar_t guid[MAX_PATH];
    if (GetVolumeNameForVolumeMountPointW(wroot.c_str(), guid, MAX_PATH)) {
        // "\\?\Volume{GUID}" + a barra invertida final
        const QString volume = QString::fromWCharArray(guid);
        static const QRegularExpression kGuid(QStringLiteral("\\{([0-9A-Fa-f-]+)\\}"));
        const auto m = kGuid.match(volume);
        if (m.hasMatch() && !info.isNetwork())
            info.key = QStringLiteral("volume:") + m.captured(1).toLower();
    }
    wchar_t label[MAX_PATH + 1] = {0};
    wchar_t fs[MAX_PATH + 1] = {0};
    DWORD serial = 0;
    if (GetVolumeInformationW(wroot.c_str(), label, MAX_PATH, &serial, nullptr, nullptr, fs, MAX_PATH)) {
        info.serial = QStringLiteral("%1-%2").arg(serial >> 16, 4, 16, QLatin1Char('0')).arg(serial & 0xFFFF, 4, 16, QLatin1Char('0')).toUpper();
        const QString l = QString::fromWCharArray(label);
        if (!l.isEmpty())
            info.label = l;
        if (info.fsType.isEmpty())
            info.fsType = QString::fromWCharArray(fs);
    }
}

#endif

DiskInfo fromStorage(const QStorageInfo &si, const QString &path)
{
    DiskInfo info;
    info.mountPoint = cleanMount(si.rootPath());
    info.fsType = QString::fromUtf8(si.fileSystemType());
    info.device = QString::fromUtf8(si.device());
    info.label = si.name();
    info.totalBytes = si.bytesTotal();
    info.kind = QStringLiteral("local");

#if defined(Q_OS_WIN)
    // Caminho UNC (\\servidor\compartilhamento\...) sem letra: rede direto.
    const QString native = QDir::toNativeSeparators(path);
    if (native.startsWith(QLatin1String("\\\\")) && !native.startsWith(QLatin1String("\\\\?\\"))) {
        info.kind = QStringLiteral("network");
        const QStringList parts = native.mid(2).split(u'\\', Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            info.device = QStringLiteral("\\\\") + parts[0] + u'\\' + parts[1];
            info.mountPoint = cleanMount(info.device);
        }
    } else {
        windowsVolume(info.mountPoint, info);
    }
#else
    Q_UNUSED(path);
#endif

    QString protocol = networkProtocol(info.fsType, info.device);
#if defined(Q_OS_WIN)
    if (info.isNetwork() && protocol.isEmpty())
        protocol = QStringLiteral("smb");  // compartilhamento do Windows
#endif
    if (!protocol.isEmpty()) {
        info.kind = QStringLiteral("network");
        info.protocol = protocol;
        parseNetworkDevice(info.device, &info.server, &info.share);
        if (!info.server.isEmpty()) {
            info.key = protocol + QStringLiteral("://") + info.server.toLower() + u'/' + info.share.toLower();
            if (info.label.isEmpty() || info.label == info.device)
                info.label = info.share.isEmpty() ? info.server : info.share.section(u'/', -1);
        }
    } else {
#if defined(Q_OS_LINUX)
        info.key = linuxUuid(info.device);
        linuxDevice(info.device, info);
#elif defined(Q_OS_MACOS)
        macDevice(info.device, info);
#endif
    }
    if (info.label.isEmpty())
        info.label = info.mountPoint == QLatin1String("/") ? QStringLiteral("/") : QFileInfo(info.mountPoint.chopped(1)).fileName();
    if (info.key.isEmpty())
        info.key = QStringLiteral("fs:%1:%2:%3").arg(info.fsType, info.label).arg(info.totalBytes);
    return info;
}

} // namespace

QString networkProtocol(const QString &fsType, const QString &device)
{
    const QString fs = fsType.toLower();
    if (fs == QLatin1String("smbfs") || fs == QLatin1String("cifs") || fs.startsWith(QLatin1String("smb")))
        return QStringLiteral("smb");
    if (fs.startsWith(QLatin1String("nfs")))
        return QStringLiteral("nfs");
    if (fs == QLatin1String("afpfs"))
        return QStringLiteral("afp");
    if (fs == QLatin1String("webdav") || fs == QLatin1String("davfs") || fs == QLatin1String("fuse.davfs"))
        return QStringLiteral("webdav");
    if (fs == QLatin1String("fuse.sshfs") || fs == QLatin1String("sshfs")
        || ((fs.contains(QLatin1String("fuse")) || fs == QLatin1String("macfuse")) && device.contains(u'@') && device.contains(u':')))
        return QStringLiteral("sshfs");
    if (fs == QLatin1String("ftp") || fs == QLatin1String("fuse.curlftpfs"))
        return QStringLiteral("ftp");
    if (fs == QLatin1String("9p") || fs == QLatin1String("virtiofs"))
        return QStringLiteral("vm");
    if (fs.startsWith(QLatin1String("fuse.rclone")))
        return QStringLiteral("rclone");
    return {};
}

void parseNetworkDevice(const QString &device, QString *server, QString *share)
{
    QString d = device.trimmed();
    server->clear();
    share->clear();
    if (d.startsWith(QLatin1String("\\\\")))
        d = QStringLiteral("//") + d.mid(2).replace(u'\\', u'/');
    if (d.contains(QLatin1String("://"))) {
        const QUrl url(d);
        *server = url.host();
        *share = url.path().section(u'/', 1);
        return;
    }
    if (d.startsWith(QLatin1String("//"))) {
        // //[user[:senha]@]servidor/compartilhamento[/sub]
        QString rest = d.mid(2);
        const qsizetype at = rest.lastIndexOf(u'@', rest.indexOf(u'/'));
        if (at >= 0)
            rest = rest.mid(at + 1);
        *server = rest.section(u'/', 0, 0);
        *share = rest.section(u'/', 1);
        return;
    }
    // servidor:/exportação (NFS) ou user@servidor:/caminho (sshfs)
    const qsizetype colon = d.indexOf(u':');
    if (colon > 0) {
        QString host = d.left(colon);
        if (host.contains(u'@'))
            host = host.section(u'@', -1);
        *server = host;
        *share = d.mid(colon + 1);
        while (share->startsWith(u'/'))
            share->remove(0, 1);
    }
}

DiskInfo identifyDisk(const QString &path)
{
    const QStorageInfo si(path);
    if (!si.isValid())
        return {};
    return fromStorage(si, path);
}

QList<DiskInfo> mountedDisks()
{
    QList<DiskInfo> out;
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes())
        if (si.isValid() && si.isReady())
            out << fromStorage(si, si.rootPath());
    return out;
}

QString relativeToMount(const QString &path, const QString &mountPoint)
{
    const QString p = QDir::fromNativeSeparators(QDir::cleanPath(path));
    const QString m = cleanMount(mountPoint);
    if ((p + u'/').startsWith(m, Qt::CaseInsensitive))
        return p.mid(m.size());
    return p;
}

} // namespace pix

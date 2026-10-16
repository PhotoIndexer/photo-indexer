// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// De que disco vem uma pasta, em qualquer SO.
//
// A IDENTIDADE do disco (``key``) é o que não muda quando ele é montado em
// outro lugar ou recebe outra letra:
//
//   rede      "<protocolo>://<servidor>/<compartilhamento>"   (smb, nfs, afp,
//             webdav, sshfs...; servidor e compartilhamento em minúsculas)
//   Linux     "uuid:<UUID do sistema de arquivos>" (/dev/disk/by-uuid)
//   macOS     "uuid:<VolumeUUID>" (DiskArbitration)
//   Windows   "volume:<GUID do volume>" (GetVolumeNameForVolumeMountPoint)
//   sem nada disso: "fs:<tipo>:<rótulo>:<bytes>"
//
// O catálogo guarda o caminho de cada pasta RELATIVO ao ponto de montagem:
// se o disco volta em outro lugar, os caminhos são refeitos (Catalog).

#include <QList>
#include <QString>

namespace pix {

struct DiskInfo
{
    QString key;            ///< identidade estável (ver acima)
    QString label;          ///< nome do volume (ou do compartilhamento)
    QString kind;           ///< "local" | "removable" | "network"
    QString protocol;       ///< rede: "smb", "nfs", "afp", "webdav", "sshfs"...
    QString server;         ///< rede: servidor
    QString share;          ///< rede: compartilhamento/exportação
    QString fsType;         ///< "apfs", "ntfs", "ext4", "smbfs"...
    QString device;         ///< "/dev/disk3s1", "\\?\Volume{...}\", "//srv/share"
    QString serial;         ///< número de série do volume (Windows)
    QString model;          ///< modelo do dispositivo, quando o SO informa
    QString mountPoint;     ///< onde está montado agora ("/Volumes/X", "D:/")
    qint64 totalBytes = 0;

    bool isValid() const { return !key.isEmpty(); }
    bool isNetwork() const { return kind == QLatin1String("network"); }
};

/// O disco de ``path`` (que precisa existir).
DiskInfo identifyDisk(const QString &path);

/// Todos os volumes montados agora (para reconhecer discos que voltaram).
QList<DiskInfo> mountedDisks();

/// ``path`` relativo ao ponto de montagem do disco ("" se for a raiz).
QString relativeToMount(const QString &path, const QString &mountPoint);

/// Protocolo de rede de um tipo de sistema de arquivos ("" se não é de rede).
QString networkProtocol(const QString &fsType, const QString &device);

/// Servidor e compartilhamento de um dispositivo de rede
/// ("//user@srv/share", "srv:/export", "\\srv\share", "https://srv/dav").
void parseNetworkDevice(const QString &device, QString *server, QString *share);

} // namespace pix

// SPDX-License-Identifier: BSD-2-Clause
#include "core/FileOps.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStorageInfo>

#include <optional>

Q_LOGGING_CATEGORY(lcFiles, "photoidx.files")

namespace pix::fileops {

namespace {

constexpr qint64 kChunk = 4 * 1024 * 1024;

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

bool sameVolume(const QString &a, const QString &b)
{
    const QStorageInfo x(a), y(b);
    return x.isValid() && y.isValid() && x.rootPath() == y.rootPath() && x.device() == y.device();
}

std::optional<QByteArray> sha256(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        const QByteArray chunk = f.read(kChunk);
        if (chunk.isEmpty() && f.error() != QFileDevice::NoError)
            return std::nullopt;
        hash.addData(chunk);
    }
    return hash.result();
}

/// Copia ``from`` para ``to`` (que não existe), conferindo: o que foi escrito
/// é relido e comparado com o original. Mantém a data de modificação.
QString copyVerified(const QString &from, const QString &to)
{
    QFile src(from);
    if (!src.open(QIODevice::ReadOnly))
        return QStringLiteral("files.readFailed");
    QSaveFile dst(to);
    if (!dst.open(QIODevice::WriteOnly))
        return QStringLiteral("files.writeFailed");
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!src.atEnd()) {
        const QByteArray chunk = src.read(kChunk);
        if (chunk.isEmpty() && src.error() != QFileDevice::NoError) {
            dst.cancelWriting();
            return QStringLiteral("files.readFailed");
        }
        hash.addData(chunk);
        if (dst.write(chunk) != chunk.size()) {
            dst.cancelWriting();
            return QStringLiteral("files.writeFailed");
        }
    }
    const QDateTime modified = QFileInfo(src).lastModified();
    src.close();
    if (!dst.commit())
        return QStringLiteral("files.writeFailed");
    const auto written = sha256(to);
    if (!written || *written != hash.result() || QFileInfo(to).size() != QFileInfo(from).size()) {
        QFile::remove(to);
        return QStringLiteral("files.verifyFailed");
    }
    QFile out(to);
    if (out.open(QIODevice::ReadWrite))
        out.setFileTime(modified, QFileDevice::FileModificationTime);
    return {};
}

} // namespace

bool validName(const QString &name)
{
    const QString n = name.trimmed();
    if (n.isEmpty() || n == QLatin1String(".") || n == QLatin1String(".."))
        return false;
    for (const QChar c : n)
        if (c == u'/' || c == u'\\' || c == u':' || c.unicode() < 32)
            return false;
    return true;
}

bool isInside(const QString &path, const QString &dir)
{
    const QString p = clean(path), d = clean(dir);
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    if (p.compare(d, cs) == 0)
        return true;
    const QString prefix = d.endsWith(u'/') ? d : d + u'/';
    return p.startsWith(prefix, cs);
}

QString freeName(const QString &dir, const QString &fileName)
{
    const QDir d(dir);
    if (!QFileInfo::exists(d.filePath(fileName)))
        return fileName;
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : u'.' + info.suffix();
    for (int i = 2;; ++i) {
        const QString candidate = QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix);
        if (!QFileInfo::exists(d.filePath(candidate)))
            return candidate;
    }
}

QString moveFile(const QString &from, const QString &to)
{
    if (!QFileInfo::exists(from))
        return QStringLiteral("files.sourceMissing");
    if (QFileInfo::exists(to))
        return QStringLiteral("files.exists");
    const QString targetDir = QFileInfo(to).absolutePath();
    if (!QFileInfo(targetDir).isDir())
        return QStringLiteral("files.targetMissing");
    if (sameVolume(from, targetDir)) {
        // O rename do próprio SO: atômico, nada é copiado. (QFile::rename,
        // se falhar, copiaria sem conferir — por isso só no mesmo volume.)
        if (QDir().rename(from, to))
            return {};
        qCWarning(lcFiles) << "Rename failed:" << from << "->" << to;
        return QStringLiteral("files.moveFailed");
    }
    if (const QString error = copyVerified(from, to); !error.isEmpty()) {
        qCWarning(lcFiles) << "Copy failed:" << from << "->" << to << error;
        return error;
    }
    if (!QFile::remove(from)) {
        // Não conseguiu tirar o original: desfaz a cópia (a foto nunca fica
        // em dois lugares).
        QFile::remove(to);
        qCWarning(lcFiles) << "Original not removed after copy:" << from;
        return QStringLiteral("files.moveFailed");
    }
    return {};
}

QString moveDir(const QString &from, const QString &to, QList<Moved> *moved)
{
    if (!QFileInfo(from).isDir())
        return QStringLiteral("files.sourceMissing");
    if (QFileInfo::exists(to))
        return QStringLiteral("files.exists");
    if (isInside(to, from))
        return QStringLiteral("files.intoItself");
    const QString parent = QFileInfo(to).absolutePath();
    if (!QFileInfo(parent).isDir())
        return QStringLiteral("files.targetMissing");
    if (sameVolume(from, parent)) {
        if (QDir().rename(from, to)) {
            if (moved)
                moved->append({from, to});
            return {};
        }
        qCWarning(lcFiles) << "Folder rename failed:" << from << "->" << to;
        return QStringLiteral("files.moveFailed");
    }
    // Outro volume: arquivo por arquivo, depois as pastas que ficaram vazias.
    const QString base = clean(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
    QStringList files;
    while (it.hasNext())
        files << it.next();
    for (const QString &file : files) {
        const QString target = clean(to) + clean(file).mid(base.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (const QString error = moveFile(file, target); !error.isEmpty())
            return error;
        if (moved)
            moved->append({file, target});
    }
    QDirIterator dirs(from, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    QStringList empty;
    while (dirs.hasNext())
        empty << dirs.next();
    std::sort(empty.begin(), empty.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &d : empty) {
        QDir().mkpath(clean(to) + clean(d).mid(base.size()));
        QDir().rmdir(d);
    }
    QDir().mkpath(to);
    QDir().rmdir(from);
    return {};
}

QString makeDir(const QString &parent, const QString &name, QString *created)
{
    if (!validName(name))
        return QStringLiteral("files.badName");
    if (!QFileInfo(parent).isDir())
        return QStringLiteral("files.targetMissing");
    const QString path = QDir(parent).filePath(name.trimmed());
    if (QFileInfo::exists(path))
        return QStringLiteral("files.exists");
    if (!QDir().mkdir(path))
        return QStringLiteral("files.writeFailed");
    if (created)
        *created = path;
    return {};
}

QString trash(const QString &path)
{
    if (!QFileInfo::exists(path))
        return QStringLiteral("files.sourceMissing");
    QString where;
    if (QFile::moveToTrash(path, &where))
        return {};
    // Nunca apaga de vez: sem Lixeira (rede, alguns discos externos), recusa.
    qCWarning(lcFiles) << "Trash not available for" << path;
    return QStringLiteral("files.noTrash");
}

} // namespace pix::fileops

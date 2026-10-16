// SPDX-License-Identifier: BSD-2-Clause
#include "core/Paths.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace pix {

namespace {

QString ensureDir(const QString &path)
{
    QDir().mkpath(path);
    return path;
}

QString datedPath(const QString &root, const QString &id, const QDate &date)
{
    const QString folder = date.isValid() ? QStringLiteral("%1/%2").arg(date.year(), 4, 10, QLatin1Char('0')).arg(date.month(), 2, 10, QLatin1Char('0'))
                                          : QStringLiteral("undated");
    return QDir(root).filePath(folder + u'/' + id + QStringLiteral(".jpg"));
}

} // namespace

QString AppPaths::dataDir()
{
    const QString env = qEnvironmentVariable("PHOTOIDX_DATA_DIR");
    if (!env.isEmpty())
        return ensureDir(QDir(env).absolutePath());
    return ensureDir(QDir::home().filePath(QStringLiteral(".photo_indexer")));
}

QString AppPaths::database()
{
    return QDir(dataDir()).filePath(QStringLiteral("data.sqlite3"));
}

QString AppPaths::thumbnailsDir()
{
    return ensureDir(QDir(dataDir()).filePath(QStringLiteral("thumbnail")));
}

QString AppPaths::facesDir()
{
    return ensureDir(QDir(dataDir()).filePath(QStringLiteral("faces")));
}

QString AppPaths::logsDir()
{
    return ensureDir(QDir(dataDir()).filePath(QStringLiteral("logs")));
}

QString AppPaths::thumbnailPath(const QString &photoId, const QDate &date)
{
    return datedPath(thumbnailsDir(), photoId, date);
}

QString AppPaths::facePath(const QString &faceId, const QDate &date)
{
    return datedPath(facesDir(), faceId, date);
}

QString AppPaths::modelDir(const QString &name)
{
    const QString env = qEnvironmentVariable("PHOTOIDX_MODELS_DIR");
    if (!env.isEmpty())
        return QDir(env).filePath(name);
    // Instalado: ao lado do executável (Windows), em share/ (Linux) ou em
    // Resources (macOS).
    const QDir app(QCoreApplication::applicationDirPath());
    for (const QString &candidate : {app.filePath(QStringLiteral("models/") + name),
                                     app.filePath(QStringLiteral("../share/photoidx/models/") + name),
                                     app.filePath(QStringLiteral("../Resources/models/") + name)}) {
        if (QFileInfo(candidate).isDir())
            return QDir::cleanPath(candidate);
    }
    // Desenvolvimento: o .cache/models/ do repositório (make model).
    QDir dir(app);
    for (int i = 0; i < 6; ++i) {
        if (QFileInfo(dir.filePath(QStringLiteral(".cache/models/") + name)).isDir())
            return dir.filePath(QStringLiteral(".cache/models/") + name);
        if (!dir.cdUp())
            break;
    }
    return app.filePath(QStringLiteral("models/") + name);
}

} // namespace pix

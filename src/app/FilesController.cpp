// SPDX-License-Identifier: BSD-2-Clause
#include "FilesController.h"

#include "LibraryController.h"
#include "core/Catalog.h"
#include "core/Perf.h"
#include "core/Pipeline.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcFilesUi, "photoidx.files.ui")

using namespace pix;

namespace {

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

QString parentOf(const QString &path)
{
    return clean(QFileInfo(clean(path)).absolutePath());
}

} // namespace

FilesController::FilesController(Catalog *catalog, Pipeline *pipeline, LibraryController *library, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_pipeline(pipeline)
    , m_library(library)
{
    // Uma operação por vez, na ordem pedida.
    m_pool.setMaxThreadCount(1);
}

bool FilesController::isRootFolder(const QString &dir) const
{
    return m_catalog->isRootFolder(dir);
}

bool FilesController::ready()
{
    if (m_busy) {
        emit failed(QStringLiteral("files.busy"), {});
        return false;
    }
    if (m_pipeline->scanning()) {
        emit failed(QStringLiteral("files.scanRunning"), {});
        return false;
    }
    return true;
}

void FilesController::run(std::function<Result()> work, std::function<void(const Result &)> done)
{
    m_busy = true;
    emit busyChanged();
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, done]() {
        watcher->deleteLater();
        Result r;
        try {
            r = watcher->result();
        } catch (const std::exception &e) {
            r.error = QStringLiteral("error.generic");
            r.errorName = QString::fromUtf8(e.what());
        }
        m_busy = false;
        emit busyChanged();
        PIX_PERF("FilesController::apply");
        done(r);
        // Fotos que uma etapa achou "sumidas" no meio da mudança voltam à fila.
        m_pipeline->filesMoved();
        if (!r.error.isEmpty())
            emit failed(r.error, {{QStringLiteral("name"), QDir::toNativeSeparators(r.errorName)},
                                  {QStringLiteral("detail"), r.errorName}});
    });
    watcher->setFuture(QtConcurrent::run(&m_pool, [work]() {
        PIX_PERF("FilesController::work");
        return work();
    }));
}

void FilesController::follow(const QString &from, const QString &to)
{
    const QString node = m_library->node();
    if (!node.startsWith(QLatin1String("dir:")))
        return;
    const QString current = node.mid(4);
    if (!fileops::isInside(current, from))
        return;
    m_library->selectNode(to.isEmpty() ? QStringLiteral("dir:") + parentOf(from)
                                       : QStringLiteral("dir:") + clean(to) + current.mid(clean(from).size()));
}

// --------------------------------------------------------------------------- //
// Fotos
// --------------------------------------------------------------------------- //

void FilesController::movePhotos(const QStringList &photoIds, const QString &targetDir)
{
    const QString target = clean(targetDir);
    if (!ready())
        return;
    if (m_catalog->rootFolderOf(target).isEmpty()) {
        emit failed(QStringLiteral("files.outsideCatalog"), {{QStringLiteral("name"), QDir::toNativeSeparators(target)}});
        return;
    }
    QList<QPair<QString, QString>> photos;
    for (const auto &p : m_catalog->photoPaths(photoIds))
        if (parentOf(p.second) != target)  // já está lá
            photos.append(p);
    if (photos.isEmpty())
        return;
    qCInfo(lcFilesUi) << "Moving" << photos.size() << "photo(s) to" << target;
    emit notice(QStringLiteral("files.moving"), {{QStringLiteral("count"), photos.size()}});
    run(
        [photos, target]() {
            Result r;
            r.total = int(photos.size());
            for (const auto &[id, from] : photos) {
                const QString to = QDir(target).filePath(fileops::freeName(target, QFileInfo(from).fileName()));
                const QString error = fileops::moveFile(from, to);
                if (error.isEmpty()) {
                    r.moved.append({from, to});
                } else if (r.error.isEmpty()) {
                    r.error = error;
                    r.errorName = from;
                }
            }
            return r;
        },
        [this](const Result &r) {
            m_catalog->applyMoved(r.moved);
            emit notice(QStringLiteral("files.moved"), {{QStringLiteral("count"), r.moved.size()}, {QStringLiteral("total"), r.total}});
        });
}

void FilesController::renamePhoto(const QString &photoId, const QString &name)
{
    if (!ready())
        return;
    const auto photos = m_catalog->photoPaths({photoId});
    if (photos.isEmpty())
        return;
    const QString from = photos.first().second;
    QString newName = name.trimmed();
    if (!fileops::validName(newName)) {
        emit failed(QStringLiteral("files.badName"), {{QStringLiteral("name"), newName}});
        return;
    }
    // A extensão é a do arquivo: renomear não muda o formato.
    const QString suffix = QFileInfo(from).suffix();
    if (!suffix.isEmpty() && QFileInfo(newName).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        newName += u'.' + suffix;
    const QString to = QDir(parentOf(from)).filePath(newName);
    if (clean(to) == clean(from))
        return;
    run(
        [from, to]() {
            Result r;
            r.total = 1;
            r.error = fileops::moveFile(from, to);
            if (r.error.isEmpty())
                r.moved.append({from, to});
            else
                r.errorName = to;
            return r;
        },
        [this](const Result &r) { m_catalog->applyMoved(r.moved); });
}

void FilesController::trashPhotos(const QStringList &photoIds)
{
    if (!ready())
        return;
    const auto photos = m_catalog->photoPaths(photoIds);
    if (photos.isEmpty())
        return;
    qCInfo(lcFilesUi) << "Moving" << photos.size() << "photo(s) to the Trash";
    run(
        [photos]() {
            Result r;
            r.total = int(photos.size());
            for (const auto &[id, path] : photos) {
                const QString error = fileops::trash(path);
                if (error.isEmpty()) {
                    r.trashed << path;
                } else if (r.error.isEmpty()) {
                    r.error = error;
                    r.errorName = path;
                }
            }
            return r;
        },
        [this](const Result &r) {
            if (!r.trashed.isEmpty())
                m_catalog->removePaths(r.trashed);
            emit notice(QStringLiteral("files.trashed"), {{QStringLiteral("count"), r.trashed.size()}, {QStringLiteral("total"), r.total}});
        });
}

// --------------------------------------------------------------------------- //
// Pastas
// --------------------------------------------------------------------------- //

void FilesController::moveFolder(const QString &dir, const QString &targetParent)
{
    const QString from = clean(dir), parent = clean(targetParent);
    if (!ready())
        return;
    if (m_catalog->isRootFolder(from)) {
        emit failed(QStringLiteral("files.rootFolder"), {{QStringLiteral("name"), QDir::toNativeSeparators(from)}});
        return;
    }
    if (m_catalog->rootFolderOf(from).isEmpty() || m_catalog->rootFolderOf(parent).isEmpty()) {
        emit failed(QStringLiteral("files.outsideCatalog"), {{QStringLiteral("name"), QDir::toNativeSeparators(parent)}});
        return;
    }
    if (fileops::isInside(parent, from)) {
        emit failed(QStringLiteral("files.intoItself"), {{QStringLiteral("name"), QFileInfo(from).fileName()}});
        return;
    }
    if (parentOf(from) == parent)
        return;  // já está lá
    const QString to = QDir(parent).filePath(QFileInfo(from).fileName());
    qCInfo(lcFilesUi) << "Moving folder" << from << "to" << to;
    run(
        [from, to]() {
            Result r;
            r.total = 1;
            r.error = fileops::moveDir(from, to, &r.moved);
            if (!r.error.isEmpty())
                r.errorName = from;
            return r;
        },
        [this, from, to](const Result &r) {
            m_catalog->applyMoved(r.moved);
            if (r.error.isEmpty()) {
                follow(from, to);
                emit notice(QStringLiteral("files.folderMoved"), {{QStringLiteral("name"), QFileInfo(to).fileName()}});
            }
        });
}

void FilesController::renameFolder(const QString &dir, const QString &name)
{
    const QString from = clean(dir);
    if (!ready())
        return;
    if (m_catalog->isRootFolder(from)) {
        emit failed(QStringLiteral("files.rootFolder"), {{QStringLiteral("name"), QDir::toNativeSeparators(from)}});
        return;
    }
    if (!fileops::validName(name)) {
        emit failed(QStringLiteral("files.badName"), {{QStringLiteral("name"), name}});
        return;
    }
    const QString to = QDir(parentOf(from)).filePath(name.trimmed());
    if (clean(to) == from)
        return;
    run(
        [from, to]() {
            Result r;
            r.total = 1;
            r.error = fileops::moveDir(from, to, &r.moved);
            if (!r.error.isEmpty())
                r.errorName = to;
            return r;
        },
        [this, from, to](const Result &r) {
            m_catalog->applyMoved(r.moved);
            if (r.error.isEmpty())
                follow(from, to);
        });
}

void FilesController::createFolder(const QString &parent, const QString &name)
{
    const QString p = clean(parent);
    if (!ready())
        return;
    if (m_catalog->rootFolderOf(p).isEmpty()) {
        emit failed(QStringLiteral("files.outsideCatalog"), {{QStringLiteral("name"), QDir::toNativeSeparators(p)}});
        return;
    }
    run(
        [p, name]() {
            Result r;
            r.error = fileops::makeDir(p, name, &r.created);
            if (!r.error.isEmpty())
                r.errorName = name;
            return r;
        },
        [this](const Result &r) {
            if (!r.created.isEmpty())
                m_catalog->addExtraDir(r.created);
        });
}

void FilesController::trashFolder(const QString &dir)
{
    const QString path = clean(dir);
    if (!ready())
        return;
    if (m_catalog->isRootFolder(path)) {
        emit failed(QStringLiteral("files.rootFolder"), {{QStringLiteral("name"), QDir::toNativeSeparators(path)}});
        return;
    }
    if (m_catalog->rootFolderOf(path).isEmpty()) {
        emit failed(QStringLiteral("files.outsideCatalog"), {{QStringLiteral("name"), QDir::toNativeSeparators(path)}});
        return;
    }
    qCInfo(lcFilesUi) << "Moving folder to the Trash:" << path;
    run(
        [path]() {
            Result r;
            r.total = 1;
            r.error = fileops::trash(path);
            if (r.error.isEmpty())
                r.trashed << path;
            else
                r.errorName = path;
            return r;
        },
        [this, path](const Result &r) {
            if (r.trashed.isEmpty())
                return;
            m_catalog->removePaths(r.trashed);
            follow(path, QString());
            emit notice(QStringLiteral("files.folderTrashed"), {{QStringLiteral("name"), QFileInfo(path).fileName()}});
        });
}

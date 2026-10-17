// SPDX-License-Identifier: BSD-2-Clause
// Modo Pastas: o catálogo acompanha o que a pessoa moveu, renomeou ou mandou
// para a Lixeira (os arquivos em si: core/FileOps, fora da thread principal).
// A foto continua a mesma (mesmo id, faces, categorias, álbuns, miniatura):
// só o caminho muda.
#include "core/Catalog.h"
#include "core/Perf.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSqlQuery>

Q_DECLARE_LOGGING_CATEGORY(lcCatalog)

namespace pix {

namespace {

const QString kExtraDirs = QStringLiteral("files/extraDirs");

QString clean(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

/// ``dir`` e as de dentro, no formato da coluna photo.dir (o mesmo do nodeFilter).
const char *const kUnderDir = "(dir = ? OR (dir >= ? || '/' AND dir < ? || '0'))";

} // namespace

QString Catalog::rootFolderOf(const QString &path) const
{
    const QString p = clean(path);
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, path FROM folder"));
    while (q.next())
        if (fileops::isInside(p, q.value(1).toString()))
            return q.value(0).toString();
    return {};
}

bool Catalog::isRootFolder(const QString &dir) const
{
    return m_db.scalar(QStringLiteral("SELECT 1 FROM folder WHERE path = ?"), {clean(dir)}).isValid();
}

bool Catalog::onNetworkDisk(const QString &path) const
{
    const QString folderId = rootFolderOf(path);
    return !folderId.isEmpty()
        && m_db.scalar(QStringLiteral("SELECT d.kind FROM folder f JOIN disk d ON d.id = f.disk_id WHERE f.id = ?"), {folderId})
                   .toString() == QLatin1String("network");
}

QList<QPair<QString, QString>> Catalog::photoPaths(const QStringList &photoIds) const
{
    QList<QPair<QString, QString>> out;
    for (const QString &id : photoIds) {
        const QString path = m_db.scalar(QStringLiteral("SELECT path FROM photo WHERE id = ?"), {id}).toString();
        if (!path.isEmpty())
            out.append({id, path});
    }
    return out;
}

QStringList Catalog::extraDirs() const
{
    QStringList out;
    for (const QJsonValue &v : QJsonDocument::fromJson(setting(kExtraDirs).toString().toUtf8()).array())
        out << v.toString();
    return out;
}

void Catalog::addExtraDir(const QString &dir)
{
    QStringList dirs = extraDirs();
    const QString d = clean(dir);
    if (dirs.contains(d) || rootFolderOf(d).isEmpty())
        return;
    dirs << d;
    setSetting(kExtraDirs, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(dirs)).toJson(QJsonDocument::Compact)));
    emit treeChanged(QStringLiteral("folders"));
}

void Catalog::applyMoved(const QList<fileops::Moved> &moved)
{
    PIX_PERF("Catalog::applyMoved");
    if (moved.isEmpty())
        return;
    QStringList extra = extraDirs();
    Transaction tx(m_db);
    for (const fileops::Moved &m : moved) {
        const QString from = clean(m.from), to = clean(m.to);
        const QString folderId = rootFolderOf(to);
        if (QFileInfo(m.to).isDir()) {
            // Pasta inteira: o começo do caminho de tudo o que está nela.
            m_db.exec(QStringLiteral("UPDATE photo SET path = ? || substr(path, length(?) + 1), "
                                     "dir = ? || substr(dir, length(?) + 1), folder_id = ? WHERE %1")
                          .arg(QLatin1String(kUnderDir)),
                      {to, from, to, from, folderId, from, from, from});
            for (QString &d : extra)
                if (fileops::isInside(d, from))
                    d = to + d.mid(from.size());
        } else {
            const QFileInfo info(to);
            m_db.exec(QStringLiteral("UPDATE photo SET path = ?, dir = ?, name = ?, folder_id = ? WHERE path = ?"),
                      {to, clean(info.absolutePath()), info.fileName(), folderId, from});
            // A pasta de onde saiu continua no painel, mesmo vazia.
            const QString source = clean(QFileInfo(from).absolutePath());
            if (!extra.contains(source) && !rootFolderOf(source).isEmpty() && QFileInfo(source).isDir())
                extra << source;
        }
    }
    setSetting(kExtraDirs, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(extra)).toJson(QJsonDocument::Compact)));
    tx.commit();
    qCInfo(lcCatalog) << "Moved on disk:" << moved.size() << "item(s)";
    emit photosChanged();
    emit treeChanged(QStringLiteral("folders"));
}

void Catalog::removePaths(const QStringList &paths)
{
    PIX_PERF("Catalog::removePaths");
    QStringList photoIds;
    QStringList extra = extraDirs();
    for (const QString &raw : paths) {
        const QString p = clean(raw);
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM photo WHERE path = ? OR %1").arg(QLatin1String(kUnderDir)), {p, p, p, p});
        while (q.next())
            photoIds << q.value(0).toString();
        extra.erase(std::remove_if(extra.begin(), extra.end(), [&p](const QString &d) { return fileops::isInside(d, p); }), extra.end());
    }
    QSet<QString> touched;
    for (const QString &id : photoIds) {
        QSqlQuery f = m_db.exec(QStringLiteral("SELECT DISTINCT person_id FROM face WHERE photo_id = ? AND person_id IS NOT NULL"), {id});
        while (f.next())
            touched.insert(f.value(0).toString());
        removePhotoFiles(id);
    }
    Transaction tx(m_db);
    for (const QString &id : photoIds)
        m_db.exec(QStringLiteral("DELETE FROM photo WHERE id = ?"), {id});
    setSetting(kExtraDirs, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(extra)).toJson(QJsonDocument::Compact)));
    tx.commit();
    for (const QString &person : touched)
        refreshPerson(person);
    m_embeddingsLoaded = false;
    qCInfo(lcCatalog) << "Moved to the Trash:" << paths.size() << "item(s)," << photoIds.size() << "photo(s) left the catalog";
    emit photosChanged();
    emit treeChanged(QStringLiteral("folders"));
    if (!touched.isEmpty())
        emit peopleChanged();
}

} // namespace pix

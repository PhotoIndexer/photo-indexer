// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include "core/Paths.h"
#include "core/Taxonomy.h"
#include "imaging/Exif.h"
#include "imaging/Formats.h"
#include "imaging/Hashes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(lcCatalog, "photoidx.catalog")

namespace pix {

namespace {

/// Data/hora "de relógio" (sem fuso) como o catálogo grava: a árvore de datas
/// agrupa pelo que a câmera marcou, não pelo UTC.
QString wallClock(const QDateTime &dt)
{
    if (!dt.isValid())
        return {};
    return dt.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"));
}

QString localWallClock(const QDateTime &dt)
{
    return dt.isValid() ? wallClock(dt.toLocalTime()) : QString();
}

QString cleanPath(const QString &path)
{
    return QDir::fromNativeSeparators(QDir::cleanPath(QDir(path).absolutePath()));
}

bool isInside(const QString &path, const QString &root)
{
    return path == root || path.startsWith(root.endsWith(u'/') ? root : root + u'/');
}

/// Pastas de discos conectados. As filas filtram por ``+p.folder_id IN``
/// (o "+" deixa o SQLite usar o índice parcial da fila, schema/0003.sql).
constexpr const char *kOnlineFolders = "(SELECT f.id FROM folder f JOIN disk d ON d.id = f.disk_id WHERE d.online = 1)";

QVariant nullable(double value, bool present)
{
    return present ? QVariant(value) : QVariant();
}

} // namespace

Catalog::Catalog(QObject *parent)
    : QObject(parent)
{
}

Catalog::~Catalog() = default;

void Catalog::open(const QString &path)
{
    m_db.open(path);
    seedCategories();
    ensureVault();
    ensurePrivateAlbum();
    loadIndexes();
    loadPeople();
    qCInfo(lcCatalog) << "Catalog open:" << (path.isEmpty() ? QStringLiteral(":memory:") : path) << "-"
                      << m_hashes.size() << "hashed photos," << m_faces.size() << "faces," << m_people.size() << "people";
}

QVariant Catalog::setting(const QString &key, const QVariant &fallback) const
{
    const QVariant v = m_db.scalar(QStringLiteral("SELECT value FROM setting WHERE key = ?"), {key});
    return v.isValid() ? v : fallback;
}

void Catalog::setSetting(const QString &key, const QVariant &value)
{
    m_db.exec(QStringLiteral("INSERT INTO setting(key, value) VALUES(?, ?) "
                             "ON CONFLICT(key) DO UPDATE SET value = excluded.value"),
              {key, value.toString()});
}

// --------------------------------------------------------------------------- //
// Abertura
// --------------------------------------------------------------------------- //

void Catalog::seedCategories()
{
    // Categorias embutidas que ainda não existem (inclusive as que uma versão
    // nova do app acrescentar): pela chave, sem tocar nas que a pessoa mudou.
    Transaction tx(m_db);
    int position = 0;
    for (const BuiltinCategory &c : builtinCategories()) {
        ++position;
        const QString key = QString::fromLatin1(c.key);
        QString id = m_db.scalar(QStringLiteral("SELECT id FROM category WHERE builtin_key = ?"), {key}).toString();
        if (id.isEmpty()) {
            QVariant parent;
            if (c.parentKey)
                parent = m_db.scalar(QStringLiteral("SELECT id FROM category WHERE builtin_key = ?"),
                                     {QString::fromLatin1(c.parentKey)});
            if (c.parentKey && !parent.isValid())
                continue;  // a pessoa apagou o pai: não ressuscita o filho
            id = Database::newId();
            m_db.exec(QStringLiteral("INSERT INTO category(id, parent_id, builtin_key, name, prompt, auto, position, created_at) "
                                     "VALUES(?, ?, ?, ?, ?, 1, ?, ?)"),
                      {id, parent, key, QString::fromLatin1(c.name), QString::fromLatin1(c.prompts), position,
                       Database::timestamp()});
        }
        m_builtinIds.insert(key, id);
    }
    tx.commit();
}

void Catalog::loadIndexes()
{
    m_hashes.clear();
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, phash FROM photo WHERE phash != ''"));
    while (q.next()) {
        bool ok = false;
        const quint64 h = imaging::hashFromHex(q.value(1).toString(), &ok);
        if (ok)
            m_hashes.insert(q.value(0).toString(), h);
    }
    m_faces.clear();
    QSqlQuery f = m_db.exec(QStringLiteral("SELECT id, photo_id, person_id, embedding FROM face"));
    while (f.next())
        m_faces.insert({f.value(0).toString(), f.value(1).toString(), f.value(2).toString()},
                       imaging::fromBlob(f.value(3).toByteArray()));
    m_embeddingsLoaded = false;
}

// --------------------------------------------------------------------------- //
// Discos e pastas
// --------------------------------------------------------------------------- //

QString Catalog::ensureDisk(const DiskInfo &disk)
{
    const QString now = Database::timestamp();
    QString id = m_db.scalar(QStringLiteral("SELECT id FROM disk WHERE disk_key = ?"), {disk.key}).toString();
    if (id.isEmpty()) {
        id = Database::newId();
        m_db.exec(QStringLiteral("INSERT INTO disk(id, disk_key, label, kind, protocol, server, share, fs_type, device, "
                                 "serial, model, mount_point, total_bytes, online, first_seen_at, last_seen_at) "
                                 "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?)"),
                  {id, disk.key, disk.label, disk.kind, disk.protocol, disk.server, disk.share, disk.fsType, disk.device,
                   disk.serial, disk.model, disk.mountPoint, disk.totalBytes, now, now});
        qCInfo(lcCatalog) << "New disk" << disk.key << disk.label << disk.kind << disk.protocol << "at" << disk.mountPoint;
    } else {
        m_db.exec(QStringLiteral("UPDATE disk SET label = ?, kind = ?, protocol = ?, server = ?, share = ?, fs_type = ?, "
                                 "device = ?, serial = ?, model = ?, total_bytes = ?, online = 1, last_seen_at = ? "
                                 "WHERE id = ?"),
                  {disk.label, disk.kind, disk.protocol, disk.server, disk.share, disk.fsType, disk.device, disk.serial,
                   disk.model, disk.totalBytes, now, id});
    }
    return id;
}

QString Catalog::addFolder(const QString &rawPath)
{
    const QString path = cleanPath(rawPath);
    if (!QFileInfo(path).isDir())
        throw ActionError(QStringLiteral("folder.notFound"), {{QStringLiteral("path"), QDir::toNativeSeparators(path)}});
    // A pasta de dados do app (miniaturas, recortes) não é fonte de fotos.
    const QString appData = cleanPath(QFileInfo(AppPaths::dataDir()).canonicalFilePath());
    const QString canonical = cleanPath(QFileInfo(path).canonicalFilePath());
    if (isInside(canonical, appData))
        throw ActionError(QStringLiteral("folder.appData"), {{QStringLiteral("path"), QDir::toNativeSeparators(path)}});
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, path FROM folder"));
    QStringList contained;
    while (q.next()) {
        const QString existing = q.value(1).toString();
        if (existing == path)
            throw ActionError(QStringLiteral("folder.alreadyAdded"), {{QStringLiteral("path"), QDir::toNativeSeparators(path)}});
        if (isInside(path, existing))
            throw ActionError(QStringLiteral("folder.insideOther"),
                              {{QStringLiteral("path"), QDir::toNativeSeparators(path)},
                               {QStringLiteral("parent"), QDir::toNativeSeparators(existing)}});
        if (isInside(existing, path))
            contained << q.value(0).toString();
    }
    const DiskInfo disk = identifyDisk(path);
    Transaction tx(m_db);
    const QString diskId = ensureDisk(disk);
    const QString id = Database::newId();
    m_db.exec(QStringLiteral("INSERT INTO folder(id, disk_id, path, rel_path, added_at) VALUES(?, ?, ?, ?, ?)"),
              {id, diskId, path, relativeToMount(path, disk.mountPoint), Database::timestamp()});
    // Pastas que ficam dentro da nova: as fotos passam para ela.
    for (const QString &child : contained) {
        m_db.exec(QStringLiteral("UPDATE photo SET folder_id = ? WHERE folder_id = ?"), {id, child});
        m_db.exec(QStringLiteral("DELETE FROM folder WHERE id = ?"), {child});
    }
    tx.commit();
    qCInfo(lcCatalog) << "Folder added:" << path << "on disk" << disk.key;
    emit treeChanged(QStringLiteral("folders"));
    return id;
}

void Catalog::removePhotoFiles(const QString &photoId)
{
    const QString thumb = m_db.scalar(QStringLiteral("SELECT thumb_path FROM photo WHERE id = ?"), {photoId}).toString();
    if (!thumb.isEmpty())
        QFile::remove(thumb);
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, crop_path FROM face WHERE photo_id = ?"), {photoId});
    while (q.next()) {
        if (!q.value(1).toString().isEmpty())
            QFile::remove(q.value(1).toString());
        m_faces.remove(q.value(0).toString());
    }
    m_hashes.remove(photoId);
}

void Catalog::removeFolder(const QString &folderId)
{
    QStringList photoIds;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM photo WHERE folder_id = ?"), {folderId});
    while (q.next())
        photoIds << q.value(0).toString();
    QSet<QString> touched;
    QSqlQuery p = m_db.exec(QStringLiteral("SELECT DISTINCT f.person_id FROM face f JOIN photo ph ON ph.id = f.photo_id "
                                           "WHERE ph.folder_id = ? AND f.person_id IS NOT NULL"),
                            {folderId});
    while (p.next())
        touched.insert(p.value(0).toString());
    for (const QString &id : photoIds)
        removePhotoFiles(id);
    Transaction tx(m_db);
    m_db.exec(QStringLiteral("DELETE FROM folder WHERE id = ?"), {folderId});
    // Disco sem pasta nenhuma sai também.
    m_db.exec(QStringLiteral("DELETE FROM disk WHERE id NOT IN (SELECT disk_id FROM folder)"));
    tx.commit();
    for (const QString &person : touched)
        refreshPerson(person);
    m_embeddingsLoaded = false;
    qCInfo(lcCatalog) << "Folder removed:" << folderId << "-" << photoIds.size() << "photos left the catalog";
    emit photosChanged();
    emit treeChanged(QStringLiteral("folders"));
    emit peopleChanged();
}

QString Catalog::folderPath(const QString &folderId) const
{
    return m_db.scalar(QStringLiteral("SELECT path FROM folder WHERE id = ?"), {folderId}).toString();
}

QStringList Catalog::folderIds() const
{
    QStringList out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT f.id FROM folder f JOIN disk d ON d.id = f.disk_id WHERE d.online = 1"));
    while (q.next())
        out << q.value(0).toString();
    return out;
}

QVariantList Catalog::folders() const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT f.id, f.path, f.disk_id, f.scanned_at, d.online, "
        "  (SELECT count(*) FROM photo p WHERE p.folder_id = f.id AND p.state != 'missing' AND p.private = 0), "
        "  (SELECT count(*) FROM photo p WHERE p.folder_id = f.id AND p.state = 'indexed' AND p.private = 0), "
        "  (SELECT count(*) FROM photo p WHERE p.folder_id = f.id AND p.state = 'error' AND p.private = 0) "
        "FROM folder f JOIN disk d ON d.id = f.disk_id ORDER BY f.path"));
    while (q.next())
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},
                           {QStringLiteral("path"), QDir::toNativeSeparators(q.value(1).toString())},
                           {QStringLiteral("diskId"), q.value(2)},
                           {QStringLiteral("scannedAt"), Database::parseTimestamp(q.value(3)).toLocalTime()},
                           {QStringLiteral("online"), q.value(4).toBool()},
                           {QStringLiteral("photoCount"), q.value(5)},
                           {QStringLiteral("indexed"), q.value(6)},
                           {QStringLiteral("errors"), q.value(7)}};
    return out;
}

QVariantList Catalog::disks() const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT d.id, d.disk_key, d.label, d.kind, d.protocol, d.server, d.share, d.fs_type, d.device, d.serial, d.model, "
        "  d.mount_point, d.total_bytes, d.online, "
        "  (SELECT count(*) FROM folder f WHERE f.disk_id = d.id), "
        "  (SELECT count(*) FROM photo p JOIN folder f ON f.id = p.folder_id WHERE f.disk_id = d.id AND p.state != 'missing' AND p.private = 0) "
        "FROM disk d ORDER BY d.kind, d.label"));
    while (q.next())
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},           {QStringLiteral("key"), q.value(1)},
                           {QStringLiteral("label"), q.value(2)},        {QStringLiteral("kind"), q.value(3)},
                           {QStringLiteral("protocol"), q.value(4)},     {QStringLiteral("server"), q.value(5)},
                           {QStringLiteral("share"), q.value(6)},        {QStringLiteral("fsType"), q.value(7)},
                           {QStringLiteral("device"), q.value(8)},       {QStringLiteral("serial"), q.value(9)},
                           {QStringLiteral("model"), q.value(10)},
                           {QStringLiteral("mountPoint"), QDir::toNativeSeparators(q.value(11).toString())},
                           {QStringLiteral("totalBytes"), q.value(12)},  {QStringLiteral("online"), q.value(13).toBool()},
                           {QStringLiteral("folderCount"), q.value(14)}, {QStringLiteral("photoCount"), q.value(15)}};
    return out;
}

int Catalog::refreshDisks()
{
    QHash<QString, DiskInfo> mounted;
    for (const DiskInfo &d : mountedDisks())
        mounted.insert(d.key, d);
    int changed = 0;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, disk_key, mount_point, online FROM disk"));
    struct Row
    {
        QString id, key, mount;
        bool online;
    };
    QList<Row> rows;
    while (q.next())
        rows.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toBool()});
    for (const Row &row : rows) {
        const auto it = mounted.constFind(row.key);
        if (it == mounted.constEnd()) {
            // Ponto de montagem ainda lá (o disco não foi reconhecido pela
            // chave, mas a pasta existe): continua online.
            bool reachable = false;
            QSqlQuery f = m_db.exec(QStringLiteral("SELECT path FROM folder WHERE disk_id = ?"), {row.id});
            while (f.next())
                reachable = reachable || QFileInfo(f.value(0).toString()).isDir();
            if (row.online != reachable) {
                m_db.exec(QStringLiteral("UPDATE disk SET online = ? WHERE id = ?"), {reachable ? 1 : 0, row.id});
                ++changed;
            }
            continue;
        }
        const QString newMount = it->mountPoint;
        if (newMount != row.mount && !row.mount.isEmpty()) {
            // O disco voltou em outro lugar (outra letra, outro /Volumes/X):
            // refaz os caminhos pelo caminho relativo ao ponto de montagem.
            qCInfo(lcCatalog) << "Disk" << row.key << "moved from" << row.mount << "to" << newMount;
            Transaction tx(m_db);
            QSqlQuery f = m_db.exec(QStringLiteral("SELECT id, path, rel_path FROM folder WHERE disk_id = ?"), {row.id});
            QList<std::tuple<QString, QString, QString>> folders;
            while (f.next())
                folders.append({f.value(0).toString(), f.value(1).toString(), f.value(2).toString()});
            for (const auto &[fid, oldPath, rel] : folders) {
                const QString newPath = QDir::cleanPath(newMount + rel);
                m_db.exec(QStringLiteral("UPDATE folder SET path = ? WHERE id = ?"), {newPath, fid});
                m_db.exec(QStringLiteral("UPDATE photo SET path = ? || substr(path, length(?) + 1), "
                                         "dir = ? || substr(dir, length(?) + 1) WHERE folder_id = ?"),
                          {newPath, oldPath, newPath, oldPath, fid});
            }
            tx.commit();
        }
        m_db.exec(QStringLiteral("UPDATE disk SET mount_point = ?, online = 1, last_seen_at = ? WHERE id = ?"),
                  {newMount, Database::timestamp(), row.id});
        if (!row.online || newMount != row.mount)
            ++changed;
    }
    if (changed) {
        emit treeChanged(QStringLiteral("folders"));
        emit photosChanged();
    }
    return changed;
}

// --------------------------------------------------------------------------- //
// Searcher
// --------------------------------------------------------------------------- //

int Catalog::ingest(const QString &folderId, const QList<FoundFile> &files)
{
    int queued = 0;
    const QString now = Database::timestamp();
    Transaction tx(m_db);
    for (const FoundFile &f : files) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, size, file_mtime, state, indexed_at FROM photo WHERE path = ?"),
                                {f.path});
        const QFileInfo fi(f.path);
        const QString dir = fi.absolutePath();
        if (!q.next()) {
            // Sumiu entre a varredura e agora (ex.: o original que a etapa
            // "privacy" acabou de cifrar): não entra.
            if (!fi.exists())
                continue;
            if (!f.keyId.isEmpty()) {
                // Um .photoidxe que o catálogo não conhece (outro catálogo,
                // banco recriado): entra como privada; o Indexer o abre com o
                // cofre aberto.
                const QString id = Database::newId();
                m_db.exec(QStringLiteral("INSERT INTO photo(id, folder_id, path, dir, name, ext, format, is_raw, size, file_mtime, "
                                         "file_created_at, file_modified_at, state, added_at, private, private_state, key_id) "
                                         "VALUES(?, ?, ?, ?, ?, '', 'PHOTOIDXE', 0, ?, ?, ?, ?, 'pending', ?, 1, 'done', ?)"),
                          {id, folderId, f.path, dir, fi.fileName(), f.size, f.mtimeMs, localWallClock(f.created),
                           localWallClock(f.modified), now, f.keyId});
                m_db.exec(QStringLiteral("INSERT OR IGNORE INTO album_photo(album_id, photo_id, added_at) VALUES(?, ?, ?)"),
                          {privateAlbumId(), id, now});
                ++queued;
                continue;
            }
            const QString ext = imaging::extensionOf(f.path);
            m_db.exec(QStringLiteral("INSERT INTO photo(id, folder_id, path, dir, name, ext, format, is_raw, size, file_mtime, "
                                     "file_created_at, file_modified_at, state, added_at) "
                                     "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending', ?)"),
                      {Database::newId(), folderId, f.path, dir, fi.fileName(), ext, imaging::formatLabel(ext),
                       imaging::isRawFile(f.path) ? 1 : 0, f.size, f.mtimeMs, localWallClock(f.created),
                       localWallClock(f.modified), now});
            ++queued;
            continue;
        }
        const QString id = q.value(0).toString();
        const bool changed = q.value(1).toLongLong() != f.size || q.value(2).toLongLong() != f.mtimeMs;
        const QString state = q.value(3).toString();
        if (changed) {
            m_db.exec(QStringLiteral("UPDATE photo SET folder_id = ?, size = ?, file_mtime = ?, file_created_at = ?, "
                                     "file_modified_at = ?, state = 'pending', error = '' WHERE id = ?"),
                      {folderId, f.size, f.mtimeMs, localWallClock(f.created), localWallClock(f.modified), id});
            ++queued;
        } else if (state == QLatin1String("missing")) {
            // Voltou como estava: o que já foi catalogado continua valendo.
            const bool indexed = !q.value(4).isNull();
            m_db.exec(QStringLiteral("UPDATE photo SET folder_id = ?, state = ? WHERE id = ?"),
                      {folderId, indexed ? QStringLiteral("indexed") : QStringLiteral("pending"), id});
            if (!indexed)
                ++queued;
        }
    }
    tx.commit();
    if (queued)
        emit photosChanged();
    return queued;
}

int Catalog::finishScan(const QString &folderId, const QSet<QString> &seenPaths)
{
    QStringList missing, dropped;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, path, indexed_at, private, private_state FROM photo "
                                           "WHERE folder_id = ? AND state != 'missing'"),
                            {folderId});
    while (q.next()) {
        if (seenPaths.contains(q.value(1).toString()))
            continue;
        // Cifrada/decifrada pela etapa "privacy" durante a varredura: mudou de
        // nome, mas está lá.
        if ((q.value(3).toBool() || !q.value(4).toString().isEmpty()) && QFileInfo::exists(q.value(1).toString()))
            continue;
        // Nunca indexado (ainda na fila): não há o que guardar — sai do
        // catálogo. Já indexado: fica como "sumido" (miniatura, categorias,
        // pessoas continuam).
        (q.value(2).isNull() ? dropped : missing) << q.value(0).toString();
    }
    Transaction tx(m_db);
    for (const QString &id : dropped)
        m_db.exec(QStringLiteral("DELETE FROM photo WHERE id = ?"), {id});
    for (const QString &id : missing)
        m_db.exec(QStringLiteral("UPDATE photo SET state = 'missing' WHERE id = ?"), {id});
    m_db.exec(QStringLiteral("UPDATE folder SET scanned_at = ? WHERE id = ?"), {Database::timestamp(), folderId});
    tx.commit();
    if (!missing.isEmpty() || !dropped.isEmpty())
        emit photosChanged();
    emit treeChanged(QStringLiteral("folders"));
    return int(missing.size());
}

// --------------------------------------------------------------------------- //
// Filas
// --------------------------------------------------------------------------- //

QList<IndexJob> Catalog::nextIndexJobs(int limit, const QSet<QString> &busy) const
{
    QList<IndexJob> out;
    QSqlQuery q = m_db.exec(QStringLiteral(
                                "SELECT p.id, p.path, p.thumb_path, p.file_created_at, p.file_modified_at, p.private FROM photo p "
                                "WHERE p.state = 'pending' AND (p.private = 0 OR ?) AND +p.folder_id IN %1 "
                                "ORDER BY p.added_at LIMIT ?").arg(QLatin1String(kOnlineFolders)),
                            {privateUnlocked() ? 1 : 0, limit + int(busy.size())});
    while (q.next() && out.size() < limit) {
        const QString id = q.value(0).toString();
        if (busy.contains(id))
            continue;
        IndexJob job;
        job.photoId = id;
        job.path = q.value(1).toString();
        job.previousThumb = q.value(2).toString();
        job.fileCreated = QDateTime::fromString(q.value(3).toString(), Qt::ISODateWithMs);
        job.fileModified = QDateTime::fromString(q.value(4).toString(), Qt::ISODateWithMs);
        job.isPrivate = q.value(5).toBool();
        if (job.isPrivate)
            job.keys = keySnapshot();
        out.append(job);
    }
    return out;
}

QList<CategorizeJob> Catalog::nextCategorizeJobs(int limit, const QSet<QString> &busy) const
{
    QList<CategorizeJob> out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT p.id, p.thumb_path, e.vector FROM photo p "
                                           "LEFT JOIN photo_embedding e ON e.photo_id = p.id "
                                           "WHERE p.state = 'indexed' AND p.ai_state = 'pending' AND p.private = 0 "
                                           "AND p.thumb_path != '' ORDER BY p.indexed_at LIMIT ?"),
                            {limit + int(busy.size())});
    while (q.next() && out.size() < limit) {
        const QString id = q.value(0).toString();
        if (!busy.contains(id))
            out.append({id, q.value(1).toString(), imaging::fromBlob(q.value(2).toByteArray())});
    }
    return out;
}

QList<FaceJob> Catalog::nextFaceJobs(int limit, const QSet<QString> &busy) const
{
    QList<FaceJob> out;
    QSqlQuery q = m_db.exec(QStringLiteral(
                                "SELECT p.id, p.path, p.year, p.month, p.day FROM photo p "
                                "WHERE p.state = 'indexed' AND p.faces_state = 'pending' AND p.private = 0 AND p.thumb_path != '' "
                                "AND +p.folder_id IN %1 ORDER BY p.indexed_at LIMIT ?").arg(QLatin1String(kOnlineFolders)),
                            {limit + int(busy.size())});
    while (q.next() && out.size() < limit) {
        const QString id = q.value(0).toString();
        if (busy.contains(id))
            continue;
        FaceJob job;
        job.photoId = id;
        job.path = q.value(1).toString();
        if (!q.value(2).isNull())
            job.date = QDate(q.value(2).toInt(), std::max(1, q.value(3).toInt()), std::max(1, q.value(4).toInt()));
        out.append(job);
    }
    return out;
}

int Catalog::pendingCount(const QString &stage) const
{
    if (stage == QLatin1String("index"))
        return m_db.scalar(QStringLiteral("SELECT count(*) FROM photo p WHERE p.state = 'pending' AND (p.private = 0 OR ?) "
                                          "AND +p.folder_id IN %1").arg(QLatin1String(kOnlineFolders)),
                           {privateUnlocked() ? 1 : 0})
            .toInt();
    if (stage == QLatin1String("categorize"))
        return m_db.scalar(QStringLiteral("SELECT count(*) FROM photo WHERE state = 'indexed' AND ai_state = 'pending' "
                                          "AND private = 0 AND thumb_path != ''"))
            .toInt();
    if (stage == QLatin1String("privacy"))
        return pendingPrivacy();
    if (stage == QLatin1String("faces"))
        return m_db.scalar(QStringLiteral("SELECT count(*) FROM photo p WHERE p.state = 'indexed' AND p.faces_state = 'pending' "
                                          "AND p.private = 0 AND p.thumb_path != '' AND +p.folder_id IN %1")
                               .arg(QLatin1String(kOnlineFolders)))
            .toInt();
    return 0;
}

int Catalog::requeue(const QString &stage, const QString &scope)
{
    const bool errorsOnly = scope == QLatin1String("errors");
    QString sql;
    if (stage == QLatin1String("index"))
        sql = errorsOnly ? QStringLiteral("UPDATE photo SET state = 'pending', error = '' WHERE state = 'error'")
                         : QStringLiteral("UPDATE photo SET state = 'pending', error = '' WHERE state IN ('indexed', 'error')");
    else if (stage == QLatin1String("categorize"))
        sql = errorsOnly ? QStringLiteral("UPDATE photo SET ai_state = 'pending' WHERE ai_state = 'error'")
                         : QStringLiteral("UPDATE photo SET ai_state = 'pending' WHERE ai_state != 'pending' AND private = 0");
    else if (stage == QLatin1String("faces"))
        sql = errorsOnly ? QStringLiteral("UPDATE photo SET faces_state = 'pending' WHERE faces_state = 'error'")
                         : QStringLiteral("UPDATE photo SET faces_state = 'pending' WHERE faces_state != 'pending' AND private = 0");
    else if (stage == QLatin1String("privacy"))
        // A operação que falhou volta para a fila ("error:encrypt" -> "encrypt").
        sql = QStringLiteral("UPDATE photo SET private_state = substr(private_state, 7), error = '' "
                             "WHERE private_state LIKE 'error:%'");
    if (sql.isEmpty())
        return 0;
    const int n = m_db.exec(sql).numRowsAffected();
    emit photosChanged();
    return n;
}

// --------------------------------------------------------------------------- //
// Indexer -> catálogo
// --------------------------------------------------------------------------- //

QString Catalog::ensurePlace(const PlaceMatch &place)
{
    if (!place.found)
        return {};
    QString id = m_db.scalar(QStringLiteral("SELECT id FROM place WHERE country_code = ? AND state = ? AND city = ?"),
                             {place.countryCode, place.state, place.city})
                     .toString();
    if (id.isEmpty()) {
        id = Database::newId();
        m_db.exec(QStringLiteral("INSERT INTO place(id, country_code, country, state, city) VALUES(?, ?, ?, ?, ?)"),
                  {id, place.countryCode, place.country, place.state, place.city});
    }
    return id;
}

void Catalog::applyIndex(const IndexResult &r)
{
    QSqlQuery p = m_db.exec(QStringLiteral("SELECT picture_hash, thumb_path, orientation, indexed_at FROM photo WHERE id = ?"),
                            {r.photoId});
    if (!p.next())
        return;  // saiu do catálogo enquanto o worker trabalhava
    const QString oldHash = p.value(0).toString();
    const QString oldThumb = p.value(1).toString();
    // Mesmos pixels, outra posição (orientação do fabricante): a miniatura
    // girou, então a IA e as faces (caixas na imagem orientada) refazem.
    const bool turned = !p.value(3).isNull() && p.value(2).toInt() != r.shot.orientation;

    Transaction tx(m_db);
    if (!r.ok) {
        m_db.exec(QStringLiteral("UPDATE photo SET state = 'error', error = ?, indexed_at = ? WHERE id = ?"),
                  {r.error.left(500), Database::timestamp(), r.photoId});
        tx.commit();
        emit photosChanged();
        return;
    }
    const imaging::ShotInfo &s = r.shot;
    const QString placeId = s.hasGps ? ensurePlace(r.place) : QString();
    const bool hasDate = r.takenAt.isValid();
    // Imagem nova (outros pixels): a IA e as faces refazem.
    const bool pixelsChanged = oldHash != r.pictureHash || oldHash.isEmpty() || turned;
    m_db.exec(QStringLiteral(
                  "UPDATE photo SET state = 'indexed', error = ?, format = ?, is_raw = ?, width = ?, height = ?, "
                  "orientation = ?, taken_at = ?, taken_source = ?, taken_offset = ?, year = ?, month = ?, day = ?, "
                  "modified_at = ?, make = ?, model = ?, camera = ?, lens = ?, software = ?, artist = ?, copyright = ?, "
                  "description = ?, iso = ?, aperture = ?, shutter = ?, focal_length = ?, focal_length_35 = ?, "
                  "exposure_bias = ?, flash = ?, latitude = ?, longitude = ?, altitude = ?, place_id = ?, "
                  "thumb_path = ?, thumb_width = ?, thumb_height = ?, picture_hash = ?, phash = ?, sharpness = ?, "
                  "exif_count = ?, indexed_at = ?, "
                  "ai_state = CASE WHEN ? AND private = 0 THEN 'pending' ELSE ai_state END, "
                  "faces_state = CASE WHEN ? AND private = 0 THEN 'pending' ELSE faces_state END "
                  "WHERE id = ?"),
              QVariantList{r.error.left(500), r.format, r.isRaw ? 1 : 0, r.width, r.height, s.orientation,
                           hasDate ? wallClock(r.takenAt) : QVariant(), r.takenSource, s.takenOffset,
                           hasDate ? QVariant(r.takenAt.date().year()) : QVariant(),
                           hasDate ? QVariant(r.takenAt.date().month()) : QVariant(),
                           hasDate ? QVariant(r.takenAt.date().day()) : QVariant(),
                           s.modifiedLocal.isValid() ? wallClock(s.modifiedLocal) : QVariant(), s.make, s.model,
                           imaging::cameraName(s.make, s.model), s.lens, s.software, s.artist, s.copyright, s.description,
                           s.iso > 0 ? QVariant(s.iso) : QVariant(), nullable(s.fNumber, s.fNumber > 0),
                           nullable(s.exposureTime, s.exposureTime > 0), nullable(s.focalLength, s.focalLength > 0),
                           s.focalLength35 > 0 ? QVariant(s.focalLength35) : QVariant(),
                           nullable(s.exposureBias, s.exposureBias != 0.0), s.flash >= 0 ? QVariant(s.flash) : QVariant(),
                           nullable(s.latitude, s.hasGps), nullable(s.longitude, s.hasGps), nullable(s.altitude, s.hasGps),
                           placeId.isEmpty() ? QVariant() : QVariant(placeId), r.thumbPath, r.thumbWidth, r.thumbHeight,
                           r.pictureHash, r.perceptualHash, nullable(r.sharpness, r.sharpness >= 0),
                           int(r.tags.size()), Database::timestamp(), pixelsChanged ? 1 : 0,
                           pixelsChanged ? 1 : 0, r.photoId});

    if (!r.originalName.isEmpty()) {
        // Um .photoidxe que chegou sem catálogo: o nome e o formato reais
        // vêm dos metadados cifrados.
        const QString ext = imaging::extensionOf(r.originalName);
        m_db.exec(QStringLiteral("UPDATE photo SET name = ?, original_name = ?, ext = ?, format = ?, is_raw = ? WHERE id = ?"),
                  {r.originalName, r.originalName, ext, imaging::formatLabel(ext), imaging::isRawFile(r.originalName) ? 1 : 0,
                   r.photoId});
    }

    // Todas as tags EXIF (uma query preparada, reaproveitada).
    m_db.exec(QStringLiteral("DELETE FROM exif_tag WHERE photo_id = ?"), {r.photoId});
    QSqlQuery insert(m_db.handle());
    insert.prepare(QStringLiteral("INSERT OR IGNORE INTO exif_tag(photo_id, ifd, tag, name, type, value) VALUES(?, ?, ?, ?, ?, ?)"));
    for (const imaging::ExifTag &t : r.tags) {
        insert.addBindValue(r.photoId);
        insert.addBindValue(t.ifd);
        insert.addBindValue(int(t.tag));
        insert.addBindValue(imaging::exifTagName(t.ifd, t.tag));
        insert.addBindValue(int(t.type));
        insert.addBindValue(imaging::exifValueText(t).left(2000));
        if (!insert.exec())
            qCWarning(lcCatalog) << "EXIF tag not stored:" << t.ifd << t.tag;
    }
    tx.commit();

    if (!oldThumb.isEmpty() && oldThumb != r.thumbPath)
        QFile::remove(oldThumb);
    bool ok = false;
    const quint64 phash = imaging::hashFromHex(r.perceptualHash, &ok);
    if (ok && !r.perceptualHash.isEmpty())
        m_hashes.insert(r.photoId, phash);
    markDuplicates(r.photoId, r.pictureHash, ok ? phash : 0);
    emit photosChanged();
}

void Catalog::applyIndexBatch(const QList<IndexResult> &results)
{
    Transaction tx(m_db);
    for (const IndexResult &r : results) {
        try {
            applyIndex(r);
        } catch (const std::exception &e) {
            qCWarning(lcCatalog) << "Index result not stored:" << r.photoId << e.what();
        }
    }
    tx.commit();
}

void Catalog::markDuplicates(const QString &photoId, const QString &pictureHash, quint64 phash)
{
    const QString dup = builtinId(builtin::kDuplicate);
    if (dup.isEmpty())
        return;
    // Privada não entra na conta: nem marca nem é marcada (não revela que existe).
    if (m_db.scalar(QStringLiteral("SELECT private FROM photo WHERE id = ?"), {photoId}).toInt() == 1)
        return;
    m_db.exec(QStringLiteral("DELETE FROM photo_category WHERE photo_id = ? AND category_id = ? AND source = 'rule'"),
              {photoId, dup});
    QSet<QString> twins;
    // Os mesmos pixels (picture_hash)...
    if (!pictureHash.isEmpty()) {
        QSqlQuery q = m_db.exec(QStringLiteral("SELECT id FROM photo WHERE picture_hash = ? AND id != ? AND state != 'missing' "
                                               "AND private = 0"),
                                {pictureHash, photoId});
        while (q.next())
            twins.insert(q.value(0).toString());
    }
    // ... ou a mesma imagem em outro tamanho/compressão: hash perceptual igual
    // e a mesma proporção.
    if (phash != 0) {
        QSqlQuery me = m_db.exec(QStringLiteral("SELECT width, height FROM photo WHERE id = ?"), {photoId});
        if (me.next() && me.value(1).toInt() > 0) {
            const double aspect = me.value(0).toDouble() / me.value(1).toDouble();
            for (const HashTree::Match &m : m_hashes.query(phash, 0)) {
                if (m.id == photoId || twins.contains(m.id))
                    continue;
                QSqlQuery o = m_db.exec(QStringLiteral("SELECT width, height FROM photo WHERE id = ? AND state != 'missing' "
                                                       "AND private = 0"),
                                        {m.id});
                if (o.next() && o.value(1).toInt() > 0
                    && std::abs(o.value(0).toDouble() / o.value(1).toDouble() - aspect) < 0.01 * aspect)
                    twins.insert(m.id);
            }
        }
    }
    if (twins.isEmpty())
        return;
    twins.insert(photoId);
    for (const QString &id : twins)
        m_db.exec(QStringLiteral("INSERT OR IGNORE INTO photo_category(photo_id, category_id, score, source) VALUES(?, ?, 1, 'rule')"),
                  {id, dup});
}

} // namespace pix

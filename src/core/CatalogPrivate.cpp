// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include "core/Crypto.h"
#include "core/Vault.h"

#include <QCoreApplication>
#include <QFile>
#include <QLoggingCategory>
#include <QSqlQuery>

Q_DECLARE_LOGGING_CATEGORY(lcCatalog)

// Fotos privadas: o chaveiro da sessão, o álbum "Private" e a fila da etapa
// "privacy" (cifrar, decifrar, recifrar). Visibilidade: uma foto privada some
// de TODAS as árvores, da grade, da busca e das pessoas; só o álbum privado a
// mostra, e só com o cofre aberto (CatalogQuery.cpp, nodeFilter/tree).

namespace pix {

namespace {

QString hexId(const QByteArray &id)
{
    return QString::fromLatin1(id.toHex());
}

} // namespace

// --------------------------------------------------------------------------- //
// Chaveiro
// --------------------------------------------------------------------------- //

void Catalog::ensureVault()
{
    // A chave nasce com o catálogo ("na instalação"): em claro até a pessoa
    // definir a senha — antes disso nada foi cifrado com ela.
    if (!setting(QStringLiteral("vault/plain")).toString().isEmpty()
        || !setting(QStringLiteral("vault/wrapped")).toString().isEmpty())
        return;
    const vault::Ring ring = vault::newRing();
    setSetting(QStringLiteral("vault/plain"), QString::fromLatin1(vault::serialize(ring).toBase64()));
    setSetting(QStringLiteral("vault/currentId"), hexId(ring.currentId));
    qCInfo(lcCatalog) << "Encryption key generated (id" << hexId(ring.currentId) << ")";
}

void Catalog::ensurePrivateAlbum()
{
    if (!privateAlbumId().isEmpty())
        return;
    m_db.exec(QStringLiteral("INSERT INTO album(id, parent_id, name, position, created_at, private) VALUES(?, NULL, ?, 0, ?, 1)"),
              {Database::newId(), QStringLiteral("Private"), Database::timestamp()});
}

QString Catalog::privateAlbumId() const
{
    return m_db.scalar(QStringLiteral("SELECT id FROM album WHERE private = 1 AND parent_id IS NULL")).toString();
}

bool Catalog::isPrivateAlbum(const QString &albumId) const
{
    return m_db.scalar(QStringLiteral("SELECT private FROM album WHERE id = ?"), {albumId}).toInt() == 1;
}

vault::Record Catalog::vaultRecord() const
{
    vault::Record r;
    r.plain = QByteArray::fromBase64(setting(QStringLiteral("vault/plain")).toString().toLatin1());
    r.wrapped = QByteArray::fromBase64(setting(QStringLiteral("vault/wrapped")).toString().toLatin1());
    r.salt = QByteArray::fromBase64(setting(QStringLiteral("vault/salt")).toString().toLatin1());
    r.iterations = setting(QStringLiteral("vault/iterations"), crypto::kDefaultIterations).toInt();
    r.currentId = QByteArray::fromHex(setting(QStringLiteral("vault/currentId")).toString().toLatin1());
    return r;
}

void Catalog::storeVault(const vault::Record &record)
{
    Transaction tx(m_db);
    setSetting(QStringLiteral("vault/wrapped"), QString::fromLatin1(record.wrapped.toBase64()));
    setSetting(QStringLiteral("vault/salt"), QString::fromLatin1(record.salt.toBase64()));
    setSetting(QStringLiteral("vault/iterations"), record.iterations);
    setSetting(QStringLiteral("vault/currentId"), hexId(record.currentId));
    // Com senha, o chaveiro em claro deixa de existir.
    m_db.exec(QStringLiteral("DELETE FROM setting WHERE key = 'vault/plain'"));
    tx.commit();
}

bool Catalog::hasVaultPassword() const
{
    return !setting(QStringLiteral("vault/wrapped")).toString().isEmpty();
}

bool Catalog::privateUnlocked() const
{
    return m_keys->isOpen();
}

KeySnapshot Catalog::keySnapshot() const
{
    return {m_keys->keys(), m_keys->currentId()};
}

void Catalog::openVault(const vault::Ring &ring, const QByteArray &passwordKey)
{
    m_keys->set(ring.keys, ring.currentId);
    m_passwordKey = passwordKey;
    qCInfo(lcCatalog) << "Private album unlocked";
    emit privacyChanged();
    emit treeChanged(QStringLiteral("albums"));
    emit photosChanged();
}

void Catalog::lockVault()
{
    if (!m_keys->isOpen() && m_passwordKey.isEmpty())
        return;
    m_keys->clear();
    m_passwordKey.fill('\0');
    m_passwordKey.clear();
    qCInfo(lcCatalog) << "Private album locked";
    emit privacyChanged();
    emit treeChanged(QStringLiteral("albums"));
    emit photosChanged();
}

std::optional<vault::Ring> Catalog::currentRing() const
{
    const vault::Record record = vaultRecord();
    if (!record.hasPassword())
        return vault::parse(record.plain);
    if (!m_keys->isOpen())
        return std::nullopt;
    return vault::Ring{m_keys->keys(), m_keys->currentId()};
}

QByteArray Catalog::rotateKey()
{
    if (!m_keys->isOpen() || m_passwordKey.isEmpty())
        throw ActionError(QStringLiteral("vault.locked"));
    vault::Ring ring{m_keys->keys(), m_keys->currentId()};
    const vault::Ring fresh = vault::newRing();
    ring.keys.insert(fresh.currentId, fresh.current());
    ring.currentId = fresh.currentId;
    storeVault(vault::rewrap(vaultRecord(), ring, m_passwordKey));
    m_keys->set(ring.keys, ring.currentId);
    // A etapa "privacy" recifra as de key_id antigo (nextPrivacyJobs).
    qCInfo(lcCatalog) << "Encryption key rotated to" << hexId(ring.currentId);
    emit privacyChanged();
    emit photosChanged();
    return fresh.current();
}

void Catalog::pruneKeys()
{
    if (!m_keys->isOpen() || m_passwordKey.isEmpty())
        return;
    vault::Ring ring{m_keys->keys(), m_keys->currentId()};
    if (ring.keys.size() <= 1)
        return;
    QSet<QByteArray> used{ring.currentId};
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT DISTINCT key_id FROM photo WHERE private = 1 AND key_id != ''"));
    while (q.next())
        used.insert(QByteArray::fromHex(q.value(0).toString().toLatin1()));
    bool changed = false;
    for (auto it = ring.keys.begin(); it != ring.keys.end();) {
        if (used.contains(it.key())) {
            ++it;
        } else {
            it = ring.keys.erase(it);
            changed = true;
        }
    }
    if (!changed)
        return;
    storeVault(vault::rewrap(vaultRecord(), ring, m_passwordKey));
    m_keys->set(ring.keys, ring.currentId);
    qCInfo(lcCatalog) << "Old encryption keys dropped; ring has" << ring.keys.size() << "key(s)";
}

// --------------------------------------------------------------------------- //
// Tornar privada / pública
// --------------------------------------------------------------------------- //

int Catalog::makePrivate(const QStringList &photoIds)
{
    if (!hasVaultPassword())
        throw ActionError(QStringLiteral("vault.noPassword"));
    if (!m_keys->isOpen())
        throw ActionError(QStringLiteral("vault.locked"));
    const QString album = privateAlbumId();
    QSet<QString> touched;
    int count = 0;
    Transaction tx(m_db);
    for (const QString &id : photoIds) {
        if (m_db.exec(QStringLiteral("UPDATE photo SET private = 1, private_state = 'encrypt' WHERE id = ? AND private = 0"), {id})
                .numRowsAffected()
            == 0)
            continue;
        ++count;
        // As faces saem das pessoas (e da busca por rosto); voltam a ser
        // detectadas se a foto voltar a ser pública.
        QSqlQuery f = m_db.exec(QStringLiteral("SELECT id, crop_path, person_id FROM face WHERE photo_id = ?"), {id});
        while (f.next()) {
            if (!f.value(1).toString().isEmpty())
                QFile::remove(f.value(1).toString());
            m_faces.remove(f.value(0).toString());
            if (!f.value(2).toString().isEmpty())
                touched.insert(f.value(2).toString());
        }
        m_db.exec(QStringLiteral("DELETE FROM face WHERE photo_id = ?"), {id});
        m_db.exec(QStringLiteral("UPDATE photo SET face_count = 0, faces_state = 'private' WHERE id = ?"), {id});
        m_db.exec(QStringLiteral("INSERT OR IGNORE INTO album_photo(album_id, photo_id, added_at) VALUES(?, ?, ?)"),
                  {album, id, Database::timestamp()});
    }
    tx.commit();
    for (const QString &person : touched)
        refreshPerson(person);
    m_embeddingsLoaded = false;
    if (count) {
        qCInfo(lcCatalog) << count << "photo(s) made private";
        emit photosChanged();
        emit treeChanged(QStringLiteral("albums"));
        emit peopleChanged();
        emit privacyChanged();
    }
    return count;
}

int Catalog::makePublic(const QStringList &photoIds)
{
    if (!m_keys->isOpen())
        throw ActionError(QStringLiteral("vault.locked"));
    int count = 0;
    Transaction tx(m_db);
    for (const QString &id : photoIds)
        count += m_db.exec(QStringLiteral("UPDATE photo SET private_state = 'decrypt' WHERE id = ? AND private = 1"), {id})
                     .numRowsAffected();
    tx.commit();
    if (count) {
        emit photosChanged();
        emit privacyChanged();
    }
    return count;
}

// --------------------------------------------------------------------------- //
// Etapa "privacy"
// --------------------------------------------------------------------------- //

QList<PrivacyJob> Catalog::nextPrivacyJobs(int limit, const QSet<QString> &busy) const
{
    QList<PrivacyJob> out;
    if (!m_keys->isOpen())
        return out;
    const KeySnapshot keys = keySnapshot();
    QSqlQuery q = m_db.exec(
        QStringLiteral("SELECT p.id, p.path, p.thumb_path, p.private_state, p.file_sha256 FROM photo p "
                       "JOIN folder f ON f.id = p.folder_id JOIN disk d ON d.id = f.disk_id "
                       "WHERE p.private = 1 AND d.online = 1 AND p.state != 'missing' "
                       "AND (p.private_state IN ('encrypt', 'decrypt') OR (p.private_state = 'done' AND p.key_id != '' AND p.key_id != ?)) "
                       "ORDER BY p.added_at LIMIT ?"),
        {hexId(keys.currentId), limit + int(busy.size())});
    while (q.next() && out.size() < limit) {
        const QString id = q.value(0).toString();
        if (busy.contains(id))
            continue;
        PrivacyJob job;
        job.photoId = id;
        job.path = q.value(1).toString();
        job.thumbPath = q.value(2).toString();
        const QString state = q.value(3).toString();
        job.op = state == QLatin1String("done") ? QStringLiteral("rekey") : state;
        job.fileSha256 = q.value(4).toString();
        job.keys = keys;
        out.append(job);
    }
    return out;
}

void Catalog::applyPrivacy(const PrivacyResult &r)
{
    if (!r.ok) {
        m_db.exec(QStringLiteral("UPDATE photo SET private_state = 'error:' || ?, error = ? WHERE id = ?"),
                  {r.op, r.error, r.photoId});
        emit privacyChanged();
        return;
    }
    Transaction tx(m_db);
    if (r.op == QLatin1String("decrypt")) {
        m_db.exec(QStringLiteral("UPDATE photo SET path = ?, name = ?, size = ?, file_mtime = ?, thumb_path = ?, private = 0, "
                                 "private_state = '', key_id = '', encrypted_sha256 = '', original_name = '', "
                                 "faces_state = 'pending', error = '' WHERE id = ?"),
                  {r.path, r.originalName, r.size, r.mtimeMs, r.thumbPath, r.photoId});
        m_db.exec(QStringLiteral("DELETE FROM album_photo WHERE photo_id = ? AND album_id IN (SELECT id FROM album WHERE private = 1)"),
                  {r.photoId});
        m_embeddingsLoaded = false;
    } else {
        // Se a pessoa pediu "pública" enquanto cifrava, o pedido fica.
        m_db.exec(QStringLiteral("UPDATE photo SET path = ?, size = ?, file_mtime = ?, thumb_path = ?, key_id = ?, "
                                 "file_sha256 = ?, encrypted_sha256 = ?, original_name = ?, error = '', "
                                 "private_state = CASE WHEN private_state = 'decrypt' THEN 'decrypt' ELSE 'done' END "
                                 "WHERE id = ?"),
                  {r.path, r.size, r.mtimeMs, r.thumbPath, r.keyId, r.fileSha256, r.encryptedSha256, r.originalName,
                   r.photoId});
    }
    tx.commit();
    if (r.op == QLatin1String("rekey"))
        pruneKeys();
    emit photosChanged();
    emit privacyChanged();
    if (r.op == QLatin1String("decrypt"))
        emit treeChanged(QStringLiteral("albums"));
}

int Catalog::pendingPrivacy() const
{
    if (!m_keys->isOpen())
        return m_db.scalar(QStringLiteral("SELECT count(*) FROM photo WHERE private = 1 AND private_state IN ('encrypt', 'decrypt')")).toInt();
    return m_db
        .scalar(QStringLiteral("SELECT count(*) FROM photo WHERE private = 1 AND (private_state IN ('encrypt', 'decrypt') "
                               "OR (private_state = 'done' AND key_id != '' AND key_id != ?))"),
                {hexId(m_keys->currentId())})
        .toInt();
}

QVariantMap Catalog::privacyStatus() const
{
    QSqlQuery q = m_db.exec(QStringLiteral(
        "SELECT count(*), sum(private_state = 'done'), sum(private_state IN ('encrypt', 'decrypt')), "
        "sum(private_state LIKE 'error:%') FROM photo WHERE private = 1"));
    q.next();
    return {{QStringLiteral("hasPassword"), hasVaultPassword()},
            {QStringLiteral("unlocked"), m_keys->isOpen()},
            {QStringLiteral("photos"), q.value(0).toInt()},
            {QStringLiteral("encrypted"), q.value(1).toInt()},
            {QStringLiteral("pending"), pendingPrivacy()},
            {QStringLiteral("errors"), q.value(3).toInt()},
            {QStringLiteral("keyId"), setting(QStringLiteral("vault/currentId")).toString()}};
}

} // namespace pix

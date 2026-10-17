// SPDX-License-Identifier: BSD-2-Clause
#include "core/Privacy.h"

#include "core/Crypto.h"
#include "imaging/Memory.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcPrivacy, "photoidx.privacy")

namespace pix {

namespace {

QString hex(const QByteArray &bytes)
{
    return QString::fromLatin1(bytes.toHex());
}

QByteArray fileSha256(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return h.result();
}

/// "IMG_1.photoidxe", "IMG_1 (2).photoidxe", ...
QString candidate(const QString &dir, const QString &base, const QString &ext, int n)
{
    const QString name = n <= 1 ? QStringLiteral("%1.%2").arg(base, ext) : QStringLiteral("%1 (%2).%3").arg(base).arg(n).arg(ext);
    return QDir(dir).filePath(name);
}

/// "foto.jpg" -> ("foto", "jpg"); "foto" -> ("foto", "").
std::pair<QString, QString> splitName(const QString &name)
{
    const qsizetype dot = name.lastIndexOf(u'.');
    if (dot <= 0)
        return {name, {}};
    return {name.left(dot), name.mid(dot + 1)};
}

/// Confere o .photoidxe inteiro (tag de cada bloco) e devolve os hashes.
crypto::StreamResult verifyEncrypted(const KeySnapshot &keys, const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        crypto::StreamResult r;
        r.error = QStringLiteral("crypto.io");
        return r;
    }
    return crypto::decryptStream(keys.keys, f, nullptr);
}

/// Grava ``bytes`` em ``path`` de uma vez (temporário + rename).
bool writeAtomic(const QString &path, const QByteArray &bytes)
{
    QSaveFile out(path);
    return out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size() && out.commit();
}

void fillFileInfo(PrivacyResult &r, const QString &path)
{
    const QFileInfo fi(path);
    r.path = QDir::fromNativeSeparators(fi.absoluteFilePath());
    r.size = fi.size();
    r.mtimeMs = fi.lastModified().toMSecsSinceEpoch();
}

/// Miniatura .jpg -> .pidx (cifrada com a chave atual).
QString encryptThumb(const KeySnapshot &keys, const QString &thumb)
{
    if (thumb.isEmpty() || thumb.endsWith(QLatin1String(".pidx")))
        return thumb;
    QFile f(thumb);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QByteArray sealed = crypto::encryptBytes(keys.current(), f.readAll(), QFileInfo(thumb).fileName());
    f.close();
    const QString target = encryptedThumbPath(thumb);
    if (sealed.isEmpty() || !writeAtomic(target, sealed))
        return thumb;
    QFile::remove(thumb);
    return target;
}

/// Miniatura .pidx -> .jpg.
QString decryptThumb(const KeySnapshot &keys, const QString &thumb)
{
    if (!thumb.endsWith(QLatin1String(".pidx")))
        return thumb;
    QFile f(thumb);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto plain = crypto::decryptBytes(keys.keys, f.readAll());
    f.close();
    if (!plain)
        return thumb;
    const QString target = thumb.left(thumb.size() - 5) + QStringLiteral(".jpg");
    if (!writeAtomic(target, *plain))
        return thumb;
    QFile::remove(thumb);
    return target;
}

PrivacyResult encrypt(const PrivacyJob &job)
{
    PrivacyResult r;
    r.photoId = job.photoId;
    r.op = job.op;
    const QFileInfo src(job.path);
    const QString dir = src.absolutePath();
    const QString name = src.fileName();
    const QString base = splitName(name).first;

    // Já é o cifrado (rodada anterior concluída antes de o banco saber).
    if (job.path.endsWith(QStringLiteral(".") + QLatin1String(crypto::kEncryptedExtension))) {
        const crypto::StreamResult v = verifyEncrypted(job.keys, job.path);
        if (!v.ok) {
            r.error = v.error;
            return r;
        }
        fillFileInfo(r, job.path);
        r.originalName = v.name;
        r.fileSha256 = hex(v.plainSha256);
        r.encryptedSha256 = hex(v.cipherSha256);
        r.keyId = hex(v.keyId);
        r.thumbPath = encryptThumb(job.keys, job.thumbPath);
        r.ok = true;
        return r;
    }

    const QByteArray originalSha = src.isFile() ? fileSha256(job.path) : QByteArray();
    // Um .photoidxe desta foto que já está lá (a rodada anterior caiu antes
    // de apagar o original, ou depois): mesmo nome nos metadados e, com o
    // original ainda presente, o mesmo hash.
    QString target;
    for (int n = 1; n < 1000; ++n) {
        const QString path = candidate(dir, base, QLatin1String(crypto::kEncryptedExtension), n);
        if (!QFileInfo::exists(path)) {
            if (target.isEmpty())
                target = path;
            break;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const crypto::StreamResult meta = crypto::readMetadata(job.keys.keys, f);
        if (!meta.ok || meta.name != name)
            continue;
        const crypto::StreamResult v = verifyEncrypted(job.keys, path);
        if (v.ok && (originalSha.isEmpty() || v.plainSha256 == originalSha)) {
            if (src.isFile() && !QFile::remove(job.path)) {
                r.error = QStringLiteral("privacy.originalLocked");
                return r;
            }
            fillFileInfo(r, path);
            r.originalName = name;
            r.fileSha256 = hex(v.plainSha256);
            r.encryptedSha256 = hex(v.cipherSha256);
            r.keyId = hex(v.keyId);
            r.thumbPath = encryptThumb(job.keys, job.thumbPath);
            r.ok = true;
            qCInfo(lcPrivacy) << "Resumed encryption of" << job.path;
            return r;
        }
    }
    if (!src.isFile()) {
        r.error = QStringLiteral("privacy.originalMissing");
        return r;
    }
    if (target.isEmpty()) {
        r.error = QStringLiteral("crypto.io");
        return r;
    }

    QFile in(job.path);
    QSaveFile out(target);
    if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly)) {
        r.error = QStringLiteral("privacy.cannotWrite");
        return r;
    }
    const crypto::StreamResult enc = crypto::encryptStream(job.keys.current(), in, in.size(), out, name);
    in.close();
    if (!enc.ok || !out.commit()) {
        out.cancelWriting();
        r.error = enc.ok ? QStringLiteral("privacy.cannotWrite") : enc.error;
        return r;
    }
    // Confere o que ficou no disco antes de apagar o original.
    const crypto::StreamResult check = verifyEncrypted(job.keys, target);
    if (!check.ok || check.plainSha256 != enc.plainSha256 || check.plainSha256 != originalSha) {
        QFile::remove(target);
        r.error = QStringLiteral("privacy.verifyFailed");
        return r;
    }
    if (!QFile::remove(job.path)) {
        // Original somente leitura: não fica a foto em dois lugares.
        QFile::remove(target);
        r.error = QStringLiteral("privacy.originalLocked");
        return r;
    }
    fillFileInfo(r, target);
    r.originalName = name;
    r.fileSha256 = hex(enc.plainSha256);
    r.encryptedSha256 = hex(check.cipherSha256);
    r.keyId = hex(enc.keyId);
    r.thumbPath = encryptThumb(job.keys, job.thumbPath);
    r.ok = true;
    qCInfo(lcPrivacy) << "Encrypted" << job.path << "->" << target;
    return r;
}

PrivacyResult decrypt(const PrivacyJob &job)
{
    PrivacyResult r;
    r.photoId = job.photoId;
    r.op = job.op;
    const QFileInfo src(job.path);
    if (!src.isFile()) {
        r.error = QStringLiteral("privacy.originalMissing");
        return r;
    }
    if (!job.path.endsWith(QStringLiteral(".") + QLatin1String(crypto::kEncryptedExtension))) {
        // Nunca chegou a ser cifrada (estava na fila): só a miniatura.
        fillFileInfo(r, job.path);
        r.originalName = src.fileName();
        r.thumbPath = decryptThumb(job.keys, job.thumbPath);
        r.ok = true;
        return r;
    }
    QFile in(job.path);
    if (!in.open(QIODevice::ReadOnly)) {
        r.error = QStringLiteral("crypto.io");
        return r;
    }
    const crypto::StreamResult meta = crypto::readMetadata(job.keys.keys, in);
    if (!meta.ok) {
        r.error = meta.error;
        return r;
    }
    in.seek(0);
    const QString name = meta.name.isEmpty() ? src.completeBaseName() : meta.name;
    const auto [base, ext] = splitName(name);
    QString target;
    for (int n = 1; n < 1000 && target.isEmpty(); ++n) {
        const QString path = ext.isEmpty() ? candidate(src.absolutePath(), base, {}, n).chopped(1)
                                           : candidate(src.absolutePath(), base, ext, n);
        if (!QFileInfo::exists(path))
            target = path;
    }
    QSaveFile out(target);
    if (target.isEmpty() || !out.open(QIODevice::WriteOnly)) {
        r.error = QStringLiteral("privacy.cannotWrite");
        return r;
    }
    const crypto::StreamResult dec = crypto::decryptStream(job.keys.keys, in, &out);
    in.close();
    if (!dec.ok || (!job.fileSha256.isEmpty() && hex(dec.plainSha256) != job.fileSha256)) {
        out.cancelWriting();
        r.error = dec.ok ? QStringLiteral("privacy.verifyFailed") : dec.error;
        return r;
    }
    if (!out.commit()) {
        r.error = QStringLiteral("privacy.cannotWrite");
        return r;
    }
    if (!QFile::remove(job.path)) {
        QFile::remove(target);
        r.error = QStringLiteral("privacy.originalLocked");
        return r;
    }
    fillFileInfo(r, target);
    r.originalName = QFileInfo(target).fileName();
    r.fileSha256 = hex(dec.plainSha256);
    r.thumbPath = decryptThumb(job.keys, job.thumbPath);
    r.ok = true;
    qCInfo(lcPrivacy) << "Decrypted" << job.path << "->" << target;
    return r;
}

PrivacyResult rekey(const PrivacyJob &job)
{
    PrivacyResult r;
    r.photoId = job.photoId;
    r.op = job.op;
    if (!QFileInfo(job.path).isFile()) {
        r.error = QStringLiteral("privacy.originalMissing");
        return r;
    }
    // Decifra na memória (nunca em claro no disco) e cifra de volta no lugar.
    const imaging::MemoryLease lease(QFileInfo(job.path).size() * 2);
    QString name, error;
    const auto plain = crypto::decryptFile(job.keys.keys, job.path, &name, &error);
    if (!plain) {
        r.error = error;
        return r;
    }
    const QByteArray plainSha = QCryptographicHash::hash(*plain, QCryptographicHash::Sha256);
    if (!job.fileSha256.isEmpty() && hex(plainSha) != job.fileSha256) {
        r.error = QStringLiteral("privacy.verifyFailed");
        return r;
    }
    QByteArray source = *plain;
    QBuffer in(&source);
    in.open(QIODevice::ReadOnly);
    QSaveFile out(job.path);
    if (!out.open(QIODevice::WriteOnly)) {
        r.error = QStringLiteral("privacy.cannotWrite");
        return r;
    }
    const crypto::StreamResult enc = crypto::encryptStream(job.keys.current(), in, source.size(), out, name);
    if (!enc.ok || !out.commit()) {
        out.cancelWriting();
        r.error = enc.ok ? QStringLiteral("privacy.cannotWrite") : enc.error;
        return r;
    }
    fillFileInfo(r, job.path);
    r.originalName = name;
    r.fileSha256 = hex(plainSha);
    r.encryptedSha256 = hex(enc.cipherSha256);
    r.keyId = hex(enc.keyId);
    // Miniatura: decifra e cifra de novo com a atual.
    r.thumbPath = job.thumbPath;
    if (job.thumbPath.endsWith(QLatin1String(".pidx"))) {
        QFile t(job.thumbPath);
        if (t.open(QIODevice::ReadOnly)) {
            const auto thumb = crypto::decryptBytes(job.keys.keys, t.readAll());
            t.close();
            if (thumb)
                writeAtomic(job.thumbPath, crypto::encryptBytes(job.keys.current(), *thumb, QFileInfo(job.thumbPath).fileName()));
        }
    }
    r.ok = true;
    return r;
}

} // namespace

QString encryptedThumbPath(const QString &jpegPath)
{
    const qsizetype dot = jpegPath.lastIndexOf(u'.');
    return (dot > 0 ? jpegPath.left(dot) : jpegPath) + QStringLiteral(".") + QLatin1String(crypto::kThumbnailExtension);
}

PrivacyResult runPrivacy(const PrivacyJob &job)
{
    if (!job.keys.isOpen()) {
        PrivacyResult r;
        r.photoId = job.photoId;
        r.op = job.op;
        r.error = QStringLiteral("vault.locked");
        return r;
    }
    if (job.op == QLatin1String("encrypt"))
        return encrypt(job);
    if (job.op == QLatin1String("decrypt"))
        return decrypt(job);
    return rekey(job);
}

} // namespace pix

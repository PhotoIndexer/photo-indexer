// SPDX-License-Identifier: BSD-2-Clause
#include "core/Crypto.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform_util.h>

#include <algorithm>
#include <cstring>

namespace pix::crypto {

namespace {

constexpr char kMagic[] = "PHOTOIDXE\x01";
constexpr int kMagicSize = 10;
constexpr int kHeaderSize = 40;
constexpr int kNonceSize = 12;
constexpr int kTagSize = 16;
constexpr quint32 kChunkSize = 1u << 20;
constexpr char kCrockford[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

void putBE32(QByteArray &out, quint32 v)
{
    out.append(char(v >> 24)).append(char(v >> 16)).append(char(v >> 8)).append(char(v));
}

void putBE64(QByteArray &out, quint64 v)
{
    putBE32(out, quint32(v >> 32));
    putBE32(out, quint32(v));
}

quint32 getBE32(const char *p)
{
    const auto *u = reinterpret_cast<const uchar *>(p);
    return (quint32(u[0]) << 24) | (quint32(u[1]) << 16) | (quint32(u[2]) << 8) | u[3];
}

quint64 getBE64(const char *p)
{
    return (quint64(getBE32(p)) << 32) | getBE32(p + 4);
}

/// Contexto GCM com a chave já posta (libera no destrutor).
class Gcm
{
public:
    explicit Gcm(const QByteArray &key)
    {
        mbedtls_gcm_init(&m_ctx);
        m_ok = key.size() == kKeyBytes
            && mbedtls_gcm_setkey(&m_ctx, MBEDTLS_CIPHER_ID_AES, reinterpret_cast<const uchar *>(key.constData()), 256) == 0;
    }
    ~Gcm() { mbedtls_gcm_free(&m_ctx); }
    Gcm(const Gcm &) = delete;
    Gcm &operator=(const Gcm &) = delete;

    bool ok() const { return m_ok; }

    /// cifrado || tag
    QByteArray encrypt(const QByteArray &nonce, const QByteArray &aad, const QByteArray &plain)
    {
        QByteArray out(plain.size() + kTagSize, Qt::Uninitialized);
        auto *dst = reinterpret_cast<uchar *>(out.data());
        if (mbedtls_gcm_crypt_and_tag(&m_ctx, MBEDTLS_GCM_ENCRYPT, size_t(plain.size()),
                                      reinterpret_cast<const uchar *>(nonce.constData()), size_t(nonce.size()),
                                      reinterpret_cast<const uchar *>(aad.constData()), size_t(aad.size()),
                                      reinterpret_cast<const uchar *>(plain.constData()), dst, kTagSize,
                                      dst + plain.size())
            != 0)
            return {};
        return out;
    }

    std::optional<QByteArray> decrypt(const QByteArray &nonce, const QByteArray &aad, const QByteArray &sealed)
    {
        if (sealed.size() < kTagSize)
            return std::nullopt;
        const qsizetype n = sealed.size() - kTagSize;
        QByteArray out(n, Qt::Uninitialized);
        const auto *src = reinterpret_cast<const uchar *>(sealed.constData());
        if (mbedtls_gcm_auth_decrypt(&m_ctx, size_t(n), reinterpret_cast<const uchar *>(nonce.constData()),
                                     size_t(nonce.size()), reinterpret_cast<const uchar *>(aad.constData()),
                                     size_t(aad.size()), src + n, kTagSize, src, reinterpret_cast<uchar *>(out.data()))
            != 0)
            return std::nullopt;
        return out;
    }

private:
    mbedtls_gcm_context m_ctx;
    bool m_ok = false;
};

QByteArray recordAad(const QByteArray &header, quint32 index, bool last)
{
    QByteArray aad = header;
    putBE32(aad, index);
    aad.append(char(last ? 1 : 0));
    return aad;
}

QByteArray recordNonce(const QByteArray &prefix, quint32 index)
{
    QByteArray nonce = prefix;
    putBE32(nonce, index);
    return nonce;
}

QByteArray checksum(const QByteArray &key)
{
    return QCryptographicHash::hash(QByteArray("photoidx-key-check") + key, QCryptographicHash::Sha256);
}

/// Bits -> Base32 Crockford (``bits`` múltiplo de 5 após completar com zeros).
QString base32(const QByteArray &data, int chars)
{
    QString out;
    quint32 buffer = 0;
    int bits = 0;
    qsizetype i = 0;
    while (out.size() < chars) {
        if (bits < 5) {
            buffer = (buffer << 8) | (i < data.size() ? uchar(data[i]) : 0);
            ++i;
            bits += 8;
        }
        out += QLatin1Char(kCrockford[(buffer >> (bits - 5)) & 31]);
        bits -= 5;
    }
    return out;
}

/// Cabeçalho lido e conferido; a chave certa escolhida pelo id.
struct Header
{
    QByteArray bytes;
    QByteArray keyId;
    QByteArray prefix;
    quint32 chunk = 0;
    quint64 plainSize = 0;
};

std::optional<Header> readHeader(QIODevice &in, StreamResult &r)
{
    Header h;
    h.bytes = in.read(kHeaderSize);
    if (h.bytes.size() != kHeaderSize || !isEncrypted(h.bytes)) {
        r.error = QStringLiteral("crypto.notEncrypted");
        return std::nullopt;
    }
    h.chunk = getBE32(h.bytes.constData() + 12);
    h.keyId = h.bytes.mid(16, 8);
    h.prefix = h.bytes.mid(24, 8);
    h.plainSize = getBE64(h.bytes.constData() + 32);
    if (h.chunk == 0 || h.chunk > 64u * 1024 * 1024) {
        r.error = QStringLiteral("crypto.corrupt");
        return std::nullopt;
    }
    r.keyId = h.keyId;
    r.plainSize = qint64(h.plainSize);
    return h;
}

/// Lê um registro (tamanho + cifrado); vazio se o arquivo acabou.
std::optional<QByteArray> readRecord(QIODevice &in, quint32 maxPlain)
{
    const QByteArray len = in.read(4);
    if (len.size() != 4)
        return std::nullopt;
    const quint32 n = getBE32(len.constData());
    if (n < quint32(kTagSize) || n > maxPlain + kTagSize)
        return std::nullopt;
    QByteArray rec = in.read(n);
    if (rec.size() != qsizetype(n))
        return std::nullopt;
    return rec;
}

} // namespace

QByteArray randomBytes(int count)
{
    // QRandomGenerator::system() = o gerador do SO (getrandom, arc4random,
    // BCryptGenRandom): serve para chave.
    QByteArray out(count, Qt::Uninitialized);
    auto *gen = QRandomGenerator::system();
    for (int i = 0; i < count; i += 4) {
        const quint32 v = gen->generate();
        for (int k = 0; k < 4 && i + k < count; ++k)
            out[i + k] = char(v >> (8 * k));
    }
    return out;
}

QByteArray deriveKey(const QString &password, const QByteArray &salt, int iterations)
{
    const QByteArray pwd = password.toUtf8();
    QByteArray out(kKeyBytes, Qt::Uninitialized);
    const int rc = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA512, reinterpret_cast<const uchar *>(pwd.constData()),
                                                 size_t(pwd.size()), reinterpret_cast<const uchar *>(salt.constData()),
                                                 size_t(salt.size()), uint(iterations), kKeyBytes,
                                                 reinterpret_cast<uchar *>(out.data()));
    return rc == 0 ? out : QByteArray();
}

QByteArray keyId(const QByteArray &key)
{
    return QCryptographicHash::hash(QByteArray("photoidx-key-id") + key, QCryptographicHash::Sha256).left(8);
}

QByteArray seal(const QByteArray &key, const QByteArray &plain, const QByteArray &aad)
{
    Gcm gcm(key);
    if (!gcm.ok())
        return {};
    const QByteArray nonce = randomBytes(kNonceSize);
    const QByteArray body = gcm.encrypt(nonce, aad, plain);
    return body.isEmpty() ? QByteArray() : nonce + body;
}

std::optional<QByteArray> open(const QByteArray &key, const QByteArray &sealed, const QByteArray &aad)
{
    Gcm gcm(key);
    if (!gcm.ok() || sealed.size() < kNonceSize + kTagSize)
        return std::nullopt;
    return gcm.decrypt(sealed.left(kNonceSize), aad, sealed.mid(kNonceSize));
}

QString formatKey(const QByteArray &key)
{
    // 256 bits = 52 letras (a última com 1 bit e 4 zeros) + 4 de verificação.
    const QString text = base32(key, 52) + base32(checksum(key), 4);
    QStringList groups;
    for (qsizetype i = 0; i < text.size(); i += 4)
        groups << text.mid(i, 4);
    return groups.join(u'-');
}

QByteArray parseKey(const QString &text)
{
    QString clean;
    for (const QChar c : text.toUpper()) {
        if (c == u'-' || c.isSpace())
            continue;
        clean += c == u'O' ? u'0' : (c == u'I' || c == u'L') ? u'1' : c;
    }
    if (clean.size() != 56)
        return {};
    QByteArray bits;
    quint32 buffer = 0;
    int count = 0;
    for (qsizetype i = 0; i < 52; ++i) {
        const char *pos = std::strchr(kCrockford, clean[i].toLatin1());
        if (!pos || !*pos)
            return {};
        buffer = (buffer << 5) | quint32(pos - kCrockford);
        count += 5;
        if (count >= 8) {
            bits.append(char(buffer >> (count - 8)));
            count -= 8;
        }
    }
    if (bits.size() != kKeyBytes || (buffer & ((1u << count) - 1)) != 0)
        return {};
    if (base32(checksum(bits), 4) != clean.right(4))
        return {};
    return bits;
}

bool isEncrypted(const QByteArray &head)
{
    return head.size() >= kMagicSize && std::equal(kMagic, kMagic + kMagicSize, head.constData());
}

// --------------------------------------------------------------------------- //
// KeyRing
// --------------------------------------------------------------------------- //

void KeyRing::set(const QHash<QByteArray, QByteArray> &keys, const QByteArray &currentId)
{
    QMutexLocker locker(&m_lock);
    m_keys = keys;
    m_currentId = currentId;
}

void KeyRing::clear()
{
    QMutexLocker locker(&m_lock);
    // Melhor esforço: as cópias que os workers ainda seguram somem com eles.
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it)
        mbedtls_platform_zeroize(it->data(), size_t(it->size()));
    m_keys.clear();
    m_currentId.clear();
}

bool KeyRing::isOpen() const
{
    QMutexLocker locker(&m_lock);
    return !m_currentId.isEmpty() && m_keys.contains(m_currentId);
}

QByteArray KeyRing::currentId() const
{
    QMutexLocker locker(&m_lock);
    return m_currentId;
}

QByteArray KeyRing::current() const
{
    QMutexLocker locker(&m_lock);
    return m_keys.value(m_currentId);
}

QHash<QByteArray, QByteArray> KeyRing::keys() const
{
    QMutexLocker locker(&m_lock);
    return m_keys;
}

// --------------------------------------------------------------------------- //
// Arquivos
// --------------------------------------------------------------------------- //

StreamResult encryptStream(const QByteArray &key, QIODevice &in, qint64 size, QIODevice &out, const QString &name)
{
    StreamResult r;
    r.name = name;
    r.plainSize = size;
    Gcm gcm(key);
    if (!gcm.ok()) {
        r.error = QStringLiteral("crypto.badKey");
        return r;
    }
    QCryptographicHash plainHash(QCryptographicHash::Sha256);
    QCryptographicHash cipherHash(QCryptographicHash::Sha256);
    const QByteArray prefix = randomBytes(8);
    QByteArray header(kMagic, kMagicSize);
    header.append('\0').append('\0');
    putBE32(header, kChunkSize);
    header.append(keyId(key));
    header.append(prefix);
    putBE64(header, quint64(size));
    r.keyId = keyId(key);

    auto write = [&](const QByteArray &bytes) {
        cipherHash.addData(bytes);
        return out.write(bytes) == bytes.size();
    };
    auto record = [&](quint32 index, const QByteArray &plain, bool last) {
        const QByteArray sealed = gcm.encrypt(recordNonce(prefix, index), recordAad(header, index, last), plain);
        if (sealed.isEmpty())
            return false;
        QByteArray len;
        putBE32(len, quint32(sealed.size()));
        return write(len) && write(sealed);
    };

    const QByteArray meta = QJsonDocument(QJsonObject{{QStringLiteral("name"), name}}).toJson(QJsonDocument::Compact);
    if (!write(header) || !record(0, meta, false)) {
        r.error = QStringLiteral("crypto.io");
        return r;
    }
    qint64 remaining = size;
    quint32 index = 1;
    if (remaining == 0 && !record(index, {}, true)) {
        r.error = QStringLiteral("crypto.io");
        return r;
    }
    while (remaining > 0) {
        const QByteArray chunk = in.read(std::min<qint64>(kChunkSize, remaining));
        if (chunk.isEmpty()) {
            r.error = QStringLiteral("crypto.io");
            return r;
        }
        remaining -= chunk.size();
        plainHash.addData(chunk);
        if (!record(index++, chunk, remaining == 0)) {
            r.error = QStringLiteral("crypto.io");
            return r;
        }
    }
    r.ok = true;
    r.plainSha256 = plainHash.result();
    r.cipherSha256 = cipherHash.result();
    return r;
}

StreamResult decryptStream(const QHash<QByteArray, QByteArray> &keys, QIODevice &in, QIODevice *out)
{
    StreamResult r;
    QCryptographicHash plainHash(QCryptographicHash::Sha256);
    QCryptographicHash cipherHash(QCryptographicHash::Sha256);
    const auto h = readHeader(in, r);
    if (!h)
        return r;
    cipherHash.addData(h->bytes);
    const QByteArray key = keys.value(h->keyId);
    if (key.isEmpty()) {
        r.error = QStringLiteral("crypto.wrongKey");
        return r;
    }
    Gcm gcm(key);
    quint64 got = 0;
    for (quint32 index = 0;; ++index) {
        const auto rec = readRecord(in, std::max<quint32>(h->chunk, 64 * 1024));
        if (!rec) {
            r.error = QStringLiteral("crypto.corrupt");
            return r;
        }
        QByteArray len;
        putBE32(len, quint32(rec->size()));
        cipherHash.addData(len);
        cipherHash.addData(*rec);
        const quint64 n = quint64(rec->size() - kTagSize);
        const bool last = index > 0 && got + n == h->plainSize;
        const auto plain = gcm.decrypt(recordNonce(h->prefix, index), recordAad(h->bytes, index, last), *rec);
        if (!plain) {
            r.error = QStringLiteral("crypto.corrupt");
            return r;
        }
        if (index == 0) {
            r.name = QJsonDocument::fromJson(*plain).object().value(QStringLiteral("name")).toString();
            continue;
        }
        got += n;
        plainHash.addData(*plain);
        if (out && out->write(*plain) != plain->size()) {
            r.error = QStringLiteral("crypto.io");
            return r;
        }
        if (last)
            break;
        if (got > h->plainSize) {
            r.error = QStringLiteral("crypto.corrupt");
            return r;
        }
    }
    if (!in.atEnd()) {
        r.error = QStringLiteral("crypto.corrupt");
        return r;
    }
    r.ok = true;
    r.plainSha256 = plainHash.result();
    r.cipherSha256 = cipherHash.result();
    return r;
}

StreamResult readMetadata(const QHash<QByteArray, QByteArray> &keys, QIODevice &in)
{
    StreamResult r;
    const auto h = readHeader(in, r);
    if (!h)
        return r;
    const QByteArray key = keys.value(h->keyId);
    if (key.isEmpty()) {
        r.error = QStringLiteral("crypto.wrongKey");
        return r;
    }
    const auto rec = readRecord(in, std::max<quint32>(h->chunk, 64 * 1024));
    Gcm gcm(key);
    const auto plain = rec ? gcm.decrypt(recordNonce(h->prefix, 0), recordAad(h->bytes, 0, false), *rec) : std::nullopt;
    if (!plain) {
        r.error = QStringLiteral("crypto.corrupt");
        return r;
    }
    r.name = QJsonDocument::fromJson(*plain).object().value(QStringLiteral("name")).toString();
    r.ok = true;
    return r;
}

QByteArray encryptBytes(const QByteArray &key, const QByteArray &plain, const QString &name)
{
    QByteArray source = plain;
    QBuffer in(&source);
    in.open(QIODevice::ReadOnly);
    QByteArray sealed;
    QBuffer out(&sealed);
    out.open(QIODevice::WriteOnly);
    return encryptStream(key, in, plain.size(), out, name).ok ? sealed : QByteArray();
}

std::optional<QByteArray> decryptBytes(const QHash<QByteArray, QByteArray> &keys, const QByteArray &data, QString *name)
{
    QByteArray source = data;
    QBuffer in(&source);
    in.open(QIODevice::ReadOnly);
    QByteArray plain;
    QBuffer out(&plain);
    out.open(QIODevice::WriteOnly);
    const StreamResult r = decryptStream(keys, in, &out);
    if (!r.ok)
        return std::nullopt;
    if (name)
        *name = r.name;
    return plain;
}

std::optional<QByteArray> decryptFile(const QHash<QByteArray, QByteArray> &keys, const QString &path, QString *name,
                                      QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("crypto.io");
        return std::nullopt;
    }
    QByteArray plain;
    plain.reserve(std::max<qint64>(0, f.size() - kHeaderSize));
    QBuffer out(&plain);
    out.open(QIODevice::WriteOnly);
    const StreamResult r = decryptStream(keys, f, &out);
    if (!r.ok) {
        if (error)
            *error = r.error;
        return std::nullopt;
    }
    if (name)
        *name = r.name;
    return plain;
}

} // namespace pix::crypto

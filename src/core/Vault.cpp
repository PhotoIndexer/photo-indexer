// SPDX-License-Identifier: BSD-2-Clause
#include "core/Vault.h"

#include "core/Crypto.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace pix::vault {

namespace {

const QByteArray kAad("photoidx-vault-v1");

} // namespace

Ring newRing()
{
    return ringWith(crypto::randomBytes(crypto::kKeyBytes));
}

Ring ringWith(const QByteArray &key)
{
    Ring ring;
    ring.currentId = crypto::keyId(key);
    ring.keys.insert(ring.currentId, key);
    return ring;
}

QByteArray serialize(const Ring &ring)
{
    QJsonArray keys;
    for (auto it = ring.keys.constBegin(); it != ring.keys.constEnd(); ++it)
        keys.append(QString::fromLatin1(it.value().toBase64()));
    return QJsonDocument(QJsonObject{{QStringLiteral("current"), QString::fromLatin1(ring.currentId.toHex())},
                                     {QStringLiteral("keys"), keys}})
        .toJson(QJsonDocument::Compact);
}

std::optional<Ring> parse(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    Ring ring;
    ring.currentId = QByteArray::fromHex(o.value(QStringLiteral("current")).toString().toLatin1());
    for (const QJsonValue &v : o.value(QStringLiteral("keys")).toArray()) {
        const QByteArray key = QByteArray::fromBase64(v.toString().toLatin1());
        if (key.size() == crypto::kKeyBytes)
            ring.keys.insert(crypto::keyId(key), key);
    }
    if (!ring.keys.contains(ring.currentId))
        return std::nullopt;
    return ring;
}

Record protect(const Ring &ring, const QString &password, int iterations, QByteArray *passwordKeyOut)
{
    Record r;
    r.salt = crypto::randomBytes(16);
    r.iterations = iterations;
    r.currentId = ring.currentId;
    const QByteArray kek = crypto::deriveKey(password, r.salt, iterations);
    r.wrapped = crypto::seal(kek, serialize(ring), kAad);
    if (passwordKeyOut)
        *passwordKeyOut = kek;
    return r;
}

QByteArray passwordKey(const Record &record, const QString &password)
{
    return crypto::deriveKey(password, record.salt, record.iterations);
}

std::optional<Ring> unwrap(const Record &record, const QByteArray &passwordKey)
{
    const auto json = crypto::open(passwordKey, record.wrapped, kAad);
    return json ? parse(*json) : std::nullopt;
}

Record rewrap(const Record &record, const Ring &ring, const QByteArray &passwordKey)
{
    Record r = record;
    r.plain.clear();
    r.currentId = ring.currentId;
    r.wrapped = crypto::seal(passwordKey, serialize(ring), kAad);
    return r;
}

} // namespace pix::vault

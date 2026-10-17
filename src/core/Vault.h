// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O chaveiro das fotos privadas como fica no banco (tabela ``setting``):
//
//   vault/plain       o chaveiro em claro — só entre a instalação (a chave
//                     nasce no primeiro Catalog::open) e a pessoa definir a
//                     senha; nada foi cifrado ainda
//   vault/wrapped     o chaveiro cifrado (AES-256-GCM) com a chave derivada da
//                     senha (PBKDF2-HMAC-SHA512)
//   vault/salt        sal do PBKDF2 (16 bytes, base64)
//   vault/iterations  iterações do PBKDF2
//   vault/currentId   id público da chave atual (confere a chave digitada na
//                     recuperação)
//
// O chaveiro guarda a chave atual e, durante uma troca de chave, as antigas
// (até a última foto ser recifrada). Funções puras: rodam fora da thread
// principal (o PBKDF2 leva ~0,5 s de propósito).

#include <QByteArray>
#include <QHash>
#include <QString>

#include <optional>

namespace pix::vault {

struct Ring
{
    QHash<QByteArray, QByteArray> keys;     ///< id -> chave
    QByteArray currentId;

    QByteArray current() const { return keys.value(currentId); }
};

/// O que o banco guarda (strings da tabela ``setting``).
struct Record
{
    QByteArray plain;           ///< chaveiro em claro (JSON) — vazio depois da senha
    QByteArray wrapped;
    QByteArray salt;
    int iterations = 0;
    QByteArray currentId;

    bool hasPassword() const { return !wrapped.isEmpty(); }
};

/// Chaveiro novo com uma chave aleatória.
Ring newRing();
/// Chaveiro com só esta chave.
Ring ringWith(const QByteArray &key);

QByteArray serialize(const Ring &ring);
std::optional<Ring> parse(const QByteArray &json);

/// O chaveiro cifrado com a senha (sal e iterações novos se ``record`` não
/// tiver): devolve o registro pronto para gravar.
Record protect(const Ring &ring, const QString &password, int iterations, QByteArray *passwordKeyOut = nullptr);
/// A chave derivada da senha (o passo lento).
QByteArray passwordKey(const Record &record, const QString &password);
std::optional<Ring> unwrap(const Record &record, const QByteArray &passwordKey);
/// Recifra o chaveiro com a mesma chave de senha (troca de chave, poda).
Record rewrap(const Record &record, const Ring &ring, const QByteArray &passwordKey);

} // namespace pix::vault

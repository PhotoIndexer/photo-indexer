// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Criptografia das fotos privadas (Mbed TLS, src/cmake/MbedTLS.cmake).
//
//   Chave         256 bits aleatórios, gerada na instalação (Catalog::open).
//                 É ela que cifra os arquivos; a pessoa a guarda (formatKey)
//                 para recuperar o acesso se esquecer a senha.
//   Senha         PBKDF2-HMAC-SHA512 (600 mil iterações, sal de 16 bytes)
//                 deriva a chave que guarda o chaveiro no banco.
//   Arquivos      AES-256-GCM em blocos de 1 MiB (formato abaixo): o
//                 original vira <nome>.photoidxe; a miniatura, <id>.pidx.
//
// Formato (cabeçalho de 40 bytes, depois registros):
//
//   "PHOTOIDXE" 0x01        magia + versão                       10 bytes
//   0x00 0x00               reservado                             2
//   tamanho do bloco        u32 BE                                4
//   id da chave             8 bytes (keyId)                       8
//   prefixo do nonce        8 bytes aleatórios                    8
//   tamanho do original     u64 BE                                8
//   registro                u32 BE (tamanho) + cifrado + tag de 16 bytes
//
// Registro 0 = metadados (JSON: {"name": "IMG_0001.CR3"}); 1..N = os dados.
// Nonce = prefixo || u32 BE(índice); AAD = cabeçalho || u32 BE(índice) ||
// 1 byte "último" — bloco trocado de lugar, cortado ou de outro arquivo não
// passa na tag.

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>

#include <optional>

class QIODevice;

namespace pix::crypto {

inline constexpr int kKeyBytes = 32;
inline constexpr int kDefaultIterations = 600'000;
/// Extensão do original cifrado (sem ponto) e da miniatura cifrada.
inline constexpr char kEncryptedExtension[] = "photoidxe";
inline constexpr char kThumbnailExtension[] = "pidx";

QByteArray randomBytes(int count);
/// PBKDF2-HMAC-SHA512 -> kKeyBytes.
QByteArray deriveKey(const QString &password, const QByteArray &salt, int iterations);
/// Id público da chave (8 bytes): diz qual chave abriu um arquivo sem revelá-la.
QByteArray keyId(const QByteArray &key);

/// AES-256-GCM de um bloco pequeno: nonce(12) || cifrado || tag(16).
QByteArray seal(const QByteArray &key, const QByteArray &plain, const QByteArray &aad);
std::optional<QByteArray> open(const QByteArray &key, const QByteArray &sealed, const QByteArray &aad);

/// A chave para a pessoa guardar: Base32 (Crockford) em grupos de 4, com 4
/// letras de verificação no fim ("7K2M-...-Q9XA").
QString formatKey(const QByteArray &key);
/// O contrário (aceita minúsculas, espaços e hífens); vazio se inválida.
QByteArray parseKey(const QString &text);

/// O conteúdo começa com a magia do formato?
bool isEncrypted(const QByteArray &head);

/// Chaves abertas na sessão (id -> chave) e a atual (a que cifra o novo).
/// Compartilhado com os workers e o provider de imagens: thread-safe.
class KeyRing
{
public:
    void set(const QHash<QByteArray, QByteArray> &keys, const QByteArray &currentId);
    /// Esquece as chaves (sobrescreve a memória antes).
    void clear();
    bool isOpen() const;
    QByteArray currentId() const;
    QByteArray current() const;
    QHash<QByteArray, QByteArray> keys() const;

private:
    mutable QMutex m_lock;
    QHash<QByteArray, QByteArray> m_keys;
    QByteArray m_currentId;
};

/// Resultado de cifrar/decifrar um arquivo (ou buffer).
struct StreamResult
{
    bool ok = false;
    QString error;              ///< "crypto.wrongKey", "crypto.corrupt", "crypto.io"...
    QString name;               ///< nome original (dos metadados)
    qint64 plainSize = 0;
    QByteArray keyId;
    QByteArray plainSha256;     ///< SHA-256 do original
    QByteArray cipherSha256;    ///< SHA-256 do cifrado
};

/// Cifra ``in`` (``size`` bytes) em ``out`` com ``key``. ``out`` só recebe
/// escrita sequencial (QSaveFile serve).
StreamResult encryptStream(const QByteArray &key, QIODevice &in, qint64 size, QIODevice &out, const QString &name);
/// Decifra ``in`` em ``out`` (nulo: só confere e calcula o hash) com a chave
/// do id do cabeçalho, entre as de ``keys``.
StreamResult decryptStream(const QHash<QByteArray, QByteArray> &keys, QIODevice &in, QIODevice *out);
/// Só os metadados (nome, tamanho, id da chave), sem decifrar os dados.
StreamResult readMetadata(const QHash<QByteArray, QByteArray> &keys, QIODevice &in);

/// Atalhos para miniaturas e leituras em memória.
QByteArray encryptBytes(const QByteArray &key, const QByteArray &plain, const QString &name);
std::optional<QByteArray> decryptBytes(const QHash<QByteArray, QByteArray> &keys, const QByteArray &data,
                                       QString *name = nullptr);
/// Lê e decifra um arquivo inteiro na memória.
std::optional<QByteArray> decryptFile(const QHash<QByteArray, QByteArray> &keys, const QString &path,
                                      QString *name = nullptr, QString *error = nullptr);

} // namespace pix::crypto

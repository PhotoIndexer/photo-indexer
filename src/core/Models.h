// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O que viaja entre a thread principal (dona do banco, regra 9) e os workers
// das etapas: o pedido que o Catalog monta e o resultado que o worker devolve.
// Tudo por valor, sem ponteiro para o banco.

#include "core/Places.h"
#include "imaging/Embedding.h"
#include "imaging/Exif.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

namespace pix {

/// Um arquivo de imagem que o Searcher encontrou.
struct FoundFile
{
    QString path;               ///< absoluto, com '/'
    qint64 size = 0;
    qint64 mtimeMs = 0;
    QDateTime created;          ///< criação do arquivo (birth time; ou a mudança de metadados)
    QDateTime modified;
    QString keyId;              ///< .photoidxe: hex do id da chave (do cabeçalho)
};

/// Indexer: abrir o arquivo, catalogar EXIF, gerar a miniatura, hashes e lugar.
/// Chaves abertas (id -> chave) e a atual, copiadas para um worker.
struct KeySnapshot
{
    QHash<QByteArray, QByteArray> keys;
    QByteArray currentId;

    bool isOpen() const { return keys.contains(currentId); }
    QByteArray current() const { return keys.value(currentId); }
};

struct IndexJob
{
    QString photoId;
    QString path;
    QString previousThumb;      ///< miniatura antiga (sai se o caminho mudar)
    QDateTime fileCreated;
    QDateTime fileModified;
    bool isPrivate = false;     ///< miniatura cifrada; ``path`` pode ser um .photoidxe
    KeySnapshot keys;           ///< só nas privadas
};

struct IndexResult
{
    QString photoId;
    bool ok = false;
    QString error;
    bool isRaw = false;
    QString format;
    int width = 0;
    int height = 0;
    imaging::ShotInfo shot;
    imaging::ExifTags tags;
    QDateTime takenAt;          ///< data de criação (relógio local, sem fuso)
    QString takenSource;        ///< "exif" | "file"
    QString thumbPath;          ///< "" se não houve imagem
    int thumbWidth = 0;
    int thumbHeight = 0;
    QString pictureHash;
    QString perceptualHash;     ///< 16 dígitos hex
    double sharpness = -1.0;
    PlaceMatch place;
    QString originalName;       ///< .photoidxe: o nome guardado nos metadados
    QString keyId;              ///< .photoidxe: hex do id da chave que cifrou
};

/// Categorizer: o CLIP olha a miniatura e compara com as categorias.
struct CategoryPrompt
{
    QString categoryId;
    imaging::Embedding text;    ///< embedding da frase
};

struct CategorizeJob
{
    QString photoId;
    QString thumbPath;
    imaging::Embedding embedding;   ///< já calculado antes: só reclassifica
};

struct CategorizeResult
{
    QString photoId;
    bool ok = false;
    QString error;
    imaging::Embedding embedding;
    QList<QPair<QString, float>> categories;  ///< (categoria, probabilidade), melhor primeiro
};

/// Faces: detectar, gerar a biometria e o recorte de cada rosto.
struct FaceJob
{
    QString photoId;
    QString path;
    QDate date;                 ///< pasta do recorte (faces/YYYY/MM)
};

struct FoundFace
{
    QString faceId;
    float x = 0, y = 0, w = 0, h = 0;   ///< normalizado (0..1)
    float score = 0;
    imaging::Embedding embedding;
    QString cropPath;
};

struct FaceResult
{
    QString photoId;
    bool ok = false;
    QString error;
    QList<FoundFace> faces;
};

/// Etapa "privacy": cifrar o original e a miniatura de uma foto que virou
/// privada, decifrar a que voltou a ser pública, ou recifrar com a chave nova.
struct PrivacyJob
{
    QString photoId;
    QString op;                 ///< "encrypt" | "decrypt" | "rekey"
    QString path;               ///< o arquivo agora (original ou .photoidxe)
    QString thumbPath;          ///< a miniatura agora (.jpg ou .pidx); pode faltar
    QString fileSha256;         ///< o esperado do original (decrypt confere)
    KeySnapshot keys;
};

struct PrivacyResult
{
    QString photoId;
    QString op;
    bool ok = false;
    QString error;
    QString path;               ///< onde o arquivo ficou
    QString thumbPath;
    qint64 size = 0;            ///< do arquivo novo (o Searcher não o vê como alterado)
    qint64 mtimeMs = 0;
    QString originalName;
    QString fileSha256;         ///< hex, antes de cifrar
    QString encryptedSha256;    ///< hex, depois de cifrar
    QString keyId;              ///< hex
};

} // namespace pix

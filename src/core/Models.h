// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O que viaja entre a thread principal (dona do banco, regra 9) e os workers
// das etapas: o pedido que o Catalog monta e o resultado que o worker devolve.
// Tudo por valor, sem ponteiro para o banco.

#include "core/Places.h"
#include "imaging/Embedding.h"
#include "imaging/Exif.h"

#include <QDate>
#include <QDateTime>
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
};

/// Indexer: abrir o arquivo, catalogar EXIF, gerar a miniatura, hashes e lugar.
struct IndexJob
{
    QString photoId;
    QString path;
    QString previousThumb;      ///< miniatura antiga (sai se o caminho mudar)
    QDateTime fileCreated;
    QDateTime fileModified;
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

} // namespace pix

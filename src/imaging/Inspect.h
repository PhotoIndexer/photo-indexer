// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O trabalho do Indexer sobre UM arquivo, numa passada: metadados e todas as
// tags EXIF, a imagem de trabalho (já orientada) e as assinaturas dos pixels.
// Sem banco e sem interface: roda em qualquer thread do pool.
//
//   JPEG/PNG        OpenCV (pixels completos: o hash é o do ../photoe)
//   TIFF/WebP/...   QImageReader (o que o Qt sabe abrir)
//   RAW             LibRaw (prévia embutida + hash do sensor)

#include "imaging/Exif.h"

#include <QByteArray>
#include <QString>

#include <opencv2/core.hpp>

namespace pix::imaging {

/// Lado maior da imagem de trabalho: a miniatura sai dela e a IA e as faces
/// a usam (as faces pedem resolução: rosto pequeno some numa imagem pequena).
inline constexpr int kWorkingSide = 2048;

struct Inspection
{
    bool ok = false;        ///< o arquivo foi lido (metadados, ao menos)
    QString error;          ///< por que não foi (ou por que não há imagem)
    bool isRaw = false;
    QString format;         ///< "JPEG", "PNG", "NEF"...
    int width = 0;          ///< tamanho real, já na orientação de exibição
    int height = 0;
    ShotInfo shot;
    ExifTags tags;
    cv::Mat image;          ///< RGB orientado, lado maior <= kWorkingSide; vazio se não decodificou
    QString pictureHash;    ///< SHA-256 dos pixels (ver Hashes.h)
    quint64 perceptualHash = 0;
    double sharpness = -1.0;
};

Inspection inspect(const QString &path);
/// O mesmo, de um arquivo já na memória (``fileName`` diz o formato pela
/// extensão): o original decifrado de uma foto privada.
Inspection inspectData(const QByteArray &data, const QString &fileName);

/// Imagem para exibir em tamanho grande (visualizador): orientada, lado
/// maior <= ``maxSide``. RAW = a prévia embutida.
cv::Mat loadDisplayImage(const QString &path, int maxSide);
cv::Mat loadDisplayImageData(const QByteArray &data, const QString &fileName, int maxSide);

} // namespace pix::imaging

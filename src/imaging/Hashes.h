// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Assinaturas de uma imagem, calculadas SÓ dos pixels (nada de EXIF, ICC ou
// nome de arquivo):
//
//   pictureHash   SHA-256 hex dos pixels decodificados — o ``picture_hash`` do
//                 ../photoe (services/images.py): "<modo>:<L>x<A>:" + bytes,
//                 com a orientação do EXIF já aplicada. Identifica a MESMA
//                 imagem com metadados diferentes; uma recompressão muda tudo.
//   perceptualHash  64 bits (dHash 9x8 da luminância): fotos PARECIDAS
//                 (redimensionadas, recomprimidas) ficam a poucos bits de
//                 distância de Hamming — é a chave da HashTree (core).
//   sharpness     variância do Laplaciano numa versão de 512 px: abaixo de
//                 ~60, a foto está tremida/desfocada.

#include <QByteArray>
#include <QString>

#include <opencv2/core.hpp>

namespace pix::imaging {

/// SHA-256 hex de ``mode:WxH:`` + os bytes de ``pixels`` (linha a linha, sem
/// o enchimento do stride). ``pixels``: CV_8UC3 em RGB (modo "RGB") ou CV_8UC1
/// (modo "L"); ``mode`` vazio = deduzido dos canais.
QString pictureHash(const cv::Mat &pixels, const QString &mode = {});

/// Hash de pixels de um RAW: os dados do sensor como o LibRaw os desempacota
/// ("RAW:WxH:" + amostras de 16 bits little-endian).
QString rawPictureHash(const quint16 *data, int width, int height, int pitchBytes);

/// dHash de 64 bits (gradiente horizontal da luminância 9x8).
quint64 perceptualHash(const cv::Mat &rgb);

/// Variância do Laplaciano da luminância (maior = mais nítida).
double sharpness(const cv::Mat &rgb);

/// Distância de Hamming entre dois hashes de 64 bits.
int hammingDistance(quint64 a, quint64 b);

/// 16 dígitos hex <-> 64 bits.
QString hashToHex(quint64 hash);
quint64 hashFromHex(const QString &hex, bool *ok = nullptr);

} // namespace pix::imaging

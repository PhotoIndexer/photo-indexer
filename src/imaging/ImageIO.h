// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Leitura e gravação de imagens: cabeçalho de JPEG (sem decodificar), pixels
// já na orientação de exibição, redução, JPEG em memória e gravação atômica.
//
// Caminhos são QString e os arquivos são lidos com QFile + cv::imdecode —
// nunca cv::imread com std::string, que quebra com nomes não ASCII no Windows.
// Imagens: cv::Mat RGB (não BGR) CV_8UC3.

#include <QByteArray>
#include <QImage>
#include <QString>

#include <opencv2/core.hpp>

#include <optional>

namespace pix::imaging {

/// Cabeçalho de um JPEG, lido sem decodificar: dimensões do SOF (como estão
/// gravadas, antes da orientação), componentes de cor e o APP1 Exif
/// (``"Exif\0\0"`` + TIFF).
struct JpegHeader
{
    int width = 0;
    int height = 0;
    int components = 3;     ///< 1 = tons de cinza, 3 = YCbCr/RGB, 4 = CMYK
    QByteArray exif;
};

std::optional<JpegHeader> parseJpegHeader(const QByteArray &data);

/// Gira/espelha conforme a Orientation do EXIF (``ImageOps.exif_transpose``).
cv::Mat applyOrientation(const cv::Mat &img, int orientation);

/// Tamanho que cabe em ``maxSide`` × ``maxSide`` mantendo a proporção (nunca amplia).
cv::Size fitSize(cv::Size size, int maxSide);

/// Redimensiona: INTER_AREA para reduzir, INTER_LANCZOS4 para ampliar.
cv::Mat resizeTo(const cv::Mat &img, cv::Size size);

/// Reduz para caber em ``maxSide`` (0 = como está).
cv::Mat fitTo(const cv::Mat &img, int maxSide);

/// Decodifica uma imagem em memória (JPEG, PNG...) com o OpenCV, SEM aplicar a
/// orientação. ``scale`` (1, 2, 4, 8) reduz um JPEG já na DCT. Devolve RGB
/// CV_8UC3 ou, com ``keepGray`` e imagem de 1 canal, CV_8UC1. Vazio se falhar.
cv::Mat decodeImage(const QByteArray &data, int scale = 1, bool keepGray = false);

/// JPEG em memória. Vazio se falhar.
QByteArray encodeJpeg(const cv::Mat &rgb, int quality);

/// Grava em arquivo temporário e renomeia: quem lê nunca vê um arquivo pela metade.
bool writeAtomic(const QString &path, const QByteArray &data);

/// cv::Mat RGB/RGBA/cinza -> QImage (cópia).
QImage toQImage(const cv::Mat &mat);
/// QImage -> cv::Mat RGB CV_8UC3 (cópia).
cv::Mat fromQImage(const QImage &image);

} // namespace pix::imaging

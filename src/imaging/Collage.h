// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Montagem "ao longo dos anos" (tela Pessoas): o recorte de retrato em volta
// de um rosto, a grade de recortes com o rótulo de cada período (a faixa
// branca com o ano, como as montagens das redes) e a gravação com metadados
// (EXIF no JPEG, texto no PNG). Roda em qualquer thread.

#include <QImage>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

namespace pix::imaging {

/// Recorte de retrato (``out``) de ``src`` (RGB, orientada) em volta do rosto
/// ``face`` (normalizado 0..1): o rosto ocupa ~40% da altura, um pouco acima
/// do meio, com ombros e cabelo. Sem sair da foto: perto da borda, o recorte
/// desliza ou encolhe. Vazio se ``src`` for vazia.
cv::Mat cropPortrait(const cv::Mat &src, const QRectF &face, cv::Size out);

struct CollageStyle
{
    int columns = 3;
    int gap = 8;             ///< espaço entre os recortes (e em volta)
    bool labels = true;      ///< a faixa com o período
    QRgb background = 0xffffffff;
};

/// A grade: recortes do mesmo tamanho (o do primeiro), a última linha
/// centralizada. ``labels`` pode ser menor que ``tiles``.
QImage composeCollage(const QList<QImage> &tiles, const QStringList &labels, const CollageStyle &style);

/// Metadados da imagem exportada.
struct ExportInfo
{
    QString title;      ///< "Ana ao longo dos anos"
    QString software;   ///< "PhotoIDX 1.4.0"
    QString url;        ///< o site do app
};

/// Segmento APP1 de EXIF (sem o marcador): "Exif\0\0" + TIFF little-endian
/// com ImageDescription, Software, DateTime, Orientation e, no Exif IFD,
/// UserComment (com o endereço do app) e DateTimeDigitized.
QByteArray buildExif(const ExportInfo &info);

/// Grava ``image`` em ``path`` (JPEG ou PNG pela extensão; sem extensão =
/// JPEG) com os metadados, por QSaveFile. Devolve o erro (vazio = gravou).
QString saveExport(const QImage &image, const QString &path, const ExportInfo &info);

} // namespace pix::imaging

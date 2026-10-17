// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// RAW de câmera pelo LibRaw: metadados, TODAS as tags EXIF (pelo callback do
// parser, que percorre inclusive CR3, RAF e os IFDs dos TIFF-like), a maior
// prévia JPEG embutida (já na orientação de exibição) e o hash dos dados do
// sensor. Nada de revelar o RAW: a prévia da própria câmera basta para a
// miniatura, as categorias e as faces.
//
// Thread-safe: cada chamada usa o seu LibRaw.

#include "imaging/Exif.h"

#include <QString>

#include <opencv2/core.hpp>

namespace pix::imaging {

struct RawInfo
{
    bool ok = false;
    QString error;
    ExifTags tags;
    ShotInfo shot;          ///< das tags; o que faltar vem dos campos do LibRaw
    int width = 0;          ///< imagem final, já na orientação de exibição
    int height = 0;
    cv::Mat preview;        ///< RGB, orientado; vazio se o RAW não traz prévia
    QString pictureHash;    ///< dos dados do sensor; vazio se não deu para desempacotar
};

/// ``wantPreview``: decodifica a maior prévia embutida. ``wantHash``:
/// desempacota o sensor (o passo mais caro) para o hash de pixels.
RawInfo readRaw(const QString &path, bool wantPreview, bool wantHash);
/// O mesmo, de um RAW já na memória.
RawInfo readRawData(const QByteArray &data, bool wantPreview, bool wantHash);

} // namespace pix::imaging

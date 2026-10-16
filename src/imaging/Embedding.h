// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Vetor de características (CLIP, ArcFace): float32, normalizado (norma 1) —
// o produto escalar é a similaridade de cosseno. No banco vai como BLOB de
// float32 little-endian.

#include <QByteArray>

#include <vector>

namespace pix::imaging {

using Embedding = std::vector<float>;

/// Divide pela norma (vetor nulo fica como está).
void normalize(Embedding &v);

/// Produto escalar (= cosseno, se os dois estão normalizados). 0 se os
/// tamanhos diferem.
float dot(const Embedding &a, const Embedding &b);
float dot(const float *a, const float *b, int n);

QByteArray toBlob(const Embedding &v);
Embedding fromBlob(const QByteArray &blob);

/// Média normalizada (o "centroide" de uma pessoa).
Embedding mean(const std::vector<Embedding> &vectors);

} // namespace pix::imaging

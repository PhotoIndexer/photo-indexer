// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Agrupar rostos em pessoas — a regra inteira, sem banco (roda fora da
// thread principal). Calibrada com 43 mil faces de um catálogo real de fotos
// esportivas (rostos de ~40 px): a regra antiga fazia uma "pessoa" de 12 mil
// faces de centenas de jogadores; esta, nenhuma face impossível e o pior
// grupo com o dobro de coerência.
//
//   1. Uma pessoa por foto: duas faces da mesma foto nunca são a mesma
//      pessoa (nem ao entrar, nem ao juntar grupos).
//   2. Entrar num grupo: cosseno com o centroide (normalizado) >= join (0,50).
//   3. Juntar grupos: a MÉDIA das similaridades entre as faces dos dois
//      (o produto das médias sem normalizar) >= merge (0,40), recalculada
//      depois de cada junção — sem "cadeia" (A~B, B~C não junta A com C).
//      O cosseno entre centroides normalizados inchava com grupos mistos (a
//      média de muitas faces diferentes vira um "rosto genérico") e juntava
//      tudo.
//   4. Grupos fixos (pessoas com nome, faces confirmadas): recebem faces,
//      absorvem grupos, mas dois fixos nunca se juntam.
//
// As faces de detecção fraca (nuca, perfil, borrão) nem entram: quem chama
// filtra pela confiança (faces/minPersonScore, 0,65).

#include "imaging/Embedding.h"

#include <vector>

namespace pix {

struct ClusterFace
{
    int photo = 0;                  ///< mesmo número = mesma foto
    imaging::Embedding embedding;   ///< normalizado
    int fixed = -1;                 ///< grupo fixo (0..fixedGroups-1) ou -1
    std::vector<int> notIn;         ///< grupos fixos de onde a face foi tirada (face_rejection)
};

struct ClusterParams
{
    float join = 0.50f;
    float merge = 0.40f;
    int mergeEvery = 300;           ///< passada de junção a cada N faces
    int mergeMinFaces = 3;          ///< grupos menores não entram na junção
};

/// O grupo de cada face (>= 0). Os grupos 0..fixedGroups-1 são os fixos; os
/// novos vêm depois, numerados em ordem.
std::vector<int> clusterFaces(const std::vector<ClusterFace> &faces, int fixedGroups, const ClusterParams &params);

} // namespace pix

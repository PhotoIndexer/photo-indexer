// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Do CLIP às categorias de uma foto (zero-shot), sem modelo nem banco — só a
// regra, testável à parte. Calibrada com o catálogo de verdade (100 fotos
// rotuladas à mão: de 24 para 64 inteiramente certas, precisão de 0,33 para
// 0,86). Ideias, em ordem de efeito:
//
//   1. Na dúvida entre IRMÃS, o PAI: basquete × vôlei × futsal numa quadra,
//      batizado × formatura × festa junina numa cerimônia — o CLIP separa
//      mal (cossenos a ~0,01 um do outro), mas acerta o grupo. "Esportes"
//      certo vale mais que "Vôlei" errado.
//   2. Classes de "nenhuma" (frases genéricas: "a photo", "a blank image"):
//      quando vencem, a foto fica sem categoria em vez de ganhar uma errada.
//   3. Limiar de 0,45 e nunca duas irmãs na mesma foto (o top-3 acima de
//      0,15 empilhava futebol e vôlei em toda foto de basquete).
//   4. Sem "melhor palpite": abaixo do limiar não entra nada.

#include <QList>
#include <QPair>
#include <QString>

namespace pix {

struct ClassifierParams
{
    float minProbability = 0.45f;     ///< a categoria precisa disto (softmax entre todas)
    int maxCategories = 2;            ///< de grupos diferentes
    float parentProbability = 0.5f;   ///< o pai entra se as filhas somam isto
    float siblingMargin = 0.3f;       ///< irmã com >= margem × a melhor: dúvida -> o pai
    float scale = 100.0f;             ///< escala do CLIP (logit = 100 × cosseno)
};

/// O cosseno da melhor frase de uma categoria.
struct CategoryScore
{
    QString categoryId;
    QString parentId;                 ///< vazio = categoria na raiz
    float cosine = 0.0f;
};

/// As categorias da foto, mais provável primeiro: [(id, probabilidade)].
/// ``background`` = o cosseno da melhor frase de "nenhuma" (NaN = sem).
QList<QPair<QString, float>> pickCategories(const QList<CategoryScore> &scores, float background,
                                            const ClassifierParams &params);

} // namespace pix

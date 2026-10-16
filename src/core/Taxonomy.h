// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// As categorias que vêm com o app (árvore de 2 níveis). O nome no código é
// em inglês e traduzido na interface (contexto "Category" do .ts); a pessoa
// pode renomear, criar, apagar e aninhar as dela.
//
// ``prompts``: frases em INGLÊS para o CLIP, separadas por '|' (o modelo só
// conhece inglês). As de regra própria (sem categoria, borrada, duplicata) não
// têm frase: o Categorizer as decide pela nitidez e pelos hashes.

#include <QList>
#include <QString>

namespace pix {

/// Chaves das categorias decididas por regra, não pelo CLIP.
namespace builtin {
inline constexpr const char *kUncategorized = "other.uncategorized";
inline constexpr const char *kBlurry = "other.blurry";
inline constexpr const char *kDuplicate = "other.duplicate";
} // namespace builtin

struct BuiltinCategory
{
    const char *key;
    const char *parentKey;  ///< nullptr = raiz
    const char *name;       ///< inglês; QT_TRANSLATE_NOOP("Category", ...)
    const char *prompts;    ///< frases do CLIP separadas por '|'; "" = por regra
};

const QList<BuiltinCategory> &builtinCategories();

/// Nome traduzido de uma categoria embutida (pela chave); vazio se não for embutida.
QString builtinCategoryName(const QString &key);

/// Frases do CLIP de um ``prompt`` gravado ('|'); sem frase, "a photo of <nome>".
QStringList clipPrompts(const QString &prompt, const QString &name);

} // namespace pix

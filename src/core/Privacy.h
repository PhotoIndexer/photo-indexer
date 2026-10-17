// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O trabalho da etapa "privacy" sobre UMA foto (sem banco; roda no pool):
//
//   encrypt   original -> <nome>.photoidxe no MESMO lugar; confere decifrando
//             (SHA-256 igual) e só então apaga o original. Miniatura .jpg ->
//             .pidx.
//   decrypt   o contrário: volta o arquivo com o nome original.
//   rekey     recifra o .photoidxe e a miniatura com a chave atual.
//
// Cada arquivo novo nasce por QSaveFile (temporário + rename atômico): uma
// queda no meio nunca deixa um arquivo pela metade. Se o app cair entre
// gravar o .photoidxe e apagar o original, a próxima rodada reconhece o
// .photoidxe pronto (metadados + hash) e continua dali.
//
// Esta é a ÚNICA exceção à regra de nunca alterar as fotos da pessoa
// (CLAUDE.md): só com a pessoa pedindo "tornar privada".

#include "core/Models.h"

namespace pix {

PrivacyResult runPrivacy(const PrivacyJob &job);

/// Caminho da miniatura cifrada de uma miniatura .jpg (mesma pasta, .pidx).
QString encryptedThumbPath(const QString &jpegPath);

} // namespace pix

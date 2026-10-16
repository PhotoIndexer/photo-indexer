// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Que arquivos o indexador considera foto: pela extensão (sem diferenciar
// maiúsculas). RAW é tudo que o LibRaw abre (Canon, Nikon, Sony, Fuji,
// Olympus, Panasonic, Pentax, Leica, Hasselblad, Phase One, DNG...).

#include <QString>
#include <QStringList>

namespace pix::imaging {

/// "jpg", "png", "heic"... (sem ponto, minúsculas).
const QStringList &imageExtensions();
/// "nef", "cr2", "cr3", "arw", "dng"...
const QStringList &rawExtensions();

/// A extensão (minúscula, sem ponto) de ``path``.
QString extensionOf(const QString &path);
bool isImageFile(const QString &path);
bool isRawFile(const QString &path);

/// Rótulo do formato para a interface ("JPEG", "PNG", "NEF"...).
QString formatLabel(const QString &extension);

} // namespace pix::imaging

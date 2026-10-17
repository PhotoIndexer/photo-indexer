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

/// Bytes do começo do arquivo que bastam para reconhecer o formato.
inline constexpr int kSniffBytes = 4096;

/// O tipo MIME do CONTEÚDO (os primeiros kSniffBytes), se for uma imagem que
/// o app sabe catalogar; vazio se não for (imagem de disco ".raw", arquivo
/// corrompido ou renomeado). Assinaturas próprias para os RAW (o
/// shared-mime-info não reconhece todos pelo conteúdo) e, para o resto, o
/// QMimeDatabase do Qt. Thread-safe.
QString sniffImageMime(const QByteArray &head);
/// O mesmo, lendo o começo de ``path``.
QString sniffImageFile(const QString &path);

} // namespace pix::imaging

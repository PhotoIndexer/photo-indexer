// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Onde mora cada coisa. Tudo do catálogo fica na pasta da MÁQUINA,
// ~/.photo_indexer/ (igual nos três SOs, também no Windows; sobrescrevível por
// PHOTOIDX_DATA_DIR):
//
//   data.sqlite3                      o catálogo (fotos, EXIF, categorias,
//                                     álbuns, pessoas, faces, discos)
//   thumbnail/YYYY/MM/<uuid>.jpg      miniatura de cada foto (YYYY/MM = data
//                                     de criação; <uuid> = id da foto)
//   faces/YYYY/MM/<uuid>.jpg          recorte de cada rosto (<uuid> = id da face)
//   logs/                             app.log rotativo
//
// As fotos NUNCA são alteradas, movidas nem apagadas: o app só lê as pastas
// que a pessoa indexa.

#include <QDate>
#include <QDir>
#include <QString>

namespace pix {

class AppPaths
{
public:
    /// ~/.photo_indexer, ou PHOTOIDX_DATA_DIR (testes, instalação portátil).
    static QString dataDir();
    static QString database();
    static QString thumbnailsDir();
    static QString facesDir();
    static QString logsDir();

    /// thumbnail/YYYY/MM/<id>.jpg (sem data: thumbnail/undated/<id>.jpg). Não cria pastas.
    static QString thumbnailPath(const QString &photoId, const QDate &date);
    /// faces/YYYY/MM/<id>.jpg. Não cria pastas.
    static QString facePath(const QString &faceId, const QDate &date);

    /// Pasta de um modelo de IA (``clip-vit-base-patch32``, ``buffalo_sc``):
    /// PHOTOIDX_MODELS_DIR, <app>/models (instalado) ou .cache/models do
    /// repositório (desenvolvimento).
    static QString modelDir(const QString &name);
};

} // namespace pix

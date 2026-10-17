// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// EXIF sem biblioteca externa: um leitor TIFF que lista TODAS as tags (IFD0,
// IFD1, Exif, GPS, Interop) e um resumo com o que o catálogo usa (datas,
// câmera, lente, ISO, abertura, velocidade, GPS...).
//
// As tags vêm de dois lugares e chegam no mesmo formato (ExifTag):
//   - JPEG, TIFF, PNG (eXIf) e WebP (EXIF): o blob TIFF lido por readExifTags;
//   - RAW: o callback do LibRaw (imaging/Raw.cpp), que vê inclusive os IFDs
//     de CR3, RAF e companhia.
//
// "EXIF" aqui é o conteúdo do segmento APP1 (``"Exif\0\0"`` + TIFF); as
// funções aceitam também o TIFF puro.

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

namespace pix::imaging {

inline constexpr int kExifOrientation = 0x0112;
inline constexpr int kExifIfdPointer = 0x8769;
inline constexpr int kGpsIfdPointer = 0x8825;
inline constexpr int kInteropIfdPointer = 0xA005;

/// Uma tag como está no arquivo: grupo (IFD), número, tipo TIFF, quantidade e
/// os bytes do valor na ordem de bytes do arquivo (``little``).
struct ExifTag
{
    QString ifd;            ///< "IFD0", "IFD1", "Exif", "GPS", "Interop", "IFD2"...
    quint16 tag = 0;
    quint16 type = 0;
    quint32 count = 0;
    QByteArray data;        ///< valor (count × tamanho do tipo), cortado em kMaxTagBytes
    bool little = true;
};
using ExifTags = QList<ExifTag>;

/// Maior valor guardado de uma tag: MakerNote e afins passam disso e não
/// cabem num catálogo (o texto só diz o tamanho).
inline constexpr int kMaxTagBytes = 4096;

/// Bytes por elemento de cada tipo TIFF (0 = tipo desconhecido).
int exifTypeSize(int type);

/// Todas as tags de um blob ``"Exif\0\0"`` + TIFF (ou TIFF puro). Nunca lança;
/// vazio se não for TIFF.
ExifTags readExifTags(const QByteArray &exif);

/// Nome da tag ("DateTimeOriginal", "GPSLatitude"); "0x1234" se desconhecida.
QString exifTagName(const QString &ifd, quint16 tag);

/// Valor legível: texto, números (racionais como "1/200" ou decimal),
/// enumerações conhecidas (Orientation, Flash...) e "(N bytes)" para binário.
QString exifValueText(const ExifTag &tag);

/// Os dados que o catálogo guarda em colunas próprias.
struct ShotInfo
{
    int orientation = 1;        ///< 1..8 (a tag Orientation)
    /// A orientação que o FABRICANTE anotou quando a tag diz "normal": as
    /// Canon com "Girar auto" desligado gravam Orientation = 1 mesmo em pé,
    /// mas o sensor de posição fica no MakerNote (ShotInfo/AutoRotate).
    /// 0 = não há.
    int makerOrientation = 0;
    /// A orientação para EXIBIR (miniatura, visualizador, faces): a tag, ou a
    /// do fabricante quando a tag diz "normal". O picture_hash continua com
    /// a da tag (o mesmo do Pillow/photoe).
    int displayOrientation() const { return orientation != 1 || makerOrientation == 0 ? orientation : makerOrientation; }
    /// Data/hora de captura como a câmera gravou (relógio LOCAL do evento) —
    /// é por ela que a árvore de datas agrupa. Inválida se não há.
    QDateTime takenLocal;
    /// "+03:00" etc. (OffsetTimeOriginal); vazio se a câmera não gravou.
    QString takenOffset;
    /// DateTime do IFD0: a última vez que um programa gravou o arquivo (edição).
    QDateTime modifiedLocal;
    QString make;
    QString model;
    QString lens;
    QString software;
    QString artist;
    QString copyright;
    QString description;
    double exposureTime = 0.0;  ///< segundos
    double fNumber = 0.0;
    double focalLength = 0.0;   ///< mm
    int focalLength35 = 0;      ///< mm (equivalente em 35 mm)
    double exposureBias = 0.0;  ///< EV
    int iso = 0;
    int flash = -1;             ///< valor da tag Flash; -1 = ausente
    int width = 0;              ///< PixelXDimension/ImageWidth (como gravado)
    int height = 0;
    bool hasGps = false;
    double latitude = 0.0;      ///< graus, sul negativo
    double longitude = 0.0;     ///< graus, oeste negativo
    double altitude = 0.0;      ///< metros
};

ShotInfo summarize(const ExifTags &tags);

/// Orientação 1..8 que a câmera anotou no MakerNote (hoje: Canon,
/// ShotInfo[27] AutoRotate); 0 se não há. Lê o bloco ``"Exif\0\0"`` + TIFF
/// inteiro (os deslocamentos do MakerNote da Canon são relativos ao TIFF).
int makerNoteOrientation(const QByteArray &exif);

/// "2026:10:15 21:24:30" (+ subsegundos) -> data/hora local; inválida se não lê.
QDateTime parseExifDateTime(const QString &text, const QString &subsec = {});

/// "1/200" abaixo de meio segundo; "0.5s", "2s", "1.3s" a partir dele. Vazio se <= 0.
QString formatShutter(double seconds);
/// "f/2.8", "f/8", "f/11". Vazio se <= 0.
QString formatAperture(double fNumber);
/// "Canon EOS R5" — sem repetir a marca quando o modelo já começa por ela.
QString cameraName(const QString &make, const QString &model);

} // namespace pix::imaging

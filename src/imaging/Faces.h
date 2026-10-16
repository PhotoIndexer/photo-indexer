// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Detecção e biometria facial — o MESMO pipeline do ../photoe
// (services/faces.py): insightface ``buffalo_sc`` em CPU.
//
//   det_500m.onnx   SCRFD-500m: caixas + 5 pontos (olhos, nariz, boca),
//                   entrada 640x640, limiar de detecção 0,35, NMS 0,4
//   w600k_mbf.onnx  ArcFace MobileFaceNet: rosto alinhado 112x112 -> 512 dims
//
// Rostos menores que 40 px (lado maior da caixa) são descartados, como no
// photoe: a plateia ao fundo vira ruído. Duas faces são a mesma pessoa a
// partir de cosseno 0,45 (``FACE_MATCH_SIMILARITY`` do photoe).
//
// ATENÇÃO: os modelos pré-treinados do insightface são liberados só para uso
// não comercial (pesquisa).
//
// Thread-safe, como o ClipModel.

#include "imaging/Embedding.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <opencv2/core.hpp>

#include <memory>

namespace pix::imaging {

inline constexpr float kFaceDetThreshold = 0.35f;
inline constexpr float kFaceNmsThreshold = 0.4f;
inline constexpr float kFaceMatchSimilarity = 0.45f;
inline constexpr int kFaceMinSize = 40;
inline constexpr int kFaceDetSize = 640;
/// Lado maior da imagem em que se procura rosto (o 1080p do photoe).
inline constexpr int kFaceImageSide = 1920;

struct DetectedFace
{
    cv::Rect2f box;             ///< em pixels da imagem passada
    cv::Point2f landmarks[5];   ///< olho esq., olho dir., nariz, boca esq., boca dir.
    float score = 0.0f;         ///< confiança da detecção
    Embedding embedding;        ///< ArcFace normalizado (512)
    QByteArray crop;            ///< JPEG do rosto com folga (galeria de pessoas)
};

class FaceEngine
{
public:
    static constexpr const char *kName = "buffalo_sc";

    explicit FaceEngine(const QString &modelDir);
    ~FaceEngine();

    bool available() const;
    bool load();
    QString error() const;

    /// Todos os rostos da imagem RGB, maior confiança primeiro. ``minSize``
    /// < 0 = kFaceMinSize. ``withCrops``: gera o JPEG de cada rosto.
    QList<DetectedFace> detect(const cv::Mat &rgb, int minSize = -1, bool withCrops = true);

    /// Só a detecção (sem biometria): caixas e pontos, como o SCRFD.detect.
    QList<DetectedFace> detectBoxes(const cv::Mat &rgb, float threshold = kFaceDetThreshold);

    /// Biometria de um rosto já detectado (alinha pelos 5 pontos).
    Embedding embed(const cv::Mat &rgb, const cv::Point2f landmarks[5]);

    /// O ``norm_crop`` do insightface: rosto alinhado 112x112.
    static cv::Mat alignFace(const cv::Mat &rgb, const cv::Point2f landmarks[5]);
    /// Recorte JPEG com folga (o ``_crop_face`` do photoe: 35%, 220 px).
    static QByteArray cropJpeg(const cv::Mat &rgb, const cv::Rect2f &box);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace pix::imaging

// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// CLIP ViT-B/32 (OpenAI) em ONNX: imagem e texto no MESMO espaço de 512
// dimensões. É ele que categoriza (a foto contra o texto de cada categoria:
// "a photo of a beach") e que faz a busca por IA ("praia ao pôr do sol" ->
// as fotos mais parecidas). O texto é em INGLÊS: o modelo só conhece inglês.
//
// O modelo (~150 MB) vai no pacote: build.py model -> .cache/models/
// clip-vit-base-patch32 (vision_model_quantized.onnx, text_model_quantized.onnx,
// vocab.json, merges.txt). Sem ele, available() é falso e o app segue sem IA.
//
// Thread-safe: as sessões carregam uma vez (na primeira chamada) e o Run do
// ONNX Runtime aceita chamadas simultâneas.

#include "imaging/Embedding.h"

#include <QHash>
#include <QString>

#include <opencv2/core.hpp>

#include <memory>
#include <mutex>

namespace pix::imaging {

/// Tokenizer BPE do CLIP (o CLIPTokenizer do Hugging Face).
class ClipTokenizer
{
public:
    bool load(const QString &vocabPath, const QString &mergesPath);
    bool isLoaded() const { return !m_vocab.isEmpty(); }

    /// [<|startoftext|>, ..., <|endoftext|>], no máximo 77 ids.
    std::vector<qint64> encode(const QString &text) const;

    static constexpr qint64 kStartToken = 49406;
    static constexpr qint64 kEndToken = 49407;
    static constexpr int kContextLength = 77;

private:
    QStringList bpe(const QString &token) const;

    QHash<QString, qint64> m_vocab;
    QHash<QString, int> m_ranks;           ///< "a b" -> prioridade
    QHash<uchar, QChar> m_byteToChar;
    mutable QHash<QString, QStringList> m_cache;
    mutable std::mutex m_cacheLock;
};

class ClipModel
{
public:
    static constexpr const char *kName = "clip-vit-base-patch32";
    static constexpr int kInputSize = 224;
    static constexpr int kDimensions = 512;

    explicit ClipModel(const QString &modelDir);
    ~ClipModel();

    /// Os arquivos do modelo estão lá (não carrega nada).
    bool available() const;
    /// Carrega as sessões (uma vez). Falso se o modelo não abre.
    bool load();
    QString error() const;

    /// Embedding normalizado da imagem RGB; vazio se o modelo não carregou.
    Embedding encodeImage(const cv::Mat &rgb);
    /// Embedding normalizado do texto; vazio se o modelo não carregou.
    Embedding encodeText(const QString &text);

    /// O pré-processamento do CLIPImageProcessor: lado menor -> 224
    /// (bicúbico), recorte central 224x224, normalizado por canal (CHW).
    static std::vector<float> preprocess(const cv::Mat &rgb);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace pix::imaging

// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Clip.h"

#include "imaging/Onnx.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <opencv2/imgproc.hpp>

#include <climits>

namespace pix::imaging {

namespace {

Q_LOGGING_CATEGORY(lcClip, "photoidx.imaging.clip")

const char *const kVisionFile = "vision_model_quantized.onnx";
const char *const kTextFile = "text_model_quantized.onnx";
const char *const kVocabFile = "vocab.json";
const char *const kMergesFile = "merges.txt";

const float kMean[3] = {0.48145466f, 0.4578275f, 0.40821073f};
const float kStd[3] = {0.26862954f, 0.26130258f, 0.27577711f};

/// bytes_to_unicode() do GPT-2/CLIP: cada byte vira um caractere imprimível.
QHash<uchar, QChar> bytesToUnicode()
{
    QList<int> bs;
    for (int b = '!'; b <= '~'; ++b)
        bs << b;
    for (int b = 0xA1; b <= 0xAC; ++b)
        bs << b;
    for (int b = 0xAE; b <= 0xFF; ++b)
        bs << b;
    QList<int> cs = bs;
    int n = 0;
    for (int b = 0; b < 256; ++b) {
        if (!bs.contains(b)) {
            bs << b;
            cs << 256 + n;
            ++n;
        }
    }
    QHash<uchar, QChar> map;
    for (qsizetype i = 0; i < bs.size(); ++i)
        map.insert(uchar(bs[i]), QChar(cs[i]));
    return map;
}

} // namespace

// --------------------------------------------------------------------------- //
// Tokenizer
// --------------------------------------------------------------------------- //

bool ClipTokenizer::load(const QString &vocabPath, const QString &mergesPath)
{
    QFile vocabFile(vocabPath);
    QFile mergesFile(mergesPath);
    if (!vocabFile.open(QIODevice::ReadOnly) || !mergesFile.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject vocab = QJsonDocument::fromJson(vocabFile.readAll()).object();
    if (vocab.isEmpty())
        return false;
    m_vocab.clear();
    m_vocab.reserve(vocab.size());
    for (auto it = vocab.begin(); it != vocab.end(); ++it)
        m_vocab.insert(it.key(), qint64(it.value().toInteger()));
    // merges.txt: "#version" e depois os pares, na ordem de prioridade; o CLIP
    // usa os primeiros 49152 - 256 - 2.
    const QStringList lines = QString::fromUtf8(mergesFile.readAll()).split(u'\n');
    m_ranks.clear();
    constexpr int kMerges = 49152 - 256 - 2;
    int rank = 0;
    for (qsizetype i = 1; i < lines.size() && rank < kMerges; ++i) {
        const QString line = lines[i].trimmed();
        if (line.isEmpty())
            continue;
        m_ranks.insert(line, rank++);
    }
    m_byteToChar = bytesToUnicode();
    return true;
}

QStringList ClipTokenizer::bpe(const QString &token) const
{
    {
        std::lock_guard lock(m_cacheLock);
        const auto it = m_cache.constFind(token);
        if (it != m_cache.constEnd())
            return *it;
    }
    // Caracteres (code points) com "</w>" no último.
    QStringList word;
    const QList<uint> ucs = token.toUcs4();
    for (qsizetype i = 0; i < ucs.size(); ++i) {
        const char32_t cp = ucs[i];
        QString ch = QString::fromUcs4(&cp, 1);
        if (i == ucs.size() - 1)
            ch += QStringLiteral("</w>");
        word << ch;
    }
    while (word.size() > 1) {
        int bestRank = INT_MAX;
        qsizetype bestAt = -1;
        for (qsizetype i = 0; i + 1 < word.size(); ++i) {
            const auto it = m_ranks.constFind(word[i] + u' ' + word[i + 1]);
            if (it != m_ranks.constEnd() && *it < bestRank) {
                bestRank = *it;
                bestAt = i;
            }
        }
        if (bestAt < 0)
            break;
        // Junta TODAS as ocorrências do par escolhido, da esquerda para a direita.
        const QString first = word[bestAt], second = word[bestAt + 1];
        QStringList merged;
        for (qsizetype i = 0; i < word.size();) {
            if (i + 1 < word.size() && word[i] == first && word[i + 1] == second) {
                merged << first + second;
                i += 2;
            } else {
                merged << word[i];
                ++i;
            }
        }
        word = merged;
    }
    std::lock_guard lock(m_cacheLock);
    m_cache.insert(token, word);
    return word;
}

std::vector<qint64> ClipTokenizer::encode(const QString &text) const
{
    static const QRegularExpression kPattern(
        QStringLiteral("<\\|startoftext\\|>|<\\|endoftext\\|>|'s|'t|'re|'ve|'m|'ll|'d|[\\p{L}]+|[\\p{N}]|[^\\s\\p{L}\\p{N}]+"),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption);
    std::vector<qint64> ids{kStartToken};
    const QString clean = text.simplified().toLower();
    auto it = kPattern.globalMatch(clean);
    while (it.hasNext() && int(ids.size()) < kContextLength - 1) {
        const QString piece = it.next().captured(0);
        QString mapped;
        for (char b : piece.toUtf8())
            mapped += m_byteToChar.value(uchar(b));
        for (const QString &sub : bpe(mapped)) {
            const auto v = m_vocab.constFind(sub);
            if (v != m_vocab.constEnd())
                ids.push_back(*v);
            if (int(ids.size()) >= kContextLength - 1)
                break;
        }
    }
    ids.push_back(kEndToken);
    return ids;
}

// --------------------------------------------------------------------------- //
// Modelo
// --------------------------------------------------------------------------- //

struct ClipModel::Impl
{
    QString dir;
    std::mutex lock;
    bool tried = false;
    QString error;
    ClipTokenizer tokenizer;
#ifdef PHOTOIDX_HAVE_ONNX
    std::unique_ptr<Ort::Session> vision;
    std::unique_ptr<Ort::Session> text;
    std::vector<std::string> visionIn, visionOut, textIn, textOut;
    int textLength = 0;  ///< comprimento fixo da entrada de texto (0 = dinâmico)
#endif
};

ClipModel::ClipModel(const QString &modelDir)
    : d(std::make_unique<Impl>())
{
    d->dir = modelDir;
}

ClipModel::~ClipModel() = default;

bool ClipModel::available() const
{
#ifdef PHOTOIDX_HAVE_ONNX
    const QDir dir(d->dir);
    for (const char *f : {kVisionFile, kTextFile, kVocabFile, kMergesFile})
        if (!QFileInfo::exists(dir.filePath(QLatin1String(f))))
            return false;
    return true;
#else
    return false;
#endif
}

QString ClipModel::error() const
{
    return d->error;
}

bool ClipModel::load()
{
    std::lock_guard guard(d->lock);
#ifdef PHOTOIDX_HAVE_ONNX
    if (d->vision)
        return true;
    if (d->tried)
        return false;
    d->tried = true;
    if (!available()) {
        d->error = QStringLiteral("model files not found in %1").arg(d->dir);
        return false;
    }
    const QDir dir(d->dir);
    try {
        if (!d->tokenizer.load(dir.filePath(QLatin1String(kVocabFile)), dir.filePath(QLatin1String(kMergesFile))))
            throw std::runtime_error("tokenizer files unreadable");
        d->vision = onnx::open(dir.filePath(QLatin1String(kVisionFile)), 1);
        d->text = onnx::open(dir.filePath(QLatin1String(kTextFile)), 1);
        d->visionIn = onnx::inputNames(*d->vision);
        d->visionOut = onnx::outputNames(*d->vision);
        d->textIn = onnx::inputNames(*d->text);
        d->textOut = onnx::outputNames(*d->text);
        const auto shape = d->text->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        d->textLength = shape.size() == 2 && shape[1] > 0 ? int(shape[1]) : 0;
        qCInfo(lcClip) << "CLIP loaded from" << d->dir;
        return true;
    } catch (const std::exception &e) {
        d->error = QString::fromUtf8(e.what());
        qCWarning(lcClip) << "CLIP failed to load:" << e.what();
        d->vision.reset();
        d->text.reset();
        return false;
    }
#else
    d->error = QStringLiteral("built without ONNX Runtime");
    return false;
#endif
}

std::vector<float> ClipModel::preprocess(const cv::Mat &rgb)
{
    const double scale = double(kInputSize) / std::min(rgb.cols, rgb.rows);
    const int w = std::max(kInputSize, int(std::lround(rgb.cols * scale)));
    const int h = std::max(kInputSize, int(std::lround(rgb.rows * scale)));
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(w, h), 0, 0, cv::INTER_CUBIC);
    const int x = (w - kInputSize) / 2, y = (h - kInputSize) / 2;
    const cv::Mat crop = resized(cv::Rect(x, y, kInputSize, kInputSize));
    std::vector<float> chw(size_t(3) * kInputSize * kInputSize);
    const size_t plane = size_t(kInputSize) * kInputSize;
    for (int r = 0; r < kInputSize; ++r) {
        const auto *row = crop.ptr<cv::Vec3b>(r);
        for (int c = 0; c < kInputSize; ++c)
            for (int k = 0; k < 3; ++k)
                chw[k * plane + size_t(r) * kInputSize + c] = (row[c][k] / 255.0f - kMean[k]) / kStd[k];
    }
    return chw;
}

Embedding ClipModel::encodeImage(const cv::Mat &rgb)
{
#ifdef PHOTOIDX_HAVE_ONNX
    if (rgb.empty() || rgb.type() != CV_8UC3 || !load())
        return {};
    std::vector<float> input = preprocess(rgb);
    const std::array<int64_t, 4> shape{1, 3, kInputSize, kInputSize};
    const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), shape.data(), shape.size());
    try {
        const auto inNames = onnx::cstrs(d->visionIn);
        const auto outNames = onnx::cstrs(d->visionOut);
        auto outs = d->vision->Run(Ort::RunOptions{nullptr}, inNames.data(), &tensor, 1, outNames.data(), 1);
        const float *data = outs[0].GetTensorData<float>();
        const size_t n = outs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        Embedding v(data, data + n);
        normalize(v);
        return v;
    } catch (const Ort::Exception &e) {
        qCWarning(lcClip) << "CLIP image failed:" << e.what();
        return {};
    }
#else
    Q_UNUSED(rgb);
    return {};
#endif
}

Embedding ClipModel::encodeText(const QString &text)
{
#ifdef PHOTOIDX_HAVE_ONNX
    if (!load())
        return {};
    std::vector<int64_t> ids;
    for (qint64 id : d->tokenizer.encode(text))
        ids.push_back(int64_t(id));
    if (d->textLength > 0)
        ids.resize(size_t(d->textLength), ClipTokenizer::kEndToken);
    const std::array<int64_t, 2> shape{1, int64_t(ids.size())};
    const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<int64_t>(mem, ids.data(), ids.size(), shape.data(), shape.size());
    try {
        const auto inNames = onnx::cstrs(d->textIn);
        const auto outNames = onnx::cstrs(d->textOut);
        auto outs = d->text->Run(Ort::RunOptions{nullptr}, inNames.data(), &tensor, 1, outNames.data(), 1);
        const float *data = outs[0].GetTensorData<float>();
        const size_t n = outs[0].GetTensorTypeAndShapeInfo().GetElementCount();
        Embedding v(data, data + n);
        normalize(v);
        return v;
    } catch (const Ort::Exception &e) {
        qCWarning(lcClip) << "CLIP text failed:" << e.what();
        return {};
    }
#else
    Q_UNUSED(text);
    return {};
#endif
}

} // namespace pix::imaging

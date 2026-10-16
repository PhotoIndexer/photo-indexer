// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Faces.h"

#include "imaging/ImageIO.h"
#include "imaging/Onnx.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <mutex>
#include <numeric>

namespace pix::imaging {

namespace {

Q_LOGGING_CATEGORY(lcFaces, "photoidx.imaging.faces")

const char *const kDetFile = "det_500m.onnx";
const char *const kRecFile = "w600k_mbf.onnx";

/// Gabarito ArcFace 112x112 (insightface/utils/face_align.py).
const float kArcface[5][2] = {
    {38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f}, {41.5493f, 92.3655f}, {70.7299f, 92.2041f},
};

const int kStrides[3] = {8, 16, 32};
constexpr int kAnchors = 2;

/// SimilarityTransform.estimate do scikit-image (Umeyama, sem reflexão
/// permitida além do que o determinante pede): matriz 2x3 de ``src`` -> ``dst``.
cv::Mat umeyama(const cv::Point2f src[5], const float dst[5][2])
{
    constexpr int n = 5;
    double srcMean[2] = {0, 0}, dstMean[2] = {0, 0};
    for (int i = 0; i < n; ++i) {
        srcMean[0] += src[i].x / n;
        srcMean[1] += src[i].y / n;
        dstMean[0] += dst[i][0] / n;
        dstMean[1] += dst[i][1] / n;
    }
    cv::Matx22d A = cv::Matx22d::zeros();
    double srcVar = 0.0;
    for (int i = 0; i < n; ++i) {
        const double sx = src[i].x - srcMean[0], sy = src[i].y - srcMean[1];
        const double dx = dst[i][0] - dstMean[0], dy = dst[i][1] - dstMean[1];
        A(0, 0) += dx * sx / n;
        A(0, 1) += dx * sy / n;
        A(1, 0) += dy * sx / n;
        A(1, 1) += dy * sy / n;
        srcVar += (sx * sx + sy * sy) / n;
    }
    cv::Mat U, S, Vt;
    cv::SVD::compute(cv::Mat(A), S, U, Vt);
    cv::Matx22d d = cv::Matx22d::eye();
    if (cv::determinant(cv::Mat(A)) < 0)
        d(1, 1) = -1;
    cv::Matx22d R;
    const double rank = (S.at<double>(0) > 1e-12 ? 1 : 0) + (S.at<double>(1) > 1e-12 ? 1 : 0);
    const cv::Matx22d u(U), vt(Vt);
    if (rank == 1 && cv::determinant(cv::Mat(A)) == 0) {
        if (cv::determinant(cv::Mat(u)) * cv::determinant(cv::Mat(vt)) > 0)
            R = u * vt;
        else {
            cv::Matx22d dd = cv::Matx22d::eye();
            dd(1, 1) = -1;
            R = u * dd * vt;
        }
    } else {
        R = u * d * vt;
    }
    const double scale = srcVar > 0 ? (S.at<double>(0) * d(0, 0) + S.at<double>(1) * d(1, 1)) / srcVar : 1.0;
    cv::Mat M(2, 3, CV_64F);
    for (int r = 0; r < 2; ++r) {
        M.at<double>(r, 0) = scale * R(r, 0);
        M.at<double>(r, 1) = scale * R(r, 1);
        M.at<double>(r, 2) = (r == 0 ? dstMean[0] : dstMean[1])
                             - scale * (R(r, 0) * srcMean[0] + R(r, 1) * srcMean[1]);
    }
    return M;
}

struct Candidate
{
    float x1, y1, x2, y2, score;
    cv::Point2f kps[5];
};

/// NMS do SCRFD (áreas com +1, como o código do insightface).
std::vector<int> nms(const std::vector<Candidate> &dets, float threshold)
{
    std::vector<int> order(dets.size());
    std::iota(order.begin(), order.end(), 0);
    std::vector<int> keep;
    std::vector<bool> removed(dets.size(), false);
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const int i = order[oi];
        if (removed[i])
            continue;
        keep.push_back(i);
        const Candidate &a = dets[i];
        const float areaA = (a.x2 - a.x1 + 1) * (a.y2 - a.y1 + 1);
        for (size_t oj = oi + 1; oj < order.size(); ++oj) {
            const int j = order[oj];
            if (removed[j])
                continue;
            const Candidate &b = dets[j];
            const float xx1 = std::max(a.x1, b.x1), yy1 = std::max(a.y1, b.y1);
            const float xx2 = std::min(a.x2, b.x2), yy2 = std::min(a.y2, b.y2);
            const float w = std::max(0.0f, xx2 - xx1 + 1), h = std::max(0.0f, yy2 - yy1 + 1);
            const float inter = w * h;
            const float areaB = (b.x2 - b.x1 + 1) * (b.y2 - b.y1 + 1);
            if (inter / (areaA + areaB - inter) > threshold)
                removed[j] = true;
        }
    }
    return keep;
}

} // namespace

struct FaceEngine::Impl
{
    QString dir;
    std::mutex lock;
    bool tried = false;
    QString error;
#ifdef PHOTOIDX_HAVE_ONNX
    std::unique_ptr<Ort::Session> det;
    std::unique_ptr<Ort::Session> rec;
    std::vector<std::string> detIn, detOut, recIn, recOut;
#endif
};

FaceEngine::FaceEngine(const QString &modelDir)
    : d(std::make_unique<Impl>())
{
    d->dir = modelDir;
}

FaceEngine::~FaceEngine() = default;

bool FaceEngine::available() const
{
#ifdef PHOTOIDX_HAVE_ONNX
    const QDir dir(d->dir);
    return QFileInfo::exists(dir.filePath(QLatin1String(kDetFile))) && QFileInfo::exists(dir.filePath(QLatin1String(kRecFile)));
#else
    return false;
#endif
}

QString FaceEngine::error() const
{
    return d->error;
}

bool FaceEngine::load()
{
    std::lock_guard guard(d->lock);
#ifdef PHOTOIDX_HAVE_ONNX
    if (d->det)
        return true;
    if (d->tried)
        return false;
    d->tried = true;
    if (!available()) {
        d->error = QStringLiteral("model files not found in %1").arg(d->dir);
        return false;
    }
    try {
        const QDir dir(d->dir);
        d->det = onnx::open(dir.filePath(QLatin1String(kDetFile)), 2);
        d->rec = onnx::open(dir.filePath(QLatin1String(kRecFile)), 1);
        d->detIn = onnx::inputNames(*d->det);
        d->detOut = onnx::outputNames(*d->det);
        d->recIn = onnx::inputNames(*d->rec);
        d->recOut = onnx::outputNames(*d->rec);
        if (d->detOut.size() < 9)
            throw std::runtime_error("unexpected detector outputs (need scores, boxes and keypoints)");
        qCInfo(lcFaces) << "Face models loaded from" << d->dir;
        return true;
    } catch (const std::exception &e) {
        d->error = QString::fromUtf8(e.what());
        qCWarning(lcFaces) << "Face models failed to load:" << e.what();
        d->det.reset();
        d->rec.reset();
        return false;
    }
#else
    d->error = QStringLiteral("built without ONNX Runtime");
    return false;
#endif
}

QList<DetectedFace> FaceEngine::detectBoxes(const cv::Mat &rgb, float threshold)
{
    QList<DetectedFace> out;
#ifdef PHOTOIDX_HAVE_ONNX
    if (rgb.empty() || rgb.type() != CV_8UC3 || !load())
        return out;
    // Redimensiona mantendo a proporção num quadro 640x640 (canto superior
    // esquerdo, resto preto) — SCRFD.detect.
    const double imRatio = double(rgb.rows) / rgb.cols;
    int newW, newH;
    if (imRatio > 1.0) {
        newH = kFaceDetSize;
        newW = int(newH / imRatio);
    } else {
        newW = kFaceDetSize;
        newH = int(newW * imRatio);
    }
    const float detScale = float(newH) / rgb.rows;
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(newW, newH));
    cv::Mat canvas = cv::Mat::zeros(kFaceDetSize, kFaceDetSize, CV_8UC3);
    resized.copyTo(canvas(cv::Rect(0, 0, newW, newH)));

    // blobFromImage(1/128, mean 127.5, swapRB): a imagem já é RGB.
    const size_t plane = size_t(kFaceDetSize) * kFaceDetSize;
    std::vector<float> blob(3 * plane);
    for (int y = 0; y < kFaceDetSize; ++y) {
        const auto *row = canvas.ptr<cv::Vec3b>(y);
        for (int x = 0; x < kFaceDetSize; ++x)
            for (int c = 0; c < 3; ++c)
                blob[c * plane + size_t(y) * kFaceDetSize + x] = (row[x][c] - 127.5f) / 128.0f;
    }
    const std::array<int64_t, 4> shape{1, 3, kFaceDetSize, kFaceDetSize};
    const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, blob.data(), blob.size(), shape.data(), shape.size());
    std::vector<Ort::Value> outs;
    try {
        const auto inNames = onnx::cstrs(d->detIn);
        const auto outNames = onnx::cstrs(d->detOut);
        outs = d->det->Run(Ort::RunOptions{nullptr}, inNames.data(), &tensor, 1, outNames.data(), outNames.size());
    } catch (const Ort::Exception &e) {
        qCWarning(lcFaces) << "Face detection failed:" << e.what();
        return out;
    }

    // Saídas na ordem do modelo: scores (3 escalas), caixas (3), pontos (3).
    std::vector<Candidate> dets;
    for (int idx = 0; idx < 3; ++idx) {
        const int stride = kStrides[idx];
        const float *scores = outs[idx].GetTensorData<float>();
        const float *boxes = outs[idx + 3].GetTensorData<float>();
        const float *kps = outs[idx + 6].GetTensorData<float>();
        const int h = kFaceDetSize / stride, w = kFaceDetSize / stride;
        const int count = h * w * kAnchors;
        for (int i = 0; i < count; ++i) {
            if (scores[i] < threshold)
                continue;
            const int cell = i / kAnchors;
            const float cx = float((cell % w) * stride), cy = float((cell / w) * stride);
            Candidate c;
            c.score = scores[i];
            c.x1 = (cx - boxes[i * 4 + 0] * stride) / detScale;
            c.y1 = (cy - boxes[i * 4 + 1] * stride) / detScale;
            c.x2 = (cx + boxes[i * 4 + 2] * stride) / detScale;
            c.y2 = (cy + boxes[i * 4 + 3] * stride) / detScale;
            for (int k = 0; k < 5; ++k)
                c.kps[k] = cv::Point2f((cx + kps[i * 10 + 2 * k] * stride) / detScale,
                                       (cy + kps[i * 10 + 2 * k + 1] * stride) / detScale);
            dets.push_back(c);
        }
    }
    std::stable_sort(dets.begin(), dets.end(), [](const Candidate &a, const Candidate &b) { return a.score > b.score; });
    for (int i : nms(dets, kFaceNmsThreshold)) {
        const Candidate &c = dets[i];
        DetectedFace f;
        f.box = cv::Rect2f(c.x1, c.y1, c.x2 - c.x1, c.y2 - c.y1);
        f.score = c.score;
        for (int k = 0; k < 5; ++k)
            f.landmarks[k] = c.kps[k];
        out.append(f);
    }
#else
    Q_UNUSED(rgb);
    Q_UNUSED(threshold);
#endif
    return out;
}

cv::Mat FaceEngine::alignFace(const cv::Mat &rgb, const cv::Point2f landmarks[5])
{
    const cv::Mat M = umeyama(landmarks, kArcface);
    cv::Mat aligned;
    cv::warpAffine(rgb, aligned, M, cv::Size(112, 112), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    return aligned;
}

Embedding FaceEngine::embed(const cv::Mat &rgb, const cv::Point2f landmarks[5])
{
#ifdef PHOTOIDX_HAVE_ONNX
    if (!load())
        return {};
    const cv::Mat aligned = alignFace(rgb, landmarks);
    constexpr size_t plane = 112 * 112;
    std::vector<float> blob(3 * plane);
    for (int y = 0; y < 112; ++y) {
        const auto *row = aligned.ptr<cv::Vec3b>(y);
        for (int x = 0; x < 112; ++x)
            for (int c = 0; c < 3; ++c)
                blob[c * plane + size_t(y) * 112 + x] = (row[x][c] - 127.5f) / 127.5f;
    }
    const std::array<int64_t, 4> shape{1, 3, 112, 112};
    const Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, blob.data(), blob.size(), shape.data(), shape.size());
    try {
        const auto inNames = onnx::cstrs(d->recIn);
        const auto outNames = onnx::cstrs(d->recOut);
        auto outs = d->rec->Run(Ort::RunOptions{nullptr}, inNames.data(), &tensor, 1, outNames.data(), 1);
        const float *data = outs[0].GetTensorData<float>();
        Embedding v(data, data + outs[0].GetTensorTypeAndShapeInfo().GetElementCount());
        normalize(v);
        return v;
    } catch (const Ort::Exception &e) {
        qCWarning(lcFaces) << "Face embedding failed:" << e.what();
        return {};
    }
#else
    Q_UNUSED(rgb);
    Q_UNUSED(landmarks);
    return {};
#endif
}

QByteArray FaceEngine::cropJpeg(const cv::Mat &rgb, const cv::Rect2f &box)
{
    const float pad = 0.35f;
    const int px = int(box.width * pad), py = int(box.height * pad);
    const int x1 = std::max(0, int(box.x) - px), y1 = std::max(0, int(box.y) - py);
    const int x2 = std::min(rgb.cols, int(box.x + box.width) + px);
    const int y2 = std::min(rgb.rows, int(box.y + box.height) + py);
    if (x2 <= x1 || y2 <= y1)
        return {};
    return encodeJpeg(fitTo(rgb(cv::Rect(x1, y1, x2 - x1, y2 - y1)), 220), 85);
}

QList<DetectedFace> FaceEngine::detect(const cv::Mat &rgb, int minSize, bool withCrops)
{
    QList<DetectedFace> faces;
    const int limit = minSize < 0 ? kFaceMinSize : minSize;
    for (DetectedFace &f : detectBoxes(rgb)) {
        if (std::max(f.box.width, f.box.height) < limit)
            continue;
        f.embedding = embed(rgb, f.landmarks);
        if (f.embedding.empty())
            continue;
        if (withCrops)
            f.crop = cropJpeg(rgb, f.box);
        faces.append(f);
    }
    std::stable_sort(faces.begin(), faces.end(), [](const DetectedFace &a, const DetectedFace &b) { return a.score > b.score; });
    return faces;
}

} // namespace pix::imaging

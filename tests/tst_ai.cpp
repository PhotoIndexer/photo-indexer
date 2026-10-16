// SPDX-License-Identifier: BSD-2-Clause
// IA: o tokenizer e o zero-shot do CLIP e o reconhecimento facial, comparado
// com o insightface (o mesmo do ../photoe) em tests/data/faces/expected.json.
// Precisa dos modelos (make model -> .cache/models); sem eles, pula.

#include "imaging/Clip.h"
#include "imaging/Faces.h"
#include "imaging/ImageIO.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace pix::imaging;

namespace {

QString data(const QString &name)
{
    return QStringLiteral(PHOTOIDX_TEST_DATA "/") + name;
}

QString modelDir(const char *name)
{
    return QDir(QStringLiteral(PHOTOIDX_MODELS_DIR)).filePath(QString::fromLatin1(name));
}

cv::Mat sample()
{
    QFile f(data(QStringLiteral("sample.jpg")));
    return f.open(QIODevice::ReadOnly) ? decodeImage(f.readAll()) : cv::Mat();
}

float iou(const cv::Rect2f &a, const cv::Rect2f &b)
{
    const float inter = (a & b).area();
    return inter / (a.area() + b.area() - inter);
}

} // namespace

class TestAi : public QObject
{
    Q_OBJECT

private slots:
    void tokenizerMatchesHuggingFace()
    {
        const QDir dir(modelDir(ClipModel::kName));
        ClipTokenizer tok;
        if (!tok.load(dir.filePath(QStringLiteral("vocab.json")), dir.filePath(QStringLiteral("merges.txt"))))
            QSKIP("CLIP model not downloaded (make model)");
        // CLIPTokenizer("a photo of a beach") do transformers.
        const std::vector<qint64> ids = tok.encode(QStringLiteral("a photo of a beach"));
        const std::vector<qint64> want{49406, 320, 1125, 539, 320, 2117, 49407};
        QCOMPARE(ids, want);
        QCOMPARE(tok.encode(QStringLiteral("  A   PHOTO of a Beach ")), want);
        QVERIFY(tok.encode(QString(500, u'x') + QStringLiteral(" y")).size() <= 77);
    }

    void zeroShot()
    {
        ClipModel clip(modelDir(ClipModel::kName));
        if (!clip.available())
            QSKIP("CLIP model not downloaded (make model)");
        const Embedding image = clip.encodeImage(sample());
        QCOMPARE(int(image.size()), ClipModel::kDimensions);
        // O mesmo resultado do onnxruntime em Python: a foto (um catálogo de
        // retratos) é "a portrait of a person".
        const QStringList labels{QStringLiteral("a portrait of a person"), QStringLiteral("a dog"), QStringLiteral("a beach"),
                                 QStringLiteral("a mountain"), QStringLiteral("food")};
        QString best;
        float bestSim = -1;
        for (const QString &l : labels) {
            const float s = dot(image, clip.encodeText(QStringLiteral("a photo of ") + l));
            if (s > bestSim) {
                bestSim = s;
                best = l;
            }
        }
        QCOMPARE(best, labels.first());
    }

    void facesMatchInsightface()
    {
        FaceEngine engine(modelDir(FaceEngine::kName));
        if (!engine.available())
            QSKIP("Face model not downloaded (make model)");
        QFile f(data(QStringLiteral("faces/expected.json")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonArray want = QJsonDocument::fromJson(f.readAll()).object()
                                    .value(QStringLiteral("faces")).toObject().value(QStringLiteral("sample.jpg")).toArray();
        const QList<DetectedFace> got = engine.detect(sample(), 0, false);
        QCOMPARE(got.size(), want.size());
        for (const QJsonValue &v : want) {
            const QJsonObject o = v.toObject();
            const QJsonArray b = o.value(QStringLiteral("bbox")).toArray();
            const cv::Rect2f box(float(b[0].toDouble()), float(b[1].toDouble()), float(b[2].toDouble() - b[0].toDouble()),
                                 float(b[3].toDouble() - b[1].toDouble()));
            const DetectedFace *match = nullptr;
            for (const DetectedFace &d : got)
                if (iou(d.box, box) > 0.9f)
                    match = &d;
            QVERIFY2(match, "face not found by the C++ detector");
            QVERIFY(std::abs(match->score - float(o.value(QStringLiteral("det_score")).toDouble())) < 0.02f);
            Embedding ref;
            for (const QJsonValue &x : o.value(QStringLiteral("embedding")).toArray())
                ref.push_back(float(x.toDouble()));
            const float cosine = dot(match->embedding, ref);
            QVERIFY2(cosine > 0.98f, qPrintable(QStringLiteral("cosine %1").arg(cosine)));
        }
    }
};

QTEST_GUILESS_MAIN(TestAi)
#include "tst_ai.moc"

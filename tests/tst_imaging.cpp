// SPDX-License-Identifier: BSD-2-Clause
// Leitura de imagens: formatos, EXIF, hashes. O picture_hash é comparado com o
// do ../photoe (Pillow), em tests/data/faces/expected.json.

#include "imaging/Exif.h"
#include "imaging/Formats.h"
#include "imaging/Hashes.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include <opencv2/imgproc.hpp>

using namespace pix::imaging;

namespace {

QString data(const QString &name)
{
    return QStringLiteral(PHOTOIDX_TEST_DATA "/") + name;
}

QJsonObject expected()
{
    QFile f(data(QStringLiteral("faces/expected.json")));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

} // namespace

class TestImaging : public QObject
{
    Q_OBJECT

private slots:
    void formats()
    {
        QVERIFY(isImageFile(QStringLiteral("/a/b/IMG_0001.JPG")));
        QVERIFY(isImageFile(QStringLiteral("C:\\x\\photo.jpeg")));
        QVERIFY(isImageFile(QStringLiteral("/x/DSC_1234.NEF")));
        QVERIFY(isRawFile(QStringLiteral("/x/IMG_1.cr3")));
        QVERIFY(isRawFile(QStringLiteral("/x/a.arw")));
        QVERIFY(!isRawFile(QStringLiteral("/x/a.jpg")));
        QVERIFY(!isImageFile(QStringLiteral("/x/readme.txt")));
        QVERIFY(!isImageFile(QStringLiteral("/x.jpg/noext")));
        QCOMPARE(formatLabel(QStringLiteral("jpeg")), QStringLiteral("JPEG"));
        QCOMPARE(formatLabel(QStringLiteral("nef")), QStringLiteral("NEF"));
    }

    void orientationFromExif_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<int>("orientation");
        QTest::newRow("o1") << "exif/o1.jpg" << 1;
        QTest::newRow("o3") << "exif/o3.jpg" << 3;
        QTest::newRow("o6") << "exif/o6.jpg" << 6;
        QTest::newRow("o8") << "exif/o8.jpg" << 8;
        QTest::newRow("ii_o8") << "exif/ii_o8.jpg" << 8;
        QTest::newRow("none") << "exif/no_exif.jpg" << 1;
    }

    void orientationFromExif()
    {
        QFETCH(QString, file);
        QFETCH(int, orientation);
        const Inspection in = inspect(data(file));
        QVERIFY2(in.ok, qPrintable(in.error));
        QCOMPARE(in.shot.orientation, orientation);
        QVERIFY(!in.image.empty());
        // As dimensões informadas são as da imagem já orientada.
        QCOMPARE(in.width > in.height, in.image.cols > in.image.rows);
    }

    void allTagsAndShotInfo()
    {
        const Inspection in = inspect(data(QStringLiteral("exif/shooting.jpg")));
        QVERIFY(in.ok);
        QVERIFY(in.tags.size() >= 5);
        QVERIFY(!in.shot.make.isEmpty() || !in.shot.model.isEmpty());
        QVERIFY(in.shot.iso > 0);
        QVERIFY(in.shot.fNumber > 0);
        QVERIFY(in.shot.exposureTime > 0);
        bool hasExifIfd = false;
        for (const ExifTag &t : in.tags) {
            QVERIFY(!exifTagName(t.ifd, t.tag).isEmpty());
            hasExifIfd = hasExifIfd || t.ifd == QLatin1String("Exif");
        }
        QVERIFY(hasExifIfd);
    }

    void dateParsing()
    {
        const QDateTime d = parseExifDateTime(QStringLiteral("2024:05:03 14:15:16"), QStringLiteral("25"));
        QCOMPARE(d.date(), QDate(2024, 5, 3));
        QCOMPARE(d.time(), QTime(14, 15, 16, 250));
        QVERIFY(!parseExifDateTime(QStringLiteral("0000:00:00 00:00:00")).isValid());
        QVERIFY(!parseExifDateTime(QStringLiteral("garbage")).isValid());
    }

    void formatting()
    {
        QCOMPARE(formatShutter(1.0 / 200), QStringLiteral("1/200"));
        QCOMPARE(formatShutter(2.0), QStringLiteral("2s"));
        QCOMPARE(formatAperture(2.8), QStringLiteral("f/2.8"));
        QCOMPARE(cameraName(QStringLiteral("NIKON CORPORATION"), QStringLiteral("NIKON D850")), QStringLiteral("NIKON D850"));
        QCOMPARE(cameraName(QStringLiteral("Canon"), QStringLiteral("EOS R5")), QStringLiteral("Canon EOS R5"));
    }

    void pictureHashMatchesPhotoe()
    {
        const QJsonObject hashes = expected().value(QStringLiteral("hashes")).toObject();
        QVERIFY2(!hashes.isEmpty(), "tests/data/faces/expected.json missing");
        for (auto it = hashes.begin(); it != hashes.end(); ++it) {
            const Inspection in = inspect(data(it.key()));
            QVERIFY2(in.ok, qPrintable(it.key()));
            QCOMPARE(in.pictureHash, it.value().toString());
        }
    }

    void pictureHashIgnoresMetadata()
    {
        // Os mesmos pixels com EXIF diferente: mesmo hash; o arquivo muda.
        QFile f(data(QStringLiteral("exif/o1.jpg")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray jpeg = f.readAll();
        const cv::Mat a = decodeImage(jpeg);
        QVERIFY(!a.empty());
        QCOMPARE(pictureHash(a), pictureHash(a.clone()));
        cv::Mat b = a.clone();
        b.at<cv::Vec3b>(0, 0)[0] ^= 1;
        QVERIFY(pictureHash(a) != pictureHash(b));
    }

    void perceptualHashSurvivesResize()
    {
        QFile f(data(QStringLiteral("sample.jpg")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const cv::Mat img = decodeImage(f.readAll());
        const quint64 h1 = perceptualHash(img);
        const quint64 h2 = perceptualHash(fitTo(img, 200));
        QVERIFY2(hammingDistance(h1, h2) <= 4, qPrintable(QString::number(hammingDistance(h1, h2))));
        cv::Mat flipped;
        cv::flip(img, flipped, 1);
        QVERIFY(hammingDistance(h1, perceptualHash(flipped)) > 10);
        QCOMPARE(hashFromHex(hashToHex(h1)), h1);
    }

    void sharpnessDropsWhenBlurred()
    {
        QFile f(data(QStringLiteral("sample.jpg")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const cv::Mat img = decodeImage(f.readAll());
        cv::Mat blurred;
        cv::GaussianBlur(img, blurred, cv::Size(15, 15), 6);
        QVERIFY(sharpness(blurred) < sharpness(img) / 4);
    }
};

QTEST_GUILESS_MAIN(TestImaging)
#include "tst_imaging.moc"

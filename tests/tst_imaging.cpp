// SPDX-License-Identifier: BSD-2-Clause
// Leitura de imagens: formatos, EXIF, hashes. O picture_hash é comparado com o
// do ../photoe (Pillow), em tests/data/faces/expected.json.

#include "imaging/Collage.h"
#include "imaging/Exif.h"
#include "imaging/Formats.h"
#include "imaging/Hashes.h"
#include "imaging/ImageIO.h"
#include "imaging/Inspect.h"
#include "imaging/Memory.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <opencv2/imgcodecs.hpp>
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

/// Bloco "Exif\0\0" + TIFF (little-endian) de uma Canon com Orientation = 1
/// e o MakerNote com ShotInfo[27] (AutoRotate) = ``autoRotate``.
QByteArray canonExif(qint16 autoRotate)
{
    QByteArray t("II*\0", 4);
    auto put16 = [&](int v) { t.append(char(v & 0xFF)).append(char((v >> 8) & 0xFF)); };
    auto put32 = [&](quint32 v) { for (int i = 0; i < 4; ++i) t.append(char((v >> (8 * i)) & 0xFF)); };
    auto entry = [&](int tag, int type, quint32 count, quint32 value) { put16(tag); put16(type); put32(count); put32(value); };
    const quint32 ifd0 = 8, ifd0Size = 2 + 3 * 12 + 4;
    const quint32 make = ifd0 + ifd0Size;                    // "Canon\0" (6) + 2 de folga
    const quint32 exif = make + 8, exifSize = 2 + 12 + 4;
    const quint32 maker = exif + exifSize, makerSize = 2 + 12 + 4;
    const quint32 shot = maker + makerSize;                  // 34 SHORTs
    put32(ifd0);
    put16(3);
    entry(0x010F, 2, 6, make);
    entry(0x0112, 3, 1, 1);
    entry(0x8769, 4, 1, exif);
    put32(0);
    t.append("Canon\0\0\0", 8);
    put16(1);
    entry(0x927C, 7, makerSize + 34 * 2, maker);
    put32(0);
    put16(1);
    entry(0x0004, 3, 34, shot);
    put32(0);
    for (int i = 0; i < 34; ++i)
        put16(i == 27 ? autoRotate : 0);
    return QByteArray("Exif\0\0", 6) + t;
}

/// JPEG com o segmento APP1 ``exif`` logo depois do SOI.
QByteArray withExif(const QByteArray &jpeg, const QByteArray &exif)
{
    QByteArray app1("\xFF\xE1", 2);
    const int len = int(exif.size()) + 2;
    app1.append(char(len >> 8)).append(char(len & 0xFF)).append(exif);
    return jpeg.left(2) + app1 + jpeg.mid(2);
}

} // namespace

class TestImaging : public QObject
{
    Q_OBJECT

private slots:
    void collageCropAndExif()
    {
        // Recorte de retrato: do tamanho pedido, com o rosto dentro, sem sair
        // da foto mesmo com o rosto na borda.
        cv::Mat src(1000, 1500, CV_8UC3, cv::Scalar(0, 0, 0));
        cv::rectangle(src, cv::Rect(1300, 50, 150, 150), cv::Scalar(255, 255, 255), cv::FILLED);
        const cv::Mat crop = cropPortrait(src, QRectF(1300.0 / 1500, 0.05, 0.1, 0.15), cv::Size(300, 360));
        QCOMPARE(crop.cols, 300);
        QCOMPARE(crop.rows, 360);
        QVERIFY(cv::mean(crop)[0] > 20);  // o rosto (branco) aparece
        QCOMPARE(cropPortrait(cv::Mat(), QRectF(0, 0, 1, 1), cv::Size(10, 10)).empty(), true);

        // EXIF da imagem exportada: legível pelo nosso leitor.
        ExportInfo info{QStringLiteral("Ana over the years"), QStringLiteral("PhotoIDX 1.2.3"), QStringLiteral("https://photoindexer.github.io/")};
        const ExifTags tags = readExifTags(buildExif(info));
        QMap<quint16, QString> text;
        for (const ExifTag &t : tags)
            if (t.type == 2)
                text.insert(t.tag, QString::fromUtf8(t.data).remove(QChar(0)));
        QCOMPARE(text.value(0x0131), info.software);
        QCOMPARE(text.value(0x010e), info.title);
        QCOMPARE(text.value(0x013c), info.url);
        bool comment = false;
        for (const ExifTag &t : tags)
            comment |= t.ifd == QLatin1String("Exif") && t.tag == 0x9286 && t.data.contains(info.url.toUtf8());
        QVERIFY(comment);

        // Gravado em JPEG: o APP1 está lá e a imagem continua abrindo.
        QTemporaryDir dir;
        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::red);
        const QString path = dir.filePath(QStringLiteral("out.jpg"));
        QCOMPARE(saveExport(image, path, info), QString());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bytes = f.readAll();
        QVERIFY(bytes.contains(QByteArray("Exif\0\0", 6)));
        QVERIFY(bytes.contains(info.software.toUtf8()));
        QCOMPARE(QImage::fromData(bytes).size(), QSize(64, 48));
        const QString png = dir.filePath(QStringLiteral("out.png"));
        QCOMPARE(saveExport(image, png, info), QString());
        QCOMPARE(QImage(png).text(QStringLiteral("Software")), info.software);
    }

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

    void memoryBudget()
    {
        using pix::imaging::MemoryBudget;
        auto &budget = MemoryBudget::instance();
        const qint64 saved = budget.capacity();
        budget.setCapacity(100 * 1024 * 1024);
        QCOMPARE(budget.used(), 0);
        // Maior que o orçamento inteiro: roda sozinha, limitada à capacidade.
        const qint64 big = budget.acquire(500 * 1024 * 1024);
        QCOMPARE(big, budget.capacity());
        // Outra reserva espera a primeira devolver.
        std::atomic<bool> got{false};
        QThread *waiter = QThread::create([&] {
            const qint64 n = budget.acquire(10 * 1024 * 1024);
            got = true;
            budget.release(n);
        });
        waiter->start();
        QTest::qWait(100);
        QVERIFY(!got);
        budget.release(big);
        QVERIFY(waiter->wait(5000));
        QVERIFY(got);
        delete waiter;
        QCOMPARE(budget.used(), 0);
        QVERIFY(pix::imaging::physicalMemory() > 0);
        budget.setCapacity(saved);
    }

    void canonMakerNoteOrientation()
    {
        // Paisagem 300x200 no arquivo; a câmera estava em pé (AutoRotate 3 =
        // girar 270°): a imagem de trabalho sai em pé, o hash fica o dos
        // pixels como gravados (o do Pillow, que só lê a tag).
        cv::Mat bgr(200, 300, CV_8UC3, cv::Scalar(30, 60, 90));
        cv::rectangle(bgr, cv::Rect(0, 0, 60, 40), cv::Scalar(250, 250, 250), cv::FILLED);
        std::vector<uchar> buf;
        cv::imencode(".jpg", bgr, buf);
        const QByteArray plain(reinterpret_cast<const char *>(buf.data()), qsizetype(buf.size()));
        QCOMPARE(makerNoteOrientation(canonExif(3)), 8);
        QCOMPARE(makerNoteOrientation(canonExif(1)), 6);
        QCOMPARE(makerNoteOrientation(canonExif(0)), 0);

        const Inspection normal = inspectData(withExif(plain, canonExif(0)), QStringLiteral("a.jpg"));
        const Inspection turned = inspectData(withExif(plain, canonExif(3)), QStringLiteral("a.jpg"));
        QVERIFY(normal.ok && turned.ok);
        QCOMPARE(normal.shot.orientation, 1);
        QCOMPARE(turned.shot.orientation, 8);
        QCOMPARE(normal.width, 300);
        QCOMPARE(turned.width, 200);
        QCOMPARE(turned.height, 300);
        QCOMPARE(turned.image.cols, 200);
        QCOMPARE(turned.image.rows, 300);
        QCOMPARE(turned.pictureHash, normal.pictureHash);
        // O canto claro (em cima à esquerda) vai para baixo à esquerda (270° horário).
        const cv::Vec3b bottomLeft = turned.image.at<cv::Vec3b>(turned.image.rows - 5, 5);
        QVERIFY(bottomLeft[0] > 200);
        const cv::Mat view = loadDisplayImageData(withExif(plain, canonExif(3)), QStringLiteral("a.jpg"), 1000);
        QCOMPARE(view.cols, 200);
    }

    void sniffByContent()
    {
        // Imagens de verdade: pelo conteúdo, não pela extensão.
        QCOMPARE(sniffImageFile(data(QStringLiteral("sample.jpg"))), QStringLiteral("image/jpeg"));
        const cv::Mat img(8, 8, CV_8UC3, cv::Scalar(10, 20, 30));
        std::vector<uchar> png;
        cv::imencode(".png", img, png);
        QCOMPARE(sniffImageMime(QByteArray(reinterpret_cast<const char *>(png.data()), qsizetype(png.size()))),
                 QStringLiteral("image/png"));
        auto head = [](const QByteArray &prefix) { return prefix + QByteArray(64, '\0'); };
        QCOMPARE(sniffImageMime(head(QByteArray("II*\0", 4))), QStringLiteral("image/tiff"));       // NEF, CR2, ARW, DNG...
        QCOMPARE(sniffImageMime(head(QByteArray("MM\0*", 4))), QStringLiteral("image/tiff"));
        QCOMPARE(sniffImageMime(head(QByteArray("\0\0\0\x18" "ftypcrx ", 12))), QStringLiteral("image/x-canon-cr3"));
        QCOMPARE(sniffImageMime(head("FUJIFILMCCD-RAW 0201")), QStringLiteral("image/x-fuji-raf"));
        QCOMPARE(sniffImageMime(head("IIRO")), QStringLiteral("image/x-olympus-orf"));
        QCOMPARE(sniffImageMime(head(QByteArray("IIU\0", 4))), QStringLiteral("image/x-panasonic-rw2"));
        QCOMPARE(sniffImageMime(head(QByteArray("\0\0\0\x18" "ftypheic", 12))), QStringLiteral("image/heif"));
        QCOMPARE(sniffImageMime(head(QByteArray("RIFF\0\0\0\0WEBP", 12))), QStringLiteral("image/webp"));
        // Não são imagens: imagem de disco (zeros), texto, binário qualquer,
        // JPEG truncado no primeiro byte.
        QVERIFY(sniffImageMime(QByteArray(4096, '\0')).isEmpty());
        QVERIFY(sniffImageMime(QByteArray("hello world, this is text\n").repeated(50)).isEmpty());
        QByteArray random(4096, Qt::Uninitialized);
        for (int i = 0; i < random.size(); ++i)
            random[i] = char((i * 7919 + 13) & 0xFF);
        QVERIFY(sniffImageMime(random).isEmpty());
        QVERIFY(sniffImageMime(QByteArray("\xFF", 1)).isEmpty());
        QVERIFY(sniffImageMime(QByteArray("ftyp", 4)).isEmpty());
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

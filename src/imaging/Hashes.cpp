// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Hashes.h"

#include "imaging/ImageIO.h"

#include <QCryptographicHash>

#include <opencv2/imgproc.hpp>

#include <bit>

namespace pix::imaging {

QString pictureHash(const cv::Mat &pixels, const QString &mode)
{
    if (pixels.empty() || pixels.depth() != CV_8U)
        return {};
    const QString m = !mode.isEmpty() ? mode : pixels.channels() == 1 ? QStringLiteral("L") : QStringLiteral("RGB");
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(QStringLiteral("%1:%2x%3:").arg(m).arg(pixels.cols).arg(pixels.rows).toLatin1());
    const qsizetype rowBytes = qsizetype(pixels.cols) * pixels.channels();
    for (int y = 0; y < pixels.rows; ++y)
        digest.addData(QByteArrayView(reinterpret_cast<const char *>(pixels.ptr(y)), rowBytes));
    return QString::fromLatin1(digest.result().toHex());
}

QString rawPictureHash(const quint16 *data, int width, int height, int pitchBytes)
{
    if (!data || width <= 0 || height <= 0)
        return {};
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(QStringLiteral("RAW:%1x%2:").arg(width).arg(height).toLatin1());
    QByteArray row(qsizetype(width) * 2, Qt::Uninitialized);
    for (int y = 0; y < height; ++y) {
        const auto *src = reinterpret_cast<const quint16 *>(reinterpret_cast<const char *>(data) + qsizetype(y) * pitchBytes);
        auto *dst = reinterpret_cast<uchar *>(row.data());
        for (int x = 0; x < width; ++x) {
            dst[2 * x] = uchar(src[x] & 0xFF);
            dst[2 * x + 1] = uchar(src[x] >> 8);
        }
        digest.addData(row);
    }
    return QString::fromLatin1(digest.result().toHex());
}

quint64 perceptualHash(const cv::Mat &rgb)
{
    if (rgb.empty())
        return 0;
    cv::Mat gray;
    if (rgb.channels() == 3)
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
    else
        gray = rgb;
    cv::Mat small;
    cv::resize(gray, small, cv::Size(9, 8), 0, 0, cv::INTER_AREA);
    quint64 hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        const uchar *row = small.ptr<uchar>(y);
        for (int x = 0; x < 8; ++x, ++bit)
            if (row[x] > row[x + 1])
                hash |= quint64(1) << bit;
    }
    return hash;
}

double sharpness(const cv::Mat &rgb)
{
    if (rgb.empty())
        return -1.0;
    cv::Mat gray;
    if (rgb.channels() == 3)
        cv::cvtColor(fitTo(rgb, 512), gray, cv::COLOR_RGB2GRAY);
    else
        gray = fitTo(rgb, 512);
    cv::Mat lap;
    cv::Laplacian(gray, lap, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(lap, mean, stddev);
    return stddev[0] * stddev[0];
}

int hammingDistance(quint64 a, quint64 b)
{
    return std::popcount(a ^ b);
}

QString hashToHex(quint64 hash)
{
    return QStringLiteral("%1").arg(hash, 16, 16, QLatin1Char('0'));
}

quint64 hashFromHex(const QString &hex, bool *ok)
{
    return hex.toULongLong(ok, 16);
}

} // namespace pix::imaging

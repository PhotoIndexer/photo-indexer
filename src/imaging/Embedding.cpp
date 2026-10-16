// SPDX-License-Identifier: BSD-2-Clause
#include "imaging/Embedding.h"

#include <QtEndian>

#include <cmath>
#include <cstring>

namespace pix::imaging {

void normalize(Embedding &v)
{
    double sum = 0.0;
    for (float x : v)
        sum += double(x) * x;
    const double norm = std::sqrt(sum);
    if (norm > 0)
        for (float &x : v)
            x = float(x / norm);
}

float dot(const float *a, const float *b, int n)
{
    float sum = 0.0f;
    for (int i = 0; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
}

float dot(const Embedding &a, const Embedding &b)
{
    if (a.size() != b.size() || a.empty())
        return 0.0f;
    return dot(a.data(), b.data(), int(a.size()));
}

QByteArray toBlob(const Embedding &v)
{
    QByteArray out(qsizetype(v.size() * sizeof(float)), Qt::Uninitialized);
    for (size_t i = 0; i < v.size(); ++i)
        qToLittleEndian<float>(v[i], out.data() + i * sizeof(float));
    return out;
}

Embedding fromBlob(const QByteArray &blob)
{
    Embedding v(size_t(blob.size()) / sizeof(float));
    for (size_t i = 0; i < v.size(); ++i)
        v[i] = qFromLittleEndian<float>(blob.constData() + i * sizeof(float));
    return v;
}

Embedding mean(const std::vector<Embedding> &vectors)
{
    if (vectors.empty())
        return {};
    Embedding out(vectors.front().size(), 0.0f);
    for (const Embedding &v : vectors)
        if (v.size() == out.size())
            for (size_t i = 0; i < v.size(); ++i)
                out[i] += v[i];
    normalize(out);
    return out;
}

} // namespace pix::imaging

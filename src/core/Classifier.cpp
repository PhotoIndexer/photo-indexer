// SPDX-License-Identifier: BSD-2-Clause
#include "core/Classifier.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace pix {

QList<QPair<QString, float>> pickCategories(const QList<CategoryScore> &scores, float background,
                                            const ClassifierParams &params)
{
    QList<QPair<QString, float>> out;
    if (scores.isEmpty())
        return out;
    const bool hasBackground = std::isfinite(background);
    // Softmax entre todas as categorias e a "nenhuma" (a última posição).
    const qsizetype n = scores.size();
    std::vector<double> p(size_t(n) + (hasBackground ? 1 : 0));
    double maxLogit = hasBackground ? params.scale * background : -1e30;
    for (const CategoryScore &s : scores)
        maxLogit = std::max(maxLogit, double(params.scale * s.cosine));
    for (qsizetype i = 0; i < n; ++i)
        p[size_t(i)] = std::exp(params.scale * scores[i].cosine - maxLogit);
    if (hasBackground)
        p.back() = std::exp(params.scale * background - maxLogit);
    const double total = std::accumulate(p.begin(), p.end(), 0.0);
    for (double &v : p)
        v /= total;

    std::vector<qsizetype> order(p.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](qsizetype a, qsizetype b) { return p[size_t(a)] > p[size_t(b)]; });
    const qsizetype best = order.front();
    if (best >= n)
        return out;  // "nenhuma" venceu

    const CategoryScore &top = scores[best];
    const double pBest = p[size_t(best)];
    double sibling = 0.0;
    if (!top.parentId.isEmpty())
        for (qsizetype i = 0; i < n; ++i)
            if (i != best && scores[i].parentId == top.parentId)
                sibling = std::max(sibling, p[size_t(i)]);
    const bool unsure = !top.parentId.isEmpty() && sibling > params.siblingMargin * pBest;

    if (pBest >= params.minProbability && !unsure) {
        out.append({top.categoryId, float(pBest)});
        QSet<QString> groups{top.parentId.isEmpty() ? top.categoryId : top.parentId};
        for (qsizetype k = 1; k < qsizetype(order.size()) && out.size() < params.maxCategories; ++k) {
            const qsizetype i = order[size_t(k)];
            if (i >= n || p[size_t(i)] < params.minProbability)
                break;  // abaixo do limiar, ou a "nenhuma" vem antes
            const QString group = scores[i].parentId.isEmpty() ? scores[i].categoryId : scores[i].parentId;
            if (groups.contains(group))
                continue;  // nunca duas irmãs
            groups.insert(group);
            out.append({scores[i].categoryId, float(p[size_t(i)])});
        }
        return out;
    }
    // Na dúvida (ou fraca demais sozinha): o grupo, se as filhas juntas
    // passam do limiar do pai.
    if (!top.parentId.isEmpty()) {
        double sum = 0.0;
        for (qsizetype i = 0; i < n; ++i)
            if (scores[i].parentId == top.parentId)
                sum += p[size_t(i)];
        if (sum >= params.parentProbability)
            out.append({top.parentId, float(sum)});
    }
    return out;
}

} // namespace pix

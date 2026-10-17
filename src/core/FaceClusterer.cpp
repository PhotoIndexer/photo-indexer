// SPDX-License-Identifier: BSD-2-Clause
#include "core/FaceClusterer.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>

namespace pix {

namespace {

struct Groups
{
    size_t dims = 0;
    std::vector<float> sums;                    ///< grupo × dims
    std::vector<float> norms;                   ///< |soma|
    std::vector<int> counts;
    std::vector<std::unordered_set<int>> photos;
    std::vector<int> alias;                     ///< grupo absorvido -> quem o absorveu (-1 = vivo)
    int fixed = 0;

    int add()
    {
        sums.resize(sums.size() + dims, 0.0f);
        norms.push_back(0.0f);
        counts.push_back(0);
        photos.emplace_back();
        alias.push_back(-1);
        return int(counts.size()) - 1;
    }
    const float *sum(int g) const { return sums.data() + size_t(g) * dims; }
    float *sum(int g) { return sums.data() + size_t(g) * dims; }
    void put(int g, const ClusterFace &f)
    {
        float *s = sum(g);
        double n = 0;
        for (size_t i = 0; i < dims; ++i) {
            s[i] += f.embedding[i];
            n += double(s[i]) * s[i];
        }
        norms[size_t(g)] = float(std::sqrt(n));
        ++counts[size_t(g)];
        photos[size_t(g)].insert(f.photo);
    }
    int root(int g) const
    {
        while (alias[size_t(g)] >= 0)
            g = alias[size_t(g)];
        return g;
    }
    bool sharePhoto(int a, int b) const
    {
        const auto &small = photos[size_t(a)].size() < photos[size_t(b)].size() ? photos[size_t(a)] : photos[size_t(b)];
        const auto &big = &small == &photos[size_t(a)] ? photos[size_t(b)] : photos[size_t(a)];
        for (int p : small)
            if (big.count(p))
                return true;
        return false;
    }
    /// Média das similaridades entre as faces de a e as de b.
    float averageLink(int a, int b) const
    {
        const float *x = sum(a), *y = sum(b);
        double d = 0;
        for (size_t i = 0; i < dims; ++i)
            d += double(x[i]) * y[i];
        return float(d / (double(counts[size_t(a)]) * counts[size_t(b)]));
    }
    void absorb(int target, int source)
    {
        float *t = sum(target), *s = sum(source);
        double n = 0;
        for (size_t i = 0; i < dims; ++i) {
            t[i] += s[i];
            s[i] = 0.0f;
            n += double(t[i]) * t[i];
        }
        norms[size_t(target)] = float(std::sqrt(n));
        norms[size_t(source)] = 0.0f;
        counts[size_t(target)] += counts[size_t(source)];
        counts[size_t(source)] = 0;
        photos[size_t(target)].insert(photos[size_t(source)].begin(), photos[size_t(source)].end());
        photos[size_t(source)].clear();
        alias[size_t(source)] = target;
    }
};

void mergePass(Groups &g, const ClusterParams &params)
{
    std::vector<int> live;
    for (int i = 0; i < int(g.counts.size()); ++i)
        if (g.alias[size_t(i)] < 0 && g.counts[size_t(i)] >= params.mergeMinFaces)
            live.push_back(i);
    struct Pair
    {
        int a, b;
        float sim;
    };
    std::vector<Pair> pairs;
    for (size_t i = 0; i < live.size(); ++i)
        for (size_t j = i + 1; j < live.size(); ++j) {
            const float s = g.averageLink(live[i], live[j]);
            if (s >= params.merge)
                pairs.push_back({live[i], live[j], s});
        }
    std::sort(pairs.begin(), pairs.end(), [](const Pair &x, const Pair &y) { return x.sim > y.sim; });
    for (const Pair &p : pairs) {
        int a = g.root(p.a), b = g.root(p.b);
        if (a == b)
            continue;
        if (a < g.fixed && b < g.fixed)
            continue;  // duas pessoas com nome/confirmadas: nunca
        // Recalculada com os grupos de agora (sem cadeia).
        if (g.averageLink(a, b) < params.merge || g.sharePhoto(a, b))
            continue;
        // Fica o fixo; entre dois livres, o maior.
        if (b < g.fixed || (a >= g.fixed && g.counts[size_t(b)] > g.counts[size_t(a)]))
            std::swap(a, b);
        g.absorb(a, b);
    }
}

} // namespace

std::vector<int> clusterFaces(const std::vector<ClusterFace> &faces, int fixedGroups, const ClusterParams &params)
{
    std::vector<int> label(faces.size(), -1);
    if (faces.empty())
        return label;
    Groups g;
    g.dims = faces.front().embedding.size();
    g.fixed = fixedGroups;
    for (int i = 0; i < fixedGroups; ++i)
        g.add();
    // Primeiro as faces dos fixos: elas são o ponto de partida deles.
    for (size_t i = 0; i < faces.size(); ++i)
        if (faces[i].fixed >= 0 && faces[i].fixed < fixedGroups && faces[i].embedding.size() == g.dims) {
            g.put(faces[i].fixed, faces[i]);
            label[i] = faces[i].fixed;
        }
    int sinceMerge = 0;
    for (size_t i = 0; i < faces.size(); ++i) {
        const ClusterFace &f = faces[i];
        if (label[i] >= 0 || f.embedding.size() != g.dims)
            continue;
        int best = -1;
        float bestSim = params.join;
        for (int c = 0; c < int(g.counts.size()); ++c) {
            if (g.alias[size_t(c)] >= 0 || g.counts[size_t(c)] == 0 || g.norms[size_t(c)] <= 0)
                continue;
            const float *s = g.sum(c);
            double d = 0;
            for (size_t k = 0; k < g.dims; ++k)
                d += double(s[k]) * f.embedding[k];
            const float sim = float(d / g.norms[size_t(c)]);
            if (sim < bestSim)
                continue;
            if (g.photos[size_t(c)].count(f.photo))
                continue;  // já tem alguém desta foto
            if (std::find(f.notIn.begin(), f.notIn.end(), c) != f.notIn.end())
                continue;  // a pessoa tirou esta face daqui
            best = c;
            bestSim = sim;
        }
        if (best < 0)
            best = g.add();
        g.put(best, f);
        label[i] = best;
        if (++sinceMerge >= params.mergeEvery) {
            sinceMerge = 0;
            mergePass(g, params);
        }
    }
    mergePass(g, params);
    // Grupos absorvidos -> quem os absorveu; os novos renumerados em ordem.
    std::vector<int> renumber(g.counts.size(), -1);
    int next = fixedGroups;
    for (int &l : label) {
        if (l < 0)
            continue;
        l = g.root(l);
        if (l >= fixedGroups) {
            if (renumber[size_t(l)] < 0)
                renumber[size_t(l)] = next++;
            l = renumber[size_t(l)];
        }
    }
    return label;
}

} // namespace pix

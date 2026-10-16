// SPDX-License-Identifier: BSD-2-Clause
#include "core/HashTree.h"

#include "imaging/Hashes.h"

#include <algorithm>

namespace pix {

HashTree::HashTree() = default;
HashTree::~HashTree() = default;

void HashTree::clear()
{
    m_root.reset();
    m_hashOf.clear();
}

void HashTree::insert(const QString &id, quint64 hash)
{
    if (m_hashOf.contains(id)) {
        if (m_hashOf.value(id) == hash)
            return;
        remove(id);
    }
    m_hashOf.insert(id, hash);
    if (!m_root) {
        m_root = std::make_unique<Node>();
        m_root->hash = hash;
        m_root->ids << id;
        return;
    }
    Node *node = m_root.get();
    for (;;) {
        const int d = imaging::hammingDistance(node->hash, hash);
        if (d == 0) {
            node->ids << id;
            return;
        }
        auto it = std::find_if(node->children.begin(), node->children.end(), [d](const auto &c) { return c.first == d; });
        if (it == node->children.end()) {
            auto child = std::make_unique<Node>();
            child->hash = hash;
            child->ids << id;
            node->children.emplace_back(d, std::move(child));
            return;
        }
        node = it->second.get();
    }
}

void HashTree::remove(const QString &id)
{
    const auto it = m_hashOf.constFind(id);
    if (it == m_hashOf.constEnd())
        return;
    const quint64 hash = *it;
    m_hashOf.erase(it);
    // O nó fica (a estrutura da árvore depende dele); só perde o id.
    Node *node = m_root.get();
    while (node) {
        const int d = imaging::hammingDistance(node->hash, hash);
        if (d == 0) {
            node->ids.removeAll(id);
            return;
        }
        auto c = std::find_if(node->children.begin(), node->children.end(), [d](const auto &x) { return x.first == d; });
        node = c == node->children.end() ? nullptr : c->second.get();
    }
}

QList<HashTree::Match> HashTree::query(quint64 hash, int maxDistance) const
{
    QList<Match> out;
    if (!m_root)
        return out;
    std::vector<const Node *> stack{m_root.get()};
    while (!stack.empty()) {
        const Node *node = stack.back();
        stack.pop_back();
        const int d = imaging::hammingDistance(node->hash, hash);
        if (d <= maxDistance)
            for (const QString &id : node->ids)
                out.append({id, d});
        for (const auto &[edge, child] : node->children)
            if (edge >= d - maxDistance && edge <= d + maxDistance)
                stack.push_back(child.get());
    }
    std::stable_sort(out.begin(), out.end(), [](const Match &a, const Match &b) { return a.distance < b.distance; });
    return out;
}

} // namespace pix

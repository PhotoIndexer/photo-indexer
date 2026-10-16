// SPDX-License-Identifier: BSD-2-Clause
#include "core/FaceIndex.h"

#include <algorithm>

namespace pix {

void FaceIndex::clear()
{
    m_dims = 0;
    m_data.clear();
    m_entries.clear();
    m_rowOf.clear();
}

void FaceIndex::insert(const Entry &entry, const imaging::Embedding &embedding)
{
    if (embedding.empty())
        return;
    if (m_dims == 0)
        m_dims = int(embedding.size());
    if (int(embedding.size()) != m_dims)
        return;
    remove(entry.faceId);
    m_rowOf.insert(entry.faceId, int(m_entries.size()));
    m_entries.push_back(entry);
    m_data.insert(m_data.end(), embedding.begin(), embedding.end());
}

void FaceIndex::remove(const QString &faceId)
{
    const auto it = m_rowOf.constFind(faceId);
    if (it == m_rowOf.constEnd())
        return;
    const int row = *it;
    const int last = int(m_entries.size()) - 1;
    // Troca com a última linha e encolhe: O(1).
    if (row != last) {
        std::copy_n(m_data.begin() + qsizetype(last) * m_dims, m_dims, m_data.begin() + qsizetype(row) * m_dims);
        m_entries[size_t(row)] = m_entries[size_t(last)];
        m_rowOf[m_entries[size_t(row)].faceId] = row;
    }
    m_entries.pop_back();
    m_data.resize(m_data.size() - size_t(m_dims));
    m_rowOf.remove(faceId);
}

void FaceIndex::setPerson(const QString &faceId, const QString &personId)
{
    const auto it = m_rowOf.constFind(faceId);
    if (it != m_rowOf.constEnd())
        m_entries[size_t(*it)].personId = personId;
}

QList<FaceIndex::Match> FaceIndex::search(const imaging::Embedding &query, float threshold, int limit) const
{
    QList<Match> out;
    if (int(query.size()) != m_dims || m_dims == 0)
        return out;
    const float *q = query.data();
    for (size_t row = 0; row < m_entries.size(); ++row) {
        const float sim = imaging::dot(q, m_data.data() + row * size_t(m_dims), m_dims);
        if (sim >= threshold)
            out.append({m_entries[row], sim});
    }
    std::sort(out.begin(), out.end(), [](const Match &a, const Match &b) { return a.similarity > b.similarity; });
    if (limit > 0 && out.size() > limit)
        out.resize(limit);
    return out;
}

imaging::Embedding FaceIndex::embeddingOf(const QString &faceId) const
{
    const auto it = m_rowOf.constFind(faceId);
    if (it == m_rowOf.constEnd())
        return {};
    const auto begin = m_data.begin() + qsizetype(*it) * m_dims;
    return imaging::Embedding(begin, begin + m_dims);
}

const FaceIndex::Entry *FaceIndex::entryOf(const QString &faceId) const
{
    const auto it = m_rowOf.constFind(faceId);
    return it == m_rowOf.constEnd() ? nullptr : &m_entries[size_t(*it)];
}

std::shared_ptr<const FaceIndex> FaceIndex::snapshot() const
{
    return std::make_shared<FaceIndex>(*this);
}

} // namespace pix

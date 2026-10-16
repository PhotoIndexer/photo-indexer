// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Índice das faces em memória: os embeddings (512 floats) numa matriz
// contígua, para a busca por uma pessoa ("estas fotos têm esta pessoa?") e
// para o agrupamento. A busca é EXATA (cosseno contra todas as faces): 100 mil
// faces custam ~50 milhões de multiplicações, dezenas de milissegundos.
//
// Montado do catálogo na abertura e atualizado pelo Catalog. Só a thread
// principal mexe nele; ``snapshot()`` dá uma cópia para buscar em outra thread.

#include "imaging/Embedding.h"

#include <QHash>
#include <QList>
#include <QString>

#include <memory>
#include <vector>

namespace pix {

class FaceIndex
{
public:
    struct Entry
    {
        QString faceId;
        QString photoId;
        QString personId;
    };
    struct Match
    {
        Entry entry;
        float similarity = 0.0f;
    };

    void clear();
    void insert(const Entry &entry, const imaging::Embedding &embedding);
    void remove(const QString &faceId);
    void setPerson(const QString &faceId, const QString &personId);
    int size() const { return int(m_entries.size()); }
    int dimensions() const { return m_dims; }

    /// Faces com cosseno >= ``threshold``, da mais parecida para a menos; no
    /// máximo ``limit`` (0 = todas).
    QList<Match> search(const imaging::Embedding &query, float threshold, int limit = 0) const;

    /// Embedding guardado de uma face (vazio se não está no índice).
    imaging::Embedding embeddingOf(const QString &faceId) const;
    const Entry *entryOf(const QString &faceId) const;

    std::shared_ptr<const FaceIndex> snapshot() const;

private:
    int m_dims = 0;
    std::vector<float> m_data;          ///< size() x m_dims
    std::vector<Entry> m_entries;
    QHash<QString, int> m_rowOf;
};

} // namespace pix

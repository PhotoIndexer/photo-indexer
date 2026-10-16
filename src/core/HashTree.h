// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Árvore de hashes: uma BK-tree sobre o hash perceptual (64 bits) de cada
// foto, com a distância de Hamming. Acha as fotos PARECIDAS com uma (mesma
// imagem redimensionada, recomprimida, com outro EXIF) sem comparar com todas:
// a desigualdade triangular poda os ramos que não podem estar a ``maxDistance``.
//
// Em memória, montada do catálogo na abertura e atualizada a cada foto
// indexada. Só a thread principal mexe nela.

#include <QHash>
#include <QList>
#include <QString>

#include <memory>
#include <vector>

namespace pix {

class HashTree
{
public:
    struct Match
    {
        QString id;
        int distance = 0;
    };

    HashTree();
    ~HashTree();

    void clear();
    /// Acrescenta (ou troca o hash de) ``id``.
    void insert(const QString &id, quint64 hash);
    void remove(const QString &id);
    int size() const { return int(m_hashOf.size()); }

    /// Tudo a no máximo ``maxDistance`` bits de ``hash``, do mais perto ao mais longe.
    QList<Match> query(quint64 hash, int maxDistance) const;

private:
    struct Node
    {
        quint64 hash = 0;
        QStringList ids;                           ///< fotos com exatamente este hash
        std::vector<std::pair<int, std::unique_ptr<Node>>> children;  ///< (distância, filho)
    };

    std::unique_ptr<Node> m_root;
    QHash<QString, quint64> m_hashOf;
};

} // namespace pix

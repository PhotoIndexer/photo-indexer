// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Uma aba do painel lateral como lista plana (ListView do QML): os nós que o
// Catalog devolve em pré-ordem, mostrando só os que têm todos os ancestrais
// abertos. O que está aberto é lembrado por aba enquanto o app roda.

#include <QAbstractListModel>
#include <QSet>
#include <QVariantList>

class TreeModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        KeyRole = Qt::UserRole + 1,
        LabelRole,
        CountRole,
        DepthRole,
        HasChildrenRole,
        ExpandedRole,
        IconRole,
        DetailRole,
        KindRole,
    };

    explicit TreeModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Nós novos da aba (mantém o que estava aberto).
    void setNodes(const QVariantList &nodes);
    int count() const { return int(m_visible.size()); }

    Q_INVOKABLE void toggle(const QString &key);
    /// Abre os ancestrais de ``key`` (para mostrar um nó escolhido de fora).
    Q_INVOKABLE void reveal(const QString &key);
    Q_INVOKABLE void expandAll(bool expanded);
    Q_INVOKABLE int indexOfKey(const QString &key) const;
    /// O nó (mapa) de uma chave, mesmo se escondido.
    Q_INVOKABLE QVariantMap node(const QString &key) const;

signals:
    void countChanged();

private:
    void rebuild();

    QVariantList m_nodes;
    QList<int> m_visible;           ///< índices em m_nodes
    QSet<QString> m_expanded;
    QSet<QString> m_known;      ///< chaves já vistas (o "open" só vale na primeira vez)
    bool m_seeded = false;
};

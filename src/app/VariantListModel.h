// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Lista de mapas (QVariantMap) para as grades do QML que hoje recebiam um
// QVariantList: trocar o array inteiro refazia a view e a mandava de volta ao
// topo. Aqui a troca é incremental (KeyedListModel), pela chave ``keyField``
// de cada mapa. Um papel só, ``modelData``: o delegate continua com
// ``required property var modelData``, como com o array.

#include "KeyedListModel.h"

#include <QVariantList>
#include <QVariantMap>

class VariantListModel : public KeyedListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit VariantListModel(const QString &keyField, QObject *parent = nullptr)
        : KeyedListModel(parent)
        , m_keyField(keyField)
    {
    }

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(m_rows.size()); }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_rows.size() || role != Qt::UserRole + 1)
            return {};
        return m_rows.at(index.row());
    }
    QHash<int, QByteArray> roleNames() const override { return {{Qt::UserRole + 1, "modelData"}}; }

    int count() const { return int(m_rows.size()); }

    void setItems(const QVariantList &items)
    {
        QList<QVariantMap> next;
        next.reserve(items.size());
        for (const QVariant &v : items)
            next.append(v.toMap());
        const qsizetype before = m_rows.size();
        updateRows(m_rows, std::move(next), [this](const QVariantMap &m) { return m.value(m_keyField).toString(); });
        if (before != m_rows.size())
            emit countChanged();
    }

signals:
    void countChanged();

private:
    QString m_keyField;
    QList<QVariantMap> m_rows;
};

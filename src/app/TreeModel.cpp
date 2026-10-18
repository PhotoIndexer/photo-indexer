// SPDX-License-Identifier: BSD-2-Clause
#include "TreeModel.h"

#include <QHash>
#include <QVariantMap>

TreeModel::TreeModel(QObject *parent)
    : KeyedListModel(parent)
{
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const QVariantMap &n = m_rows.at(index.row());
    switch (role) {
    case KeyRole: return n.value(QStringLiteral("key"));
    case LabelRole: return n.value(QStringLiteral("label"));
    case CountRole: return n.value(QStringLiteral("count"));
    case DepthRole: return n.value(QStringLiteral("depth"));
    case HasChildrenRole: return n.value(QStringLiteral("hasChildren"));
    case ExpandedRole: return n.value(QStringLiteral("expanded"));
    case IconRole: return n.value(QStringLiteral("icon"));
    case DetailRole: return n.value(QStringLiteral("detail"));
    case KindRole: return n.value(QStringLiteral("kind"));
    }
    return {};
}

QHash<int, QByteArray> TreeModel::roleNames() const
{
    return {{KeyRole, "key"},           {LabelRole, "label"},       {CountRole, "count"},
            {DepthRole, "depth"},       {HasChildrenRole, "hasChildren"}, {ExpandedRole, "expanded"},
            {IconRole, "nodeIcon"},         {DetailRole, "detail"},     {KindRole, "kind"}};
}

void TreeModel::setNodes(const QVariantList &nodes)
{
    m_nodes = nodes;
    if (!m_seeded) {
        // Na primeira carga, as raízes que têm filhos já vêm abertas só no
        // primeiro nível (disco -> pastas indexadas): o resto, fechado.
        for (const QVariant &v : m_nodes) {
            const QVariantMap n = v.toMap();
            if (n.value(QStringLiteral("depth")).toInt() == 0 && n.value(QStringLiteral("key")).toString().startsWith(QLatin1String("disk:")))
                m_expanded.insert(n.value(QStringLiteral("key")).toString());
        }
        m_seeded = !m_nodes.isEmpty();
    }
    // Nó que pede para nascer aberto ("open": grupos de pessoas, "Com
    // nome"): aberto da primeira vez que aparece; depois, como a pessoa deixou.
    for (const QVariant &v : m_nodes) {
        const QVariantMap n = v.toMap();
        const QString key = n.value(QStringLiteral("key")).toString();
        if (!m_known.contains(key)) {
            m_known.insert(key);
            if (n.value(QStringLiteral("open")).toBool())
                m_expanded.insert(key);
        }
    }
    rebuild();
}

void TreeModel::rebuild()
{
    const qsizetype old = m_rows.size();
    QList<QVariantMap> rows;
    // Pré-ordem: um nó aparece se o pai aparece e está aberto.
    QHash<QString, bool> shown;
    for (int i = 0; i < m_nodes.size(); ++i) {
        QVariantMap n = m_nodes.at(i).toMap();
        const QString key = n.value(QStringLiteral("key")).toString();
        const QString parent = n.value(QStringLiteral("parent")).toString();
        const bool visible = parent.isEmpty() || (shown.value(parent) && m_expanded.contains(parent));
        shown.insert(key, visible);
        if (visible) {
            n.insert(QStringLiteral("expanded"), m_expanded.contains(key));
            rows.append(std::move(n));
        }
    }
    // Sem reset: o painel não volta ao topo quando a árvore se atualiza
    // (contagens mudando durante a indexação, abrir/fechar um nó).
    updateRows(m_rows, std::move(rows), [](const QVariantMap &n) {
        return n.value(QStringLiteral("parent")).toString() + u'\x1f' + n.value(QStringLiteral("key")).toString();
    });
    if (old != m_rows.size())
        emit countChanged();
}

void TreeModel::toggle(const QString &key)
{
    if (!m_expanded.remove(key))
        m_expanded.insert(key);
    rebuild();
}

void TreeModel::reveal(const QString &key)
{
    QHash<QString, QString> parentOf;
    for (const QVariant &v : m_nodes) {
        const QVariantMap n = v.toMap();
        parentOf.insert(n.value(QStringLiteral("key")).toString(), n.value(QStringLiteral("parent")).toString());
    }
    bool changed = false;
    for (QString p = parentOf.value(key); !p.isEmpty(); p = parentOf.value(p))
        if (!m_expanded.contains(p)) {
            m_expanded.insert(p);
            changed = true;
        }
    if (changed)
        rebuild();
}

void TreeModel::expandAll(bool expanded)
{
    m_expanded.clear();
    if (expanded)
        for (const QVariant &v : m_nodes)
            if (v.toMap().value(QStringLiteral("hasChildren")).toBool())
                m_expanded.insert(v.toMap().value(QStringLiteral("key")).toString());
    rebuild();
}

int TreeModel::indexOfKey(const QString &key) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).value(QStringLiteral("key")).toString() == key)
            return i;
    return -1;
}

QVariantMap TreeModel::node(const QString &key) const
{
    for (const QVariant &v : m_nodes)
        if (v.toMap().value(QStringLiteral("key")).toString() == key)
            return v.toMap();
    return {};
}

// SPDX-License-Identifier: BSD-2-Clause
#include "PeopleListModel.h"

namespace {

/// Minúsculas e sem acento ("José" -> "jose").
QString fold(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed)
        if (c.category() != QChar::Mark_NonSpacing)
            out += c.toLower();
    return out;
}

} // namespace

PeopleListModel::PeopleListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PeopleListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant PeopleListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size() || role != PersonRole)
        return {};
    return m_rows.at(index.row());
}

QHash<int, QByteArray> PeopleListModel::roleNames() const
{
    return {{PersonRole, "person"}};
}

void PeopleListModel::setPeople(const QVariantList &people)
{
    if (people == m_all)
        return;
    m_all = people;
    rebuild();
}

void PeopleListModel::setFilter(const QString &filter)
{
    const QString f = fold(filter.trimmed());
    if (f == m_filter)
        return;
    m_filter = f;
    emit filterChanged();
    rebuild();
}

void PeopleListModel::rebuild()
{
    QList<QVariantMap> rows;
    rows.reserve(m_all.size());
    for (const QVariant &v : std::as_const(m_all)) {
        QVariantMap p = v.toMap();
        if (!m_filter.isEmpty() && !fold(p.value(QStringLiteral("label")).toString()).contains(m_filter))
            continue;
        rows.append(std::move(p));
    }
    const qsizetype before = m_rows.size();
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    if (before != m_rows.size())
        emit countChanged();
}

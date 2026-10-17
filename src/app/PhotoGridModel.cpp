// SPDX-License-Identifier: BSD-2-Clause
#include "PhotoGridModel.h"

#include "core/Perf.h"

#include "ImageProviders.h"

PhotoGridModel::PhotoGridModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PhotoGridModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant PhotoGridModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const pix::GridRow &r = m_rows.at(index.row());
    switch (role) {
    case PhotoIdRole: return r.id;
    case NameRole: return r.name;
    case ThumbUrlRole: return thumbnailUrl(r.thumbPath);
    case IsPrivateRole: return r.isPrivate;
    case AspectRole: return r.width > 0 && r.height > 0 ? double(r.width) / r.height : 1.5;
    case IsRawRole: return r.isRaw;
    case TakenAtRole: return r.takenAt;
    case SelectedRole: return m_selected.contains(r.id);
    case ScoreRole: return r.score;
    case StateRole: return r.state;
    }
    return {};
}

QHash<int, QByteArray> PhotoGridModel::roleNames() const
{
    return {{PhotoIdRole, "photoId"},   {NameRole, "name"},         {ThumbUrlRole, "thumbUrl"},
            {AspectRole, "aspect"},     {IsRawRole, "isRaw"},       {TakenAtRole, "takenAt"},
            {SelectedRole, "selected"}, {ScoreRole, "score"},       {StateRole, "state"},
            {IsPrivateRole, "isPrivate"}};
}

namespace {

bool sameCell(const pix::GridRow &a, const pix::GridRow &b)
{
    return a.thumbPath == b.thumbPath && a.name == b.name && a.width == b.width && a.height == b.height
        && a.state == b.state && a.takenAt == b.takenAt && a.score == b.score && a.isPrivate == b.isPrivate;
}

} // namespace

void PhotoGridModel::setRows(const QList<pix::GridRow> &rows, bool keepPosition)
{
    PIX_PERF("PhotoGridModel::setRows");
    m_keepPosition = keepPosition;
    // As mesmas fotos na mesma ordem (o caso comum de uma atualização de
    // fundo): troca os dados e avisa só as células diferentes.
    bool sameOrder = rows.size() == m_rows.size();
    for (qsizetype i = 0; sameOrder && i < rows.size(); ++i)
        sameOrder = rows[i].id == m_rows[i].id;
    if (sameOrder) {
        int first = -1, last = -1;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            if (sameCell(rows[i], m_rows[i]))
                continue;
            m_rows[i] = rows[i];
            if (first < 0)
                first = int(i);
            last = int(i);
        }
        if (first >= 0)
            emit dataChanged(index(first), index(last));
        return;
    }
    const qsizetype oldCount = m_rows.size();
    QString anchorId = idAt(m_anchor);
    beginResetModel();
    m_rows = rows;
    m_index.clear();
    m_index.reserve(m_rows.size());
    for (qsizetype i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows[i].id, int(i));
    // A seleção fica só com o que continua na grade.
    const qsizetype before = m_selected.size();
    m_selected.removeIf([this](const QString &id) { return !m_index.contains(id); });
    m_anchor = m_index.value(anchorId, -1);
    endResetModel();
    if (oldCount != m_rows.size())
        emit countChanged();
    if (before != m_selected.size())
        emit selectionChanged();
}

QString PhotoGridModel::idAt(int index) const
{
    return index >= 0 && index < m_rows.size() ? m_rows.at(index).id : QString();
}

int PhotoGridModel::indexOf(const QString &photoId) const
{
    return m_index.value(photoId, -1);
}

void PhotoGridModel::emitSelected(int from, int to)
{
    if (from > to || m_rows.isEmpty())
        return;
    emit dataChanged(index(std::max(0, from)), index(std::min(int(m_rows.size()) - 1, to)), {SelectedRole});
}

void PhotoGridModel::click(int idx, int modifiers)
{
    if (idx < 0 || idx >= m_rows.size())
        return;
    const QString id = m_rows.at(idx).id;
    if (modifiers & Qt::ShiftModifier && m_anchor >= 0) {
        const int from = std::min(m_anchor, idx), to = std::max(m_anchor, idx);
        if (!(modifiers & Qt::ControlModifier))
            m_selected.clear();
        for (int i = from; i <= to; ++i)
            m_selected.insert(m_rows.at(i).id);
        emitSelected(0, int(m_rows.size()) - 1);
    } else if (modifiers & (Qt::ControlModifier | Qt::MetaModifier)) {
        if (!m_selected.remove(id))
            m_selected.insert(id);
        m_anchor = idx;
        emitSelected(idx, idx);
    } else {
        m_selected.clear();
        m_selected.insert(id);
        m_anchor = idx;
        emitSelected(0, int(m_rows.size()) - 1);
    }
    emit selectionChanged();
}

void PhotoGridModel::selectAll()
{
    for (const pix::GridRow &r : m_rows)
        m_selected.insert(r.id);
    emitSelected(0, int(m_rows.size()) - 1);
    emit selectionChanged();
}

void PhotoGridModel::clearSelection()
{
    if (m_selected.isEmpty())
        return;
    m_selected.clear();
    emitSelected(0, int(m_rows.size()) - 1);
    emit selectionChanged();
}

bool PhotoGridModel::isSelected(int index) const
{
    return m_selected.contains(idAt(index));
}

QStringList PhotoGridModel::selectedIds(int fallback) const
{
    QStringList out;
    for (const pix::GridRow &r : m_rows)
        if (m_selected.contains(r.id))
            out << r.id;
    if (out.isEmpty() && fallback >= 0 && fallback < m_rows.size())
        out << m_rows.at(fallback).id;
    return out;
}

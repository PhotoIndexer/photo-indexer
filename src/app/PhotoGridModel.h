// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// As fotos da grade (GridView do QML) e a seleção delas.
//
// Seleção como nos gerenciadores de arquivo: clique seleciona só a foto; com
// Ctrl (⌘ no macOS) liga/desliga; com Shift estende do último clicado.

#include "core/Catalog.h"

#include <QAbstractListModel>
#include <QSet>

class PhotoGridModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    /// A última troca de linhas foi uma atualização de fundo (indexando): a
    /// grade mantém a rolagem em vez de voltar ao topo.
    Q_PROPERTY(bool keepPosition READ keepPosition CONSTANT)

public:
    enum Role {
        PhotoIdRole = Qt::UserRole + 1,
        NameRole,
        ThumbUrlRole,
        AspectRole,
        IsRawRole,
        TakenAtRole,
        SelectedRole,
        ScoreRole,
        StateRole,
        IsPrivateRole,
        FolderRole,             ///< só nas listas ranqueadas (cópias, busca por IA)
        SizeTextRole,           ///< "6000×4000"
        LockedRole,             ///< .photoidxe de outra chave, nunca aberto
    };

    explicit PhotoGridModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Troca as linhas, mantendo a seleção das fotos que continuam.
    /// Mesma ordem de fotos: só as células que mudaram (miniatura que
    /// chegou, estado) são redesenhadas — sem reset, sem perder a rolagem.
    void setRows(const QList<pix::GridRow> &rows, bool keepPosition = false);
    bool keepPosition() const { return m_keepPosition; }
    int count() const { return int(m_rows.size()); }
    int selectedCount() const { return int(m_selected.size()); }

    Q_INVOKABLE QString idAt(int index) const;
    Q_INVOKABLE int indexOf(const QString &photoId) const;
    /// .photoidxe "trancado" (outra chave): abrir pede a chave.
    Q_INVOKABLE bool isLocked(const QString &photoId) const;
    /// Clique numa foto com os modificadores do evento (Qt.KeyboardModifiers).
    Q_INVOKABLE void click(int index, int modifiers);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool isSelected(int index) const;
    /// A seleção na ordem da grade; sem seleção, a foto ``fallback``.
    Q_INVOKABLE QStringList selectedIds(int fallback = -1) const;

signals:
    void countChanged();
    void selectionChanged();

private:
    void emitSelected(int from, int to);

    QList<pix::GridRow> m_rows;
    QHash<QString, int> m_index;
    QSet<QString> m_selected;
    int m_anchor = -1;
    bool m_keepPosition = false;
};

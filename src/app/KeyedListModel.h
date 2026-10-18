// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Base das listas do QML que se atualizam sozinhas (pessoas, árvores do
// painel): em vez de refazer o modelo inteiro (beginResetModel), que manda a
// view de volta ao topo a cada atualização de fundo, leva as linhas atuais às
// novas com remoções, movimentos, inserções e dataChanged só do que mudou —
// a rolagem, o foco e as células visíveis ficam onde estavam.
//
//   updateRows(m_rows, std::move(next), [](const QVariantMap &r) { return r.value("id").toString(); });
//
// As chaves são únicas na lista. Linha igual (operator==) não avisa nada.

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>

class KeyedListModel : public QAbstractListModel
{
public:
    using QAbstractListModel::QAbstractListModel;

protected:
    template<typename Row, typename KeyFn>
    void updateRows(QList<Row> &rows, QList<Row> next, KeyFn key)
    {
        // Lista vazia (primeira carga) ou esvaziada: nada a preservar.
        if (rows.isEmpty() || next.isEmpty()) {
            if (rows.isEmpty() && next.isEmpty())
                return;
            beginResetModel();
            rows = std::move(next);
            endResetModel();
            return;
        }
        QSet<QString> nextKeys;
        nextKeys.reserve(next.size());
        for (const Row &r : next)
            nextKeys.insert(key(r));

        // 1. Sai o que não está mais (em blocos, de trás para frente).
        for (qsizetype i = rows.size() - 1; i >= 0;) {
            if (nextKeys.contains(key(rows[i]))) {
                --i;
                continue;
            }
            const qsizetype last = i;
            while (i >= 0 && !nextKeys.contains(key(rows[i])))
                --i;
            beginRemoveRows({}, int(i + 1), int(last));
            rows.remove(i + 1, last - i);
            endRemoveRows();
        }
        QSet<QString> present;
        present.reserve(rows.size());
        for (const Row &r : std::as_const(rows))
            present.insert(key(r));

        // 2. Posição por posição: igual fica, existente vem de onde está,
        // nova entra.
        for (qsizetype i = 0; i < next.size(); ++i) {
            const QString k = key(next[i]);
            if (i < rows.size() && key(rows[i]) == k) {
                if (!(rows[i] == next[i])) {
                    rows[i] = next[i];
                    emit dataChanged(index(int(i)), index(int(i)));
                }
                continue;
            }
            qsizetype from = -1;
            if (present.contains(k))
                for (qsizetype t = i + 1; t < rows.size(); ++t)
                    if (key(rows[t]) == k) {
                        from = t;
                        break;
                    }
            if (from >= 0) {
                beginMoveRows({}, int(from), int(from), {}, int(i));
                rows.move(from, i);
                endMoveRows();
                if (!(rows[i] == next[i])) {
                    rows[i] = next[i];
                    emit dataChanged(index(int(i)), index(int(i)));
                }
            } else {
                beginInsertRows({}, int(i), int(i));
                rows.insert(i, next[i]);
                endInsertRows();
            }
        }
    }
};

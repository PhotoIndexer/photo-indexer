// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// As pessoas da tela Pessoas, como modelo de lista (milhares de pessoas). A
// lista em JavaScript (QVariantList filtrada no QML) convertia cada pessoa
// num objeto JS a cada mudança e prendia a interface por segundos; aqui o
// filtro por nome é em C++ e a grade só cria as células visíveis.

#include "KeyedListModel.h"

#include <QVariantList>

class PeopleListModel : public KeyedListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /// Mostra só quem tem isto no nome (sem diferenciar maiúsculas e acentos).
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Role {
        PersonRole = Qt::UserRole + 1,  ///< o mapa inteiro (id, label, name, coverUrl, photoCount...)
    };

    explicit PeopleListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Troca as pessoas (o filtro continua valendo).
    void setPeople(const QVariantList &people);
    int count() const { return int(m_rows.size()); }
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

signals:
    void countChanged();
    void filterChanged();

private:
    void rebuild();

    QVariantList m_all;
    QList<QVariantMap> m_rows;      ///< as que passam no filtro
    QString m_filter;
};

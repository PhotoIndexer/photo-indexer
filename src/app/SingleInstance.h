// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Um PhotoIDX por catálogo. Dois clicaram num .photoidxe com o app aberto: o
// segundo processo entrega os caminhos ao primeiro (QLocalServer) e sai —
// dois processos no mesmo banco fariam o mesmo trabalho duas vezes. No macOS
// o sistema já reaproveita o app aberto (QFileOpenEvent); isto vale para o
// Windows e o Linux.

#include <QLocalServer>
#include <QObject>
#include <QStringList>

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    /// ``key`` identifica o catálogo (a pasta de dados).
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    /// Já há um aberto: manda ``args`` para ele. true = entregue (este sai).
    bool forward(const QStringList &args);
    /// Este é o primeiro: passa a receber os dos próximos.
    void listen();

signals:
    void received(const QStringList &args);

private:
    QString m_name;
    QLocalServer m_server;
};

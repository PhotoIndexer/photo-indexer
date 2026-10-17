// SPDX-License-Identifier: BSD-2-Clause
#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcInstance, "photoidx.instance")

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_name(QStringLiteral("photoidx-")
             + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(16)))
{
}

bool SingleInstance::forward(const QStringList &args)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(500))
        return false;
    socket.write(QJsonDocument(QJsonArray::fromStringList(args)).toJson(QJsonDocument::Compact));
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    qCInfo(lcInstance) << "Handed" << args.size() << "path(s) to the running PhotoIDX";
    return true;
}

void SingleInstance::listen()
{
    // Um socket que sobrou de um app que caiu (Unix) impediria o listen.
    QLocalServer::removeServer(m_name);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(m_name)) {
        qCWarning(lcInstance) << "Single-instance server not started:" << m_server.errorString();
        return;
    }
    connect(&m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            auto *buffer = new QByteArray;
            connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer]() { buffer->append(socket->readAll()); });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer]() {
                buffer->append(socket->readAll());
                QStringList args;
                for (const QJsonValue &v : QJsonDocument::fromJson(*buffer).array())
                    args << v.toString();
                delete buffer;
                socket->deleteLater();
                emit received(args);
            });
        }
    });
}

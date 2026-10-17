// SPDX-License-Identifier: BSD-2-Clause
#include "core/CatalogReader.h"

#include <QLoggingCategory>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>

#include <atomic>

Q_LOGGING_CATEGORY(lcReader, "photoidx.reader")

namespace pix {

CatalogReader::CatalogReader(const Catalog &main, QObject *parent)
    : QObject(parent)
    , m_main(main)
{
    if (main.db().path().isEmpty())
        return;  // em memória: lê no próprio principal
    m_thread.setObjectName(QStringLiteral("catalog-reader"));
    m_thread.start();
    m_worker = new QObject;
    m_worker->moveToThread(&m_thread);
    // A conexão nasce na thread que vai usá-la (QSqlDatabase não cruza threads).
    bool ok = false;
    QMetaObject::invokeMethod(
        m_worker,
        [this, &ok]() {
            try {
                auto catalog = std::make_unique<Catalog>();
                catalog->openReaderOf(m_main);
                m_catalog = std::move(catalog);
                ok = true;
            } catch (const std::exception &e) {
                qCWarning(lcReader) << "Read-only catalog not opened; reading on the main thread:" << e.what();
            }
        },
        Qt::BlockingQueuedConnection);
    if (!ok) {
        m_thread.quit();
        m_thread.wait();
        delete m_worker;
        m_worker = nullptr;
        return;
    }
    // Checkpoint fora da thread principal (ver o cabeçalho).
    m_main.db().exec(QStringLiteral("PRAGMA wal_autocheckpoint = 0"));
    const QString path = m_main.db().path();
    QMetaObject::invokeMethod(
        m_worker,
        [this, path]() {
            static std::atomic<int> counter{0};
            m_checkpointConnection = QStringLiteral("photoidx-checkpoint-%1").arg(counter++);
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_checkpointConnection);
            db.setDatabaseName(path);
            db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=1000"));
            if (!db.open()) {
                qCWarning(lcReader) << "Checkpoint connection not opened:" << db.lastError().text();
                return;
            }
            auto *timer = new QTimer(m_worker);
            timer->setInterval(15000);
            QObject::connect(timer, &QTimer::timeout, m_worker, [name = m_checkpointConnection]() {
                PIX_PERF("CatalogReader::checkpoint");
                QSqlQuery q(QSqlDatabase::database(name, false));
                q.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)"));
            });
            timer->start();
        },
        Qt::BlockingQueuedConnection);
}

CatalogReader::~CatalogReader()
{
    if (!m_worker)
        return;
    // O catálogo (e as conexões) morrem na thread deles; um último checkpoint
    // deixa o WAL curto para a próxima abertura.
    QMetaObject::invokeMethod(
        m_worker,
        [this]() {
            m_catalog.reset();
            if (!m_checkpointConnection.isEmpty()) {
                {
                    QSqlQuery q(QSqlDatabase::database(m_checkpointConnection, false));
                    q.exec(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)"));
                }
                QSqlDatabase::removeDatabase(m_checkpointConnection);
            }
        },
        Qt::BlockingQueuedConnection);
    m_main.db().exec(QStringLiteral("PRAGMA wal_autocheckpoint = 1000"));
    m_worker->deleteLater();
    m_thread.quit();
    m_thread.wait();
}

} // namespace pix

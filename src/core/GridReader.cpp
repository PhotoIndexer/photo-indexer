// SPDX-License-Identifier: BSD-2-Clause
#include "core/GridReader.h"
#include "core/Perf.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QThread>

#include <atomic>

namespace pix {

QList<GridRow> readGridRows(const QString &databasePath, const QString &sql, const QVariantList &params)
{
    PIX_PERF("readGridRows");
    // Uma conexão por thread (QSqlDatabase não cruza threads); a thread do
    // leitor vive o app inteiro.
    static thread_local QString connection;
    if (connection.isEmpty()) {
        static std::atomic<int> counter{0};
        connection = QStringLiteral("photoidx-reader-%1").arg(counter++);
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(databasePath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000"));
        if (!db.open())
            throw DatabaseError(db.lastError().text().toStdString());
    }
    QSqlQuery q(QSqlDatabase::database(connection, false));
    q.setForwardOnly(true);
    if (!q.prepare(sql))
        throw DatabaseError(q.lastError().text().toStdString());
    for (const QVariant &p : params)
        q.addBindValue(p);
    if (!q.exec())
        throw DatabaseError(q.lastError().text().toStdString());
    QList<GridRow> out;
    while (q.next())
        out.append(Catalog::gridRow(q));
    return out;
}

} // namespace pix

// SPDX-License-Identifier: BSD-2-Clause
#include "core/Database.h"

#include <QFile>
#include <QLoggingCategory>
#include <QSqlError>
#include <QTimeZone>
#include <QUuid>

#include <atomic>

Q_LOGGING_CATEGORY(lcDb, "photoidx.db")

namespace pix {

namespace {

// Passos do esquema (src/core/schema). Acrescente o próximo; nunca edite um
// já publicado.
constexpr int kSchemaSteps = 2;

/// Comandos de um passo: linhas acumuladas até uma que termina em ';'
/// (comentários "--" e linhas vazias fora).
QStringList splitStatements(const QString &script)
{
    QStringList out;
    QString current;
    for (const QString &line : script.split(u'\n')) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1String("--")))
            continue;
        current += (current.isEmpty() ? QString() : QStringLiteral(" ")) + trimmed;
        if (trimmed.endsWith(u';')) {
            out << current;
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty())
        out << current;
    return out;
}

void bindAll(QSqlQuery &q, const auto &params)
{
    for (const QVariant &p : params) {
        // QString nula viraria NULL: texto é sempre texto; NULL de verdade é
        // um QVariant() vazio.
        if (p.typeId() == QMetaType::QString && p.toString().isNull())
            q.addBindValue(QStringLiteral(""));
        else
            q.addBindValue(p);
    }
}

} // namespace

Database::~Database()
{
    close();
}

void Database::open(const QString &path)
{
    close();
    static std::atomic<int> counter{0};
    m_connection = QStringLiteral("photoidx-%1").arg(counter++);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(path.isEmpty() ? QStringLiteral(":memory:") : path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
    if (!db.open())
        throw DatabaseError(db.lastError().text().toStdString());
    exec(QStringLiteral("PRAGMA foreign_keys = ON"));
    if (!path.isEmpty()) {
        exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        exec(QStringLiteral("PRAGMA synchronous = NORMAL"));
    }
    migrate();
}

void Database::close()
{
    if (m_connection.isEmpty())
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connection, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connection);
    m_connection.clear();
}

bool Database::isOpen() const
{
    return !m_connection.isEmpty() && handle().isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connection, false);
}

int Database::schemaVersion() const
{
    return scalar(QStringLiteral("PRAGMA user_version")).toInt();
}

int Database::latestSchemaVersion()
{
    return kSchemaSteps;
}

QSqlQuery Database::exec(const QString &sql, std::initializer_list<QVariant> params) const
{
    return exec(sql, QVariantList(params));
}

QSqlQuery Database::exec(const QString &sql, const QVariantList &params) const
{
    QSqlQuery q(handle());
    q.setForwardOnly(true);
    const auto fail = [&]() {
        const QString message = q.lastError().text() + QStringLiteral(" | ") + sql;
        qCWarning(lcDb).noquote() << message;
        throw DatabaseError(message.toStdString());
    };
    if (!q.prepare(sql))
        fail();
    bindAll(q, params);
    if (!q.exec())
        fail();
    return q;
}

QVariant Database::scalar(const QString &sql, std::initializer_list<QVariant> params) const
{
    return scalar(sql, QVariantList(params));
}

QVariant Database::scalar(const QString &sql, const QVariantList &params) const
{
    QSqlQuery q = exec(sql, params);
    return q.next() ? q.value(0) : QVariant();
}

void Database::transaction()
{
    if (!handle().transaction())
        throw DatabaseError(handle().lastError().text().toStdString());
}

void Database::commit()
{
    if (!handle().commit())
        throw DatabaseError(handle().lastError().text().toStdString());
}

void Database::rollback()
{
    handle().rollback();
}

QString Database::newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString Database::timestamp(const QDateTime &when)
{
    return when.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime Database::parseTimestamp(const QVariant &value)
{
    if (value.isNull())
        return {};
    QDateTime dt = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(value.toString(), Qt::ISODate);
    return dt;
}

void Database::migrate()
{
    const int version = schemaVersion();
    if (version > kSchemaSteps)
        throw DatabaseError("This catalog was created by a newer version of PhotoIDX.");
    for (int step = version + 1; step <= kSchemaSteps; ++step) {
        QFile file(QStringLiteral(":/photoidx/schema/%1.sql").arg(step, 4, 10, QLatin1Char('0')));
        if (!file.open(QIODevice::ReadOnly))
            throw DatabaseError(("Missing schema step " + std::to_string(step)));
        const QStringList statements = splitStatements(QString::fromUtf8(file.readAll()));
        transaction();
        try {
            for (const QString &sql : statements)
                exec(sql);
            exec(QStringLiteral("PRAGMA user_version = %1").arg(step));
            commit();
        } catch (...) {
            rollback();
            throw;
        }
        qCInfo(lcDb) << "Catalog schema migrated to" << step;
    }
}

Transaction::Transaction(Database &db)
    : m_db(db)
{
    m_db.transaction();
}

Transaction::~Transaction()
{
    if (!m_done)
        m_db.rollback();
}

void Transaction::commit()
{
    m_db.commit();
    m_done = true;
}

} // namespace pix

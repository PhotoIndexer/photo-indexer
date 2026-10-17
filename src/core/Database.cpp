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
constexpr int kSchemaSteps = 7;

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
    m_path = path;
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
        // O WAL não encolhe sozinho: depois de um lote grande (reagrupar,
        // refazer categorias) ficava com centenas de MB, e abrir o app depois
        // de um fechamento sujo relia tudo. Volta a 64 MB a cada checkpoint.
        exec(QStringLiteral("PRAGMA journal_size_limit = 67108864"));
        tuneCache();
    }
    migrate();
}

void Database::openReadOnly(const QString &path)
{
    close();
    m_path = path;
    static std::atomic<int> counter{0};
    m_connection = QStringLiteral("photoidx-ro-%1").arg(counter++);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=5000"));
    if (!db.open())
        throw DatabaseError(db.lastError().text().toStdString());
    tuneCache();
}

void Database::tuneCache()
{
    // Catálogos de centenas de MB: o cache padrão do SQLite (2 MB) relia o
    // arquivo a cada varredura. 64 MB de páginas e leitura por mmap.
    exec(QStringLiteral("PRAGMA cache_size = -65536"));
    exec(QStringLiteral("PRAGMA mmap_size = 268435456"));
    exec(QStringLiteral("PRAGMA temp_store = MEMORY"));
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

// Transações aninham: a de fora é BEGIN/COMMIT; as de dentro, SAVEPOINTs
// (um lote de resultados numa transação só, com cada applyX abrindo a sua).
void Database::transaction()
{
    if (m_depth == 0) {
        if (!handle().transaction())
            throw DatabaseError(handle().lastError().text().toStdString());
    } else {
        exec(QStringLiteral("SAVEPOINT tx%1").arg(m_depth));
    }
    ++m_depth;
}

void Database::commit()
{
    --m_depth;
    if (m_depth == 0) {
        if (!handle().commit())
            throw DatabaseError(handle().lastError().text().toStdString());
    } else {
        exec(QStringLiteral("RELEASE tx%1").arg(m_depth));
    }
}

void Database::rollback()
{
    --m_depth;
    if (m_depth == 0) {
        handle().rollback();
    } else {
        exec(QStringLiteral("ROLLBACK TO tx%1").arg(m_depth));
        exec(QStringLiteral("RELEASE tx%1").arg(m_depth));
    }
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

// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O catálogo SQLite: ~/.photo_indexer/data.sqlite3.
//
// O esquema mora em src/core/schema/NNNN.sql (embutido no binário) e migra de
// forma INCREMENTAL pelo PRAGMA user_version (regra 8): o banco é dado real e
// persiste entre versões.
//
// Só a thread principal usa o banco (regra 9): trabalho em segundo plano
// recebe os dados prontos e devolve o resultado por sinal.

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

#include <initializer_list>
#include <stdexcept>

namespace pix {

class DatabaseError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class Database
{
public:
    Database() = default;
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /// Abre (cria e migra, se preciso). ``path`` vazio = banco em memória.
    void open(const QString &path);
    void close();
    bool isOpen() const;

    QSqlDatabase handle() const;
    int schemaVersion() const;

    /// Prepara, liga os parâmetros posicionais e executa; lança DatabaseError.
    QSqlQuery exec(const QString &sql, std::initializer_list<QVariant> params = {}) const;
    QSqlQuery exec(const QString &sql, const QVariantList &params) const;
    /// Primeira coluna da primeira linha (ou inválido).
    QVariant scalar(const QString &sql, std::initializer_list<QVariant> params = {}) const;
    QVariant scalar(const QString &sql, const QVariantList &params) const;

    void transaction();
    void commit();
    void rollback();

    /// Número de passos de esquema que o app conhece.
    static int latestSchemaVersion();

    /// Id novo: UUID em minúsculas com hífens ("3f2a...-...").
    static QString newId();
    /// Instante em UTC, ISO 8601 ("2026-10-16T21:24:30.123Z").
    static QString timestamp(const QDateTime &when = QDateTime::currentDateTimeUtc());
    static QDateTime parseTimestamp(const QVariant &value);

private:
    void migrate();

    QString m_connection;
};

/// Transação RAII: desfaz se sair do escopo sem commit().
class Transaction
{
public:
    explicit Transaction(Database &db);
    ~Transaction();
    void commit();

private:
    Database &m_db;
    bool m_done = false;
};

} // namespace pix

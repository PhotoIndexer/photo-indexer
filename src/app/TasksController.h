// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// As tarefas de indexação (App.tasks no QML): o andamento de cada etapa do
// Pipeline, os controles (iniciar, pausar, cancelar, threads), os erros, a
// manutenção (refazer uma etapa inteira) e os ajustes da IA.

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

namespace pix {
class Catalog;
class Pipeline;
}

class TasksController : public QObject
{
    Q_OBJECT
    /// [{key, state, done, failed, pending, inFlight, threads, current, rate, detail}]
    Q_PROPERTY(QVariantList stages READ stages NOTIFY changed)
    /// {photos, indexed, raw, errors, missing, categorized, facesDone, faces, people, named, folders, disks}
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    /// Total pendente em todas as etapas (o cabeçalho mostra).
    Q_PROPERTY(int pending READ pending NOTIFY changed)
    Q_PROPERTY(QVariantList errors READ errors NOTIFY errorsChanged)
    /// Ajustes: {minProbability, maxCategories, blurThreshold, minPersonScore, matchSimilarity,
    /// mergeSimilarity, mergeMinFaces, mergeMinScore}
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)

public:
    TasksController(pix::Catalog *catalog, pix::Pipeline *pipeline, QObject *parent = nullptr);

    QVariantList stages() const { return m_stages; }
    QVariantMap stats() const { return m_stats; }
    bool busy() const;
    int pending() const;
    QVariantList errors() const { return m_errors; }
    QVariantMap settings() const;

    Q_INVOKABLE void start(const QString &stage);
    Q_INVOKABLE void pause(const QString &stage);
    Q_INVOKABLE void cancel(const QString &stage);
    Q_INVOKABLE void setThreads(const QString &stage, int threads);
    Q_INVOKABLE void startAll();
    Q_INVOKABLE void pauseAll();
    Q_INVOKABLE void scanAll();
    /// Volta uma etapa para a fila: ``scope`` "errors" ou "all".
    Q_INVOKABLE void requeue(const QString &stage, const QString &scope);
    Q_INVOKABLE void reclusterPeople();
    Q_INVOKABLE void mergePeopleNow();
    Q_INVOKABLE void refreshErrors();
    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);

signals:
    void changed();
    void errorsChanged();
    void settingsChanged();
    /// Mensagem curta para o rodapé/aviso (chave de i18n + parâmetros).
    void notice(const QString &key, const QVariantMap &params);

private:
    void update();

    pix::Catalog *m_catalog;
    pix::Pipeline *m_pipeline;
    QVariantList m_stages;
    QVariantMap m_stats;
    QVariantList m_errors;
    QTimer m_statsTimer;
};

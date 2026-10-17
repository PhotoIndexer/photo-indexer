// SPDX-License-Identifier: BSD-2-Clause
#include "TasksController.h"

#include "core/Catalog.h"
#include "core/Pipeline.h"
#include "imaging/Faces.h"
#include "imaging/Memory.h"

using namespace pix;

namespace {

/// Chave da preferência no catálogo e o valor padrão de cada ajuste.
struct Setting
{
    const char *name;
    const char *key;
    double fallback;
};

const Setting kSettings[] = {
    {"minProbability", "ai/minProbability", 0.15},
    {"maxCategories", "ai/maxCategories", 3},
    {"blurThreshold", "ai/blurThreshold", 40},
    {"minPersonScore", "faces/minPersonScore", 0.5},
    {"matchSimilarity", "faces/matchSimilarity", double(imaging::kFaceMatchSimilarity)},
    {"mergeSimilarity", "faces/mergeSimilarity", 0.55},
    {"mergeMinFaces", "faces/mergeMinFaces", 3},
    {"mergeMinScore", "faces/mergeMinScore", 0.6},
};

} // namespace

TasksController::TasksController(Catalog *catalog, Pipeline *pipeline, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
    , m_pipeline(pipeline)
{
    connect(pipeline, &Pipeline::statusChanged, this, &TasksController::update);
    // Os totais do catálogo varrem a tabela (~60 ms com 70 mil fotos): no
    // máximo a cada 3 s.
    m_statsTimer.setInterval(3000);
    m_statsTimer.setSingleShot(true);
    connect(&m_statsTimer, &QTimer::timeout, this, [this]() {
        m_stats = m_catalog->stats();
        emit changed();
    });
    connect(catalog, &Catalog::photosChanged, this, [this]() {
        if (!m_statsTimer.isActive())
            m_statsTimer.start();
    });
    connect(pipeline, &Pipeline::scanFinished, this, [this](const QString &, int found, int queued, int missing) {
        emit notice(QStringLiteral("scan.finished"), {{QStringLiteral("found"), found},
                                                      {QStringLiteral("queued"), queued},
                                                      {QStringLiteral("missing"), missing}});
    });
    m_stats = catalog->stats();
    update();
}

void TasksController::update()
{
    m_stages = m_pipeline->status();
    emit changed();
}

bool TasksController::busy() const
{
    return m_pipeline->busy();
}

int TasksController::pending() const
{
    int total = 0;
    for (const QVariant &s : m_stages) {
        const QVariantMap m = s.toMap();
        if (m.value(QStringLiteral("state")).toString() != QLatin1String("unavailable"))
            total += m.value(QStringLiteral("pending")).toInt() + m.value(QStringLiteral("inFlight")).toInt();
    }
    return total;
}

void TasksController::start(const QString &stage)
{
    m_pipeline->start(stage);
}

void TasksController::pause(const QString &stage)
{
    m_pipeline->pause(stage);
}

void TasksController::cancel(const QString &stage)
{
    m_pipeline->cancel(stage);
}

void TasksController::setThreads(const QString &stage, int threads)
{
    m_pipeline->setThreads(stage, threads);
}

int TasksController::maxTasks() const
{
    return m_pipeline->maxTasks();
}

int TasksController::runningTasks() const
{
    int total = 0;
    for (const QVariant &v : m_stages)
        total += v.toMap().value(QStringLiteral("inFlight")).toInt();
    return total;
}

QVariantMap TasksController::memory() const
{
    const auto &budget = imaging::MemoryBudget::instance();
    constexpr qint64 kMB = 1024 * 1024;
    return {{QStringLiteral("capacity"), budget.capacity() / kMB}, {QStringLiteral("used"), budget.used() / kMB}};
}

void TasksController::setMaxTasks(int tasks)
{
    m_pipeline->setMaxTasks(tasks);
}

void TasksController::startAll()
{
    m_pipeline->startAll();
}

void TasksController::pauseAll()
{
    m_pipeline->pauseAll();
}

void TasksController::scanAll()
{
    m_catalog->refreshDisks();
    m_pipeline->scan();
}

void TasksController::requeue(const QString &stage, const QString &scope)
{
    const int n = m_catalog->requeue(stage, scope);
    if (stage == QLatin1String("categorize"))
        m_pipeline->invalidatePrompts();
    m_pipeline->start(stage);
    emit notice(QStringLiteral("tasks.requeued"), {{QStringLiteral("count"), n}, {QStringLiteral("stage"), stage}});
    refreshErrors();
}

void TasksController::reclusterPeople()
{
    m_catalog->reclusterPeople();
    emit notice(QStringLiteral("people.reclustered"), {});
}

void TasksController::mergePeopleNow()
{
    const int n = m_catalog->autoMergePeople();
    emit notice(QStringLiteral("people.merged"), {{QStringLiteral("count"), n}});
}

void TasksController::refreshErrors()
{
    m_errors = m_catalog->errors(500);
    emit errorsChanged();
}

QVariantMap TasksController::settings() const
{
    QVariantMap out;
    for (const Setting &s : kSettings)
        out.insert(QString::fromLatin1(s.name), m_catalog->setting(QString::fromLatin1(s.key), s.fallback).toDouble());
    return out;
}

void TasksController::setSetting(const QString &name, const QVariant &value)
{
    for (const Setting &s : kSettings) {
        if (name != QLatin1String(s.name))
            continue;
        m_catalog->setSetting(QString::fromLatin1(s.key), value.toDouble());
        emit settingsChanged();
        return;
    }
}

// SPDX-License-Identifier: BSD-2-Clause
#include "core/Perf.h"

#include "core/Paths.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <thread>

namespace pix::perf {

namespace {

constexpr qint64 kMaxBytes = 5 * 1024 * 1024;
constexpr int kBeatMs = 50;
constexpr int kStallMs = 250;

/// Relógio monotônico compartilhado (ms desde o início).
qint64 nowMs()
{
    static QElapsedTimer clock = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return clock.elapsed();
}

struct Stat
{
    qint64 count = 0;
    double totalMs = 0;
    double maxMs = 0;
};

struct State
{
    QMutex lock;                                ///< arquivo e estatísticas
    QFile file;
    QHash<QString, Stat> mainStats;             ///< thread principal
    QHash<QString, Stat> workerStats;           ///< as outras
    std::atomic<const char *> current{nullptr}; ///< escopo aberto na thread principal
    std::atomic<qint64> lastBeat{0};
    std::atomic<bool> running{false};
    std::thread watcher;
    QTimer *beat = nullptr;
    QTimer *summary = nullptr;

    // Saída sem stopWatchdog() (o main volta cedo: outra instância já
    // aberta, a interface não carregou): uma std::thread ainda "joinable"
    // no destrutor estático chama std::terminate e o app cai ao sair.
    ~State()
    {
        running.store(false);
        if (watcher.joinable())
            watcher.join();
    }
};

State &state()
{
    static State s;
    return s;
}

void writeLocked(State &s, const QString &line)
{
    if (!s.file.isOpen())
        return;
    if (s.file.size() > kMaxBytes) {
        const QString base = s.file.fileName();
        s.file.close();
        QFile::remove(base + QStringLiteral(".1"));
        QFile::rename(base, base + QStringLiteral(".1"));
        s.file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
    const QByteArray out = (QDateTime::currentDateTime().toString(QStringLiteral("[yyyy-MM-dd HH:mm:ss.zzz] ")) + line + u'\n').toUtf8();
    s.file.write(out);
    s.file.flush();
}

void add(QHash<QString, Stat> &stats, const char *label, double ms)
{
    Stat &st = stats[QString::fromLatin1(label)];
    ++st.count;
    st.totalMs += ms;
    st.maxMs = std::max(st.maxMs, ms);
}

void writeSummary()
{
    State &s = state();
    QMutexLocker locker(&s.lock);
    auto top = [](const QHash<QString, Stat> &stats) {
        QList<QPair<QString, Stat>> list;
        for (auto it = stats.constBegin(); it != stats.constEnd(); ++it)
            list.append({it.key(), it.value()});
        std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.second.totalMs > b.second.totalMs; });
        return list.mid(0, 15);
    };
    const auto mainTop = top(s.mainStats);
    if (!mainTop.isEmpty()) {
        writeLocked(s, QStringLiteral("summary (main thread, last 60 s): label | calls | total ms | avg ms | max ms"));
        for (const auto &[label, st] : mainTop)
            writeLocked(s, QStringLiteral("  %1 | %2 | %3 | %4 | %5")
                               .arg(label, -36)
                               .arg(st.count)
                               .arg(st.totalMs, 0, 'f', 0)
                               .arg(st.totalMs / std::max<qint64>(1, st.count), 0, 'f', 1)
                               .arg(st.maxMs, 0, 'f', 0));
    }
    const auto workerTop = top(s.workerStats);
    if (!workerTop.isEmpty()) {
        writeLocked(s, QStringLiteral("summary (workers, last 60 s): label | calls | total ms | avg ms | max ms"));
        for (const auto &[label, st] : workerTop.mid(0, 8))
            writeLocked(s, QStringLiteral("  %1 | %2 | %3 | %4 | %5")
                               .arg(label, -36)
                               .arg(st.count)
                               .arg(st.totalMs, 0, 'f', 0)
                               .arg(st.totalMs / std::max<qint64>(1, st.count), 0, 'f', 1)
                               .arg(st.maxMs, 0, 'f', 0));
    }
    s.mainStats.clear();
    s.workerStats.clear();
}

/// A thread que vigia o batimento da principal.
void watch()
{
    State &s = state();
    bool stalled = false;
    qint64 stallStart = 0;
    qint64 lastReport = 0;
    QSet<QString> seen;
    while (s.running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const qint64 now = nowMs();
        const qint64 gap = now - s.lastBeat.load();
        const char *label = s.current.load();
        const QString where = label ? QString::fromLatin1(label) : QStringLiteral("(fora de escopo medido: QML/desenho/eventos)");
        if (gap > kStallMs) {
            if (!stalled) {
                stalled = true;
                stallStart = s.lastBeat.load();
                lastReport = now;
                seen.clear();
            }
            seen.insert(where);
            if (now - lastReport >= 1000) {
                lastReport = now;
                QMutexLocker locker(&s.lock);
                writeLocked(s, QStringLiteral("UI STILL BLOCKED %1 ms; now in: %2").arg(now - stallStart).arg(where));
            }
        } else if (stalled) {
            stalled = false;
            const qint64 total = s.lastBeat.load() - stallStart;
            QStringList where2(seen.cbegin(), seen.cend());
            QMutexLocker locker(&s.lock);
            writeLocked(s, QStringLiteral("UI BLOCKED %1 ms; seen running: %2").arg(total).arg(where2.join(QStringLiteral(" | "))));
        }
    }
}

} // namespace

void log(const QString &line)
{
    State &s = state();
    QMutexLocker locker(&s.lock);
    writeLocked(s, line);
}

void startWatchdog()
{
    State &s = state();
    if (s.running.load())
        return;
    {
        QMutexLocker locker(&s.lock);
        s.file.setFileName(QDir(AppPaths::logsDir()).filePath(QStringLiteral("perf.log")));
        s.file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        writeLocked(s, QStringLiteral("---- watchdog started (stall > %1 ms, slow scope > %2 ms) ----").arg(kStallMs).arg(kSlowMs));
    }
    s.lastBeat.store(nowMs());
    s.beat = new QTimer(QCoreApplication::instance());
    s.beat->setInterval(kBeatMs);
    QObject::connect(s.beat, &QTimer::timeout, [] { state().lastBeat.store(nowMs()); });
    s.beat->start();
    s.summary = new QTimer(QCoreApplication::instance());
    s.summary->setInterval(60000);
    QObject::connect(s.summary, &QTimer::timeout, &writeSummary);
    s.summary->start();
    s.running.store(true);
    s.watcher = std::thread(&watch);
}

void stopWatchdog()
{
    State &s = state();
    if (!s.running.exchange(false))
        return;
    if (s.watcher.joinable())
        s.watcher.join();
    writeSummary();
}

Scope::Scope(const char *label)
    : m_label(label)
    , m_start(nowMs())
    , m_main(QThread::isMainThread())
{
    if (m_main)
        m_parent = state().current.exchange(label);
}

Scope::~Scope()
{
    State &s = state();
    const double ms = double(nowMs() - m_start);
    if (m_main) {
        s.current.store(m_parent);
        if (ms >= 1 || ms >= kSlowMs) {
            QMutexLocker locker(&s.lock);
            add(s.mainStats, m_label, ms);
            if (ms >= kSlowMs)
                writeLocked(s, QStringLiteral("slow %1 ms  %2%3")
                                   .arg(ms, 0, 'f', 0)
                                   .arg(QLatin1String(m_label))
                                   .arg(m_parent ? QStringLiteral("  (inside %1)").arg(QLatin1String(m_parent)) : QString()));
        }
    } else if (ms >= 1) {
        QMutexLocker locker(&s.lock);
        add(s.workerStats, m_label, ms);
    }
}

} // namespace pix::perf

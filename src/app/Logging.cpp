// SPDX-License-Identifier: BSD-2-Clause
#include "Logging.h"

#include "core/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

namespace pix::app {

namespace {

constexpr qint64 kMaxBytes = 5 * 1024 * 1024;
constexpr int kBackups = 3;

QMutex g_lock;
QFile *g_file = nullptr;
bool g_console = false;

void rotate()
{
    g_file->close();
    const QString base = g_file->fileName();
    QFile::remove(base + QStringLiteral(".%1").arg(kBackups));
    for (int i = kBackups - 1; i >= 1; --i)
        QFile::rename(base + QStringLiteral(".%1").arg(i), base + QStringLiteral(".%1").arg(i + 1));
    QFile::rename(base, base + QStringLiteral(".1"));
    g_file->open(QIODevice::Append | QIODevice::Text);
}

void handler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    static const char *const kLevels[] = {"DEBUG", "WARNING", "CRITICAL", "FATAL", "INFO"};
    const QByteArray line = QStringLiteral("[%1] %2 %3: %4\n")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                     QString::fromLatin1(kLevels[type]),
                                     QString::fromLatin1(context.category ? context.category : "default"), message)
                                .toUtf8();
    QMutexLocker locker(&g_lock);
    if (g_file && g_file->isOpen()) {
        if (g_file->size() > kMaxBytes)
            rotate();
        g_file->write(line);
        g_file->flush();
    }
    if (g_console || !g_file) {
        std::fwrite(line.constData(), 1, static_cast<size_t>(line.size()), stderr);
        std::fflush(stderr);
    }
}

} // namespace

QString logFile()
{
    return QDir(AppPaths::logsDir()).filePath(QStringLiteral("app.log"));
}

void setupLogging(bool debug)
{
    const QString level = qEnvironmentVariable("PHOTOIDX_LOG_LEVEL").toLower();
    if (debug || level == QLatin1String("debug"))
        QLoggingCategory::setFilterRules(QStringLiteral("photoidx.*.debug=true"));
    else if (level == QLatin1String("warning"))
        QLoggingCategory::setFilterRules(QStringLiteral("photoidx.*.info=false"));
    else
        QLoggingCategory::setFilterRules(QStringLiteral("photoidx.*.info=true\nphotoidx.*.debug=false"));

    g_file = new QFile(logFile());
    if (!g_file->open(QIODevice::Append | QIODevice::Text)) {
        delete g_file;
        g_file = nullptr;
    }
#ifdef Q_OS_WIN
    g_console = qEnvironmentVariableIsSet("PROMPT");
#else
    g_console = isatty(fileno(stderr));
#endif
    qInstallMessageHandler(handler);
}

} // namespace pix::app

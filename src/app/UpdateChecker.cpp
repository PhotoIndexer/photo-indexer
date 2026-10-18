// SPDX-License-Identifier: BSD-2-Clause
#include "UpdateChecker.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#include <array>
#include <optional>

Q_LOGGING_CATEGORY(lcUpdates, "photoidx.updates")

namespace {

constexpr int kTimeoutMs = 15000;

std::optional<std::array<int, 3>> parseVersion(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^\\s*v?(\\d+)\\.(\\d+)\\.(\\d+)"));
    const auto m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    return std::array<int, 3>{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()};
}

} // namespace

UpdateChecker::UpdateChecker(const QString &repoUrl, const QString &current, QObject *parent)
    : QObject(parent)
    , m_current(current)
{
    // https://github.com/<dono>/<repositório>(.git)(/) -> a API da última Release.
    static const QRegularExpression re(QStringLiteral("github\\.com/([^/]+)/([^/]+?)(?:\\.git)?/?$"));
    const auto m = re.match(repoUrl);
    if (m.hasMatch())
        m_apiUrl = QStringLiteral("https://api.github.com/repos/%1/%2/releases/latest").arg(m.captured(1), m.captured(2));
    m_enabled = QSettings().value(QStringLiteral("updates/check"), true).toBool();
}

void UpdateChecker::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    QSettings().setValue(QStringLiteral("updates/check"), on);
    emit enabledChanged();
    if (on && m_latest.isEmpty())
        check();
}

bool UpdateChecker::isNewer(const QString &latest, const QString &current)
{
    const auto l = parseVersion(latest), c = parseVersion(current);
    if (!l)
        return false;
    if (!c)
        return true;   // build sem versão (0.0.0 de desenvolvimento local cai acima)
    return *l > *c;
}

void UpdateChecker::checkLater(int delayMs)
{
    if (m_enabled)
        QTimer::singleShot(delayMs, this, &UpdateChecker::check);
}

void UpdateChecker::check()
{
    if (m_checking || m_apiUrl.isEmpty())
        return;
    if (!m_network)
        m_network = new QNetworkAccessManager(this);
    m_checking = true;
    QNetworkRequest request{QUrl(m_apiUrl)};
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(QCoreApplication::applicationName(), m_current));
    request.setTransferTimeout(kTimeoutMs);
    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_checking = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            // 404 = nenhuma Release publicada; 403 = limite anônimo da API.
            qCInfo(lcUpdates) << "Update check skipped:" << status << reply->errorString();
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if (!parseVersion(tag)) {
            qCInfo(lcUpdates) << "Update check: unexpected tag" << tag;
            return;
        }
        m_latest = tag.mid(tag.startsWith(u'v') ? 1 : 0);
        m_url = release.value(QStringLiteral("html_url")).toString();
        m_notes = release.value(QStringLiteral("body")).toString();
        m_available = isNewer(m_latest, m_current);
        qCInfo(lcUpdates) << "Latest release" << m_latest << "- this build" << m_current
                          << (m_available ? "(update available)" : "(up to date)");
        emit changed();
        if (m_available && QSettings().value(QStringLiteral("updates/notified")).toString() != m_latest)
            emit updateFound(m_latest);
    });
}

void UpdateChecker::markNotified()
{
    if (!m_latest.isEmpty())
        QSettings().setValue(QStringLiteral("updates/notified"), m_latest);
}

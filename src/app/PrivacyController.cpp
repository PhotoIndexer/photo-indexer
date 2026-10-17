// SPDX-License-Identifier: BSD-2-Clause
#include "PrivacyController.h"

#include "core/Catalog.h"
#include "core/Crypto.h"
#include "core/Vault.h"

#include <QClipboard>
#include <QEvent>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QSettings>
#include <QtConcurrent/QtConcurrentRun>

using namespace pix;

namespace {

/// O que volta do trabalho lento (PBKDF2) para a thread principal.
struct Outcome
{
    QString error;              ///< chave de i18n; vazio = deu certo
    vault::Record record;       ///< para gravar (vazio = não grava)
    vault::Ring ring;
    QByteArray passwordKey;
};

} // namespace

PrivacyController::PrivacyController(Catalog *catalog, QObject *parent)
    : QObject(parent)
    , m_catalog(catalog)
{
    m_autoLockMinutes = std::clamp(QSettings().value(QStringLiteral("privacy/autoLockMinutes"), 10).toInt(), 0, 240);
    m_autoLock.setSingleShot(true);
    connect(&m_autoLock, &QTimer::timeout, this, [this]() {
        if (!m_catalog->privateUnlocked())
            return;
        m_catalog->lockVault();
        emit notice(QStringLiteral("vault.autoLocked"), {});
    });
    m_clipboardClear.setSingleShot(true);
    m_clipboardClear.setInterval(120000);
    connect(&m_clipboardClear, &QTimer::timeout, this, [this]() {
        // Só limpa se ainda for a chave (a pessoa pode ter copiado outra coisa).
        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard && clipboard->text() == m_copied)
            clipboard->clear();
        m_copied.clear();
    });
    connect(catalog, &Catalog::privacyChanged, this, [this]() {
        restartAutoLock();
        emit changed();
    });
    if (qApp)
        qApp->installEventFilter(this);
}

bool PrivacyController::hasPassword() const
{
    return m_catalog->hasVaultPassword();
}

bool PrivacyController::unlocked() const
{
    return m_catalog->privateUnlocked();
}

QString PrivacyController::albumNode() const
{
    return QStringLiteral("album:") + m_catalog->privateAlbumId();
}

QVariantMap PrivacyController::status() const
{
    return m_catalog->privacyStatus();
}

void PrivacyController::setAutoLockMinutes(int minutes)
{
    minutes = std::clamp(minutes, 0, 240);
    if (minutes == m_autoLockMinutes)
        return;
    m_autoLockMinutes = minutes;
    QSettings().setValue(QStringLiteral("privacy/autoLockMinutes"), minutes);
    restartAutoLock();
    emit autoLockChanged();
}

void PrivacyController::restartAutoLock()
{
    if (m_catalog->privateUnlocked() && m_autoLockMinutes > 0)
        m_autoLock.start(m_autoLockMinutes * 60 * 1000);
    else
        m_autoLock.stop();
}

bool PrivacyController::eventFilter(QObject *watched, QEvent *event)
{
    // Uso do app (tecla, clique, rolagem) adia o trancamento automático.
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        if (m_autoLock.isActive())
            m_autoLock.start(m_autoLockMinutes * 60 * 1000);
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

QString PrivacyController::revealKey() const
{
    const auto ring = m_catalog->currentRing();
    return ring ? crypto::formatKey(ring->current()) : QString();
}

void PrivacyController::copyKey()
{
    const QString key = revealKey();
    if (key.isEmpty()) {
        emit failed(QStringLiteral("vault.locked"), {});
        return;
    }
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(key);
        m_copied = key;
        m_clipboardClear.start();
        emit notice(QStringLiteral("vault.keyCopied"), {});
    }
}

bool PrivacyController::checkNewPassword(const QString &password, const QString &confirm)
{
    if (password.size() < minPasswordLength()) {
        emit failed(QStringLiteral("vault.passwordShort"), {{QStringLiteral("min"), minPasswordLength()}});
        return false;
    }
    if (password != confirm) {
        emit failed(QStringLiteral("vault.passwordMismatch"), {});
        return false;
    }
    return true;
}

void PrivacyController::setWorking(bool on)
{
    if (m_working == on)
        return;
    m_working = on;
    emit workingChanged();
}

namespace {

/// Roda ``work`` no pool global e entrega o resultado em ``done`` na thread
/// principal.
template<typename Work, typename Done>
void runAsync(QObject *context, Work work, Done done)
{
    auto *watcher = new QFutureWatcher<Outcome>(context);
    QObject::connect(watcher, &QFutureWatcher<Outcome>::finished, context, [watcher, done]() {
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

} // namespace

void PrivacyController::setPassword(const QString &password, const QString &confirm)
{
    if (m_working)
        return;
    if (hasPassword()) {
        emit failed(QStringLiteral("vault.passwordExists"), {});
        return;
    }
    if (!checkNewPassword(password, confirm))
        return;
    const auto ring = m_catalog->currentRing();
    if (!ring) {
        emit failed(QStringLiteral("vault.locked"), {});
        return;
    }
    setWorking(true);
    runAsync(
        this,
        [ring = *ring, password]() {
            Outcome o;
            o.ring = ring;
            o.record = vault::protect(ring, password, crypto::kDefaultIterations, &o.passwordKey);
            if (o.record.wrapped.isEmpty())
                o.error = QStringLiteral("vault.failed");
            return o;
        },
        [this](const Outcome &o) {
            setWorking(false);
            if (!o.error.isEmpty()) {
                emit failed(o.error, {});
                return;
            }
            m_catalog->storeVault(o.record);
            m_catalog->openVault(o.ring, o.passwordKey);
            emit succeeded(QStringLiteral("setup"));
        });
}

void PrivacyController::unlock(const QString &password)
{
    if (m_working)
        return;
    const vault::Record record = m_catalog->vaultRecord();
    if (!record.hasPassword()) {
        emit failed(QStringLiteral("vault.noPassword"), {});
        return;
    }
    setWorking(true);
    runAsync(
        this,
        [record, password]() {
            Outcome o;
            o.passwordKey = vault::passwordKey(record, password);
            const auto ring = vault::unwrap(record, o.passwordKey);
            if (ring)
                o.ring = *ring;
            else
                o.error = QStringLiteral("vault.wrongPassword");
            return o;
        },
        [this](const Outcome &o) {
            setWorking(false);
            if (!o.error.isEmpty()) {
                emit failed(o.error, {});
                return;
            }
            m_catalog->openVault(o.ring, o.passwordKey);
            emit succeeded(QStringLiteral("unlock"));
        });
}

void PrivacyController::recover(const QString &keyText, const QString &password, const QString &confirm)
{
    if (m_working)
        return;
    const QByteArray key = crypto::parseKey(keyText);
    if (key.isEmpty()) {
        emit failed(QStringLiteral("vault.invalidKey"), {});
        return;
    }
    if (crypto::keyId(key) != m_catalog->vaultRecord().currentId) {
        emit failed(QStringLiteral("vault.wrongKey"), {});
        return;
    }
    if (!checkNewPassword(password, confirm))
        return;
    setWorking(true);
    runAsync(
        this,
        [key, password]() {
            Outcome o;
            o.ring = vault::ringWith(key);
            o.record = vault::protect(o.ring, password, crypto::kDefaultIterations, &o.passwordKey);
            return o;
        },
        [this](const Outcome &o) {
            setWorking(false);
            m_catalog->storeVault(o.record);
            m_catalog->openVault(o.ring, o.passwordKey);
            emit succeeded(QStringLiteral("recover"));
        });
}

void PrivacyController::changePassword(const QString &current, const QString &password, const QString &confirm)
{
    if (m_working)
        return;
    if (!checkNewPassword(password, confirm))
        return;
    const vault::Record record = m_catalog->vaultRecord();
    setWorking(true);
    runAsync(
        this,
        [record, current, password]() {
            Outcome o;
            const auto ring = vault::unwrap(record, vault::passwordKey(record, current));
            if (!ring) {
                o.error = QStringLiteral("vault.wrongPassword");
                return o;
            }
            o.ring = *ring;
            o.record = vault::protect(*ring, password, crypto::kDefaultIterations, &o.passwordKey);
            return o;
        },
        [this](const Outcome &o) {
            setWorking(false);
            if (!o.error.isEmpty()) {
                emit failed(o.error, {});
                return;
            }
            m_catalog->storeVault(o.record);
            m_catalog->openVault(o.ring, o.passwordKey);
            emit succeeded(QStringLiteral("change"));
        });
}

QString PrivacyController::rotateKey()
{
    try {
        return crypto::formatKey(m_catalog->rotateKey());
    } catch (const ActionError &e) {
        emit failed(e.key(), e.params());
        return {};
    }
}

void PrivacyController::lock()
{
    m_catalog->lockVault();
}

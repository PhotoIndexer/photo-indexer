// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Fotos privadas (App.privacy no QML): a senha, o cofre (abrir/trancar), a
// chave de criptografia (ver, copiar, gerar outra) e o trancamento automático
// por inatividade. O PBKDF2 (lento de propósito) roda fora da thread
// principal; o resto é do Catalog (CatalogPrivate.cpp).

#include <QObject>
#include <QTimer>
#include <QVariantMap>

namespace pix {
class Catalog;
}

class PrivacyController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY changed)
    Q_PROPERTY(bool unlocked READ unlocked NOTIFY changed)
    /// Derivando a senha (os botões do modal ficam em espera).
    Q_PROPERTY(bool working READ working NOTIFY workingChanged)
    /// {photos, encrypted, pending, errors, keyId}
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
    /// Tranca sozinho depois de N minutos sem uso (0 = nunca).
    Q_PROPERTY(int autoLockMinutes READ autoLockMinutes WRITE setAutoLockMinutes NOTIFY autoLockChanged)
    Q_PROPERTY(int minPasswordLength READ minPasswordLength CONSTANT)
    /// Nó da biblioteca do álbum "Private" ("album:<id>").
    Q_PROPERTY(QString albumNode READ albumNode CONSTANT)

public:
    explicit PrivacyController(pix::Catalog *catalog, QObject *parent = nullptr);

    bool hasPassword() const;
    bool unlocked() const;
    bool working() const { return m_working; }
    QVariantMap status() const;
    int autoLockMinutes() const { return m_autoLockMinutes; }
    void setAutoLockMinutes(int minutes);
    static int minPasswordLength() { return 8; }
    QString albumNode() const;

    /// A chave para guardar ("XXXX-XXXX-..."): antes da senha ou com o cofre
    /// aberto; vazio se trancado.
    Q_INVOKABLE QString revealKey() const;
    /// Copia a chave; a área de transferência é limpa depois de 2 minutos.
    Q_INVOKABLE void copyKey();

    /// Primeira vez: a senha que protege a chave. Sucesso: succeeded("setup").
    Q_INVOKABLE void setPassword(const QString &password, const QString &confirm);
    Q_INVOKABLE void unlock(const QString &password);
    /// Esqueceu a senha: a chave guardada + uma senha nova.
    Q_INVOKABLE void recover(const QString &key, const QString &password, const QString &confirm);
    Q_INVOKABLE void changePassword(const QString &current, const QString &password, const QString &confirm);
    /// Chave nova (as privadas são recifradas pela etapa "privacy"); devolve
    /// a chave nova para a pessoa guardar.
    Q_INVOKABLE QString rotateKey();
    /// A chave do catálogo que cifrou .photoidxe "trancados" (vindos de outro
    /// catálogo): as fotos abrem e são recifradas com a chave atual. Avisa
    /// por notice("vault.keyAdded", {count}) ou failed(vault.invalidKey,
    /// vault.keyUnused, vault.locked).
    Q_INVOKABLE void addKey(const QString &keyText);
    Q_INVOKABLE void lock();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void changed();
    void workingChanged();
    void autoLockChanged();
    void succeeded(const QString &operation);
    void failed(const QString &key, const QVariantMap &params);
    void notice(const QString &key, const QVariantMap &params);

private:
    bool checkNewPassword(const QString &password, const QString &confirm);
    void setWorking(bool on);
    void restartAutoLock();

    pix::Catalog *m_catalog;
    bool m_working = false;
    int m_autoLockMinutes = 10;
    QTimer m_autoLock;
    QTimer m_clipboardClear;
    QString m_copied;
};

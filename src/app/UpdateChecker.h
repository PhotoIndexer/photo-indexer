// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Versão nova (App.updates no QML): ao abrir, em segundo plano, pergunta ao
// GitHub qual é a última Release publicada (a mesma consulta do
// packaging/build.py) e compara com a deste build. Havendo uma maior, o
// cabeçalho mostra "Atualização disponível" e, uma vez por versão, o app
// recomenda baixar. Nada é baixado nem instalado sozinho: o botão abre a
// página da Release no navegador.
//
// Só a consulta anônima à API pública (sem dados da pessoa nem do catálogo);
// a pessoa pode desligar no Sobre (QSettings "updates/check"). Sem rede ou
// com o limite da API estourado, fica quieto (só o log).

#include <QObject>
#include <QString>

class QNetworkAccessManager;

class UpdateChecker : public QObject
{
    Q_OBJECT
    /// Verificar ao abrir (lembrado nas preferências).
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// Há uma Release mais nova que este build.
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)

public:
    /// ``repoUrl`` = https://github.com/<dono>/<repositório>; ``current`` =
    /// a versão do build (1.2.3 ou 1.2.3-dev+abc1234).
    UpdateChecker(const QString &repoUrl, const QString &current, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool available() const { return m_available; }
    QString latestVersion() const { return m_latest; }
    QString releaseUrl() const { return m_url; }
    QString releaseNotes() const { return m_notes; }

    /// Verifica daqui a ``delayMs`` (se ligado): a abertura não espera a rede.
    void checkLater(int delayMs);
    Q_INVOKABLE void check();
    /// A recomendação desta versão já foi mostrada (não repete a cada abertura).
    Q_INVOKABLE void markNotified();

    /// ``latest`` é maior que ``current``? Os dois em X.Y.Z (com ou sem "v",
    /// sufixo -dev+... ignorado: um build dev de 1.2.3 vem DEPOIS da 1.2.3).
    static bool isNewer(const QString &latest, const QString &current);

signals:
    void enabledChanged();
    void changed();
    /// Versão nova que a pessoa ainda não viu: o Main.qml recomenda atualizar.
    void updateFound(const QString &version);

private:
    QString m_apiUrl;
    QString m_current;
    bool m_enabled = true;
    bool m_available = false;
    bool m_checking = false;
    QString m_latest;
    QString m_url;
    QString m_notes;
    QNetworkAccessManager *m_network = nullptr;
};

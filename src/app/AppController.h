// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// O app visto do QML (singleton ``App``): o catálogo aberto, o idioma, o
// tema, a tela atual, as pastas indexadas e os discos, e os controllers de
// cada área (biblioteca, foto aberta, pessoas, tarefas).
//
// O catálogo é um só (~/.photo_indexer/data.sqlite3) e abre com o app: não há
// conceito de projeto.

#include "LibraryController.h"
#include "PeopleController.h"
#include "PrivacyController.h"
#include "TasksController.h"
#include "ViewerController.h"

#include "core/Catalog.h"

#include <QObject>
#include <QTimer>
#include <QQmlEngine>
#include <QUrl>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <memory>

namespace pix {
class CatalogReader;
class Catalog;
class Pipeline;
}

class AppController : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString brandName READ brandName CONSTANT)
    Q_PROPERTY(QString brandUrl READ brandUrl CONSTANT)
    Q_PROPERTY(QString repoUrl READ repoUrl CONSTANT)
    Q_PROPERTY(QString language READ language NOTIFY languageChanged)
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    /// "library" | "viewer" | "people" | "faceSearch" | "folders" | "tasks"
    Q_PROPERTY(QString page READ page WRITE setPage NOTIFY pageChanged)
    /// Erro ao abrir o catálogo ("" = aberto).
    Q_PROPERTY(QString catalogError READ catalogError CONSTANT)
    /// [{id, path, diskId, photoCount, indexed, errors, scannedAt, online}]
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    /// [{id, key, label, kind, protocol, server, share, fsType, device, serial, model,
    ///   mountPoint, totalBytes, online, folderCount, photoCount}]
    Q_PROPERTY(QVariantList disks READ disks NOTIFY foldersChanged)
    Q_PROPERTY(LibraryController *library READ library CONSTANT)
    Q_PROPERTY(ViewerController *viewer READ viewer CONSTANT)
    Q_PROPERTY(PeopleController *people READ people CONSTANT)
    Q_PROPERTY(TasksController *tasks READ tasks CONSTANT)
    Q_PROPERTY(PrivacyController *privacy READ privacy CONSTANT)
    Q_PROPERTY(QString logsDir READ logsDir CONSTANT)
    Q_PROPERTY(QString dataDir READ dataDir CONSTANT)

public:
    // Sem argumento padrão de propósito: com um construtor sem argumentos o QML
    // criaria um segundo App (vazio) em vez de usar create().
    explicit AppController(QObject *parent);
    ~AppController() override;

    static AppController *instance();
    static AppController *create(QQmlEngine *, QJSEngine *);

    QString version() const;
    QString brandName() const;
    QString brandUrl() const;
    QString repoUrl() const;
    QString language() const;
    QVariantList languages() const;
    bool darkTheme() const { return m_dark; }
    void setDarkTheme(bool dark);
    QString page() const { return m_page; }
    void setPage(const QString &page);
    QString catalogError() const { return m_catalogError; }
    QVariantList folders() const;
    QVariantList disks() const;
    LibraryController *library() { return m_library.get(); }
    ViewerController *viewer() { return m_viewer.get(); }
    PrivacyController *privacy() { return m_privacy.get(); }
    /// O chaveiro da sessão (o provider image://private decifra com ele).
    std::shared_ptr<const pix::crypto::KeyRing> keyRing() const;
    PeopleController *people() { return m_people.get(); }
    TasksController *tasks() { return m_tasks.get(); }
    QString logsDir() const;
    QString dataDir() const;

    Q_INVOKABLE void setLanguage(const QString &code);

    // Pastas
    Q_INVOKABLE bool addFolder(const QUrl &folder);
    Q_INVOKABLE bool addFolderPath(const QString &path) { return addFolder(QUrl::fromLocalFile(path)); }
    /// Arrastar e soltar na janela: cada pasta passa pelo mesmo addFolder do
    /// "Adicionar pasta" (avisos e erros iguais); um arquivo abre como no
    /// dois cliques (openPaths). Devolve quantas pastas entraram.
    Q_INVOKABLE int dropUrls(const QList<QUrl> &urls);
    Q_INVOKABLE void removeFolder(const QString &folderId);
    Q_INVOKABLE void rescanFolder(const QString &folderId);
    Q_INVOKABLE void refreshDisks();

    // Foto aberta
    Q_INVOKABLE void openPhoto(const QString &photoId);
    /// Caminhos vindos de fora (linha de comando, outra instância, o SO
    /// abrindo um .photoidxe): pasta = adicionar; foto do catálogo = abrir
    /// (a privada pede a senha no QML); arquivo fora do catálogo = oferecer
    /// adicionar a pasta dele.
    void openPaths(const QStringList &paths);
    /// Caminho (no formato do SO) do arquivo de uma foto.
    Q_INVOKABLE QString photoPath(const QString &photoId) const;

    // Sistema operacional
    Q_INVOKABLE void revealInFolder(const QString &path);
    Q_INVOKABLE void openExternally(const QString &path);
    Q_INVOKABLE void openLogsFolder();
    Q_INVOKABLE void openUrl(const QUrl &url);
    Q_INVOKABLE void copyText(const QString &text);
    /// Teclas modificadoras apertadas agora (Qt.KeyboardModifiers): o soltar
    /// do arrastar não as traz no evento.
    Q_INVOKABLE int keyboardModifiers() const;
    /// Atalho no formato da plataforma ("Ctrl+K" -> "⌘K" no macOS).
    Q_INVOKABLE QString shortcutText(const QString &sequence) const;
    Q_INVOKABLE bool sameShortcut(const QString &a, const QString &b) const;
    /// Tamanho legível ("1.2 GB"), base 1024.
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    /// "1/200", "f/2.8", "2026-10-16 14:05" para o QML.
    Q_INVOKABLE QString formatShutter(double seconds) const;
    Q_INVOKABLE QString formatAperture(double fNumber) const;
    Q_INVOKABLE QString formatDateTime(const QString &wallClock) const;
    Q_INVOKABLE QString fileUrl(const QString &path) const;

signals:
    void languageChanged();
    void darkThemeChanged();
    void pageChanged();
    void foldersChanged();
    /// Erro de uma ação: chave de i18n + parâmetros (o QML mostra o modal).
    void failed(const QString &key, const QVariantMap &params);
    /// Aviso curto (o QML mostra numa faixa que some sozinha).
    void notice(const QString &key, const QVariantMap &params);
    /// Abrir esta foto (vinda de fora); ``isPrivate``: pedir a senha antes.
    void photoRequested(const QString &photoId, bool isPrivate);
    /// Arquivo que o catálogo não conhece: perguntar se adiciona ``folder``.
    void unknownFileRequested(const QString &path, const QString &folder);
    /// Trazer a janela para a frente.
    void raiseRequested();

private:
    /// Os volumes montados mudaram (pendrive, HD externo, compartilhamento
    /// de rede): reconhece os discos e revarre os que voltaram.
    void checkVolumes();
    void applyVolumes(const QStringList &roots, const pix::Catalog::DiskProbe &probe);
    /// Relê pastas e discos no leitor (agrupando pedidos seguidos).
    void readFolders();

    std::unique_ptr<pix::Catalog> m_catalog;
    std::unique_ptr<pix::CatalogReader> m_reader;  ///< antes dos controllers: eles o usam
    std::unique_ptr<pix::Pipeline> m_pipeline;
    std::unique_ptr<LibraryController> m_library;
    std::unique_ptr<ViewerController> m_viewer;
    std::unique_ptr<PeopleController> m_people;
    std::unique_ptr<TasksController> m_tasks;
    std::unique_ptr<PrivacyController> m_privacy;
    QString m_page = QStringLiteral("library");
    QString m_catalogError;
    bool m_dark = true;
    QTimer m_volumeWatch;
    QTimer m_foldersTimer;
    QVariantList m_folders;         ///< cache de folders()/disks() (lidos no CatalogReader)
    QVariantList m_disks;
    QStringList m_volumes;          ///< pontos de montagem da última olhada
    bool m_probing = false;
};

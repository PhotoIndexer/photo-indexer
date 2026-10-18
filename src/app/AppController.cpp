// SPDX-License-Identifier: BSD-2-Clause
#include "AppController.h"

#include "core/Perf.h"

#include "ImageProviders.h"
#include "Language.h"

#include "core/Catalog.h"
#include "core/CatalogReader.h"
#include "core/Paths.h"
#include "core/Pipeline.h"
#include "imaging/Exif.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLocale>
#include <QLoggingCategory>
#include <QProcess>
#include <QSettings>
#include <QStorageInfo>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>

#include <optional>

Q_LOGGING_CATEGORY(lcApp, "photoidx.app")

using namespace pix;

namespace {

/// Pontos de montagem prontos agora, em ordem (para comparar entre olhadas).
QStringList mountedRoots()
{
    QStringList roots;
    for (const QStorageInfo &v : QStorageInfo::mountedVolumes())
        if (v.isValid() && v.isReady())
            roots << v.rootPath();
    roots.sort();
    return roots;
}

AppController *g_instance = nullptr;

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_catalog(std::make_unique<Catalog>())
{
    g_instance = this;
    m_dark = QSettings().value(QStringLiteral("ui/dark"), true).toBool();
    try {
        m_catalog->open(AppPaths::database());
    } catch (const std::exception &e) {
        // Sem catálogo, o app abre mesmo assim (em memória) e mostra o erro.
        m_catalogError = QString::fromUtf8(e.what());
        qCWarning(lcApp) << "Could not open the catalog:" << e.what();
        m_catalog = std::make_unique<Catalog>();
        m_catalog->open(QString());
    }
    m_pipeline = std::make_unique<Pipeline>(m_catalog.get());
    // Leituras da navegação numa conexão só de leitura, em outra thread
    // (core/CatalogReader.h): a interface não espera o banco.
    m_reader = std::make_unique<CatalogReader>(*m_catalog);
    m_pipeline->setReader(m_reader.get());
    m_library = std::make_unique<LibraryController>(m_catalog.get(), m_pipeline.get(), m_reader.get());
    // As pastas já na abertura (a tela não começa em "adicione uma pasta");
    // os discos (mais caros) chegam pelo leitor logo depois.
    m_folders = m_catalog->folders();
    m_foldersTimer.setSingleShot(true);
    m_foldersTimer.setInterval(500);
    connect(&m_foldersTimer, &QTimer::timeout, this, [this]() {
        struct Lists
        {
            QVariantList folders, disks;
        };
        m_reader->read<Lists>(
            this, [](const Catalog &c) { return Lists{c.folders(), c.disks()}; },
            [this](Lists l) {
                if (l.folders == m_folders && l.disks == m_disks)
                    return;
                m_folders = l.folders;
                m_disks = l.disks;
                emit foldersChanged();
            });
    });
    m_viewer = std::make_unique<ViewerController>(m_catalog.get(), m_library->grid());
    m_people = std::make_unique<PeopleController>(m_catalog.get(), m_pipeline.get(), m_reader.get());
    m_timeline = std::make_unique<TimelineController>(m_catalog.get(), m_reader.get());
    connect(m_timeline.get(), &TimelineController::notice, this, &AppController::notice);
    connect(m_timeline.get(), &TimelineController::failed, this, &AppController::failed);
    m_updates = std::make_unique<UpdateChecker>(QStringLiteral(PHOTOIDX_REPO_URL), version());
    m_tasks = std::make_unique<TasksController>(m_catalog.get(), m_pipeline.get(), m_reader.get());
    m_privacy = std::make_unique<PrivacyController>(m_catalog.get());
    connect(m_privacy.get(), &PrivacyController::notice, this, &AppController::notice);
    m_files = std::make_unique<FilesController>(m_catalog.get(), m_pipeline.get(), m_library.get());
    connect(m_files.get(), &FilesController::notice, this, &AppController::notice);
    connect(m_files.get(), &FilesController::failed, this, &AppController::failed);
    // Trancou com uma foto privada aberta: volta para a biblioteca.
    connect(m_catalog.get(), &Catalog::privacyChanged, this, [this]() {
        if (!m_catalog->privateUnlocked() && m_page == QLatin1String("viewer")
            && m_viewer->detail().value(QStringLiteral("isPrivate")).toBool())
            setPage(QStringLiteral("library"));
    });
    connect(m_library.get(), &LibraryController::failed, this, &AppController::failed);
    connect(m_viewer.get(), &ViewerController::failed, this, &AppController::failed);
    connect(m_people.get(), &PeopleController::failed, this, &AppController::failed);
    connect(m_tasks.get(), &TasksController::notice, this, &AppController::notice);
    connect(m_catalog.get(), &Catalog::treeChanged, this, [this](const QString &tab) {
        if (tab == QLatin1String("folders"))
            readFolders();
    });
    connect(m_pipeline.get(), &Pipeline::scanFinished, this, &AppController::readFolders);

    // Ao abrir: reconhece os discos (algum pode ter voltado em outro lugar),
    // liga as etapas e revarre as pastas — o que mudou com o app fechado entra.
    m_pipeline->startAll();
    // Regra nova de agrupar rostos (core/FaceClusterer.h, "uma pessoa por
    // foto", junção sem cadeia): as pessoas automáticas de um catálogo antigo
    // são refeitas uma vez, em segundo plano. Nomes e confirmações ficam.
    constexpr int kFaceRule = 2;
    if (m_catalog->setting(QStringLiteral("faces/clusterRule"), 1).toInt() < kFaceRule
        && m_catalog->db().scalar(QStringLiteral("SELECT count(*) FROM face")).toInt() > 0) {
        QTimer::singleShot(1500, this, [this]() {
            emit notice(QStringLiteral("people.regrouping"), {});
            m_pipeline->regroupPeople([this](int people) {
                m_catalog->setSetting(QStringLiteral("faces/clusterRule"), kFaceRule);
                emit notice(QStringLiteral("people.reclustered"), {{QStringLiteral("count"), people}});
            });
        });
    } else {
        m_catalog->setSetting(QStringLiteral("faces/clusterRule"), kFaceRule);
    }
    // Disco conectado ou desconectado com o app aberto: a lista de volumes
    // montados é barata de ler (QStorageInfo, igual nos três SOs).
    // A primeira olhada (m_volumes vazio) sonda todos os discos, fora da
    // thread principal; até ela voltar, valem os estados da última sessão.
    checkVolumes();
    // Uma etapa achou um arquivo sumido: o disco pode ter saído sem a lista
    // de volumes mudar ainda (ou antes da primeira sonda) — sonda já.
    connect(m_pipeline.get(), &Pipeline::filesVanished, this, [this]() {
        if (m_vanishProbe)
            return;
        m_vanishProbe = true;
        QTimer::singleShot(1000, this, [this]() {
            m_vanishProbe = false;
            m_volumes.clear();
            checkVolumes();
        });
    });
    m_volumeWatch.setInterval(3000);
    connect(&m_volumeWatch, &QTimer::timeout, this, &AppController::checkVolumes);
    m_volumeWatch.start();
    readFolders();
    if (m_catalog->setting(QStringLiteral("scan/onStartup"), true).toBool())
        m_pipeline->scan();
}

AppController::~AppController()
{
    m_pipeline.reset();  // espera os workers antes de o catálogo sair
    g_instance = nullptr;
}

std::shared_ptr<const pix::crypto::KeyRing> AppController::keyRing() const
{
    return m_catalog->keyRing();
}

QString AppController::photoIdByPath(const QString &path) const
{
    return m_catalog->photoIdByPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

AppController *AppController::instance()
{
    return g_instance;
}

AppController *AppController::create(QQmlEngine *, QJSEngine *)
{
    // O main cria a instância antes do QML; o motor não é dono dela.
    QJSEngine::setObjectOwnership(g_instance, QJSEngine::CppOwnership);
    return g_instance;
}

QString AppController::version() const
{
    return QCoreApplication::applicationVersion();
}

QString AppController::brandName() const
{
    return QStringLiteral(PHOTOIDX_BRAND_NAME);
}

QString AppController::brandUrl() const
{
    return QStringLiteral(PHOTOIDX_BRAND_URL);
}

QString AppController::repoUrl() const
{
    return QStringLiteral(PHOTOIDX_REPO_URL);
}

QString AppController::language() const
{
    return app::activeLanguage();
}

QVariantList AppController::languages() const
{
    return app::availableLanguages();
}

void AppController::setLanguage(const QString &code)
{
    app::saveLanguage(code);
    app::installLanguage(code);
    // O QML retraduz (retranslate do motor, no main); as árvores trazem nomes
    // traduzidos pelo C++ (categorias embutidas, meses): refaz.
    emit languageChanged();
    m_library->refreshTree();
}

void AppController::setDarkTheme(bool dark)
{
    if (dark == m_dark)
        return;
    m_dark = dark;
    QSettings().setValue(QStringLiteral("ui/dark"), dark);
    emit darkThemeChanged();
}

void AppController::setPage(const QString &page)
{
    if (page == m_page)
        return;
    m_page = page;
    emit pageChanged();
}

QString AppController::logsDir() const
{
    return QDir::toNativeSeparators(AppPaths::logsDir());
}

QString AppController::dataDir() const
{
    return QDir::toNativeSeparators(AppPaths::dataDir());
}

// --------------------------------------------------------------------------- //
// Pastas
// --------------------------------------------------------------------------- //

QVariantList AppController::folders() const
{
    return m_folders;
}

QVariantList AppController::disks() const
{
    return m_disks;
}

void AppController::readFolders()
{
    // Pastas e discos (com as contagens): no leitor; o sinal só sai quando os
    // dados chegam e se mudaram. Vários pedidos seguidos viram uma leitura.
    if (m_foldersTimer.isActive())
        return;
    m_foldersTimer.start();
}

bool AppController::addFolder(const QUrl &folder)
{
    const QString path = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    try {
        const QString id = m_catalog->addFolder(path);
        m_pipeline->scan({id});
        readFolders();
        emit notice(QStringLiteral("folder.added"), {{QStringLiteral("path"), QDir::toNativeSeparators(path)}});
        return true;
    } catch (const ActionError &e) {
        emit failed(e.key(), e.params());
    } catch (const std::exception &e) {
        emit failed(QStringLiteral("error.generic"), {{QStringLiteral("detail"), QString::fromUtf8(e.what())}});
    }
    return false;
}

int AppController::dropUrls(const QList<QUrl> &urls)
{
    int added = 0;
    QStringList files;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QFileInfo fi(url.toLocalFile());
        if (fi.isDir())
            added += addFolder(QUrl::fromLocalFile(fi.absoluteFilePath())) ? 1 : 0;
        else if (fi.isFile())
            files << fi.absoluteFilePath();
    }
    if (!files.isEmpty())
        openPaths(files);
    return added;
}

void AppController::removeFolder(const QString &folderId)
{
    m_pipeline->cancel(QStringLiteral("search"));
    m_catalog->removeFolder(folderId);
    m_pipeline->start(QStringLiteral("search"));
    readFolders();
}

void AppController::rescanFolder(const QString &folderId)
{
    m_volumes.clear();  // força a sonda dos discos (fora da thread principal)
    checkVolumes();
    m_pipeline->scan(folderId.isEmpty() ? QStringList() : QStringList{folderId});
}

void AppController::checkVolumes()
{
    // Tudo o que pergunta ao SO roda fora da thread principal: um
    // compartilhamento de rede caído pode demorar segundos para responder.
    if (m_probing)
        return;
    m_probing = true;
    const QStringList last = m_volumes;
    const QStringList folders = m_catalog->allFolderPaths();
    auto *watcher = new QFutureWatcher<std::optional<std::pair<QStringList, Catalog::DiskProbe>>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        watcher->deleteLater();
        m_probing = false;
        const auto result = watcher->result();
        if (result)
            applyVolumes(result->first, result->second);
    });
    watcher->setFuture(QtConcurrent::run([last, folders]() -> std::optional<std::pair<QStringList, Catalog::DiskProbe>> {
        const QStringList now = mountedRoots();
        if (now == last)
            return std::nullopt;  // nada mudou: nem sonda os discos
        return std::make_pair(now, Catalog::probeDisks(folders));
    }));
}

void AppController::applyVolumes(const QStringList &roots, const Catalog::DiskProbe &probe)
{
    m_volumes = roots;
    QStringList back, gone;
    if (m_catalog->applyDiskProbe(probe, &back, &gone) == 0)
        return;
    QHash<QString, QString> labels;
    for (const QVariant &d : std::as_const(m_disks))
        labels.insert(d.toMap().value(QStringLiteral("id")).toString(), d.toMap().value(QStringLiteral("label")).toString());
    // O que voltou: revarre (o que mudou com ele desconectado entra) e a
    // fila parada dele anda sozinha (as etapas só pegam discos online).
    QStringList folders;
    for (const QString &disk : back) {
        folders += m_catalog->folderIdsOnDisk(disk);
        emit notice(QStringLiteral("disk.online"), {{QStringLiteral("label"), labels.value(disk)}});
    }
    for (const QString &disk : gone)
        emit notice(QStringLiteral("disk.offline"), {{QStringLiteral("label"), labels.value(disk)}});
    if (!folders.isEmpty())
        m_pipeline->scan(folders);
    readFolders();
}

void AppController::refreshDisks()
{
    m_volumes.clear();  // força a sonda dos discos (fora da thread principal)
    checkVolumes();
}

void AppController::openPhoto(const QString &photoId)
{
    PIX_PERF("AppController::openPhoto");
    m_viewer->open(photoId);
    setPage(QStringLiteral("viewer"));
}

void AppController::openPaths(const QStringList &paths)
{
    for (const QString &raw : paths) {
        const QFileInfo fi(raw);
        if (!fi.exists())
            continue;
        if (fi.isDir()) {
            // Pasta já cadastrada: addFolder avisa (folder.alreadyAdded) — aqui
            // só cadastra as novas, em silêncio.
            bool known = false;
            for (const QVariant &f : folders())
                known = known
                    || QDir::fromNativeSeparators(f.toMap().value(QStringLiteral("path")).toString())
                           == QDir::fromNativeSeparators(fi.absoluteFilePath());
            if (!known)
                addFolder(QUrl::fromLocalFile(fi.absoluteFilePath()));
            continue;
        }
        const QString id = m_catalog->photoIdByPath(fi.absoluteFilePath());
        emit raiseRequested();
        if (id.isEmpty()) {
            emit unknownFileRequested(QDir::toNativeSeparators(fi.absoluteFilePath()),
                                      QDir::toNativeSeparators(fi.absolutePath()));
            continue;
        }
        const bool isPrivate = m_catalog->db().scalar(QStringLiteral("SELECT private FROM photo WHERE id = ?"), {id}).toInt() == 1;
        emit photoRequested(id, isPrivate);
    }
}

QString AppController::photoPath(const QString &photoId) const
{
    return QDir::toNativeSeparators(m_catalog->photoPath(photoId));
}

// --------------------------------------------------------------------------- //
// Sistema operacional
// --------------------------------------------------------------------------- //

void AppController::revealInFolder(const QString &path)
{
    const QString native = QDir::toNativeSeparators(QDir::fromNativeSeparators(path));
#if defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), native});
#elif defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,") + native});
#else
    const QFileInfo fi(QDir::fromNativeSeparators(path));
    QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath()));
#endif
}

void AppController::openExternally(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::fromNativeSeparators(path)));
}

void AppController::openLogsFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::logsDir()));
}

void AppController::openUrl(const QUrl &url)
{
    // Só links da web vão para o navegador do sistema.
    if (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"))
        QDesktopServices::openUrl(url);
}

void AppController::copyText(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

QString AppController::shortcutText(const QString &sequence) const
{
    return QKeySequence(sequence, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

bool AppController::sameShortcut(const QString &a, const QString &b) const
{
    const QKeySequence ka(a, QKeySequence::PortableText);
    return !ka.isEmpty() && ka == QKeySequence(b, QKeySequence::PortableText);
}

QString AppController::formatBytes(qint64 bytes) const
{
    const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    const QLocale locale;
    return (u == 0 || v >= 100 ? locale.toString(v, 'f', 0) : locale.toString(v, 'f', 1)) + u' '
        + QLatin1String(units[u]);
}

QString AppController::formatShutter(double seconds) const
{
    return imaging::formatShutter(seconds);
}

QString AppController::formatAperture(double fNumber) const
{
    return imaging::formatAperture(fNumber);
}

QString AppController::formatDateTime(const QString &wallClock) const
{
    // A data do catálogo é "de relógio" (sem fuso): mostra como está.
    const QDateTime dt = QDateTime::fromString(wallClock.left(19), QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    return dt.isValid() ? QLocale().toString(dt, QLocale::ShortFormat) : QString();
}

int AppController::keyboardModifiers() const
{
    return int(QGuiApplication::queryKeyboardModifiers());
}

QString AppController::fileUrl(const QString &path) const
{
    return thumbnailUrl(path);
}

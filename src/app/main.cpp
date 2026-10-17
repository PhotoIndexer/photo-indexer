// SPDX-License-Identifier: BSD-2-Clause
//
// PhotoIDX — catálogo de fotos com categorias por IA, faces e lugares (C++/Qt Quick).
//
//   PhotoIDX [PASTA|ARQUIVO...] [--debug] [--screenshot ARQUIVO [--page PAGINA] [--wait MS]]
//
// PASTA: pastas para indexar (como o "Adicionar pasta"; as já cadastradas
// são ignoradas). ARQUIVO: uma foto para abrir — é assim que o SO abre um
// .photoidxe com dois cliques (no macOS, pelo QFileOpenEvent). Com o app já
// aberto, os caminhos vão para ele (SingleInstance) e este processo sai.
//
// --screenshot abre o app, espera a primeira tela carregar, salva a imagem da
// janela e sai: teste de fumaça sem ninguém olhando.

#include "AppController.h"
#include "ImageProviders.h"
#include "Language.h"
#include "Logging.h"
#include "SingleInstance.h"

#include "core/Paths.h"
#include "core/Perf.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThreadPool>
#include <QTimer>

Q_LOGGING_CATEGORY(lcMain, "photoidx.main")

namespace {

/// macOS: o Finder abre um .photoidxe com o app mandando um evento, não argv.
class FileOpenFilter : public QObject
{
public:
    explicit FileOpenFilter(AppController *controller)
        : m_controller(controller)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen) {
            const QString file = static_cast<QFileOpenEvent *>(event)->file();
            if (!file.isEmpty())
                m_controller->openPaths({file});
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    AppController *m_controller;
};

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // Nome e organização decidem onde o QSettings guarda as preferências.
    QGuiApplication::setApplicationName(QStringLiteral("PhotoIDX"));
    QGuiApplication::setOrganizationName(QStringLiteral("PhotoE"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("photoe.com.br"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PHOTOIDX_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral(PHOTOIDX_BRAND_NAME));
    // O nome do .desktop instalado (photoidx.desktop): o Wayland liga a janela
    // ao ícone e ao nome por ele.
    QGuiApplication::setDesktopFileName(QStringLiteral("photoidx"));
#ifndef Q_OS_MACOS
    // No macOS o Dock usa o ícone do pacote (PhotoIDX.icns); definir outro
    // aqui o trocaria enquanto o app roda. favicon.png = o mesmo desenho do
    // packaging/art.py (atualize com `make art` se o logo mudar).
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/PhotoIDX/assets/favicon.png")));
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Photo catalog with AI categories, faces and places"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption debugOption(QStringLiteral("debug"), QStringLiteral("Verbose logging."));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Save a screenshot of the first screen and quit."),
                                              QStringLiteral("file"));
    const QCommandLineOption pageOption(QStringLiteral("page"),
                                        QStringLiteral("Screen to open (library, people, folders, tasks)."),
                                        QStringLiteral("page"));
    const QCommandLineOption waitOption(QStringLiteral("wait"),
                                        QStringLiteral("Milliseconds to wait before the screenshot (default 2500)."),
                                        QStringLiteral("ms"));
    const QCommandLineOption sizeOption(QStringLiteral("size"), QStringLiteral("Window size for the screenshot (WxH)."),
                                        QStringLiteral("WxH"));
    const QCommandLineOption openOption(QStringLiteral("open"), QStringLiteral("Open the Nth photo of the library (screenshot)."),
                                        QStringLiteral("n"));
    const QCommandLineOption faceOption(QStringLiteral("face-search"),
                                        QStringLiteral("Open the face search with this photo (screenshot)."), QStringLiteral("file"));
    const QCommandLineOption ruleOption(QStringLiteral("rule"),
                                        QStringLiteral("Add a filter condition field:op:value (screenshot; repeatable)."),
                                        QStringLiteral("rule"));
    const QCommandLineOption personOption(QStringLiteral("person"), QStringLiteral("Open the Nth person (screenshot)."),
                                          QStringLiteral("n"));
    const QCommandLineOption passwordOption(QStringLiteral("private-password"),
                                            QStringLiteral("Create or enter the private photos password (screenshot)."),
                                            QStringLiteral("password"));
    const QCommandLineOption makePrivateOption(QStringLiteral("make-private"),
                                               QStringLiteral("Make the first N photos of the library private (screenshot)."),
                                               QStringLiteral("n"));
    const QCommandLineOption privateAlbumOption(QStringLiteral("private-album"),
                                                QStringLiteral("Show the Private album (screenshot)."));
    parser.addOptions({sizeOption, openOption, faceOption, ruleOption, personOption, passwordOption, makePrivateOption,
                       privateAlbumOption});
    parser.addPositionalArgument(QStringLiteral("folders"), QStringLiteral("Folders to index."), QStringLiteral("[folder...]"));
    const QCommandLineOption tabOption(QStringLiteral("tab"),
                                       QStringLiteral("Side panel tab of the library (folders, dates, categories, albums, places, people)."),
                                       QStringLiteral("tab"));
    const QCommandLineOption copiesOption(QStringLiteral("copies"),
                                          QStringLiteral("Show the copies of the Nth photo of the library (screenshot)."), QStringLiteral("n"));
    parser.addOptions({debugOption, screenshotOption, pageOption, waitOption, tabOption, copiesOption});
    // O Finder às vezes passa -psn_0_12345: ignorado.
    QStringList args = QCoreApplication::arguments();
    args.erase(std::remove_if(args.begin(), args.end(), [](const QString &a) { return a.startsWith(QLatin1String("-psn_")); }),
               args.end());
    parser.process(args);

    pix::app::setupLogging(parser.isSet(debugOption));
    // Onde a interface trava: ~/.photo_indexer/logs/perf.log (core/Perf.h).
    pix::perf::startWatchdog();
    qCInfo(lcMain) << "PhotoIDX" << PHOTOIDX_VERSION << "starting; data in" << pix::AppPaths::dataDir();

    QStringList paths;
    for (const QString &arg : parser.positionalArguments())
        paths << QFileInfo(arg).absoluteFilePath();
    // Um app por catálogo: se já houver um aberto, os caminhos vão para ele.
    SingleInstance instance(pix::AppPaths::dataDir());
    if (!parser.isSet(screenshotOption)) {
        if (instance.forward(paths))
            return 0;
        instance.listen();
    }

    pix::app::installLanguage(pix::app::savedLanguage());

    // Inter vai no próprio binário: nada da interface depende de rede.
    for (const char *weight : {"Light", "Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/PhotoIDX/fonts/Inter-%1.ttf").arg(QLatin1String(weight)));
    QFont font(QStringLiteral("Inter"));
    font.setPixelSize(14);
    QGuiApplication::setFont(font);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController controller(nullptr);
    if (parser.isSet(pageOption))
        controller.setPage(parser.value(pageOption));
    if (parser.isSet(tabOption))
        controller.library()->setTab(parser.value(tabOption));
    // Pastas da linha de comando entram já (o teste de captura conta com elas).
    QStringList files;
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir())
            controller.openPaths({path});
        else
            files << path;
    }
    QObject::connect(&instance, &SingleInstance::received, &controller, [&controller](const QStringList &more) {
        controller.openPaths(more);
        emit controller.raiseRequested();
    });
    FileOpenFilter fileOpen(&controller);
    app.installEventFilter(&fileOpen);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("photo"), new PhotoImageProvider);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    engine.addImageProvider(QStringLiteral("private"), new PrivateImageProvider(controller.keyRing()));
    QObject::connect(&controller, &AppController::languageChanged, &engine, &QQmlEngine::retranslate);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("PhotoIDX", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    // Arquivos: com a janela pronta (a privada pede a senha num modal).
    if (!files.isEmpty())
        QTimer::singleShot(0, &controller, [&controller, files]() { controller.openPaths(files); });

    if (parser.isSet(screenshotOption)) {
        const QString file = parser.value(screenshotOption);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        const int wait = parser.isSet(waitOption) ? parser.value(waitOption).toInt() : 2500;
        if (parser.isSet(sizeOption)) {
            const QStringList wh = parser.value(sizeOption).split(u'x');
            if (wh.size() == 2)
                window->resize(wh[0].toInt(), wh[1].toInt());
        }
        const QStringList rules = parser.values(ruleOption);
        if (!rules.isEmpty()) {
            QTimer::singleShot(std::max(0, wait - 4000), &controller, [&controller, rules]() {
                for (const QString &rule : rules) {
                    const QStringList p = rule.split(u':');
                    if (p.size() < 2)
                        continue;
                    controller.library()->addRule(p[0]);
                    const int index = int(controller.library()->rules().size()) - 1;
                    controller.library()->updateRule(index, {{QStringLiteral("op"), p[1]},
                                                             {QStringLiteral("value"), p.value(2)},
                                                             {QStringLiteral("value2"), p.value(3)}});
                }
            });
        }
        if (parser.isSet(passwordOption)) {
            QTimer::singleShot(std::max(0, wait - 9000), &controller, [&controller, pw = parser.value(passwordOption)]() {
                PrivacyController *privacy = controller.privacy();
                privacy->hasPassword() ? privacy->unlock(pw) : privacy->setPassword(pw, pw);
            });
        }
        if (parser.isSet(makePrivateOption)) {
            QTimer::singleShot(std::max(0, wait - 7000), &controller, [&controller, n = parser.value(makePrivateOption).toInt()]() {
                QStringList ids;
                for (int i = 0; i < n; ++i)
                    if (const QString id = controller.library()->grid()->idAt(i); !id.isEmpty())
                        ids << id;
                controller.library()->makePrivate(ids);
            });
        }
        if (parser.isSet(privateAlbumOption)) {
            QTimer::singleShot(std::max(0, wait - 3500), &controller, [&controller]() {
                controller.library()->setTab(QStringLiteral("albums"));
                controller.library()->selectNode(controller.privacy()->albumNode());
            });
        }
        if (parser.isSet(personOption)) {
            QTimer::singleShot(std::max(0, wait - 3000), &controller, [&controller, n = parser.value(personOption).toInt()]() {
                const QVariantList people = controller.people()->people();
                if (n >= 0 && n < people.size()) {
                    controller.people()->openPerson(people[n].toMap().value(QStringLiteral("id")).toString());
                    controller.setPage(QStringLiteral("people"));
                }
            });
        }
        if (parser.isSet(faceOption)) {
            QTimer::singleShot(std::max(0, wait - 6000), &controller, [&controller, file = parser.value(faceOption)]() {
                controller.people()->searchByPhoto(QUrl::fromLocalFile(QDir(file).absolutePath()));
                controller.setPage(QStringLiteral("faceSearch"));
            });
        }
        if (parser.isSet(openOption)) {
            // Abre a foto um pouco antes da captura (a grade já carregou).
            QTimer::singleShot(std::max(0, wait - 3000), &controller, [&controller, n = parser.value(openOption).toInt()]() {
                const QString id = controller.library()->grid()->idAt(n);
                if (!id.isEmpty())
                    controller.openPhoto(id);
            });
        }
        if (parser.isSet(copiesOption)) {
            QTimer::singleShot(std::max(0, wait - 3000), &controller, [&controller, n = parser.value(copiesOption).toInt()]() {
                const QString id = controller.library()->grid()->idAt(n);
                if (!id.isEmpty())
                    controller.library()->findCopies(id);
            });
        }
        // Tempo para as imagens assíncronas chegarem; o timeout fecha mesmo se travar.
        QTimer::singleShot(wait, window, [window, file]() {
            const QImage shot = window->grabWindow();
            const bool ok = shot.save(file);
            qCInfo(lcMain) << "Screenshot" << (ok ? "saved to" : "FAILED:") << file;
            QCoreApplication::exit(ok ? 0 : 2);
        });
        QTimer::singleShot(180000, &app, []() { QCoreApplication::exit(3); });
    }

    const int code = QGuiApplication::exec();
    pix::perf::stopWatchdog();
    QThreadPool::globalInstance()->waitForDone();
    qCInfo(lcMain) << "PhotoIDX exiting with" << code;
    return code;
}

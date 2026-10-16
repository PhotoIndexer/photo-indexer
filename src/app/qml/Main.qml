pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Dialogs
import QtCore
import PhotoIDX

// Janela do app: cabeçalho, trilho lateral e a tela de App.page.
//
//   "library"     pages/LibraryPage.qml    (sempre carregada: volta onde estava)
//   "viewer"      pages/ViewerPage.qml     a foto aberta
//   "people"      pages/PeoplePage.qml     pessoas e as faces de cada uma
//   "faceSearch"  pages/FaceSearchPage.qml busca pela foto de uma pessoa
//   "folders"     pages/FoldersPage.qml    pastas indexadas e discos
//   "tasks"       pages/TasksPage.qml      as etapas da indexação
//
// Também é daqui: os diálogos globais (DialogHost), o modal de erro de
// qualquer ``failed``, a faixa de avisos, o Sobre, a paleta de comandos, o
// editor de atalhos, os seletores de pasta/arquivo e os atalhos globais.
QQC.ApplicationWindow {
    id: window

    minimumWidth: 960
    minimumHeight: 600
    visible: false
    color: Theme.background
    title: App.brandName

    Settings {
        id: geometry
        category: "window"
        property int x: 0
        property int y: 0
        property int width: 0
        property int height: 0
        property bool maximized: false
    }

    Component.onCompleted: {
        if (geometry.width >= minimumWidth && geometry.height >= minimumHeight) {
            x = geometry.x;
            y = geometry.y;
            width = geometry.width;
            height = geometry.height;
        } else {
            width = Math.max(minimumWidth, Math.round(Screen.desktopAvailableWidth * 0.85));
            height = Math.max(minimumHeight, Math.round(Screen.desktopAvailableHeight * 0.85));
            x = Math.round((Screen.desktopAvailableWidth - width) / 2) + Screen.virtualX;
            y = Math.round((Screen.desktopAvailableHeight - height) / 2) + Screen.virtualY;
        }
        if (geometry.maximized)
            showMaximized();
        else
            show();
        if (App.catalogError !== "")
            Dialogs.alert({
                title: qsTr("Could not open the catalog"),
                description: qsTr("The catalog in %1 could not be opened, so a temporary one is in use and nothing will be saved.")
                    .arg(Messages.escapeHtml(App.dataDir)) + "<br><br><small>" + Messages.escapeHtml(App.catalogError) + "</small>",
                variant: "danger",
                iconName: "circle-alert"
            });
    }

    onClosing: {
        geometry.maximized = visibility === Window.Maximized;
        if (visibility === Window.Windowed) {
            geometry.x = x;
            geometry.y = y;
            geometry.width = width;
            geometry.height = height;
        }
    }

    header: AppHeader {
        id: appHeader
        onPaletteRequested: palette.open()
        onAddFolderRequested: folderDialog.open()
    }

    // ------------------------------------------------------------------ //
    // Ações (trilho, cabeçalho, paleta e atalhos)
    // ------------------------------------------------------------------ //

    function toggleFullScreen() {
        visibility = visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }

    function runAction(id) {
        switch (id) {
        case "library": case "people": case "folders": case "tasks": App.page = id; break;
        case "palette": palette.opened ? palette.close() : palette.open(); break;
        case "shortcuts": shortcutsDialog.open(); break;
        case "theme": App.darkTheme = !App.darkTheme; break;
        case "fullScreen": toggleFullScreen(); break;
        case "quit": window.close(); break;
        case "addFolder": folderDialog.open(); break;
        case "rescan": App.tasks.scanAll(); break;
        case "faceSearch": faceFileDialog.open(); break;
        case "search":
            App.page = "library";
            appHeader.searchField.forceActiveFocus();
            appHeader.searchField.selectAll();
            break;
        case "aiSearch": App.library.aiSearch = !App.library.aiSearch; break;
        case "zoomIn": App.library.thumbSize = App.library.thumbSize + 40; break;
        case "zoomOut": App.library.thumbSize = App.library.thumbSize - 40; break;
        case "newAlbum": window.newAlbum(""); break;
        }
    }

    /// Pede o nome e cria um álbum (dentro de ``parentId``, se houver).
    function newAlbum(parentId) {
        nameDialog.ask({
            title: parentId ? qsTr("New sub-album") : qsTr("New album"),
            label: qsTr("Album name"),
            confirmLabel: qsTr("Create")
        }, (name) => {
            const id = App.library.createAlbum(parentId, name);
            if (id) {
                App.library.tab = "albums";
                App.library.selectNode("album:" + id);
                App.page = "library";
            }
        });
    }

    readonly property bool globalKeys: !Keymap.recording && Dialogs.current === null && !nameDialog.opened
    readonly property var globalShortcuts: ["library", "people", "folders", "tasks", "palette", "shortcuts", "theme",
        "fullScreen", "quit", "addFolder", "rescan", "faceSearch", "search", "aiSearch", "zoomIn", "zoomOut", "newAlbum"]

    // O Shortcut acha a janela pelo Item pai: cada um vai num Item vazio.
    Repeater {
        model: window.globalShortcuts
        delegate: Item {
            id: binding
            required property string modelData
            Shortcut {
                sequences: Keymap.keys(binding.modelData)
                autoRepeat: binding.modelData === "zoomIn" || binding.modelData === "zoomOut"
                enabled: window.globalKeys && Keymap.keys(binding.modelData).length > 0
                onActivated: window.runAction(binding.modelData)
            }
        }
    }

    /// Comandos da paleta.
    readonly property var commands: {
        const out = [];
        const add = (actionId, name, icon, run) => out.push({
            name: name || Keymap.action(actionId).name,
            shortcut: actionId ? Keymap.label(actionId) : "",
            icon: icon || (actionId ? Keymap.action(actionId).icon : "search"),
            run: run || (() => window.runAction(actionId)),
            keywords: actionId ? actionId.replace(/([A-Z])/g, " $1").toLowerCase() : ""
        });
        add("library");
        add("people");
        add("folders");
        add("tasks");
        add("addFolder");
        add("rescan");
        add("faceSearch");
        add("search");
        add("aiSearch");
        add("filter", "", "", () => { App.page = "library"; App.library.addRule("name"); });
        add("newAlbum");
        add("", qsTr("Photos with errors"), "circle-alert", () => { App.page = "library"; App.library.selectNode("errors"); });
        add("", qsTr("Missing files"), "image-off", () => { App.page = "library"; App.library.selectNode("missing"); });
        for (const tab of ["folders", "dates", "categories", "albums", "places", "people"]) {
            const t = tab;
            add("", qsTr("Side panel: %1").arg(Messages.tabName(t)), "list-tree", () => { App.page = "library"; App.library.tab = t; });
        }
        for (const f of App.library.savedFilters) {
            const id = f.id;
            add("", qsTr("Saved filter: %1").arg(f.name), "sliders-horizontal", () => { App.page = "library"; App.library.applySavedFilter(id); });
        }
        add("", qsTr("About %1").arg(App.brandName), "info", () => about.open());
        add("shortcuts");
        for (const language of App.languages) {
            const code = language.code;
            add("", qsTr("Language: %1").arg(language.name), "languages", () => App.setLanguage(code));
        }
        add("theme", Theme.dark ? qsTr("Light theme") : qsTr("Dark theme"), Theme.dark ? "sun" : "moon");
        add("fullScreen");
        add("", qsTr("Open logs folder"), "file-text", () => App.openLogsFolder());
        add("quit");
        return out;
    }

    Connections {
        target: App

        function onFailed(key, params) {
            Dialogs.alert(Messages.errorDialog(key, params));
        }

        function onNotice(key, params) {
            const text = Messages.notice(key, params);
            if (text !== "")
                toast.show(text);
        }
    }

    // ------------------------------------------------------------------ //
    // Telas
    // ------------------------------------------------------------------ //
    ActivityRail {
        id: rail
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        onAboutRequested: about.open()
        onShortcutsRequested: shortcutsDialog.open()
        onFullScreenRequested: window.toggleFullScreen()
        onQuitRequested: window.close()
    }

    Item {
        id: pages
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        // A biblioteca fica viva: voltar da foto não perde rolagem nem seleção.
        LibraryPage {
            anchors.fill: parent
            visible: App.page === "library"
            onAddFolderRequested: folderDialog.open()
            onNewAlbumRequested: (parentId) => window.newAlbum(parentId)
            names: nameDialog
        }

        Loader {
            anchors.fill: parent
            active: App.page === "viewer"
            visible: active
            sourceComponent: ViewerPage {
                names: nameDialog
            }
        }

        Loader {
            anchors.fill: parent
            active: App.page === "people"
            visible: active
            sourceComponent: PeoplePage {
                names: nameDialog
                onFaceSearchRequested: faceFileDialog.open()
            }
        }

        Loader {
            anchors.fill: parent
            active: App.page === "faceSearch"
            visible: active
            sourceComponent: FaceSearchPage {
                onChooseFileRequested: faceFileDialog.open()
            }
        }

        Loader {
            anchors.fill: parent
            active: App.page === "folders"
            visible: active
            sourceComponent: FoldersPage {
                onAddFolderRequested: folderDialog.open()
            }
        }

        Loader {
            anchors.fill: parent
            active: App.page === "tasks"
            visible: active
            sourceComponent: TasksPage {}
        }
    }

    // ------------------------------------------------------------------ //
    // Diálogos
    // ------------------------------------------------------------------ //

    // Escolher pasta/arquivo é o seletor do SO (regra 1 permite).
    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder to index")
        onAccepted: App.addFolder(selectedFolder)
    }

    FileDialog {
        id: faceFileDialog
        title: qsTr("Choose a photo of the person")
        nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.heic *.webp *.tif *.tiff *.bmp *.nef *.cr2 *.cr3 *.arw *.dng *.raf *.orf *.rw2)"),
                      qsTr("All files (*)")]
        onAccepted: {
            App.people.searchByPhoto(selectedFile);
            App.page = "faceSearch";
        }
    }

    NameDialog {
        id: nameDialog
    }

    AboutDialog {
        id: about
    }

    CommandPalette {
        id: palette
        commands: window.commands
    }

    ShortcutsDialog {
        id: shortcutsDialog
    }

    // Faixa de aviso no rodapé, que some sozinha.
    Rectangle {
        id: toast

        function show(text) {
            toastText.text = text;
            opacity = 1;
            hideTimer.restart();
        }

        parent: QQC.Overlay.overlay
        z: 900
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(parent.width - 48, toastText.implicitWidth + 40)
        height: toastText.implicitHeight + 20
        radius: Theme.radiusLg
        color: Theme.popover
        border.color: Theme.border
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animationNormal } }

        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, toast.width - 40)
            font.pixelSize: Theme.fontBase
            color: Theme.popoverForeground
            wrapMode: Text.Wrap
        }
        Timer {
            id: hideTimer
            interval: 4500
            onTriggered: toast.opacity = 0
        }
    }

    DialogHost {}
}

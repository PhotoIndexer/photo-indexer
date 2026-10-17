pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtCore
import QtQuick.Dialogs
import PhotoIDX

// A biblioteca: à esquerda o painel lateral (abas Pastas, Datas, Categorias,
// Álbuns, Lugares e Pessoas, cada uma uma árvore); à direita a grade das
// fotos do nó escolhido, com a busca do cabeçalho e o filtro personalizado.
Item {
    id: page

    /// NameDialog do Main.
    required property var names
    /// PasswordDialog do Main (fotos privadas).
    required property var passwords

    signal addFolderRequested()
    signal newAlbumRequested(string parentId)

    property bool filterOpen: false
    property real panelWidth: 300

    Settings {
        category: "library"
        property alias panelWidth: page.panelWidth
        property alias filterOpen: page.filterOpen
    }

    readonly property var tabs: [
        { key: "folders", icon: "hard-drive" },
        { key: "dates", icon: "calendar" },
        { key: "categories", icon: "tag" },
        { key: "albums", icon: "book-image" },
        { key: "places", icon: "globe" },
        { key: "people", icon: "users" }
    ]

    readonly property string nodeType: App.library.node.split(":")[0]
    readonly property string nodeId: App.library.node.substring(App.library.node.indexOf(":") + 1)
    readonly property bool hasFolders: App.folders.length > 0
    /// No álbum privado (ou num de dentro): as ações viram "tornar pública".
    readonly property bool inPrivate: App.library.privateNode
    /// Aba Pastas: a grade age também sobre os arquivos (mover, renomear, Lixeira).
    readonly property bool fileMode: App.library.tab === "folders" && !page.inPrivate && !page.copies
    /// A grade mostra as cópias de uma foto (LibraryController::findCopies).
    readonly property bool copies: App.library.copiesOf !== ""
    readonly property bool privateLocked: page.inPrivate && !App.privacy.unlocked

    /// Tornar privada a seleção: explica o que acontece com o arquivo e pede
    /// a senha (ou a cria) antes.
    function makeSelectionPrivate() {
        const n = App.library.grid.selectedCount;
        Dialogs.confirm({
            title: qsTr("Make %n photo(s) private?", "", n),
            description: qsTr("The original file is encrypted into a <b>.photoidxe</b> file in the same folder and the original is deleted after the copy is checked. The photos disappear from the library and only show up in the Private album, with the password.")
                + "<br><br>" + qsTr("Other programs cannot open .photoidxe files. To get the normal file back, make the photo public."),
            variant: "warning",
            iconName: "lock",
            confirmLabel: qsTr("Make private")
        }, () => page.passwords.gate(() => App.library.makePrivate([])));
    }

    function makeSelectionPublic() {
        page.passwords.gate(() => App.library.makePublic([]));
    }

    /// A seleção da grade solta num nó do painel: categoria = mover (corrige
    /// a IA); álbum = pôr (vindo de outro álbum, sai dele). Com ⌥/Ctrl, só
    /// acrescenta (não tira de onde está).
    function dropOnNode(key) {
        const keep = (App.keyboardModifiers() & (Qt.AltModifier | Qt.ControlModifier | Qt.MetaModifier)) !== 0;
        const id = key.substring(key.indexOf(":") + 1);
        if (key.startsWith("cat:")) {
            keep ? App.library.addToCategory([], id) : App.library.moveToCategory([], id);
        } else if (key.startsWith("album:")) {
            if (key === App.privacy.albumNode)
                page.makeSelectionPrivate();
            else
                keep ? App.library.addToAlbum([], id) : App.library.moveToAlbum([], id);
        } else if (key.startsWith("dir:")) {
            page.moveSelectionTo(id);
        }
    }

    // ---- Modo Pastas: os arquivos mudam de lugar (App.files) ----------- //

    function selection() {
        return App.library.grid.selectedIds(grid.currentIndex);
    }

    function folderName(dir) {
        return dir.replace(/\//g, Qt.platform.os === "windows" ? "\\" : "/");
    }

    function moveSelectionTo(dir, chosen) {
        const ids = chosen || page.selection();
        if (ids.length === 0)
            return;
        Dialogs.confirm({
            title: qsTr("Move photos?"),
            description: qsTr("Move %n photo(s) to <b>%1</b>? The files change place on the disk.", "", ids.length)
                .arg(Messages.escapeHtml(page.folderName(dir))),
            variant: "warning",
            iconName: "folder-open",
            confirmLabel: qsTr("Move")
        }, () => App.files.movePhotos(ids, dir));
    }

    function trashSelection() {
        const ids = page.selection();
        if (ids.length === 0)
            return;
        Dialogs.confirm({
            title: qsTr("Move to the Trash?"),
            description: qsTr("%n photo(s) go to the system Trash (you can restore them from there) and leave the catalog.", "", ids.length)
                + (page.networkWarning(App.files.photosOnNetwork(ids)) ? "<br><br>" + page.networkText() : ""),
            variant: "danger",
            confirmLabel: qsTr("Move to Trash")
        }, () => App.files.trashPhotos(ids));
    }

    /// Disco de rede no Windows: lá a Lixeira costuma não existir e o
    /// próprio sistema pode apagar de vez — a pessoa decide sabendo disso.
    function networkWarning(onNetwork) {
        return onNetwork && Qt.platform.os === "windows";
    }
    function networkText() {
        return "<b>" + qsTr("Warning:") + "</b> " + qsTr("this is on a network drive. Network drives usually have no Recycle Bin, so Windows may delete the files permanently.");
    }

    function moveFolderTo(from, to) {
        Dialogs.confirm({
            title: qsTr("Move folder?"),
            description: qsTr("Move <b>%1</b> and everything in it into <b>%2</b>? The files change place on the disk.")
                .arg(Messages.escapeHtml(from.substring(from.lastIndexOf("/") + 1))).arg(Messages.escapeHtml(page.folderName(to))),
            variant: "warning",
            iconName: "folder-open",
            confirmLabel: qsTr("Move")
        }, () => App.files.moveFolder(from, to));
    }

    // O filtro (Ctrl+Shift+F) e o Esc da grade valem só aqui.
    Shortcut {
        sequences: Keymap.keys("filter")
        enabled: page.visible && !Keymap.recording && Dialogs.current === null
        onActivated: page.filterOpen = !page.filterOpen
    }
    Shortcut {
        sequences: Keymap.keys("selectAll")
        enabled: page.visible && grid.activeFocus
        onActivated: App.library.grid.selectAll()
    }
    Shortcut {
        sequences: Keymap.keys("clearSelection")
        enabled: page.visible && App.library.grid.selectedCount > 0 && Dialogs.current === null
        onActivated: App.library.grid.clearSelection()
    }

    // ------------------------------------------------------------------ //
    // Painel lateral
    // ------------------------------------------------------------------ //
    Rectangle {
        id: side
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.max(220, Math.min(page.panelWidth, page.width * 0.5))
        color: Theme.card

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Abas: só ícones, com dica.
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 6
                spacing: 2
                Repeater {
                    model: page.tabs
                    IconButton {
                        id: tabButton
                        required property var modelData
                        Layout.fillWidth: true
                        diameter: 34
                        iconSize: 16
                        iconName: modelData.icon
                        active: App.library.tab === modelData.key
                        tooltip: Messages.tabName(modelData.key)
                        onClicked: App.library.tab = modelData.key
                        // Arrastando fotos: passar por Categorias/Álbuns abre a aba.
                        DropArea {
                            anchors.fill: parent
                            enabled: tabButton.modelData.key === "categories" || tabButton.modelData.key === "albums"
                            keys: ["photoidx/photos"]
                            onEntered: App.library.tab = tabButton.modelData.key
                        }
                    }
                }
            }
            Separator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 6
                Layout.topMargin: 6
                spacing: 2
                SectionTitle {
                    Layout.fillWidth: true
                    text: Messages.tabName(App.library.tab)
                }
                IconButton {
                    visible: App.library.tab === "albums"
                    diameter: 26
                    iconSize: 14
                    iconName: "plus"
                    tooltip: qsTr("New album")
                    onClicked: page.newAlbumRequested("")
                }
                IconButton {
                    visible: App.library.tab === "people"
                    diameter: 26
                    iconSize: 14
                    iconName: "plus"
                    tooltip: qsTr("New group of people")
                    onClicked: page.names.ask({ title: qsTr("New group"), label: qsTr("Group name"),
                                                help: qsTr("For example: Family, Friends, Basketball team. Drag people onto the group, or right-click a person."),
                                                confirmLabel: qsTr("Create") },
                                              (name) => App.library.createPersonGroup(name))
                }
                IconButton {
                    visible: App.library.tab === "categories"
                    diameter: 26
                    iconSize: 14
                    iconName: "plus"
                    tooltip: qsTr("New category")
                    onClicked: page.names.ask({ title: qsTr("New category"), label: qsTr("Category name"),
                                                help: qsTr("The AI looks for photos of new categories by their name (it understands English best). You can set English phrases for it later."),
                                                confirmLabel: qsTr("Create") },
                                              (name) => App.library.createCategory("", name))
                }
                IconButton {
                    visible: App.library.tab === "folders"
                    diameter: 26
                    iconSize: 14
                    iconName: "folder-plus"
                    tooltip: qsTr("Add folder")
                    onClicked: page.addFolderRequested()
                }
                IconButton {
                    diameter: 26
                    iconSize: 14
                    iconName: "list-tree"
                    tooltip: qsTr("Expand or collapse all")
                    property bool expanded: false
                    onClicked: {
                        expanded = !expanded;
                        App.library.tree.expandAll(expanded);
                    }
                }
            }

            // "Todas as fotos" fica sempre por cima da árvore.
            T.ItemDelegate {
                id: allRow
                Layout.fillWidth: true
                implicitHeight: 30
                leftPadding: 14
                hoverEnabled: true
                onClicked: App.library.selectNode("all")
                background: Rectangle {
                    x: 4
                    width: parent.width - 8
                    radius: Theme.radiusMd
                    color: App.library.node === "all" ? Theme.brandSoft : (allRow.hovered ? Theme.muted : "transparent")
                }
                contentItem: RowLayout {
                    spacing: 8
                    Icon {
                        name: "images"
                        size: 14
                        color: App.library.node === "all" ? Theme.brandForeground : Theme.mutedForeground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("All photos")
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.Medium
                        color: Theme.foreground
                    }
                    Text {
                        Layout.rightMargin: 10
                        text: Number(App.tasks.stats.photos || 0).toLocaleString(Qt.locale(), "f", 0)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }
            }

            TreeList {
                id: tree
                Layout.fillWidth: true
                Layout.fillHeight: true
                current: App.library.node
                // O álbum privado trancado pede a senha antes de abrir.
                onPicked: (key) => {
                    const kind = App.library.tree.node(key).kind || "";
                    if (kind === "private-locked")
                        page.passwords.gate(() => App.library.selectNode(key));
                    else
                        App.library.selectNode(key);
                }
                onPhotosDropped: (key) => page.dropOnNode(key)
                // Pessoa solta num grupo; em "Com nome"/"Sem nome", sai do grupo.
                onFolderDropped: (folderKey, key) => page.moveFolderTo(folderKey.substring(4), key.substring(4))
                onPersonDropped: (personKey, key) => App.library.setPersonGroup(personKey.substring(7),
                                                                                key.startsWith("pgroup:") ? key.substring(7) : "")
                onContextRequested: (key, kind, row) => {
                    nodeMenu.key = key;
                    nodeMenu.kind = kind;
                    nodeMenu.label = App.library.tree.node(key).label || "";
                    nodeMenu.popup(row, row.width / 3, row.height);
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.margins: 16
                visible: tree.count === 0
                text: App.library.tab === "places" ? qsTr("Photos with GPS show up here by country, state and city.")
                    : App.library.tab === "people" ? qsTr("People appear as the faces stage finds them.")
                    : App.library.tab === "albums" ? qsTr("No albums yet. Create one with +.")
                    : qsTr("Nothing here yet.")
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.border
        }
    }

    Splitter {
        id: splitter
        x: side.width - width / 2
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        container: page
        mode: "pixels"
        value: side.width
        from: 220
        to: Math.max(260, page.width * 0.5)
        stepSize: 16
        defaultValue: 300
        label: qsTr("Side panel width")
        onMoved: (v) => page.panelWidth = v
    }

    // Pessoa no painel: mover para um grupo ("Mover para grupo…" do nodeMenu).
    ChoiceMenu {
        id: groupMenu
        title: qsTr("Move to group")
        choices: nodeMenu.groups.filter((g) => g.id !== nodeMenu.personGroup)
        emptyText: qsTr("No other groups")
        onPicked: (groupId) => App.library.setPersonGroup(nodeMenu.id, groupId)
    }

    // Menu do nó (botão direito), conforme o tipo.
    AppMenu {
        id: nodeMenu

        property string key: ""
        property string kind: ""
        property string label: ""
        readonly property string type: key.split(":")[0]
        readonly property string id: key.substring(key.indexOf(":") + 1)
        readonly property bool privateRoot: kind === "private-locked" || kind === "private-open"
        /// Pessoa: o grupo dela e os grupos que existem (lidos ao abrir).
        property string personGroup: ""
        property var groups: []
        onAboutToShow: {
            if (type === "person") {
                personGroup = App.library.personGroupOf(id);
                groups = App.library.personGroups();
            }
        }

        AppMenuItem {
            text: qsTr("Show photos")
            onTriggered: App.library.selectNode(nodeMenu.key)
        }
        AppMenuItem {
            visible: nodeMenu.type === "dir"
            height: visible ? implicitHeight : 0
            text: qsTr("Show in file manager")
            onTriggered: App.revealInFolder(nodeMenu.id)
        }
        AppMenuItem {
            visible: nodeMenu.type === "dir"
            height: visible ? implicitHeight : 0
            text: qsTr("New folder…")
            onTriggered: page.names.ask({ title: qsTr("New folder"), label: qsTr("Folder name"), confirmLabel: qsTr("Create") },
                                        (name) => App.files.createFolder(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.kind === "dir"
            height: visible ? implicitHeight : 0
            text: qsTr("Rename folder…")
            onTriggered: page.names.ask({ title: qsTr("Rename folder"), label: qsTr("Folder name"),
                                          text: nodeMenu.label, confirmLabel: qsTr("Rename") },
                                        (name) => App.files.renameFolder(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.kind === "dir"
            height: visible ? implicitHeight : 0
            text: qsTr("Move folder to Trash…")
            onTriggered: Dialogs.confirm({
                title: qsTr("Move folder to the Trash?"),
                description: qsTr("<b>%1</b> and everything in it go to the system Trash (you can restore them from there). Its photos leave the catalog.")
                    .arg(Messages.escapeHtml(nodeMenu.label))
                    + (page.networkWarning(App.files.folderOnNetwork(nodeMenu.id)) ? "<br><br>" + page.networkText() : ""),
                variant: "danger",
                confirmLabel: qsTr("Move to Trash")
            }, () => App.files.trashFolder(nodeMenu.id))
        }
        AppMenuItem {
            visible: nodeMenu.type === "disk"
            height: visible ? implicitHeight : 0
            text: qsTr("Folders and disks…")
            onTriggered: App.page = "folders"
        }
        AppMenuItem {
            visible: nodeMenu.privateRoot
            height: visible ? implicitHeight : 0
            text: App.privacy.unlocked ? qsTr("Lock private photos") : qsTr("Unlock private photos…")
            onTriggered: App.privacy.unlocked ? App.privacy.lock() : page.passwords.gate(() => App.library.selectNode(nodeMenu.key))
        }
        AppMenuItem {
            visible: nodeMenu.privateRoot
            height: visible ? implicitHeight : 0
            text: qsTr("Privacy settings…")
            onTriggered: App.page = "privacy"
        }
        AppMenuItem {
            visible: nodeMenu.type === "album" && (!nodeMenu.privateRoot || App.privacy.unlocked)
            height: visible ? implicitHeight : 0
            text: qsTr("New sub-album…")
            onTriggered: page.newAlbumRequested(nodeMenu.id)
        }
        AppMenuItem {
            visible: nodeMenu.type === "album" && !nodeMenu.privateRoot
            height: visible ? implicitHeight : 0
            text: qsTr("Rename…")
            onTriggered: page.names.ask({ title: qsTr("Rename album"), label: qsTr("Album name"), text: nodeMenu.label },
                                        (name) => App.library.renameAlbum(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.type === "album" && !nodeMenu.privateRoot
            height: visible ? implicitHeight : 0
            text: qsTr("Delete album…")
            onTriggered: Dialogs.confirm({
                title: qsTr("Delete album?"),
                description: qsTr("Delete <b>%1</b> and its sub-albums? The photos stay in the catalog and on disk.")
                    .arg(Messages.escapeHtml(nodeMenu.label)),
                variant: "danger",
                confirmLabel: qsTr("Delete")
            }, () => App.library.deleteAlbum(nodeMenu.id))
        }
        AppMenuItem {
            visible: nodeMenu.type === "cat"
            height: visible ? implicitHeight : 0
            text: qsTr("New subcategory…")
            onTriggered: page.names.ask({ title: qsTr("New subcategory"), label: qsTr("Category name"), confirmLabel: qsTr("Create") },
                                        (name) => App.library.createCategory(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.type === "cat"
            height: visible ? implicitHeight : 0
            text: qsTr("Rename…")
            onTriggered: page.names.ask({ title: qsTr("Rename category"), label: qsTr("Category name"), text: nodeMenu.label },
                                        (name) => App.library.renameCategory(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.type === "cat"
            height: visible ? implicitHeight : 0
            text: qsTr("AI phrases…")
            onTriggered: page.names.ask({
                title: qsTr("What the AI looks for"),
                label: qsTr("Phrases in English, separated by |"),
                help: qsTr("Example: a photo of a beach|waves on the sea. Empty: the AI uses the category name."),
                text: App.library.categoryPrompt(nodeMenu.id),
                allowEmpty: true
            }, (prompt) => App.library.setCategoryPrompt(nodeMenu.id, prompt))
        }
        AppMenuItem {
            visible: nodeMenu.type === "cat"
            height: visible ? implicitHeight : 0
            text: qsTr("Delete category…")
            onTriggered: Dialogs.confirm({
                title: qsTr("Delete category?"),
                description: qsTr("Delete <b>%1</b> and its subcategories? The photos stay in the catalog and on disk.")
                    .arg(Messages.escapeHtml(nodeMenu.label)),
                variant: "danger",
                confirmLabel: qsTr("Delete")
            }, () => App.library.deleteCategory(nodeMenu.id))
        }
        AppMenuItem {
            visible: nodeMenu.type === "pgroup"
            height: visible ? implicitHeight : 0
            text: qsTr("Rename…")
            onTriggered: page.names.ask({ title: qsTr("Rename group"), label: qsTr("Group name"), text: nodeMenu.label },
                                        (name) => App.library.renamePersonGroup(nodeMenu.id, name))
        }
        AppMenuItem {
            visible: nodeMenu.type === "pgroup"
            height: visible ? implicitHeight : 0
            text: qsTr("Delete group…")
            onTriggered: Dialogs.confirm({
                title: qsTr("Delete group?"),
                description: qsTr("Delete the group <b>%1</b>? The people in it stay, back in “Named people” or “Unnamed people”.")
                    .arg(Messages.escapeHtml(nodeMenu.label)),
                variant: "danger",
                confirmLabel: qsTr("Delete")
            }, () => App.library.deletePersonGroup(nodeMenu.id))
        }
        // Os grupos abrem como um menu à parte (não submenu): um submenu
        // posto e tirado em tempo de execução derrubava o app ao fechar.
        AppMenuItem {
            visible: nodeMenu.type === "person"
            height: visible ? implicitHeight : 0
            text: qsTr("Move to group…")
            onTriggered: groupMenu.popup()
        }
        AppMenuItem {
            id: newGroupItem
            visible: nodeMenu.type === "person"
            height: visible ? implicitHeight : 0
            text: qsTr("New group with this person…")
            onTriggered: page.names.ask({ title: qsTr("New group"), label: qsTr("Group name"), confirmLabel: qsTr("Create") },
                                        (name) => {
                                            const groupId = App.library.createPersonGroup(name);
                                            if (groupId)
                                                App.library.setPersonGroup(nodeMenu.id, groupId);
                                        })
        }
        AppMenuItem {
            visible: nodeMenu.type === "person" && nodeMenu.personGroup !== ""
            height: visible ? implicitHeight : 0
            text: qsTr("Remove from group")
            onTriggered: App.library.setPersonGroup(nodeMenu.id, "")
        }
        AppMenuItem {
            visible: nodeMenu.type === "person" && App.people.facesAvailable
            height: visible ? implicitHeight : 0
            text: qsTr("Find similar faces")
            onTriggered: {
                App.people.searchByPerson(nodeMenu.id);
                App.page = "faceSearch";
            }
        }
        AppMenuItem {
            visible: nodeMenu.type === "person"
            height: visible ? implicitHeight : 0
            text: qsTr("Open in People")
            onTriggered: {
                App.people.openPerson(nodeMenu.id);
                App.page = "people";
            }
        }
    }

    // ------------------------------------------------------------------ //
    // Grade
    // ------------------------------------------------------------------ //
    ColumnLayout {
        anchors.left: side.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        // Barra: nó, contagem, ações da seleção, ordem, filtro, tamanho.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            color: Theme.background

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                spacing: 8

                // Busca de cópias: volta para onde estava.
                IconButton {
                    visible: page.copies
                    diameter: 30
                    iconSize: 16
                    iconName: "arrow-left"
                    tooltip: qsTr("Back")
                    onClicked: App.library.clearCopies()
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: page.copies ? qsTr("Copies of “%1”").arg(App.library.copiesName) : App.library.nodeLabel
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        text: App.library.grid.selectedCount > 0
                            ? qsTr("%1 of %n photo(s) selected", "", App.library.grid.count).arg(App.library.grid.selectedCount)
                            : page.copies
                            ? qsTr("%n copy(ies) in other places — the first photo is the one you picked", "", Math.max(0, App.library.grid.count - 1))
                            : qsTr("%n photo(s)", "", App.library.grid.count)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }

                // Ações sobre a seleção.
                Button {
                    id: albumButton
                    visible: App.library.grid.selectedCount > 0
                    size: "sm"
                    implicitHeight: 30
                    variant: "outline"
                    iconName: "book-image"
                    text: qsTr("Album")
                    onClicked: albumMenu.popup(albumButton, 0, albumButton.height + 4)
                }
                Button {
                    id: categoryButton
                    visible: App.library.grid.selectedCount > 0
                    size: "sm"
                    implicitHeight: 30
                    variant: "outline"
                    iconName: "tag"
                    text: qsTr("Category")
                    onClicked: categoryMenu.popup(categoryButton, 0, categoryButton.height + 4)
                }
                // A IA errou: tira da categoria atual (ou das que a IA pôs)
                // e põe na escolhida. O mesmo que arrastar para o painel.
                Button {
                    id: moveButton
                    visible: App.library.grid.selectedCount > 0
                    size: "sm"
                    implicitHeight: 30
                    variant: "outline"
                    iconName: "replace"
                    text: qsTr("Move to…")
                    onClicked: moveMenu.popup(moveButton, 0, moveButton.height + 4)
                    Tip {
                        text: page.nodeType === "cat"
                            ? qsTr("Take the selected photos out of “%1” and put them in another category. Tip: you can also drag them onto a category in the side panel.").arg(App.library.nodeLabel)
                            : qsTr("Replace the AI's categories of the selected photos with another one. Tip: you can also drag them onto a category in the side panel.")
                        visible: moveButton.hovered
                    }
                }
                Button {
                    visible: App.library.grid.selectedCount > 0
                    size: "sm"
                    implicitHeight: 30
                    variant: "outline"
                    iconName: page.inPrivate ? "lock-open" : "lock"
                    text: page.inPrivate ? qsTr("Make public") : qsTr("Make private")
                    onClicked: page.inPrivate ? page.makeSelectionPublic() : page.makeSelectionPrivate()
                }
                Button {
                    visible: App.library.grid.selectedCount > 0 && !page.copies && (page.nodeType === "album" || page.nodeType === "cat")
                                && !(page.inPrivate && page.nodeId === App.privacy.albumNode.substring(6))
                    size: "sm"
                    implicitHeight: 30
                    variant: "ghost"
                    iconName: "x"
                    text: page.nodeType === "album" ? qsTr("Remove from album") : qsTr("Remove from category")
                    onClicked: page.nodeType === "album" ? App.library.removeFromAlbum([], page.nodeId)
                                                         : App.library.removeFromCategory([], page.nodeId)
                }
                Separator {
                    vertical: true
                    visible: App.library.grid.selectedCount > 0
                    Layout.preferredHeight: 22
                }

                // Tolerância da busca de cópias (Catalog::findCopies).
                Select {
                    visible: page.copies
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSm
                    model: [
                        { value: 0, text: qsTr("Identical") },
                        { value: 1, text: qsTr("Close: resized, recompressed, watermark") },
                        { value: 2, text: qsTr("Loose: also crops and color edits") }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: App.library.copiesTolerance
                    onActivated: App.library.copiesTolerance = currentValue
                    Accessible.name: qsTr("Tolerance")
                }
                Select {
                    visible: !page.copies
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSm
                    model: ["date-desc", "date-asc", "name", "added", "size", "resolution", "folder"]
                        .map((s) => ({ value: s, text: Messages.sortName(s) }))
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: model.findIndex((m) => m.value === App.library.sort)
                    onActivated: App.library.sort = currentValue
                }
                IconButton {
                    diameter: 30
                    iconSize: 16
                    iconName: "sliders-horizontal"
                    active: page.filterOpen || App.library.rules.length > 0
                    tooltip: App.library.rules.length > 0 ? qsTr("Filter (%n condition(s))", "", App.library.rules.length)
                                                          : qsTr("Filter")
                    shortcut: Keymap.label("filter")
                    onClicked: page.filterOpen = !page.filterOpen
                }
                Icon {
                    name: "zoom-out"
                    size: 14
                    color: Theme.mutedForeground
                }
                T.Slider {
                    id: sizeSlider
                    Layout.preferredWidth: 110
                    from: 80
                    to: 480
                    stepSize: 10
                    value: App.library.thumbSize
                    onMoved: App.library.thumbSize = value
                    Accessible.name: qsTr("Thumbnail size")
                    background: Rectangle {
                        x: sizeSlider.leftPadding
                        y: sizeSlider.topPadding + sizeSlider.availableHeight / 2 - height / 2
                        width: sizeSlider.availableWidth
                        height: 3
                        radius: 2
                        color: Theme.muted
                        Rectangle {
                            width: sizeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 2
                            color: Theme.sliderFill
                        }
                    }
                    handle: Rectangle {
                        x: sizeSlider.leftPadding + sizeSlider.visualPosition * (sizeSlider.availableWidth - width)
                        y: sizeSlider.topPadding + sizeSlider.availableHeight / 2 - height / 2
                        width: 14
                        height: 14
                        radius: 7
                        color: Theme.sliderHandle
                        border.color: Theme.sliderHandleBorder
                    }
                }
                Icon {
                    name: "zoom-in"
                    size: 14
                    color: Theme.mutedForeground
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        FilterBar {
            Layout.fillWidth: true
            visible: page.filterOpen || App.library.rules.length > 0
            names: page.names
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            PhotoGrid {
                id: grid
                anchors.fill: parent
                photos: App.library.grid
                dragEnabled: true
                showScore: page.copies || (App.library.aiSearch && App.library.search !== "")
                showFolder: page.copies
                visible: App.library.grid.count > 0 && !page.copies
                onOpened: (photoId) => App.openPhoto(photoId)
                onContextRequested: (index, cell) => photoMenu.popup(cell, cell.width / 2, cell.height / 2)
                Connections {
                    target: App.library
                    function onFocusRequested(index) {
                        grid.currentIndex = index;
                        grid.positionViewAtIndex(index, GridView.Center);
                    }
                }
            }

            // Cópias: por pasta, o caminho e as fotos dela embaixo.
            CopiesView {
                anchors.fill: parent
                visible: page.copies && App.library.grid.count > 0
                onOpened: (photoId) => App.openPhoto(photoId)
                onContextRequested: (photoId, item) => {
                    copyMenu.photoId = photoId;
                    copyMenu.popup(item, item.width / 2, item.height / 2);
                }
            }

            // Vazio: sem pasta nenhuma, ou nada no nó/busca/filtro.
            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 64, 520)
                visible: App.library.grid.count === 0
                spacing: 14

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    name: page.privateLocked ? "lock" : page.hasFolders ? "image-off" : "folder-plus"
                    size: 44
                    color: Theme.mutedForeground
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.privateLocked ? qsTr("Private photos are locked")
                        : page.inPrivate ? qsTr("No private photos yet")
                        : !page.hasFolders ? qsTr("Add a folder to start your catalog")
                        : App.tasks.pending > 0 && App.library.node === "all" && App.library.search === "" ? qsTr("Indexing your photos…")
                        : qsTr("No photos here")
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    color: Theme.foreground
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.privateLocked ? qsTr("Enter the password to see the Private album and its sub-albums.")
                        : page.inPrivate ? qsTr("Select photos in the library and choose “Make private”. Their files are encrypted and they only show up here.")
                        : !page.hasFolders
                        ? qsTr("%1 looks for photos in the folder and in all its subfolders, including RAW files, on local disks, USB drives and network shares. Your files are never changed.").arg(App.brandName)
                        : qsTr("Try another item in the side panel, another search or fewer filter conditions.")
                    font.pixelSize: Theme.fontBase
                    color: Theme.mutedForeground
                    wrapMode: Text.Wrap
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    visible: page.privateLocked
                    iconName: "lock-open"
                    text: qsTr("Unlock")
                    onClicked: page.passwords.gate(null)
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !page.hasFolders && !page.inPrivate
                    iconName: "folder-plus"
                    text: qsTr("Add folder")
                    onClicked: page.addFolderRequested()
                }
            }
        }
    }

    // Menus da seleção.
    ChoiceMenu {
        id: albumMenu
        choices: App.library.albumChoices
        emptyText: qsTr("No albums yet")
        onPicked: (id) => App.library.addToAlbum([], id)
    }
    ChoiceMenu {
        id: categoryMenu
        choices: App.library.categoryChoices
        onPicked: (id) => App.library.addToCategory([], id)
    }
    ChoiceMenu {
        id: moveMenu
        choices: App.library.categoryChoices
        onPicked: (id) => App.library.moveToCategory([], id)
    }

    AppMenu {
        id: photoMenu

        readonly property string firstId: App.library.grid.selectedIds(grid.currentIndex)[0] || ""
        /// As pessoas da foto clicada, lidas ao abrir o menu.
        property var people: ({})
        onAboutToShow: people = App.library.photoPeople(firstId)

        AppMenuItem {
            text: qsTr("Open")
            onTriggered: App.openPhoto(photoMenu.firstId)
        }
        AppMenuItem {
            // A pasta da foto no painel (aba Pastas), com a foto escolhida.
            visible: !page.inPrivate
            height: visible ? implicitHeight : 0
            text: qsTr("Show in folder")
            onTriggered: App.library.showInFolder(photoMenu.firstId)
        }
        AppMenuItem {
            text: qsTr("Show in file manager")
            onTriggered: App.revealInFolder(App.photoPath(photoMenu.firstId))
        }
        AppMenuItem {
            // Um .photoidxe só o PhotoIDX abre.
            visible: !page.inPrivate
            height: visible ? implicitHeight : 0
            text: qsTr("Open with the default app")
            onTriggered: App.openExternally(App.photoPath(photoMenu.firstId))
        }
        // As pessoas da foto: escolher abre a pessoa; o submenu busca o rosto
        // dela (nesta foto) nas outras. Só submenus declarados aqui — nada de
        // submenu criado em tempo de execução (derrubava o app ao fechar).
        AppMenu {
            id: peopleMenu
            title: qsTr("People")
            readonly property var entries: (photoMenu.people.people || []).map((p) => ({
                id: p.id, faceId: p.faceId,
                name: p.name || (p.id ? App.people.labelOf(p.id) : qsTr("Unknown person"))
            }))
            Instantiator {
                model: peopleMenu.entries.length > 0 ? peopleMenu.entries : [null]
                delegate: AppMenuItem {
                    required property var modelData
                    text: modelData ? modelData.name
                                    : (photoMenu.people.facesState === "pending" ? qsTr("Waiting in the faces queue")
                                                                                : qsTr("No people in this photo"))
                    enabled: !!modelData && !!modelData.id
                    onTriggered: {
                        App.people.openPerson(modelData.id);
                        App.page = "people";
                    }
                }
                onObjectAdded: (index, object) => peopleMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => peopleMenu.removeItem(object)
            }
            AppMenuSeparator {}
            ChoiceMenu {
                title: qsTr("Find this person in other photos")
                choices: peopleMenu.entries.map((e) => ({ id: e.faceId, name: e.name }))
                emptyText: qsTr("No people in this photo")
                onPicked: (faceId) => {
                    App.people.searchByFace(faceId);
                    App.page = "faceSearch";
                }
            }
        }
        AppMenuItem {
            visible: !page.inPrivate && photoMenu.people.facesState === "pending" && App.people.facesAvailable
            height: visible ? implicitHeight : 0
            text: qsTr("Find faces now")
            onTriggered: App.library.findFacesNow([])
        }
        // Modo Pastas: o arquivo em si.
        AppMenuSeparator {
            visible: page.fileMode
            height: visible ? implicitHeight : 0
        }
        AppMenuItem {
            visible: page.fileMode
            height: visible ? implicitHeight : 0
            text: qsTr("Move to folder…")
            onTriggered: {
                moveDialog.ids = page.selection();
                moveDialog.open();
            }
        }
        AppMenuItem {
            visible: page.fileMode && App.library.grid.selectedCount <= 1
            height: visible ? implicitHeight : 0
            text: qsTr("Rename file…")
            onTriggered: {
                const id = photoMenu.firstId;
                const name = App.photoPath(id).split(/[\\/]/).pop();
                page.names.ask({ title: qsTr("Rename file"), label: qsTr("File name"), text: name, confirmLabel: qsTr("Rename") },
                               (newName) => App.files.renamePhoto(id, newName));
            }
        }
        AppMenuItem {
            visible: page.fileMode
            height: visible ? implicitHeight : 0
            text: qsTr("Move to Trash…")
            onTriggered: page.trashSelection()
        }
        AppMenuSeparator {
            visible: page.fileMode
            height: visible ? implicitHeight : 0
        }
        AppMenuItem {
            // A mesma imagem em outras pastas e discos, com tolerância
            // (tamanho, compressão, marca d'água).
            visible: !page.inPrivate
            height: visible ? implicitHeight : 0
            text: qsTr("Find copies in other places")
            onTriggered: App.library.findCopies(photoMenu.firstId)
        }
        AppMenuItem {
            text: page.inPrivate ? qsTr("Make public") : qsTr("Make private…")
            onTriggered: page.inPrivate ? page.makeSelectionPublic() : page.makeSelectionPrivate()
        }
        AppMenuSeparator {}
        ChoiceMenu {
            title: qsTr("Add to album")
            choices: App.library.albumChoices
            emptyText: qsTr("No albums yet")
            onPicked: (id) => App.library.addToAlbum([], id)
        }
        ChoiceMenu {
            title: qsTr("Add to category")
            choices: App.library.categoryChoices
            onPicked: (id) => App.library.addToCategory([], id)
        }
        ChoiceMenu {
            title: qsTr("Move to category")
            choices: App.library.categoryChoices
            onPicked: (id) => App.library.moveToCategory([], id)
        }
        AppMenuItem {
            visible: !page.copies && page.nodeType === "album" && page.nodeId !== App.privacy.albumNode.substring(6)
            height: visible ? implicitHeight : 0
            text: qsTr("Remove from this album")
            onTriggered: App.library.removeFromAlbum([], page.nodeId)
        }
        AppMenuItem {
            visible: !page.copies && page.nodeType === "cat"
            height: visible ? implicitHeight : 0
            text: qsTr("Remove from this category")
            onTriggered: App.library.removeFromCategory([], page.nodeId)
        }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTr("New album with the selection…")
            onTriggered: page.names.ask({ title: qsTr("New album"), label: qsTr("Album name"), confirmLabel: qsTr("Create") },
                                        (name) => {
                                            const id = App.library.createAlbum("", name);
                                            if (id)
                                                App.library.addToAlbum([], id);
                                        })
        }
    }

    // "Mover para pasta…": escolher a pasta (seletor do sistema é permitido —
    // é escolha de arquivo, não aviso). Tem de estar dentro de uma indexada.
    FolderDialog {
        id: moveDialog
        property var ids: []
        title: qsTr("Move to folder")
        onAccepted: page.moveSelectionTo(decodeURIComponent(String(selectedFolder).replace(/^file:\/\//, "")
                                                                .replace(/^\/([A-Za-z]:)/, "$1")), moveDialog.ids)
    }

    // Menu de uma foto na vista de cópias: abrir, mostrar na pasta e, para
    // ficar com uma só, mandar a cópia para a Lixeira.
    AppMenu {
        id: copyMenu
        property string photoId: ""
        AppMenuItem {
            text: qsTr("Open")
            onTriggered: App.openPhoto(copyMenu.photoId)
        }
        AppMenuItem {
            text: qsTr("Show in file manager")
            onTriggered: App.revealInFolder(App.photoPath(copyMenu.photoId))
        }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTr("Move to Trash…")
            onTriggered: {
                const id = copyMenu.photoId;
                Dialogs.confirm({
                    title: qsTr("Move to the Trash?"),
                    description: qsTr("%n photo(s) go to the system Trash (you can restore them from there) and leave the catalog.", "", 1)
                        + (page.networkWarning(App.files.photosOnNetwork([id])) ? "<br><br>" + page.networkText() : ""),
                    variant: "danger",
                    confirmLabel: qsTr("Move to Trash")
                }, () => App.files.trashPhotos([id]));
            }
        }
    }
}

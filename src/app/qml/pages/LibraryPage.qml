pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtCore
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
                        required property var modelData
                        Layout.fillWidth: true
                        diameter: 34
                        iconSize: 16
                        iconName: modelData.icon
                        active: App.library.tab === modelData.key
                        tooltip: Messages.tabName(modelData.key)
                        onClicked: App.library.tab = modelData.key
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

    // Menu do nó (botão direito), conforme o tipo.
    AppMenu {
        id: nodeMenu

        property string key: ""
        property string kind: ""
        property string label: ""
        readonly property string type: key.split(":")[0]
        readonly property string id: key.substring(key.indexOf(":") + 1)
        readonly property bool privateRoot: kind === "private-locked" || kind === "private-open"

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

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: App.library.nodeLabel
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        text: App.library.grid.selectedCount > 0
                            ? qsTr("%1 of %n photo(s) selected", "", App.library.grid.count).arg(App.library.grid.selectedCount)
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
                    visible: App.library.grid.selectedCount > 0 && (page.nodeType === "album" || page.nodeType === "cat")
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

                Select {
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
                showScore: App.library.aiSearch && App.library.search !== ""
                visible: App.library.grid.count > 0
                onOpened: (photoId) => App.openPhoto(photoId)
                onContextRequested: (index, cell) => photoMenu.popup(cell, cell.width / 2, cell.height / 2)
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

    AppMenu {
        id: photoMenu

        readonly property string firstId: App.library.grid.selectedIds(grid.currentIndex)[0] || ""

        AppMenuItem {
            text: qsTr("Open")
            onTriggered: App.openPhoto(photoMenu.firstId)
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
        AppMenuItem {
            visible: page.nodeType === "album" && page.nodeId !== App.privacy.albumNode.substring(6)
            height: visible ? implicitHeight : 0
            text: qsTr("Remove from this album")
            onTriggered: App.library.removeFromAlbum([], page.nodeId)
        }
        AppMenuItem {
            visible: page.nodeType === "cat"
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
}

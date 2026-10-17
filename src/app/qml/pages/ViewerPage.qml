pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtCore
import PhotoIDX

// A foto aberta (App.viewer): a imagem grande com as faces, e ao lado tudo o
// que o catálogo sabe dela — datas, câmera, lugar, disco, categorias, álbuns,
// pessoas, fotos iguais/parecidas e o EXIF completo.
Item {
    id: page

    required property var names
    /// PasswordDialog do Main (fotos privadas).
    required property var passwords

    readonly property var d: App.viewer.detail
    property bool showFaces: true
    property real panelWidth: 380
    property string exifFilter: ""

    Settings {
        category: "viewer"
        property alias showFaces: page.showFaces
        property alias panelWidth: page.panelWidth
    }

    function back() {
        App.page = "library";
    }

    function line(label, value) {
        return value === undefined || value === null || value === "" ? null : { label: label, value: String(value) };
    }

    readonly property var summary: {
        const o = d || {};
        const shot = [];
        if (o.aperture) shot.push(App.formatAperture(o.aperture));
        if (o.shutter) shot.push(App.formatShutter(o.shutter));
        if (o.iso) shot.push("ISO " + o.iso);
        if (o.focalLength) shot.push(Math.round(o.focalLength) + " mm" + (o.focalLength35 ? " (" + o.focalLength35 + " mm 35mm)" : ""));
        const mp = o.width && o.height ? (o.width * o.height / 1e6).toFixed(1) : "";
        return [
            line(o.takenSource === "file" ? qsTr("Date (file)") : qsTr("Date taken"), App.formatDateTime(o.takenAt || "")),
            line(qsTr("Edited"), App.formatDateTime(o.modifiedAt || "")),
            line(qsTr("Camera"), o.camera),
            line(qsTr("Lens"), o.lens),
            line(qsTr("Exposure"), shot.join(" · ")),
            line(qsTr("Exposure compensation"), o.exposureBias ? (o.exposureBias > 0 ? "+" : "") + Number(o.exposureBias).toFixed(1) + " EV" : ""),
            line(qsTr("Flash"), o.flash === undefined || o.flash === null ? "" : (o.flash % 2 ? qsTr("Fired") : qsTr("Did not fire"))),
            line(qsTr("Dimensions"), o.width ? o.width + " × " + o.height + " px (" + mp + " MP)" : ""),
            line(qsTr("Format"), o.format ? o.format + (o.isRaw ? " (RAW)" : "") : ""),
            line(qsTr("Size"), o.size ? App.formatBytes(o.size) : ""),
            line(qsTr("Software"), o.software),
            line(qsTr("Artist"), o.artist),
            line(qsTr("Copyright"), o.copyright),
            line(qsTr("Description"), o.description)
        ].filter((x) => x !== null);
    }

    focus: true
    Keys.onEscapePressed: page.back()
    Shortcut {
        sequences: Keymap.keys("nextPhoto")
        enabled: page.visible && !Keymap.recording && Dialogs.current === null
        onActivated: App.viewer.next()
    }
    Shortcut {
        sequences: Keymap.keys("previousPhoto")
        enabled: page.visible && !Keymap.recording && Dialogs.current === null
        onActivated: App.viewer.previous()
    }
    Shortcut {
        sequences: Keymap.keys("faceBoxes")
        enabled: page.visible && !Keymap.recording && Dialogs.current === null
        onActivated: page.showFaces = !page.showFaces
    }

    // ------------------------------------------------------------------ //
    // Barra
    // ------------------------------------------------------------------ //
    Rectangle {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        height: 44
        color: Theme.card
        z: 2

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 12
            spacing: 4

            IconButton {
                diameter: 32
                iconName: "arrow-left"
                tooltip: qsTr("Back to the library")
                shortcut: "Esc"
                onClicked: page.back()
            }
            Text {
                Layout.fillWidth: true
                text: page.d.name || ""
                font.pixelSize: Theme.fontBase
                font.weight: Font.DemiBold
                color: Theme.foreground
                elide: Text.ElideMiddle
            }
            Text {
                visible: App.viewer.position >= 0
                text: qsTr("%1 of %2").arg(App.viewer.position + 1).arg(App.viewer.total)
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
            }
            IconButton {
                diameter: 32
                iconName: "chevron-left"
                enabled: App.viewer.position > 0
                tooltip: qsTr("Previous photo")
                shortcut: Keymap.label("previousPhoto")
                onClicked: App.viewer.previous()
            }
            IconButton {
                diameter: 32
                iconName: "chevron-right"
                enabled: App.viewer.position >= 0 && App.viewer.position + 1 < App.viewer.total
                tooltip: qsTr("Next photo")
                shortcut: Keymap.label("nextPhoto")
                onClicked: App.viewer.next()
            }
            Separator { vertical: true; Layout.preferredHeight: 22 }
            IconButton {
                diameter: 32
                iconName: "scan-face"
                active: page.showFaces
                enabled: (page.d.faces || []).length > 0
                tooltip: page.showFaces ? qsTr("Hide faces") : qsTr("Show faces")
                shortcut: Keymap.label("faceBoxes")
                onClicked: page.showFaces = !page.showFaces
            }
            IconButton {
                diameter: 32
                iconName: "folder-search"
                enabled: !page.d.offline
                tooltip: qsTr("Show in file manager")
                onClicked: App.revealInFolder(page.d.path)
            }
            IconButton {
                diameter: 32
                iconName: "external-link"
                // Um .photoidxe só o PhotoIDX abre.
                visible: !page.d.isPrivate
                enabled: !page.d.offline
                tooltip: qsTr("Open with the default app")
                onClicked: App.openExternally(page.d.path)
            }
            IconButton {
                diameter: 32
                iconName: page.d.isPrivate ? "lock-open" : "lock"
                active: !!page.d.isPrivate
                tooltip: page.d.isPrivate ? qsTr("Make public") : qsTr("Make private…")
                onClicked: {
                    if (page.d.isPrivate) {
                        page.passwords.gate(() => App.viewer.makePublic());
                        return;
                    }
                    Dialogs.confirm({
                        title: qsTr("Make this photo private?"),
                        description: qsTr("The original file is encrypted into a <b>.photoidxe</b> file in the same folder and the original is deleted after the copy is checked. The photo disappears from the library and only shows up in the Private album, with the password."),
                        variant: "warning",
                        iconName: "lock",
                        confirmLabel: qsTr("Make private")
                    }, () => page.passwords.gate(() => {
                        App.viewer.makePrivate();
                        page.back();
                    }));
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }
    }

    // ------------------------------------------------------------------ //
    // Foto
    // ------------------------------------------------------------------ //
    Rectangle {
        id: stage
        anchors.left: parent.left
        anchors.right: panel.left
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        color: Theme.imagePane

        Image {
            id: photo
            anchors.fill: parent
            anchors.margins: 16
            source: App.viewer.imageUrl
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            cache: false
            smooth: true
            mipmap: true
        }

        Spinner {
            anchors.centerIn: parent
            size: 28
            color: Theme.white
            visible: photo.status === Image.Loading
        }

        // Faces: caixa normalizada (0..1) sobre a área pintada da imagem.
        Item {
            id: painted
            visible: page.showFaces && photo.status === Image.Ready
            x: photo.x + (photo.width - photo.paintedWidth) / 2
            y: photo.y + (photo.height - photo.paintedHeight) / 2
            width: photo.paintedWidth
            height: photo.paintedHeight

            Repeater {
                model: page.d.faces || []
                Rectangle {
                    id: box
                    required property var modelData
                    x: modelData.x * painted.width
                    y: modelData.y * painted.height
                    width: modelData.w * painted.width
                    height: modelData.h * painted.height
                    color: "transparent"
                    radius: 3
                    border.width: 2
                    border.color: modelData.personName ? Theme.brand400 : Theme.white
                    opacity: 0.9

                    Rectangle {
                        visible: label.text !== ""
                        anchors.top: parent.bottom
                        anchors.topMargin: 3
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: label.implicitWidth + 10
                        height: label.implicitHeight + 4
                        radius: Theme.radiusSm
                        color: Theme.imageLabel
                        Text {
                            id: label
                            anchors.centerIn: parent
                            text: box.modelData.personName || ""
                            font.pixelSize: Theme.fontXs
                            color: Theme.white
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: faceMenu.openFor(box.modelData, box)
                    }
                }
            }
        }

        // Disco desconectado: a miniatura do cache, com aviso.
        Rectangle {
            visible: page.d.offline === true
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 16
            width: offlineRow.implicitWidth + 24
            height: 34
            radius: Theme.radiusLg
            color: Theme.alpha(Theme.black, 0.75)
            RowLayout {
                id: offlineRow
                anchors.centerIn: parent
                spacing: 8
                Icon {
                    name: "triangle-alert"
                    size: 15
                    color: Theme.amber400
                }
                Text {
                    text: qsTr("The original is offline (%1): showing the thumbnail.").arg(page.d.diskLabel || "")
                    font.pixelSize: Theme.fontSm
                    color: Theme.white
                }
            }
        }
    }

    Splitter {
        x: panel.x - width / 2
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        container: page
        mode: "pixelsFromEnd"
        value: panel.width
        from: 300
        to: Math.max(320, page.width * 0.5)
        stepSize: 16
        defaultValue: 380
        label: qsTr("Info panel width")
        onMoved: (v) => page.panelWidth = v
    }

    // ------------------------------------------------------------------ //
    // Painel de informações
    // ------------------------------------------------------------------ //
    Rectangle {
        id: panel
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom
        width: Math.max(300, Math.min(page.panelWidth, page.width * 0.5))
        color: Theme.card

        ScrollPage {
            anchors.fill: parent
            padding: 16
            spacing: 18

            // Resumo
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                SectionTitle { text: qsTr("Photo") }
                Repeater {
                    model: page.summary
                    InfoRow {
                        required property var modelData
                        label: modelData.label
                        value: modelData.value
                    }
                }
            }

            // Arquivo e disco
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                SectionTitle { text: qsTr("File") }
                InfoRow { label: qsTr("Path"); value: page.d.path || ""; selectable: true }
                InfoRow {
                    label: qsTr("Disk")
                    value: (page.d.diskLabel || "") + " · " + Messages.diskKind(page.d.diskKind)
                           + (page.d.diskServer ? " (" + page.d.diskProtocol + "://" + page.d.diskServer + "/" + page.d.diskShare + ")" : "")
                           + (page.d.offline ? " · " + qsTr("offline") : "")
                }
                InfoRow { label: qsTr("Disk ID"); value: page.d.diskKey || ""; selectable: true }
                InfoRow { label: qsTr("File created"); value: App.formatDateTime(page.d.fileCreatedAt || "") }
                InfoRow { label: qsTr("File modified"); value: App.formatDateTime(page.d.fileModifiedAt || "") }
                InfoRow { label: qsTr("Pixel hash"); value: page.d.pictureHash || ""; selectable: true; mono: true }
                InfoRow { label: qsTr("Similarity hash"); value: page.d.phash || ""; selectable: true; mono: true }
                InfoRow { label: qsTr("Sharpness"); value: page.d.sharpness ? Math.round(page.d.sharpness) : "" }
                // Privada: o arquivo original virou .photoidxe; os dois SHA-256.
                InfoRow { label: qsTr("Original name"); value: page.d.isPrivate ? (page.d.originalName || "") : "" }
                InfoRow { label: qsTr("SHA-256 (original)"); value: page.d.isPrivate ? (page.d.fileSha256 || "") : ""; selectable: true; mono: true }
                InfoRow { label: qsTr("SHA-256 (encrypted)"); value: page.d.isPrivate ? (page.d.encryptedSha256 || "") : ""; selectable: true; mono: true }
            }

            // Lugar
            ColumnLayout {
                Layout.fillWidth: true
                visible: page.d.latitude !== undefined && page.d.latitude !== null
                spacing: 6
                SectionTitle { text: qsTr("Place") }
                InfoRow {
                    label: qsTr("Location")
                    value: [page.d.city, page.d.placeState, page.d.country].filter((x) => x).join(", ")
                }
                InfoRow {
                    label: qsTr("GPS")
                    value: page.d.latitude !== undefined && page.d.latitude !== null
                        ? Number(page.d.latitude).toFixed(6) + ", " + Number(page.d.longitude).toFixed(6)
                          + (page.d.altitude ? " · " + Math.round(page.d.altitude) + " m" : "")
                        : ""
                    selectable: true
                }
            }

            // Categorias
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle { Layout.fillWidth: true; text: qsTr("Categories") }
                    IconButton {
                        id: addCategory
                        diameter: 26
                        iconSize: 14
                        iconName: "plus"
                        tooltip: qsTr("Add to a category")
                        onClicked: categoryMenu.popup(addCategory, 0, addCategory.height)
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: page.d.categories || []
                        Chip {
                            required property var modelData
                            text: modelData.name
                            hint: Messages.categorySource(modelData.source)
                                  + (modelData.source === "ai" ? " · " + Math.round(modelData.score * 100) + "%" : "")
                            onRemoveClicked: App.viewer.removeCategory(modelData.id)
                            onClicked: {
                                App.library.tab = "categories";
                                App.library.selectNode("cat:" + modelData.id);
                                App.page = "library";
                            }
                        }
                    }
                }
            }

            // Álbuns
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle { Layout.fillWidth: true; text: qsTr("Albums") }
                    IconButton {
                        id: addAlbum
                        diameter: 26
                        iconSize: 14
                        iconName: "plus"
                        tooltip: qsTr("Add to an album")
                        onClicked: albumMenu.popup(addAlbum, 0, addAlbum.height)
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: page.d.albums || []
                        Chip {
                            required property var modelData
                            text: modelData.name
                            onRemoveClicked: App.viewer.removeAlbum(modelData.id)
                            onClicked: {
                                App.library.tab = "albums";
                                App.library.selectNode("album:" + modelData.id);
                                App.page = "library";
                            }
                        }
                    }
                }
                Text {
                    visible: (page.d.albums || []).length === 0
                    text: qsTr("Not in any album.")
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                }
            }

            // Pessoas: sempre à vista — com as faces, ou dizendo por que não
            // há (na fila da etapa de faces, nenhum rosto, erro).
            ColumnLayout {
                id: peopleSection
                Layout.fillWidth: true
                visible: !page.d.isPrivate
                spacing: 8
                /// "Procurar rostos agora" pedido para esta foto.
                property string requestedFor: ""
                readonly property bool queued: page.d.facesState === "pending" && (page.d.faces || []).length === 0
                readonly property bool searching: queued && requestedFor === page.d.id
                SectionTitle { text: qsTr("People in this photo") }
                RowLayout {
                    Layout.fillWidth: true
                    visible: (page.d.faces || []).length === 0
                    spacing: 8
                    Spinner {
                        visible: peopleSection.searching
                        size: 14
                        color: Theme.mutedForeground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: peopleSection.searching ? qsTr("Looking for faces…")
                            : peopleSection.queued ? qsTr("Waiting in the faces queue.")
                            : page.d.facesState === "error" ? qsTr("Faces could not be read in this photo.")
                            : qsTr("No faces found in this photo.")
                        font.pixelSize: Theme.fontSm
                        color: Theme.mutedForeground
                        wrapMode: Text.Wrap
                    }
                    Button {
                        visible: peopleSection.queued && !peopleSection.searching && App.people.facesAvailable
                        size: "sm"
                        variant: "outline"
                        iconName: "scan-face"
                        text: qsTr("Find faces now")
                        onClicked: {
                            peopleSection.requestedFor = page.d.id;
                            App.library.findFacesNow([page.d.id]);
                        }
                    }
                }
                Repeater {
                    model: page.d.faces || []
                    T.ItemDelegate {
                        id: faceRow
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 44
                        hoverEnabled: true
                        onClicked: faceMenu.openFor(modelData, faceRow)
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: faceRow.hovered ? Theme.muted : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 18
                                clip: true
                                color: Theme.muted
                                Image {
                                    anchors.fill: parent
                                    source: App.fileUrl(faceRow.modelData.cropPath)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: faceRow.modelData.personName || (faceRow.modelData.personId ? App.people.labelOf(faceRow.modelData.personId)
                                                                                                   : qsTr("Unknown person"))
                                font.pixelSize: Theme.fontBase
                                font.italic: !faceRow.modelData.personName
                                color: Theme.foreground
                                elide: Text.ElideRight
                            }
                            Icon {
                                name: "chevron-right"
                                size: 14
                                color: Theme.mutedForeground
                            }
                        }
                    }
                }
            }

            // Iguais e parecidas (árvore de hashes)
            ColumnLayout {
                Layout.fillWidth: true
                visible: App.viewer.similar.length > 0
                spacing: 8
                SectionTitle { text: qsTr("Same or similar photos") }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: App.viewer.similar
                        Rectangle {
                            id: twin
                            required property var modelData
                            width: 76
                            height: 76
                            radius: Theme.radiusSm
                            color: Theme.muted
                            border.width: modelData.identical ? 2 : 0
                            border.color: Theme.brand400
                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                source: App.fileUrl(twin.modelData.thumbPath)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }
                            MouseArea {
                                id: twinMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.viewer.open(twin.modelData.id)
                            }
                            Tip {
                                text: (twin.modelData.identical ? qsTr("Same pixels") : qsTr("Similar (%1 bits apart)").arg(twin.modelData.distance))
                                      + "\n" + twin.modelData.path
                                visible: twinMouse.containsMouse
                            }
                        }
                    }
                }
            }

            // EXIF completo
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle {
                        Layout.fillWidth: true
                        text: qsTr("EXIF (%1 tags)").arg((page.d.exif || []).length)
                    }
                }
                TextField {
                    Layout.fillWidth: true
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSm
                    iconName: "search"
                    placeholderText: qsTr("Filter tags")
                    visible: (page.d.exif || []).length > 8
                    onTextChanged: page.exifFilter = text.toLowerCase()
                }
                Repeater {
                    model: (page.d.exif || []).filter((t) => page.exifFilter === ""
                        || (t.name + " " + t.value + " " + t.ifd).toLowerCase().indexOf(page.exifFilter) >= 0)
                    RowLayout {
                        id: tagRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        Text {
                            Layout.preferredWidth: 150
                            Layout.alignment: Qt.AlignTop
                            text: tagRow.modelData.name
                            font.pixelSize: Theme.fontXs
                            color: Theme.mutedForeground
                            elide: Text.ElideRight
                            MouseArea {
                                id: tagMouse
                                anchors.fill: parent
                                hoverEnabled: true
                            }
                            Tip {
                                text: tagRow.modelData.ifd + " · " + tagRow.modelData.tag
                                visible: tagMouse.containsMouse
                            }
                        }
                        TextEdit {
                            Layout.fillWidth: true
                            text: tagRow.modelData.value
                            readOnly: true
                            selectByMouse: true
                            wrapMode: TextEdit.WrapAnywhere
                            font.pixelSize: Theme.fontXs
                            color: Theme.foreground
                            selectionColor: Theme.alpha(Theme.brand400, 0.45)
                        }
                    }
                }
                Text {
                    visible: (page.d.exif || []).length === 0
                    text: qsTr("This file has no EXIF.")
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            width: 1
            height: parent.height
            color: Theme.border
        }
    }

    ChoiceMenu {
        id: categoryMenu
        choices: App.library.categoryChoices
        onPicked: (id) => App.viewer.addCategory(id)
    }
    ChoiceMenu {
        id: albumMenu
        choices: App.library.albumChoices
        emptyText: qsTr("No albums yet")
        onPicked: (id) => App.viewer.addAlbum(id)
    }

    // Menu de uma face: quem é, não é, pessoa nova, buscar.
    AppMenu {
        id: faceMenu

        property var face: ({})

        function openFor(face, item) {
            faceMenu.face = face;
            popup(item, 0, item.height);
        }

        AppMenuItem {
            enabled: !!faceMenu.face.personId
            text: qsTr("Open this person")
            onTriggered: {
                App.people.openPerson(faceMenu.face.personId);
                App.page = "people";
            }
        }
        AppMenuItem {
            text: qsTr("Find this person in other photos")
            onTriggered: {
                App.people.searchByFace(faceMenu.face.id);
                App.page = "faceSearch";
            }
        }
        AppMenuSeparator {}
        ChoiceMenu {
            title: qsTr("This is…")
            choices: App.library.personChoices
            emptyText: qsTr("No people yet")
            onPicked: (id) => App.viewer.assignFace(faceMenu.face.id, id)
        }
        AppMenuItem {
            text: qsTr("New person…")
            onTriggered: page.names.ask({ title: qsTr("New person"), label: qsTr("Name"), confirmLabel: qsTr("Create") },
                                        (name) => App.viewer.newPersonFromFace(faceMenu.face.id, name))
        }
        AppMenuItem {
            enabled: !!faceMenu.face.personId
            text: qsTr("This is not %1").arg(faceMenu.face.personName || qsTr("this person"))
            onTriggered: App.viewer.rejectFace(faceMenu.face.id)
        }
    }

    // Linha "rótulo: valor" do painel.
    component InfoRow: RowLayout {
        id: info
        property string label: ""
        property string value: ""
        property bool selectable: false
        property bool mono: false
        visible: value !== ""
        Layout.fillWidth: true
        spacing: 8
        Text {
            Layout.preferredWidth: 110
            Layout.alignment: Qt.AlignTop
            text: info.label
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
        }
        TextEdit {
            Layout.fillWidth: true
            text: info.value
            readOnly: true
            selectByMouse: info.selectable
            wrapMode: TextEdit.WrapAnywhere
            font.pixelSize: Theme.fontSm
            font.family: info.mono ? "monospace" : Theme.fontFamily
            color: Theme.foreground
            selectionColor: Theme.alpha(Theme.brand400, 0.45)
        }
    }

    // Etiqueta clicável com X para tirar.
    component Chip: T.AbstractButton {
        id: chip
        property string hint: ""
        signal removeClicked()
        implicitHeight: 26
        implicitWidth: chipRow.implicitWidth + 16
        hoverEnabled: true
        background: Rectangle {
            radius: 13
            color: chip.hovered ? Theme.accent : Theme.muted
            border.color: Theme.border
        }
        contentItem: RowLayout {
            id: chipRow
            spacing: 4
            Text {
                Layout.leftMargin: 10
                text: chip.text
                font.pixelSize: Theme.fontSm
                color: Theme.foreground
            }
            IconButton {
                Layout.rightMargin: 2
                diameter: 20
                iconSize: 12
                iconName: "x"
                tooltip: qsTr("Remove")
                onClicked: chip.removeClicked()
            }
        }
        Tip {
            text: chip.hint
            visible: chip.hovered && chip.hint !== ""
        }
    }
}

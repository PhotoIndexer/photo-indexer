pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Catálogo de pessoas (App.people). Cada pessoa tem um UUID e nasce sozinha
// quando um rosto não casa com ninguém; aqui a pessoa dá nome, junta pessoas,
// tira faces que não são dela (falso positivo) e põe as que são (falso
// negativo). A busca por foto acha uma pessoa a partir de uma imagem do disco.
Item {
    id: page

    required property var names

    signal faceSearchRequested()

    property string nameFilter: ""
    /// Faces marcadas na pessoa aberta (ids).
    property var picked: []

    readonly property bool detailOpen: Object.keys(App.people.person).length > 0
    readonly property bool unassignedOpen: detailOpen && !App.people.person.id

    function togglePick(id) {
        const i = picked.indexOf(id);
        picked = i >= 0 ? picked.filter((x) => x !== id) : picked.concat([id]);
    }

    function rename(personId, current) {
        names.ask({ title: qsTr("Name this person"), label: qsTr("Name"), text: current, confirmLabel: qsTr("Save") }, (name) => {
            const twin = App.people.personNamed(name, personId);
            if (twin === "") {
                App.people.rename(personId, name);
                return;
            }
            // Já há alguém com esse nome: provavelmente a mesma pessoa.
            Dialogs.choose({
                title: qsTr("“%1” already exists").arg(name),
                description: qsTr("Another person is already called <b>%1</b>. Are they the same person?").arg(Messages.escapeHtml(name)),
                variant: "info",
                buttons: [{ id: "merge", label: qsTr("Yes, merge them"), variant: "default", primary: true },
                          { id: "rename", label: qsTr("No, just rename"), variant: "outline" },
                          { id: "cancel", label: qsTr("Cancel"), variant: "ghost" }]
            }, (id) => {
                if (id === "merge") {
                    App.people.rename(personId, name);
                    App.people.merge(twin, personId);
                } else if (id === "rename") {
                    App.people.rename(personId, name);
                }
            });
        });
    }

    Connections {
        target: App.people
        function onPersonChanged() {
            page.picked = page.picked.filter((id) => App.people.faces.some((f) => f.id === id));
        }
    }

    // ------------------------------------------------------------------ //
    // Lista
    // ------------------------------------------------------------------ //
    ColumnLayout {
        anchors.fill: parent
        visible: !page.detailOpen
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 56
            color: Theme.background
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 16
                spacing: 10
                ColumnLayout {
                    spacing: 0
                    Text {
                        text: qsTr("People")
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                    }
                    Text {
                        text: qsTr("%n person(s)", "", App.people.people.length)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }
                Item { Layout.fillWidth: true }
                TextField {
                    Layout.preferredWidth: 240
                    implicitHeight: 32
                    iconName: "search"
                    placeholderText: qsTr("Filter by name")
                    font.pixelSize: Theme.fontSm
                    onTextChanged: page.nameFilter = text.toLowerCase()
                }
                Button {
                    size: "sm"
                    implicitHeight: 32
                    variant: "outline"
                    iconName: "user-x"
                    visible: App.people.unassignedCount > 0
                    text: qsTr("Faces without a person (%1)").arg(App.people.unassignedCount)
                    onClicked: App.people.openPerson("")
                }
                Button {
                    size: "sm"
                    implicitHeight: 32
                    iconName: "scan-face"
                    text: qsTr("Search by photo…")
                    disabled: !App.people.facesAvailable
                    onClicked: page.faceSearchRequested()
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        GridView {
            id: peopleGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property int columns: Math.max(1, Math.floor((width - 24) / 168))
            leftMargin: 16
            topMargin: 16
            cellWidth: Math.floor((width - 32) / columns)
            cellHeight: 200
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}
            model: App.people.people.filter((p) => page.nameFilter === "" || p.label.toLowerCase().indexOf(page.nameFilter) >= 0)

            delegate: T.AbstractButton {
                id: card
                required property var modelData
                width: peopleGrid.cellWidth
                height: peopleGrid.cellHeight
                hoverEnabled: true
                onClicked: App.people.openPerson(modelData.id)

                background: Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: Theme.radiusXl
                    color: card.hovered ? Theme.muted : "transparent"
                }
                contentItem: ColumnLayout {
                    spacing: 6
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 14
                        Layout.preferredWidth: 112
                        Layout.preferredHeight: 112
                        radius: 56
                        clip: true
                        color: Theme.muted
                        border.width: card.hovered ? 2 : 0
                        border.color: Theme.brand400
                        Image {
                            anchors.fill: parent
                            source: card.modelData.coverUrl
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            sourceSize: Qt.size(224, 224)
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 10
                        Layout.rightMargin: 10
                        horizontalAlignment: Text.AlignHCenter
                        text: card.modelData.label
                        font.pixelSize: Theme.fontBase
                        font.weight: Font.Medium
                        font.italic: card.modelData.name === ""
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("%n photo(s)", "", card.modelData.photoCount)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }

        // Vazio
        ColumnLayout {
            Layout.alignment: Qt.AlignCenter
            Layout.bottomMargin: 120
            visible: App.people.people.length === 0
            spacing: 12
            Icon {
                Layout.alignment: Qt.AlignHCenter
                name: "users"
                size: 44
                color: Theme.mutedForeground
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: App.people.facesAvailable ? qsTr("No people yet") : qsTr("Face recognition is not installed")
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
                color: Theme.foreground
            }
            Text {
                Layout.preferredWidth: 460
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("As the Faces stage goes through your photos, each face is grouped with the most similar one. People appear here, ready to be named.")
                font.pixelSize: Theme.fontBase
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }
    }

    // ------------------------------------------------------------------ //
    // Pessoa aberta
    // ------------------------------------------------------------------ //
    ColumnLayout {
        anchors.fill: parent
        visible: page.detailOpen
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 72
            color: Theme.background
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 16
                spacing: 12
                IconButton {
                    iconName: "arrow-left"
                    tooltip: qsTr("All people")
                    onClicked: {
                        page.picked = [];
                        App.people.closePerson();
                    }
                }
                Rectangle {
                    visible: !page.unassignedOpen
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    clip: true
                    color: Theme.muted
                    Image {
                        anchors.fill: parent
                        source: App.people.person.coverUrl || ""
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: App.people.person.label || ""
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                        font.italic: !page.unassignedOpen && !App.people.person.name
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        text: page.unassignedOpen ? qsTr("Faces you removed from someone. Put them on the right person.")
                            : qsTr("%n photo(s)", "", App.people.person.photoCount || 0) + " · " + qsTr("%n face(s)", "", App.people.person.faceCount || 0)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }
                Button {
                    visible: !page.unassignedOpen
                    size: "sm"
                    variant: "outline"
                    iconName: "pencil"
                    text: App.people.person.name ? qsTr("Rename") : qsTr("Name")
                    onClicked: page.rename(App.people.person.id, App.people.person.name || "")
                }
                Button {
                    id: mergeButton
                    visible: !page.unassignedOpen
                    size: "sm"
                    variant: "outline"
                    iconName: "merge"
                    text: qsTr("Merge with…")
                    onClicked: mergeMenu.popup(mergeButton, 0, mergeButton.height + 4)
                }
                Button {
                    visible: !page.unassignedOpen
                    size: "sm"
                    variant: "outline"
                    iconName: "images"
                    text: qsTr("Photos")
                    onClicked: {
                        App.library.tab = "people";
                        App.library.selectNode("person:" + App.people.person.id);
                        App.page = "library";
                    }
                }
                IconButton {
                    visible: !page.unassignedOpen
                    iconName: "trash-2"
                    tooltip: qsTr("Delete person")
                    onClicked: Dialogs.confirm({
                        title: qsTr("Delete person?"),
                        description: qsTr("<b>%1</b> stops existing; the faces become unknown and the photos stay in the catalog.")
                            .arg(Messages.escapeHtml(App.people.person.label || "")),
                        variant: "danger",
                        confirmLabel: qsTr("Delete")
                    }, () => App.people.remove(App.people.person.id))
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        // Barra das faces: ordem e ações na seleção.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 16
            Layout.topMargin: 10
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: page.picked.length > 0 ? qsTr("%n face(s) selected", "", page.picked.length)
                                             : qsTr("Click faces to select them.")
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
            }
            Toggle {
                visible: !page.unassignedOpen
                label: qsTr("Least similar first")
                checked: App.people.leastSimilarFirst
                onToggled: (value) => App.people.leastSimilarFirst = value
            }
            Button {
                visible: page.picked.length > 0 && !page.unassignedOpen
                size: "sm"
                variant: "destructive"
                iconName: "user-x"
                text: qsTr("Not this person")
                onClicked: {
                    App.people.rejectFaces(page.picked);
                    page.picked = [];
                }
            }
            Button {
                id: moveButton
                visible: page.picked.length > 0
                size: "sm"
                variant: "outline"
                iconName: "user-check"
                text: qsTr("This is…")
                onClicked: moveMenu.popup(moveButton, 0, moveButton.height + 4)
            }
            Button {
                visible: page.picked.length > 0
                size: "sm"
                variant: "outline"
                iconName: "user-plus"
                text: qsTr("New person…")
                onClicked: page.names.ask({ title: qsTr("New person"), label: qsTr("Name"), confirmLabel: qsTr("Create") }, (name) => {
                    App.people.createPerson(name, page.picked);
                    page.picked = [];
                })
            }
        }

        GridView {
            id: faceGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property int columns: Math.max(1, Math.floor((width - 24) / 136))
            leftMargin: 16
            topMargin: 10
            cellWidth: Math.floor((width - 32) / columns)
            cellHeight: cellWidth + 26
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}
            model: App.people.faces

            delegate: Item {
                id: faceCell
                required property var modelData
                readonly property bool marked: page.picked.indexOf(modelData.id) >= 0
                width: faceGrid.cellWidth
                height: faceGrid.cellHeight

                Rectangle {
                    id: faceFrame
                    x: 6
                    y: 6
                    width: parent.width - 12
                    height: width
                    radius: Theme.radiusLg
                    color: Theme.muted
                    clip: true
                    border.width: faceCell.marked ? 3 : 0
                    border.color: Theme.brand400
                    Image {
                        anchors.fill: parent
                        anchors.margins: faceCell.marked ? 3 : 0
                        source: faceCell.modelData.cropUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                    SelectionCheck {
                        visible: page.picked.length > 0
                        x: 6
                        y: 6
                        checked: faceCell.marked
                        color: checked ? Theme.primary : Theme.alpha(Theme.black, 0.5)
                        border.color: checked ? Theme.primary : Theme.white
                    }
                    Badge {
                        visible: faceCell.modelData.assignment === "user"
                        x: parent.width - width - 6
                        y: 6
                        text: qsTr("Confirmed")
                        variant: "dark"
                    }
                }
                Text {
                    anchors.top: faceFrame.bottom
                    anchors.topMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !page.unassignedOpen
                    text: qsTr("similarity %1%").arg(Math.round(faceCell.modelData.similarity * 100))
                    font.pixelSize: Theme.fontXs
                    color: faceCell.modelData.similarity < 0.45 ? Theme.warning : Theme.mutedForeground
                }
                MouseArea {
                    id: faceMouse
                    anchors.fill: faceFrame
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    onClicked: page.togglePick(faceCell.modelData.id)
                    onDoubleClicked: App.openPhoto(faceCell.modelData.photoId)
                }
                Tip {
                    text: faceCell.modelData.photoName + "\n" + qsTr("Double-click to open the photo")
                    visible: faceMouse.containsMouse
                }
            }
        }
    }

    ChoiceMenu {
        id: mergeMenu
        choices: App.library.personChoices.filter((p) => p.id !== App.people.person.id)
        emptyText: qsTr("No other people")
        onPicked: (id) => Dialogs.confirm({
            title: qsTr("Merge people?"),
            description: qsTr("All faces of <b>%1</b> go to <b>%2</b>.")
                .arg(Messages.escapeHtml(App.people.person.label || "")).arg(Messages.escapeHtml(App.people.labelOf(id))),
            variant: "warning",
            confirmLabel: qsTr("Merge")
        }, () => App.people.merge(id, App.people.person.id))
    }

    ChoiceMenu {
        id: moveMenu
        choices: App.library.personChoices.filter((p) => p.id !== App.people.person.id)
        emptyText: qsTr("No other people")
        onPicked: (id) => {
            App.people.assignFaces(page.picked, id);
            page.picked = [];
        }
    }
}

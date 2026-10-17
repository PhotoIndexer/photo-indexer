pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// A pessoa aberta (App.people.person): dar nome, juntar com outra, tirar as
// faces que não são dela, pôr as faces em outra pessoa. Usada na tela
// Pessoas e na busca por rosto (ao lado da lista de quem pode ser, para ir
// juntando e corrigindo sem perder a busca).
Item {
    id: detail

    required property var names
    property string backTooltip: qsTr("All people")

    /// A seta de voltar: quem usa decide para onde.
    signal back()

    /// Faces marcadas na pessoa aberta (ids).
    property var picked: []

    readonly property bool unassignedOpen: Object.keys(App.people.person).length > 0 && !App.people.person.id

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
            detail.picked = detail.picked.filter((id) => App.people.faces.some((f) => f.id === id));
        }
    }

    ColumnLayout {
        anchors.fill: parent
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
                    tooltip: detail.backTooltip
                    onClicked: {
                        detail.picked = [];
                        detail.back();
                    }
                }
                Rectangle {
                    visible: !detail.unassignedOpen
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
                        font.italic: !detail.unassignedOpen && !App.people.person.name
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        text: detail.unassignedOpen ? qsTr("Faces you removed from someone. Put them on the right person.")
                            : qsTr("%n photo(s)", "", App.people.person.photoCount || 0) + " · " + qsTr("%n face(s)", "", App.people.person.faceCount || 0)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }
                Button {
                    visible: !detail.unassignedOpen
                    size: "sm"
                    variant: "outline"
                    iconName: "pencil"
                    text: App.people.person.name ? qsTr("Rename") : qsTr("Name")
                    onClicked: detail.rename(App.people.person.id, App.people.person.name || "")
                }
                Button {
                    id: mergeButton
                    visible: !detail.unassignedOpen
                    size: "sm"
                    variant: "outline"
                    iconName: "merge"
                    text: qsTr("Merge with…")
                    onClicked: mergeMenu.popup(mergeButton, 0, mergeButton.height + 4)
                }
                Button {
                    visible: !detail.unassignedOpen
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
                    visible: !detail.unassignedOpen
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
                text: detail.picked.length > 0 ? qsTr("%n face(s) selected", "", detail.picked.length)
                                             : qsTr("Click faces to select them.")
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
            }
            Toggle {
                visible: !detail.unassignedOpen
                label: qsTr("Least similar first")
                checked: App.people.leastSimilarFirst
                onToggled: (value) => App.people.leastSimilarFirst = value
            }
            Button {
                visible: detail.picked.length > 0 && !detail.unassignedOpen
                size: "sm"
                variant: "destructive"
                iconName: "user-x"
                text: qsTr("Not this person")
                onClicked: {
                    App.people.rejectFaces(detail.picked);
                    detail.picked = [];
                }
            }
            Button {
                id: moveButton
                visible: detail.picked.length > 0
                size: "sm"
                variant: "outline"
                iconName: "user-check"
                text: qsTr("This is…")
                onClicked: moveMenu.popup(moveButton, 0, moveButton.height + 4)
            }
            Button {
                visible: detail.picked.length > 0
                size: "sm"
                variant: "outline"
                iconName: "user-plus"
                text: qsTr("New person…")
                onClicked: detail.names.ask({ title: qsTr("New person"), label: qsTr("Name"), confirmLabel: qsTr("Create") }, (name) => {
                    App.people.createPerson(name, detail.picked);
                    detail.picked = [];
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
            reuseItems: true
            ScrollWatch {
                id: faceScroll
                target: faceGrid
            }

            delegate: Item {
                id: faceCell
                required property var modelData
                readonly property bool marked: detail.picked.indexOf(modelData.id) >= 0
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
                        property string loadedUrl: ""
                        anchors.fill: parent
                        anchors.margins: faceCell.marked ? 3 : 0
                        source: faceScroll.fast && loadedUrl !== faceCell.modelData.cropUrl ? "" : faceCell.modelData.cropUrl
                        onStatusChanged: if (status === Image.Ready) loadedUrl = String(source)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        sourceSize: Qt.size(256, 256)
                    }
                    SelectionCheck {
                        visible: detail.picked.length > 0
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
                    visible: !detail.unassignedOpen
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
                    onClicked: detail.togglePick(faceCell.modelData.id)
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
            App.people.assignFaces(detail.picked, id);
            detail.picked = [];
        }
    }
}

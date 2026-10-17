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

    readonly property bool detailOpen: Object.keys(App.people.person).length > 0

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
                        text: qsTr("%n person(s)", "", App.people.list.count)
                            + " · " + qsTr("with a name, or with %1+ photos. The others appear in “Search by photo”.")
                                .arg(App.tasks.settings.minPersonPhotos || 20)
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
                    onTextChanged: App.people.list.filter = text
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
            // Modelo C++ (filtro por nome em C++): milhares de pessoas sem
            // passar por listas em JavaScript.
            model: App.people.list
            reuseItems: true
            // Rolando rápido, as fotos que só passam não são lidas.
            ScrollWatch {
                id: peopleScroll
                target: peopleGrid
            }

            delegate: T.AbstractButton {
                id: card
                required property var person
                readonly property var modelData: person
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
                            property string loadedUrl: ""
                            anchors.fill: parent
                            source: peopleScroll.fast && loadedUrl !== card.modelData.coverUrl ? "" : card.modelData.coverUrl
                            onStatusChanged: if (status === Image.Ready) loadedUrl = String(source)
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
            visible: App.people.list.count === 0 && App.people.list.filter === ""
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
    PersonDetail {
        anchors.fill: parent
        visible: page.detailOpen
        names: page.names
        onBack: App.people.closePerson()
    }
}

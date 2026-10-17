pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// "Procurar cópias em outros lugares": as cópias agrupadas por pasta — o
// caminho em cima e, embaixo, as fotos dela (App.library.copyGroups). A pasta
// da foto escolhida vem primeiro. Poucas fotos (as cópias de uma): Repeater.
ListView {
    id: view

    /// Duplo clique: abrir a foto.
    signal opened(string photoId)
    /// Botão direito numa foto.
    signal contextRequested(string photoId, Item item)

    readonly property int thumb: Math.max(120, App.library.thumbSize)

    clip: true
    spacing: 18
    topMargin: 14
    bottomMargin: 24
    boundsBehavior: Flickable.StopAtBounds
    T.ScrollBar.vertical: ThinScrollBar {}
    model: App.library.copyGroups

    delegate: ColumnLayout {
        id: group
        required property var modelData
        required property int index
        width: ListView.view.width
        spacing: 8

        // A pasta.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8
            Icon {
                name: group.index === 0 ? "folder-open" : "folder"
                size: 16
                color: group.index === 0 ? Theme.brand400 : Theme.mutedForeground
            }
            TextEdit {
                Layout.fillWidth: true
                text: group.modelData.folder
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                font.pixelSize: Theme.fontSm
                font.weight: Font.Medium
                color: Theme.foreground
                selectionColor: Theme.brandSoft
            }
            Text {
                text: qsTr("%n photo(s)", "", group.modelData.photos.length)
                font.pixelSize: Theme.fontXs
                color: Theme.mutedForeground
            }
            IconButton {
                diameter: 28
                iconSize: 14
                iconName: "folder-search"
                tooltip: qsTr("Show in file manager")
                onClicked: App.revealInFolder(group.modelData.folder)
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            implicitHeight: 1
            color: Theme.border
        }

        // As fotos dela.
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 8
            Repeater {
                model: group.modelData.photos
                Item {
                    id: cell
                    required property var modelData
                    width: view.thumb
                    height: view.thumb + 34

                    Rectangle {
                        id: frame
                        width: parent.width
                        height: view.thumb
                        radius: Theme.radiusMd
                        color: Theme.dark ? Theme.card : Theme.muted
                        border.width: cell.modelData.original ? 2 : 0
                        border.color: Theme.brand400
                        Image {
                            anchors.fill: parent
                            anchors.margins: 4
                            source: cell.modelData.thumbUrl
                            sourceSize: Qt.size(view.thumb * Screen.devicePixelRatio, view.thumb * Screen.devicePixelRatio)
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                        }
                        Badge {
                            visible: cell.modelData.original
                            x: 8
                            y: 8
                            text: qsTr("Original")
                            variant: "dark"
                        }
                        Badge {
                            visible: !cell.modelData.original
                            x: parent.width - width - 8
                            y: parent.height - height - 8
                            text: Math.round(cell.modelData.score * 100) + "%"
                            variant: "brand"
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            cursorShape: Qt.PointingHandCursor
                            onClicked: (event) => {
                                if (event.button === Qt.RightButton)
                                    view.contextRequested(cell.modelData.id, frame);
                            }
                            onDoubleClicked: view.opened(cell.modelData.id)
                        }
                    }
                    Text {
                        anchors.top: frame.bottom
                        anchors.topMargin: 3
                        width: parent.width
                        text: cell.modelData.name
                        font.pixelSize: Theme.fontXs
                        color: Theme.foreground
                        elide: Text.ElideMiddle
                    }
                    Text {
                        anchors.top: frame.bottom
                        anchors.topMargin: 17
                        width: parent.width
                        text: cell.modelData.sizeText
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                    }
                }
            }
        }
    }
}

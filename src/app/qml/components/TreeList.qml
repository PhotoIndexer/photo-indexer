pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// A árvore de uma aba do painel lateral (App.library.tree): uma linha por nó
// visível, recuada pela profundidade. Clique escolhe o nó (a grade mostra as
// fotos dele); a seta abre/fecha; o botão direito pede o menu do nó. Fotos
// arrastadas da grade (PhotoGrid) podem cair numa categoria ou num álbum.
ListView {
    id: list

    /// Chave do nó escolhido (destacado).
    property string current: ""

    signal picked(string key)
    signal contextRequested(string key, string kind, Item row)
    /// Fotos da grade soltas neste nó (categoria ou álbum).
    signal photosDropped(string key)

    clip: true
    model: App.library.tree
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: true
    T.ScrollBar.vertical: ThinScrollBar {}

    delegate: T.ItemDelegate {
        id: row

        required property string key
        required property string label
        required property int count
        required property int depth
        required property bool hasChildren
        required property bool expanded
        required property string nodeIcon
        required property string detail
        required property string kind

        readonly property bool selected: key === list.current
        readonly property bool offline: kind === "offline"
        readonly property bool dropTarget: (key.startsWith("cat:") || key.startsWith("album:")) && kind !== "private-locked"

        width: ListView.view.width
        implicitHeight: 28
        hoverEnabled: true
        leftPadding: 8 + depth * 14
        rightPadding: 10
        Accessible.name: label

        onClicked: list.picked(row.key)
        onDoubleClicked: if (row.hasChildren) App.library.tree.toggle(row.key)

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: list.contextRequested(row.key, row.kind, row)
        }

        background: Rectangle {
            radius: Theme.radiusMd
            x: 4
            width: parent.width - 8
            color: drop.containsDrag ? Theme.brandSoft : row.selected ? Theme.brandSoft : (row.hovered ? Theme.muted : "transparent")
            border.width: drop.containsDrag ? 1 : 0
            border.color: Theme.brand400
        }

        DropArea {
            id: drop
            anchors.fill: parent
            enabled: row.dropTarget
            keys: ["photoidx/photos"]
            onDropped: (event) => {
                event.accept(Qt.MoveAction);
                list.photosDropped(row.key);
            }
            // Parado em cima de um nó fechado: abre (para chegar nas de dentro).
            Timer {
                interval: 700
                running: drop.containsDrag && row.hasChildren && !row.expanded
                onTriggered: App.library.tree.toggle(row.key)
            }
        }

        contentItem: Row {
            spacing: 4

            // Seta: abre/fecha (sem filhos, só o espaço).
            Item {
                width: 14
                height: 28
                Icon {
                    anchors.centerIn: parent
                    visible: row.hasChildren
                    name: row.expanded ? "chevron-down" : "chevron-right"
                    size: 12
                    color: Theme.mutedForeground
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: row.hasChildren
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.library.tree.toggle(row.key)
                }
            }
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: row.nodeIcon !== "" ? row.nodeIcon : "folder"
                size: 14
                color: row.selected ? Theme.brandForeground : Theme.mutedForeground
                opacity: row.offline ? 0.5 : 1
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: row.availableWidth - 14 - 14 - 8 - countText.width - 6
                text: row.label
                font.pixelSize: Theme.fontSm
                font.weight: row.selected || row.depth === 0 ? Font.Medium : Font.Normal
                font.italic: row.kind === "special" || row.kind === "unnamed"
                color: row.offline ? Theme.mutedForeground : Theme.foreground
                elide: Text.ElideMiddle
            }
            Text {
                id: countText
                anchors.verticalCenter: parent.verticalCenter
                text: row.offline ? qsTr("offline") : (row.count > 0 ? Number(row.count).toLocaleString(Qt.locale(), "f", 0) : "")
                font.pixelSize: Theme.fontXs
                color: row.offline ? Theme.warning : Theme.mutedForeground
            }
        }

        Tip {
            text: row.detail !== "" ? row.label + "\n" + row.detail : row.label
            visible: row.hovered && (row.detail !== "" || row.label.length > 28)
            x: Math.min(row.width - implicitWidth, 40)
            y: row.height + 2
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// A árvore de uma aba do painel lateral (App.library.tree): uma linha por nó
// visível, recuada pela profundidade. Clique escolhe o nó (a grade mostra as
// fotos dele); a seta abre/fecha; o botão direito pede o menu do nó. Fotos
// arrastadas da grade (PhotoGrid) podem cair numa categoria ou num álbum; uma
// pessoa arrastada cai num grupo de pessoas (ou em "Com nome"/"Sem nome",
// que a tira do grupo).
ListView {
    id: list

    /// Chave do nó escolhido (destacado).
    property string current: ""

    signal picked(string key)
    signal contextRequested(string key, string kind, Item row)
    /// Fotos da grade soltas neste nó (categoria ou álbum).
    signal photosDropped(string key)
    /// Uma pessoa (person:<id>) solta num grupo (pgroup:<id>, people:named/unnamed).
    signal personDropped(string personKey, string key)

    // O que voa com o mouse ao arrastar uma pessoa.
    Item {
        id: personProxy
        property string personKey: ""
        property string label: ""
        parent: T.Overlay.overlay
        width: proxyText.implicitWidth + 24
        height: 26
        z: 1000
        visible: Drag.active
        Drag.keys: ["photoidx/person"]
        Drag.hotSpot.x: 12
        Drag.hotSpot.y: height / 2
        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            color: Theme.card
            border.width: 1
            border.color: Theme.brand400
            Text {
                id: proxyText
                anchors.centerIn: parent
                text: personProxy.label
                font.pixelSize: Theme.fontSm
                color: Theme.foreground
            }
        }
    }

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
        readonly property bool personTarget: kind === "pgroup" || kind === "pbucket"
        readonly property bool isPerson: key.startsWith("person:")

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
            enabled: row.dropTarget || row.personTarget
            keys: row.personTarget ? ["photoidx/person"] : ["photoidx/photos"]
            onDropped: (event) => {
                event.accept(Qt.MoveAction);
                if (row.personTarget)
                    list.personDropped(personProxy.personKey, row.key);
                else
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

        // Pessoa: arrastar leva para um grupo. O clique continua escolhendo
        // o nó (o MouseArea fica por cima só nas linhas de pessoa).
        MouseArea {
            id: personMouse
            anchors.fill: parent
            enabled: row.isPerson
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
            drag.target: personProxy
            drag.threshold: 8
            preventStealing: true
            onPressed: (event) => {
                if (event.button !== Qt.LeftButton)
                    return;
                const p = personMouse.mapToItem(personProxy.parent, event.x, event.y);
                personProxy.x = p.x - 12;
                personProxy.y = p.y - personProxy.height / 2;
                personProxy.personKey = row.key;
                personProxy.label = row.label;
            }
            onClicked: (event) => {
                if (event.button === Qt.RightButton)
                    list.contextRequested(row.key, row.kind, row);
                else
                    list.picked(row.key);
            }
        }
        Connections {
            target: personMouse.drag
            function onActiveChanged() {
                if (personMouse.drag.active) {
                    personProxy.Drag.active = true;
                } else if (personProxy.Drag.active) {
                    personProxy.Drag.drop();
                    personProxy.Drag.active = false;
                }
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

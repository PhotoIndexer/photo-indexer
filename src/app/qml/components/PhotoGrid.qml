pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Grade de miniaturas (um PhotoGridModel). As células enchem a largura toda
// (regra 2): o número de colunas sai do tamanho pedido e a sobra é
// repartida. A miniatura é o arquivo em ~/.photo_indexer/thumbnail, lido em
// segundo plano.
//
//   Clique: seleciona (Ctrl liga/desliga, Shift estende)   Duplo clique/Enter: abre
//   Ctrl + roda do mouse: tamanho das miniaturas            Botão direito: menu
GridView {
    id: grid

    /// PhotoGridModel (App.library.grid, App.people.results).
    property var photos: null
    property int thumbSize: App.library.thumbSize
    /// Mostra a similaridade (busca por IA/rosto) no canto.
    property bool showScore: false

    signal opened(string photoId)
    signal contextRequested(int index, Item cell)

    readonly property int gap: 6
    readonly property int columns: Math.max(1, Math.floor((width - gap) / (thumbSize + gap)))

    model: photos
    clip: true
    focus: true
    cellWidth: Math.floor((width - gap) / columns)
    cellHeight: cellWidth
    leftMargin: gap / 2
    topMargin: gap / 2
    boundsBehavior: Flickable.StopAtBounds
    reuseItems: true
    cacheBuffer: 2 * cellHeight
    keyNavigationEnabled: true
    highlightFollowsCurrentItem: false
    currentIndex: -1
    T.ScrollBar.vertical: ThinScrollBar {}

    Keys.onReturnPressed: if (currentIndex >= 0) grid.opened(photos.idAt(currentIndex))
    Keys.onEnterPressed: if (currentIndex >= 0) grid.opened(photos.idAt(currentIndex))
    Keys.onSpacePressed: if (currentIndex >= 0) photos.click(currentIndex, Qt.ControlModifier)

    // Ctrl + roda: zoom das miniaturas; sem Ctrl, rola como sempre.
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: (event) => App.library.thumbSize = App.library.thumbSize + (event.angleDelta.y > 0 ? 20 : -20)
    }

    delegate: Item {
        id: cell

        required property int index
        required property string photoId
        required property string name
        required property string thumbUrl
        required property bool isRaw
        required property bool selected
        required property double score
        required property string state

        width: grid.cellWidth
        height: grid.cellHeight

        Rectangle {
            id: frame
            anchors.fill: parent
            anchors.margins: grid.gap / 2
            radius: Theme.radiusMd
            color: Theme.dark ? Theme.card : Theme.muted
            border.width: cell.selected ? 2 : (grid.currentIndex === cell.index && grid.activeFocus ? 1 : 0)
            border.color: cell.selected ? Theme.brand400 : Theme.focusRing

            Image {
                id: thumb
                anchors.fill: parent
                anchors.margins: cell.selected ? 6 : 4
                source: cell.thumbUrl
                sourceSize: Qt.size(grid.cellWidth * Screen.devicePixelRatio, grid.cellHeight * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false
                smooth: true
                opacity: status === Image.Ready ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }
            }

            // Sem miniatura (formato sem prévia, ainda indexando).
            Icon {
                anchors.centerIn: parent
                visible: cell.thumbUrl === "" || thumb.status === Image.Error
                name: cell.state === "pending" ? "loader-circle" : "image-off"
                size: 24
                color: Theme.mutedForeground
            }

            SelectionCheck {
                visible: cell.selected
                x: 8
                y: 8
                checked: true
                color: Theme.primary
                border.color: Theme.primary
            }

            Badge {
                visible: cell.isRaw
                x: parent.width - width - 8
                y: 8
                text: "RAW"
                variant: "dark"
            }

            Badge {
                visible: grid.showScore && cell.score > 0
                x: parent.width - width - 8
                y: parent.height - height - 8
                text: Math.round(cell.score * 100) + "%"
                variant: "brand"
            }

            // Nome por cima, embaixo, ao passar o mouse.
            Rectangle {
                visible: mouse.containsMouse && grid.cellWidth >= 120
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 4
                height: nameText.implicitHeight + 8
                radius: Theme.radiusSm
                color: Theme.imageLabel
                Text {
                    id: nameText
                    anchors.fill: parent
                    anchors.margins: 4
                    text: cell.name
                    font.pixelSize: Theme.fontXs
                    color: Theme.white
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: (event) => {
                grid.forceActiveFocus();
                grid.currentIndex = cell.index;
                if (event.button === Qt.RightButton) {
                    if (!cell.selected)
                        grid.photos.click(cell.index, 0);
                    grid.contextRequested(cell.index, cell);
                    return;
                }
                grid.photos.click(cell.index, event.modifiers);
            }
            onDoubleClicked: (event) => {
                if (event.button === Qt.LeftButton)
                    grid.opened(cell.photoId);
            }
        }

        Tip {
            text: cell.name
            visible: mouse.containsMouse && grid.cellWidth < 120
        }
    }
}

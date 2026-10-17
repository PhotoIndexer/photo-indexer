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
//   Arrastar (``dragEnabled``): as selecionadas vão para uma categoria ou um
//   álbum do painel lateral (TreeList, chave "photoidx/photos")
GridView {
    id: grid

    /// PhotoGridModel (App.library.grid, App.people.results).
    property var photos: null
    property int thumbSize: App.library.thumbSize
    /// Mostra a similaridade (busca por IA/rosto) no canto.
    property bool showScore: false
    /// Arrastar as fotos selecionadas para o painel lateral (só a biblioteca).
    property bool dragEnabled: false

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

    // Atualização de fundo (indexando) que mudou a ordem: a grade é refeita,
    // mas fica onde a pessoa estava (o reset voltaria ao topo).
    property real _savedY: 0
    Connections {
        target: grid.photos
        function onModelAboutToBeReset() {
            grid._savedY = grid.contentY;
        }
        function onModelReset() {
            if (!grid.photos.keepPosition)
                return;
            const y = grid._savedY;
            Qt.callLater(() => grid.contentY = Math.max(grid.originY,
                                                       Math.min(y, grid.originY + grid.contentHeight - grid.height)));
        }
    }

    // Rolagem rápida (barra arrastada, arremesso forte): as células que só
    // passam não pedem a miniatura; quando a rolagem para, só as visíveis
    // carregam. Sem isso, varrer milhares de fotos enfileirava milhares de
    // leituras de arquivo.
    readonly property bool fastScroll: scrollWatch.fast
    ScrollWatch {
        id: scrollWatch
        target: grid
    }

    Keys.onReturnPressed: if (currentIndex >= 0) grid.opened(photos.idAt(currentIndex))
    Keys.onEnterPressed: if (currentIndex >= 0) grid.opened(photos.idAt(currentIndex))
    Keys.onSpacePressed: if (currentIndex >= 0) photos.click(currentIndex, Qt.ControlModifier)

    // Ctrl + roda: zoom das miniaturas; sem Ctrl, rola como sempre.
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: (event) => App.library.thumbSize = App.library.thumbSize + (event.angleDelta.y > 0 ? 20 : -20)
    }

    // O que segue o ponteiro enquanto arrasta: a miniatura de onde saiu e
    // quantas fotos vão. Na camada de cima da janela (passa por cima do
    // painel lateral).
    Item {
        id: dragProxy
        property string source: ""
        property int count: 0
        parent: T.Overlay.overlay
        width: 88
        height: 88
        z: 1000
        visible: Drag.active
        Drag.keys: ["photoidx/photos"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            color: Theme.card
            border.width: 2
            border.color: Theme.brand400
            opacity: 0.95
            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: dragProxy.source
                sourceSize: Qt.size(160, 160)
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: !dragProxy.source.startsWith("image://private")
            }
        }
        Badge {
            x: parent.width - width / 2 - 4
            y: -height / 2 + 4
            text: dragProxy.count
            variant: "brand"
        }
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
                // A que já carregou fica; a que ainda não, espera a rolagem parar.
                property string loadedUrl: ""
                anchors.fill: parent
                anchors.margins: cell.selected ? 6 : 4
                source: grid.fastScroll && loadedUrl !== cell.thumbUrl ? "" : cell.thumbUrl
                onStatusChanged: if (status === Image.Ready) loadedUrl = String(source)
                sourceSize: Qt.size(grid.cellWidth * Screen.devicePixelRatio, grid.cellHeight * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                // Cache para as comuns (rolar não relê do disco); a cifrada
                // (image://private) não fica na memória depois de trancar.
                cache: !cell.thumbUrl.startsWith("image://private")
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
            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
            // Arrastar com o botão esquerdo leva a seleção; a grade não rola
            // pelo arrasto do mouse (a roda e a barra rolam).
            drag.target: grid.dragEnabled ? dragProxy : null
            drag.threshold: 8
            preventStealing: grid.dragEnabled
            onPressed: (event) => {
                if (!grid.dragEnabled || event.button !== Qt.LeftButton)
                    return;
                const p = mouse.mapToItem(dragProxy.parent, event.x, event.y);
                dragProxy.x = p.x - dragProxy.width / 2;
                dragProxy.y = p.y - dragProxy.height / 2;
                dragProxy.source = cell.thumbUrl;
            }
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

        Connections {
            target: mouse.drag
            function onActiveChanged() {
                if (mouse.drag.active) {
                    // Arrastou uma que não estava selecionada: vai só ela.
                    if (!cell.selected)
                        grid.photos.click(cell.index, 0);
                    dragProxy.count = grid.photos.selectedCount;
                    dragProxy.Drag.active = true;
                } else if (dragProxy.Drag.active) {
                    dragProxy.Drag.drop();
                    dragProxy.Drag.active = false;
                }
            }
        }

        Tip {
            text: cell.name
            visible: mouse.containsMouse && grid.cellWidth < 120 && !mouse.drag.active
        }
    }
}

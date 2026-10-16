pragma ComponentBehavior: Bound
import QtQuick
import PhotoIDX

// Divisória arrastável entre dois painéis (components/ui/splitter.jsx).
//
// Visual: faixa de 5px (duas linhas de 1px + 3px entre elas) com a pegada no
// meio — três bolinhas de 3px. Ao passar o mouse, arrastar ou focar pelo
// teclado a faixa inteira fica no acento a 60% e as bolinhas em branco suave.
//
// ``orientation``: Qt.Vertical (padrão) = linha vertical que separa COLUNAS
// (arrasta na horizontal); Qt.Horizontal = separa LINHAS.
//
// É controlado: ``value`` vem de quem usa; arrastar/teclado emitem
// ``moved(novoValor)`` já limitado a [from, to]. Como a posição do ponteiro
// vira valor é dito por ``mode``, medido em ``container``:
//   "fraction"      valor = posição / tamanho do container (0..1)
//   "pixels"        valor = posição em px a partir do início do container
//   "pixelsFromEnd" valor = tamanho do container - posição (painel à direita)
//
//   Splitter {
//       container: photosRow; mode: "fraction"
//       value: layout.split; from: 0.2; to: 0.8; stepSize: 0.02; defaultValue: 0.5
//       onMoved: (v) => layout.split = v
//   }
//
// Teclado: setas = ±stepSize; Home/End e duplo clique = defaultValue. As
// setas não propagam (no editor elas trocam de foto).
Item {
    id: root

    property int orientation: Qt.Vertical
    property Item container: parent
    property string mode: "fraction"
    property real value: 0.5
    property real from: 0
    property real to: 1
    property real stepSize: 0.02
    property real defaultValue: 0.5
    property string label: ""

    readonly property bool vertical: orientation === Qt.Vertical
    readonly property bool active: mouse.pressed || mouse.containsMouse || activeFocus
    readonly property bool dragging: mouse.pressed

    signal moved(real value)
    /// Fim do arraste (bom momento para persistir).
    signal released()

    function clamp(v) {
        return Math.min(to, Math.max(from, v));
    }

    function valueAt(pos) {
        const extent = vertical ? container.width : container.height;
        if (mode === "pixels")
            return pos;
        if (mode === "pixelsFromEnd")
            return extent - pos;
        return extent > 0 ? pos / extent : value;
    }

    implicitWidth: vertical ? 5 : 100
    implicitHeight: vertical ? 100 : 5
    z: 20
    activeFocusOnTab: true

    Accessible.role: Accessible.Separator
    Accessible.name: label

    Keys.onPressed: (event) => {
        const back = vertical ? Qt.Key_Left : Qt.Key_Up;
        const forward = vertical ? Qt.Key_Right : Qt.Key_Down;
        if (event.key === back || event.key === forward) {
            root.moved(clamp(value + (event.key === forward ? 1 : -1) * stepSize));
            root.released();
            event.accepted = true;
        } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
            root.moved(clamp(defaultValue));
            root.released();
            event.accepted = true;
        }
    }

    // A faixa: fundo + linhas de 1px nas bordas.
    Rectangle {
        anchors.fill: parent
        color: root.active ? Theme.splitterActive : Theme.splitterBand
        Behavior on color { ColorAnimation { duration: Theme.animationFast } }
    }
    Rectangle {
        visible: !root.active
        color: Theme.splitterLine
        x: 0
        y: 0
        width: root.vertical ? 1 : root.width
        height: root.vertical ? root.height : 1
    }
    Rectangle {
        visible: !root.active
        color: Theme.splitterLine
        x: root.vertical ? root.width - 1 : 0
        y: root.vertical ? 0 : root.height - 1
        width: root.vertical ? 1 : root.width
        height: root.vertical ? root.height : 1
    }

    // A pegada: três círculos na direção da divisória.
    Grid {
        anchors.centerIn: parent
        columns: root.vertical ? 1 : 3
        spacing: 2
        Repeater {
            model: 3
            Rectangle {
                width: 3
                height: 3
                radius: 1.5
                color: root.active ? Theme.splitterGripActive : Theme.splitterGrip
            }
        }
    }

    MouseArea {
        id: mouse
        // Alvo um pouco maior que o desenho: 5px é fino para o mouse.
        anchors.fill: parent
        anchors.leftMargin: root.vertical ? -2 : 0
        anchors.rightMargin: root.vertical ? -2 : 0
        anchors.topMargin: root.vertical ? 0 : -2
        anchors.bottomMargin: root.vertical ? 0 : -2
        hoverEnabled: true
        preventStealing: true
        cursorShape: root.vertical ? Qt.SplitHCursor : Qt.SplitVCursor

        onPositionChanged: (event) => {
            if (!pressed || !root.container)
                return;
            const p = mapToItem(root.container, event.x, event.y);
            root.moved(root.clamp(root.valueAt(root.vertical ? p.x : p.y)));
        }
        onReleased: root.released()
        onDoubleClicked: {
            root.moved(root.clamp(root.defaultValue));
            root.released();
        }
    }
}

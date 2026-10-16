import QtQuick
import PhotoIDX

// Liga/desliga — o controle booleano padrão de todo o app (CLAUDE.md 2.2;
// components/ui/toggle.jsx). Trilho 36x20, bola de 16; ligado = brand500.
//
//   Toggle {
//       label: qsTr("Visible")
//       checked: layer.visible
//       onToggled: (value) => App.merge.setVisible(layer.id, value)
//   }
//
// É CONTROLADO, como o React: clicar emite ``toggled(!checked)`` e quem usa
// atualiza ``checked`` (direto ou pelo modelo). Assim um binding em
// ``checked`` nunca é quebrado pelo clique. O rótulo também é clicável.
Item {
    id: control

    property bool checked: false
    property string label: ""
    property bool disabled: false
    readonly property bool hovered: mouse.containsMouse

    /// Novo valor pedido pelo clique/teclado.
    signal toggled(bool value)

    function toggle() {
        if (!disabled)
            toggled(!checked);
    }

    implicitWidth: row.implicitWidth
    implicitHeight: Math.max(20, row.implicitHeight)
    opacity: disabled ? 0.5 : 1
    activeFocusOnTab: !disabled

    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.onToggleAction: toggle()
    Accessible.onPressAction: toggle()

    Keys.onSpacePressed: toggle()
    Keys.onReturnPressed: toggle()
    Keys.onEnterPressed: toggle()

    Row {
        id: row
        spacing: 8
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            id: track
            width: 36
            height: 20
            radius: 10
            anchors.verticalCenter: parent.verticalCenter
            color: control.checked ? Theme.brand500 : Theme.muted
            Behavior on color { ColorAnimation { duration: Theme.animationFast } }

            Rectangle {
                width: 16
                height: 16
                radius: 8
                color: Theme.white
                anchors.verticalCenter: parent.verticalCenter
                // 2px de folga nas duas pontas: 2 desligado, 18 ligado.
                x: control.checked ? 18 : 2
                Behavior on x { NumberAnimation { duration: Theme.animationFast; easing.type: Easing.OutCubic } }
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: height / 2
                color: "transparent"
                border.width: 2
                border.color: Theme.focusRing
                visible: control.activeFocus
            }
        }

        Text {
            visible: control.label !== ""
            text: control.label
            font.pixelSize: Theme.fontBase
            color: control.checked || control.hovered ? Theme.foreground : Theme.mutedForeground
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: !control.disabled
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            control.forceActiveFocus(Qt.MouseFocusReason);
            control.toggle();
        }
    }
}

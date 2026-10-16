import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Campo de texto de uma linha (components/ui/input.jsx): 40px, borda
// ``input``, raio 6, anel de foco. Ocupa a largura que receber.
//
//   TextField { Layout.fillWidth: true; placeholderText: "@"; onAccepted: ... }
//   TextField { iconName: "search"; placeholderText: qsTr("Search projects") }
//
// ``error`` pinta a borda de vermelho (a mensagem vai num FormError).
T.TextField {
    id: control

    /// Ícone à esquerda, dentro do campo (ex.: "search").
    property string iconName: ""
    property bool error: false

    implicitWidth: 240
    implicitHeight: Theme.controlHeight
    leftPadding: iconName !== "" ? 32 : 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter

    font.pixelSize: Theme.fontBase
    color: Theme.foreground
    selectionColor: Theme.alpha(Theme.brand400, 0.45)
    selectedTextColor: Theme.foreground
    placeholderTextColor: Theme.mutedForeground
    selectByMouse: true
    opacity: enabled ? 1 : 0.5

    Text {
        x: control.leftPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height
        verticalAlignment: Text.AlignVCenter
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        elide: Text.ElideRight
        visible: !control.length && !control.preeditText
    }

    Icon {
        visible: control.iconName !== ""
        name: control.iconName
        size: 16
        color: Theme.mutedForeground
        x: 10
        anchors.verticalCenter: parent.verticalCenter
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.background
        border.color: control.error ? Theme.red500 : Theme.input

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: control.error ? Theme.red500 : Theme.focusRing
            visible: control.activeFocus
        }
    }
}

import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Texto de várias linhas no mesmo estilo do TextField. Cresce com o
// conteúdo a partir de ``rows`` linhas (a página rola, não o campo).
//
//   TextArea { Layout.fillWidth: true; rows: 6; text: template; onTextChanged: ... }
T.TextArea {
    id: control

    property int rows: 4
    property bool error: false

    implicitWidth: 240
    implicitHeight: Math.max(rows * Math.ceil(fontMetrics.lineSpacing) + topPadding + bottomPadding,
                             contentHeight + topPadding + bottomPadding)
    padding: 8
    leftPadding: 12
    rightPadding: 12

    font.pixelSize: Theme.fontBase
    color: Theme.foreground
    selectionColor: Theme.alpha(Theme.brand400, 0.45)
    selectedTextColor: Theme.foreground
    placeholderTextColor: Theme.mutedForeground
    selectByMouse: true
    wrapMode: TextEdit.Wrap
    opacity: enabled ? 1 : 0.5

    FontMetrics {
        id: fontMetrics
        font: control.font
    }

    Text {
        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        wrapMode: Text.Wrap
        visible: !control.length && !control.preeditText
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

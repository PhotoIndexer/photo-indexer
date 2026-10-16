import QtQuick
import PhotoIDX

// Rótulo de campo de formulário (components/ui/label.jsx): 14px, médio.
// ``buddy`` recebe o foco ao clicar no rótulo.
Text {
    id: label

    property Item buddy: null

    font.pixelSize: Theme.fontBase
    font.weight: Font.Medium
    color: Theme.foreground
    wrapMode: Text.Wrap

    MouseArea {
        anchors.fill: parent
        enabled: label.buddy !== null
        onClicked: label.buddy.forceActiveFocus(Qt.MouseFocusReason)
    }
}

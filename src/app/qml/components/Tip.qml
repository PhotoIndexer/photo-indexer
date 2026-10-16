import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Dica ao passar o mouse, no tema do app (o "title" das telas React).
//
//   IconButton { ...; Tip { text: qsTr("Home"); visible: parent.hovered } }
//
// O IconButton já traz a sua (propriedade ``tooltip``).
T.ToolTip {
    id: tip

    delay: 500
    timeout: -1
    x: parent ? Math.round((parent.width - implicitWidth) / 2) : 0
    y: parent ? parent.height + 6 : 0
    margins: 6
    padding: 6
    leftPadding: 8
    rightPadding: 8
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    implicitWidth: Math.min(360, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    contentItem: Text {
        text: tip.text
        font.pixelSize: Theme.fontSm
        color: Theme.popoverForeground
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        color: Theme.popover
        border.color: Theme.border
        radius: Theme.radiusMd
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animationFast }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
    }
}

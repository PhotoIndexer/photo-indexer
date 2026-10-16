import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Barra de rolagem fina (App.css .scrollbar-thin): 8px, alça arredondada
// translúcida, sem trilho. Some quando não há o que rolar.
//
//   Flickable { ...; T.ScrollBar.vertical: ThinScrollBar {} }
T.ScrollBar {
    id: control

    implicitWidth: orientation === Qt.Vertical ? Theme.scrollbarSize + 2 : 100
    implicitHeight: orientation === Qt.Horizontal ? Theme.scrollbarSize + 2 : 100
    padding: 1
    minimumSize: 0.05
    visible: policy === T.ScrollBar.AlwaysOn || size < 1.0
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: Theme.scrollbarSize
        implicitHeight: Theme.scrollbarSize
        radius: Theme.radiusSm
        color: control.hovered || control.pressed ? Theme.scrollbarThumbHover : Theme.scrollbarThumb
    }

    background: Rectangle {
        color: Theme.scrollbarTrack
    }
}

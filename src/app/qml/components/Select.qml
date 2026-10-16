pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Lista de escolha (o <select> das telas), em ComboBox com o tema.
//
//   Select {
//       model: [{value: "date", text: qsTr("Date/time")}, ...]
//       textRole: "text"; valueRole: "value"
//       currentIndex: indexOfValue(App.editor.sortMode)
//       onActivated: App.editor.sortMode = currentValue
//   }
//
// variant: "default" caixa com borda (formulários, barra da filmstrip)
//          "ghost"   só texto apagado + seta (seletor de idioma do cabeçalho)
T.ComboBox {
    id: control

    property string variant: "default"

    readonly property bool _ghost: variant === "ghost"

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: _ghost ? 4 : 12
    rightPadding: (_ghost ? 4 : 12) + 16 + 6
    hoverEnabled: true
    font.pixelSize: Theme.fontBase
    opacity: enabled ? 1 : 0.5

    delegate: T.ItemDelegate {
        id: item
        required property var model
        required property int index

        width: ListView.view ? ListView.view.width : implicitWidth
        implicitHeight: 32
        implicitWidth: itemText.implicitWidth + leftPadding + rightPadding
        leftPadding: 10
        rightPadding: 28
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            id: itemText
            text: item.model[control.textRole] !== undefined ? item.model[control.textRole] : item.model.modelData
            font: control.font
            color: Theme.popoverForeground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Icon {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            visible: control.currentIndex === item.index
            name: "check"
            size: 14
            color: Theme.brand400
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: item.highlighted || item.hovered ? Theme.accent : "transparent"
        }
    }

    indicator: Icon {
        x: control.width - width - (control._ghost ? 4 : 12)
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 16
        color: control._ghost && !control.hovered ? Theme.mutedForeground : Theme.foreground
        opacity: control._ghost ? 1 : 0.6
    }

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control._ghost ? (control.hovered ? Theme.foreground : Theme.mutedForeground) : Theme.foreground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: control._ghost ? 40 : 160
        radius: Theme.radiusMd
        color: control._ghost ? "transparent" : Theme.background
        border.width: control._ghost ? 0 : 1
        border.color: Theme.input

        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.focusRing
            visible: control.visualFocus
        }
    }

    popup: T.Popup {
        x: control._ghost ? control.width - width : 0
        y: control.height + 4
        width: Math.max(control.width, 180)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)
        padding: 4

        contentItem: ListView {
            id: list
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}
        }

        background: Rectangle {
            color: Theme.popover
            border.color: Theme.border
            radius: Theme.radiusMd
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 100 }
        }
    }
}

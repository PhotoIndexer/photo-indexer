import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import PhotoIDX

// Item do AppMenu: ✓ dos itens marcáveis, texto, atalho e seta de submenu.
//
//   AppMenuItem { text: qsTr("Quit"); shortcutText: Keymap.label("quit"); onTriggered: ... }
//
// O atalho e a seta ficam numa coluna alinhada à direita (a mesma para todas
// as linhas do menu): o texto ocupa o resto. O atalho é só mostrado — quem
// dispara são os Shortcut globais do Main.qml.
QQC.MenuItem {
    id: item

    /// Atalho já no formato do SO (``Keymap.label("quit")`` → "⌘Q" / "Ctrl+Q").
    property string shortcutText: ""

    implicitHeight: 36
    leftPadding: 12
    rightPadding: 12
    font.pixelSize: Theme.fontBase

    contentItem: RowLayout {
        spacing: 10

        // Item marcável (idioma): ✓ no acento quando marcado.
        Icon {
            visible: item.checkable
            name: "check"
            size: 14
            color: Theme.brandForeground
            opacity: item.checked ? 1 : 0
        }
        Text {
            Layout.fillWidth: true
            Layout.rightMargin: 22
            text: item.text.replace("&", "")
            font: item.font
            color: Theme.popoverForeground
            opacity: item.enabled ? 1 : 0.45
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            visible: item.shortcutText !== ""
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            text: item.shortcutText
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
            opacity: item.enabled ? 1 : 0.45
            horizontalAlignment: Text.AlignRight
        }
        Icon {
            visible: item.subMenu !== null
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            name: "chevron-right"
            size: 14
            color: Theme.mutedForeground
        }
    }

    arrow: null
    indicator: null

    background: Rectangle {
        implicitWidth: 220
        radius: Theme.radiusSm
        color: item.highlighted && item.enabled ? Theme.accent : "transparent"
    }
}

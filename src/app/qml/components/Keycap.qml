import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Tecla de atalho desenhada como tecla ("⌘K", "Ctrl+Shift+Z"): busca do
// cabeçalho, linhas da paleta de comandos e editor de atalhos.
//
//   Keycap { text: Keymap.label("palette") }
//   Keycap { text: App.shortcutText(seq); removable: true; onRemoveClicked: ... }
//
// Vazio (``text: ""``) não ocupa espaço.
Rectangle {
    id: keycap

    property string text: ""
    /// Mostra um × que remove o atalho (editor de atalhos).
    property bool removable: false
    property string removeTooltip: ""

    signal removeClicked()

    visible: text !== ""
    implicitWidth: row.implicitWidth + 12
    implicitHeight: 20
    radius: Theme.radiusSm
    color: Theme.muted
    border.color: Theme.border

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 4

        Text {
            text: keycap.text
            font.pixelSize: Theme.fontXs
            font.weight: Font.Medium
            color: Theme.mutedForeground
        }
        Icon {
            id: removeIcon
            visible: keycap.removable
            name: "x"
            size: 11
            color: removeHover.hovered ? Theme.danger : Theme.mutedForeground

            HoverHandler {
                id: removeHover
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                onTapped: keycap.removeClicked()
            }
            Tip {
                text: keycap.removeTooltip
                visible: removeHover.hovered && keycap.removeTooltip !== ""
            }
        }
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: text
}

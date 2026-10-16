import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Booleano com título e texto de ajuda, em caixa: rótulo à esquerda e o
// Toggle à direita (ToggleField do toggle.jsx). Ocupa a largura disponível.
//
//   ToggleField {
//       Layout.fillWidth: true
//       label: qsTr("Detect the object (AI)")
//       description: qsTr("On: the stroke becomes the whole object under it.")
//       checked: smart
//       onToggled: (value) => smart = value
//   }
Rectangle {
    id: field

    property bool checked: false
    property string label: ""
    property string description: ""
    property bool disabled: false

    signal toggled(bool value)

    implicitWidth: layout.implicitWidth + 32
    implicitHeight: layout.implicitHeight + 32
    radius: Theme.radiusLg
    color: "transparent"
    border.color: Theme.border

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: field.label
                font.pixelSize: Theme.fontBase
                font.weight: Font.Medium
                color: Theme.foreground
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: field.description !== ""
                text: field.description
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }

        Toggle {
            checked: field.checked
            disabled: field.disabled
            Accessible.name: field.label
            onToggled: (value) => field.toggled(value)
        }
    }
}

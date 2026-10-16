import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Cartão (components/ui/card.jsx): borda, fundo ``card``, raio 8. Ocupa a
// largura de quem o contém (regra 2: nada de card estreito centralizado).
//
//   Card {
//       Layout.fillWidth: true
//       title: qsTr("Account")
//       description: qsTr("The account the photos are published to.")
//       Text { ... }              // conteúdo: empilhado numa ColumnLayout
//   }
//
// O conteúdo vai numa ColumnLayout (spacing ``contentSpacing``) com p-6 e
// pt-0 abaixo do cabeçalho. Use Layout.fillWidth nos filhos.
Rectangle {
    id: card

    property string title: ""
    property string description: ""
    property int padding: 24
    property alias contentSpacing: content.spacing
    default property alias content: content.data

    implicitWidth: column.implicitWidth + 2 * padding
    implicitHeight: column.implicitHeight + 2 * padding
    radius: Theme.radiusLg
    color: Theme.card
    border.color: Theme.border

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: card.padding
        spacing: 24

        ColumnLayout {
            Layout.fillWidth: true
            visible: card.title !== "" || card.description !== ""
            spacing: 6

            Text {
                Layout.fillWidth: true
                visible: card.title !== ""
                text: card.title
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
                color: Theme.cardForeground
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                visible: card.description !== ""
                text: card.description
                font.pixelSize: Theme.fontBase
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }

        ColumnLayout {
            id: content
            Layout.fillWidth: true
            spacing: 16
        }
    }
}

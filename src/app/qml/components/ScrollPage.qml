import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Corpo rolável das telas que não são o editor (Home, Instagram): respiro
// p-6 e conteúdo numa ColumnLayout de LARGURA TOTAL (regra 2).
//
//   ScrollPage {
//       spacing: 32
//       Card { Layout.fillWidth: true; ... }
//   }
Flickable {
    id: page

    property int padding: Theme.pagePadding
    property alias spacing: column.spacing
    default property alias content: column.data

    clip: true
    contentWidth: width
    contentHeight: column.implicitHeight + 2 * padding
    boundsBehavior: Flickable.StopAtBounds
    flickableDirection: Flickable.VerticalFlick

    T.ScrollBar.vertical: ThinScrollBar {}

    ColumnLayout {
        id: column
        x: page.padding
        y: page.padding
        width: page.width - 2 * page.padding
        spacing: 16
    }
}

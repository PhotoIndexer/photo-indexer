import QtQuick
import QtQuick.Controls as QQC
import PhotoIDX

// Menu no tema do app (os menus do trilho lateral; não há barra de menus):
// fundo arredondado, linhas de 36px com o atalho numa coluna à direita
// (AppMenuItem) e separadores de 1px (AppMenuSeparator).
//
//   AppMenu {
//       title: qsTr("Open recent")   // só em submenu
//       AppMenuItem { text: qsTr("Open…"); shortcutText: Keymap.label("openProject"); onTriggered: ... }
//       AppMenuSeparator {}
//   }
QQC.Menu {
    id: menu

    topPadding: 6
    bottomPadding: 6
    leftPadding: 6
    rightPadding: 6
    font.pixelSize: Theme.fontBase

    delegate: AppMenuItem {}

    background: Rectangle {
        implicitWidth: 300
        color: Theme.popover
        border.color: Theme.border
        radius: Theme.radiusXl
    }
}

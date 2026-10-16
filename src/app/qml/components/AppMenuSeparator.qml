import QtQuick
import QtQuick.Controls as QQC
import PhotoIDX

// Separador do AppMenu: linha de 1px na cor da borda (como o Separator).
//
//   AppMenu { AppMenuItem {...}; AppMenuSeparator {}; AppMenuItem {...} }
QQC.MenuSeparator {
    topPadding: 4
    bottomPadding: 4
    leftPadding: 6
    rightPadding: 6

    contentItem: Rectangle {
        implicitWidth: 200
        implicitHeight: 1
        color: Theme.border
    }
}

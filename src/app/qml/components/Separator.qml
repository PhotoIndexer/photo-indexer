import QtQuick
import PhotoIDX

// Linha divisória de 1px na cor da borda. ``vertical`` para separar itens
// numa linha (a barrinha do cabeçalho: h-6 w-px).
Rectangle {
    property bool vertical: false

    implicitWidth: vertical ? 1 : 100
    implicitHeight: vertical ? 24 : 1
    color: Theme.border
}

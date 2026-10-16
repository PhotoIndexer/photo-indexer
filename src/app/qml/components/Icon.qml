import QtQuick
import PhotoIDX

// Ícone Lucide (src/app/icons/<name>.svg) recolorido pelo provider
// image://icon. Renderiza na densidade da tela, então fica nítido em HiDPI.
//
//   Icon { name: "folder-open"; color: Theme.brand400; size: 20 }
//
// O SVG não aceita cor com alfa: a transparência de ``color`` vira a
// ``opacity`` do item.
// Um Item com o tamanho LÓGICO e a imagem dentro: o tamanho natural de uma
// Image é o do bitmap em pixels FÍSICOS (size × devicePixelRatio) e é só
// leitura — num Retina (2×) os layouts liam o dobro e o ícone saía gigante.
Item {
    id: icon

    /// Nome do arquivo em src/app/icons, sem ".svg" ("" = nada).
    property string name: ""
    property color color: Theme.foreground
    /// Lado do quadrado, em px lógicos.
    property int size: 16

    implicitWidth: size
    implicitHeight: size
    opacity: color.a

    Image {
        anchors.fill: parent
        sourceSize: Qt.size(Math.ceil(icon.size * Screen.devicePixelRatio), Math.ceil(icon.size * Screen.devicePixelRatio))
        source: icon.name ? Theme.iconUrl(icon.name, icon.color) : ""
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        cache: true
    }
}

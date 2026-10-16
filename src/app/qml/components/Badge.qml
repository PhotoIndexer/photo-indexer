import QtQuick
import PhotoIDX

// Selo arredondado (o "Aberto" dos cards da Home, "MERGE" da filmstrip).
//
// variant: "brand"   fundo brand500, texto branco (padrão)
//          "dark"    fundo preto/60, texto branco (rótulo sobre foto)
//          "muted"   fundo muted, texto apagado
//          "success" fundo esmeralda translúcido
Rectangle {
    id: badge

    property string text: ""
    property string variant: "brand"
    property bool uppercase: false

    implicitWidth: label.implicitWidth + 16
    implicitHeight: label.implicitHeight + 4
    radius: height / 2
    color: {
        switch (variant) {
        case "dark": return Theme.alpha(Theme.black, 0.6);
        case "muted": return Theme.muted;
        case "success": return Theme.alpha(Theme.emerald400, 0.15);
        }
        return Theme.brand500;
    }

    Text {
        id: label
        anchors.centerIn: parent
        text: badge.uppercase ? badge.text.toUpperCase() : badge.text
        font.pixelSize: Theme.fontTiny
        font.weight: Font.DemiBold
        font.letterSpacing: badge.uppercase ? 0.5 : 0
        color: badge.variant === "muted" ? Theme.mutedForeground
             : badge.variant === "success" ? Theme.success : Theme.white
    }
}

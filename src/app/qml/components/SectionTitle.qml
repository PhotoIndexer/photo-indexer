import QtQuick
import PhotoIDX

// Título de seção dos painéis ("PRESETS", "LUZ", "CAMADAS"): 11px,
// semibold, maiúsculas, espaçado, apagado.
Text {
    font.pixelSize: Theme.fontXs
    font.weight: Font.DemiBold
    font.capitalization: Font.AllUppercase
    font.letterSpacing: 0.6
    color: Theme.mutedForeground
}

import QtQuick
import PhotoIDX

// Indicador de "carregando": o loader-circle do Lucide girando (o Loader2
// animate-spin do React).
Icon {
    id: spinner

    /// Gira só enquanto visível.
    property bool running: visible

    name: "loader-circle"
    color: Theme.mutedForeground
    size: 16

    RotationAnimator on rotation {
        from: 0
        to: 360
        duration: 1000
        loops: Animation.Infinite
        running: spinner.running
    }
}

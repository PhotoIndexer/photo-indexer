import QtQuick
import QtQuick.Templates as T
import PhotoIDX

// Botão padrão do app (components/ui/button.jsx).
//
//   Button { text: qsTr("Export"); iconName: "upload"; variant: "default"; onClicked: ... }
//
// variant: "default"     azul (primary do tema escuro) — ação principal da tela
//          "primary"     vermelho da marca — chamada de destaque
//          "secondary"   cinza preenchido
//          "outline"     borda, fundo transparente
//          "ghost"       só texto; fundo ao passar o mouse
//          "destructive" vermelho escuro — excluir, desfazer algo
// size:    "sm" (36px, texto 12) | "default" (40px, 14) | "lg" (44px, 16)
// loading: troca o ícone pelo spinner e ignora cliques.
//
// Para desabilitar use ``disabled`` (o ``enabled`` já é !disabled && !loading;
// escrever direto nele perde a trava do loading).
T.Button {
    id: control

    property string variant: "default"
    property string size: "default"
    property string iconName: ""
    property bool loading: false
    property bool disabled: false
    /// Cor do texto/ícone; por padrão a da variante.
    property color textColor: {
        switch (variant) {
        case "default": return Theme.primaryForeground;
        case "primary": return Theme.white;
        case "destructive": return Theme.destructiveForeground;
        case "secondary": return Theme.secondaryForeground;
        default: return (hovered && enabled) ? Theme.accentForeground : Theme.foreground;
        }
    }
    /// Alinhamento do conteúdo (Qt.AlignHCenter ou Qt.AlignLeft).
    property int contentAlignment: Qt.AlignHCenter

    readonly property int _height: size === "sm" ? Theme.controlHeightSm : size === "lg" ? Theme.controlHeightLg : Theme.controlHeight
    readonly property int _iconSize: size === "lg" ? 18 : 16

    enabled: !disabled && !loading
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    font.pixelSize: size === "sm" ? Theme.fontSm : size === "lg" ? Theme.fontMd : Theme.fontBase
    font.weight: Font.Medium

    leftPadding: size === "sm" ? 12 : size === "lg" ? 32 : 16
    rightPadding: leftPadding
    topPadding: 0
    bottomPadding: 0
    spacing: 8

    implicitHeight: _height
    implicitWidth: Math.max(_height, implicitContentWidth + leftPadding + rightPadding)

    opacity: (disabled && !loading) ? 0.5 : (loading ? 0.85 : 1)

    readonly property bool _hasIcon: loading || iconName !== ""
    readonly property real _iconSpace: _hasIcon ? _iconSize + (text !== "" ? spacing : 0) : 0

    contentItem: Item {
        implicitWidth: control._iconSpace + (control.text !== "" ? label.implicitWidth : 0)
        implicitHeight: Math.max(control._iconSize, label.implicitHeight)

        Row {
            id: row
            spacing: control.spacing
            anchors.verticalCenter: parent.verticalCenter
            x: control.contentAlignment === Qt.AlignLeft ? 0 : Math.round((parent.width - width) / 2)

            Spinner {
                visible: control.loading
                size: control._iconSize
                color: control.textColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Icon {
                visible: !control.loading && control.iconName !== ""
                name: control.iconName
                size: control._iconSize
                color: control.textColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                id: label
                visible: control.text !== ""
                text: control.text
                font: control.font
                color: control.textColor
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, Math.min(implicitWidth, row.parent.width - control._iconSpace))
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: {
            const hot = control.hovered && control.enabled;
            const down = control.down;
            switch (control.variant) {
            case "default": return down ? Theme.primaryPressed : hot ? Theme.primaryHover : Theme.primary;
            case "primary": return down ? Theme.brand700 : hot ? Theme.brand600 : Theme.brand500;
            case "destructive": return hot || down ? Theme.destructiveHover : Theme.destructive;
            case "secondary": return hot || down ? Theme.alpha(Theme.secondary, 0.8) : Theme.secondary;
            case "outline":
            case "ghost": return hot || down ? Theme.accent : "transparent";
            }
            return Theme.primary;
        }
        border.width: control.variant === "outline" ? 1 : 0
        border.color: Theme.input

        Behavior on color { ColorAnimation { duration: Theme.animationFast } }

        // Anel de foco do teclado (focus-visible:ring-2).
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.ring
            visible: control.visualFocus
        }
    }
}

import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Modal base do app (components/ui/modal.jsx) — para confirmações e
// mensagens curtas, nunca para o detalhe de um objeto. Para confirmar/avisar
// use o singleton ``Dialogs``; o Modal direto é para diálogos com conteúdo
// próprio (Sobre, Exportar, Publicar...).
//
//   Modal {
//       id: about
//       title: qsTr("About PhotoIDX")
//       description: qsTr("Event photo editor.")
//       iconName: "info"; variant: "info"
//       size: "md"
//       Text { ... }                                  // conteúdo (centro)
//       footer: [ Button { text: qsTr("Close"); onClicked: about.close() } ]
//       // footerVertical: true -> botões empilhados (use Layout.fillWidth neles)
//   }
//   about.open()
//
// Centralizado, fundo preto/40, cartão rounded-2xl, Esc e clique fora
// fecham (``dismissible``; desligue durante uma ação em andamento), botão X,
// ícone de 48px num círculo colorido pela ``variant``, entrada em fade.
T.Popup {
    id: modal

    property string title: ""
    /// Aceita rich text simples (<b>, <br>).
    property string description: ""
    /// Ícone no círculo; vazio = o da variante (se ``showIcon``).
    property string iconName: ""
    /// danger | warning | success | info | brand — cor do círculo do ícone.
    property string variant: "info"
    property bool showIcon: iconName !== ""
    /// sm (384) | md (448) | lg (512) | xl (672)
    property string size: "sm"
    property bool showClose: true
    property bool dismissible: true
    /// Alinhamento do texto: centro (padrão das confirmações) ou esquerda.
    property int textAlignment: Text.AlignHCenter
    default property alias content: body.data
    property alias footer: footerGrid.data
    /// Botões do rodapé empilhados em largura total (muitas opções ou
    /// rótulos longos); padrão: lado a lado, centralizados.
    property bool footerVertical: false
    /// Mostra a área de conteúdo; por padrão, quando há filhos nela. Quem tem
    /// conteúdo só às vezes (o campo de confirmação do DialogHost) liga aqui.
    property bool showContent: body.children.length > 0

    readonly property var variantStyle: {
        switch (variant) {
        case "danger":
            return { icon: "trash-2", fg: Theme.danger, bg: Theme.dark ? Theme.alpha(Theme.red900, 0.3) : Theme.red100 };
        case "warning":
            return { icon: "triangle-alert", fg: Theme.warning, bg: Theme.dark ? Theme.alpha(Theme.amber900, 0.3) : Theme.amber100 };
        case "success":
            return { icon: "circle-check", fg: Theme.success, bg: Theme.dark ? Theme.alpha(Theme.emerald900, 0.3) : Theme.emerald100 };
        case "brand":
            // Acento da marca (Exportar, Instagram): não é aviso nem erro.
            return { icon: "info", fg: Theme.brand400, bg: Theme.alpha(Theme.brand400, 0.15) };
        }
        return { icon: "info", fg: Theme.info, bg: Theme.dark ? Theme.alpha(Theme.blue900, 0.3) : Theme.blue100 };
    }

    readonly property int maxWidth: size === "xl" ? 672 : size === "lg" ? 512 : size === "md" ? 448 : 384

    parent: T.Overlay.overlay
    modal: true
    dim: true
    focus: true
    anchors.centerIn: parent
    // O Popup dos Templates não calcula tamanho implícito: é daqui.
    implicitWidth: maxWidth
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    width: parent ? Math.min(maxWidth, parent.width - 32) : maxWidth
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)
    padding: 24
    closePolicy: dismissible ? (T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside) : T.Popup.NoAutoClose

    T.Overlay.modal: Rectangle {
        color: Theme.backdrop
        Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animationNormal; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.animationNormal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Rectangle {
        color: Theme.card
        radius: Theme.radius2xl
        border.color: Theme.border
    }

    contentItem: Flickable {
        implicitWidth: column.implicitWidth
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        contentWidth: width
        clip: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: column
            width: parent.width
            spacing: 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 16
                visible: modal.showIcon
                implicitWidth: 48
                implicitHeight: 48
                radius: 24
                color: modal.variantStyle.bg

                Icon {
                    anchors.centerIn: parent
                    name: modal.iconName !== "" ? modal.iconName : modal.variantStyle.icon
                    size: 20
                    color: modal.variantStyle.fg
                }
            }

            Text {
                Layout.fillWidth: true
                Layout.bottomMargin: 8
                Layout.leftMargin: modal.showClose ? 16 : 0
                Layout.rightMargin: modal.showClose ? 16 : 0
                visible: modal.title !== ""
                text: modal.title
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
                color: Theme.cardForeground
                horizontalAlignment: modal.textAlignment
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: modal.description !== ""
                text: modal.description
                textFormat: Text.StyledText
                font.pixelSize: Theme.fontBase
                color: Theme.mutedForeground
                linkColor: Theme.brand400
                horizontalAlignment: modal.textAlignment
                wrapMode: Text.Wrap
                lineHeight: 1.15
            }

            ColumnLayout {
                id: body
                Layout.fillWidth: true
                Layout.topMargin: modal.showContent ? 8 : 0
                visible: modal.showContent
                spacing: 8
            }

            GridLayout {
                id: footerGrid
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: modal.footerVertical
                Layout.topMargin: 24
                visible: children.length > 0
                columns: modal.footerVertical ? 1 : Math.max(1, children.length)
                columnSpacing: 12
                rowSpacing: 8
            }
        }
    }

    // O X do canto: fora do contentItem para não empurrar o título.
    IconButton {
        parent: modal.background
        visible: modal.showClose && modal.dismissible
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        diameter: 28
        iconSize: 16
        iconName: "x"
        tooltip: qsTr("Close")
        onClicked: modal.close()
    }
}

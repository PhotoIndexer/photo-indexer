import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Tela de espera sobre a janela inteira enquanto App.loading não está vazio
// (abrindo projeto, copiando fotos) — o pages.loading() do shell antigo:
// logo, spinner, mensagem e a dica. Bloqueia cliques por baixo.
Rectangle {
    id: overlay

    readonly property string key: App.loading
    readonly property var params: App.loadingParams

    color: Theme.dark ? Theme.zinc950 : Theme.background
    visible: opacity > 0
    opacity: key !== "" ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animationFast } }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 640)
        spacing: 18

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 34
            Layout.preferredWidth: 34 * 3114 / 575
            source: Theme.dark ? "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-dark.png"
                               : "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-light.png"
            sourceSize.height: 34 * Screen.devicePixelRatio
            fillMode: Image.PreserveAspectFit
            opacity: 0.95
        }

        Spinner {
            Layout.alignment: Qt.AlignHCenter
            size: 28
            color: Theme.brand500
            running: overlay.visible
        }

        Text {
            Layout.fillWidth: true
            text: Messages.loading(overlay.key, overlay.params)
            font.pixelSize: Theme.fontLg
            font.weight: Font.DemiBold
            color: Theme.foreground
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Text {
            Layout.fillWidth: true
            visible: text !== ""
            text: Messages.loadingHint(overlay.key)
            font.pixelSize: Theme.fontBase
            color: Theme.mutedForeground
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        // Progresso da cópia de fotos.
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 280
            Layout.preferredHeight: 4
            visible: overlay.key === "loading.importing" && (overlay.params.total || 0) > 0
            radius: 2
            color: Theme.muted

            Rectangle {
                height: parent.height
                radius: 2
                color: Theme.brand500
                width: parent.width * Math.min(1, (overlay.params.done || 0) / Math.max(1, overlay.params.total || 1))
                Behavior on width { NumberAnimation { duration: 120 } }
            }
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Sobre: versão, onde ficam os dados e os logs, e os créditos (licenças) do
// que vem de terceiros — inclusive o aviso de uso dos modelos de face.
Modal {
    id: about

    title: qsTr("About %1").arg(App.brandName)
    size: "md"
    showIcon: false

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 12

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 28
            Layout.preferredWidth: 28 * 3114 / 575
            source: Theme.dark ? "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-dark.png"
                               : "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-light.png"
            sourceSize.height: 28 * Screen.devicePixelRatio
            fillMode: Image.PreserveAspectFit
        }

        Text {
            Layout.fillWidth: true
            text: App.brandName + " " + App.version
            font.pixelSize: Theme.fontBase
            font.weight: Font.Medium
            color: Theme.foreground
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Photo catalog with AI categories, faces and places.")
            font.pixelSize: Theme.fontBase
            color: Theme.mutedForeground
            horizontalAlignment: Text.AlignHCenter
        }

        // Versão nova: o que a verificação ao abrir achou, e o liga/desliga.
        Text {
            Layout.fillWidth: true
            visible: App.updates.available
            text: qsTr("Version %1 is available.").arg(App.updates.latestVersion)
            font.pixelSize: Theme.fontSm
            font.weight: Font.Medium
            color: Theme.success
            horizontalAlignment: Text.AlignHCenter
        }
        Toggle {
            Layout.alignment: Qt.AlignHCenter
            label: qsTr("Check for a new version when the app opens")
            checked: App.updates.enabled
            onToggled: (value) => App.updates.enabled = value
        }

        // Site e código-fonte: abrem no navegador do sistema.
        Repeater {
            model: [
                { url: App.brandUrl, tip: qsTr("Open the PhotoIDX website") },
                { url: App.repoUrl, tip: qsTr("Source code and releases on GitHub") }
            ]
            Text {
                id: link
                required property var modelData
                Layout.alignment: Qt.AlignHCenter
                text: link.modelData.url.replace(/^https?:\/\//, "").replace(/\/$/, "")
                font.pixelSize: Theme.fontBase
                font.underline: linkMouse.containsMouse
                color: Theme.brand400
                Accessible.role: Accessible.Link
                Accessible.name: link.modelData.tip

                MouseArea {
                    id: linkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.openUrl(link.modelData.url)
                }
                Tip {
                    text: link.modelData.tip
                    visible: linkMouse.containsMouse
                }
            }
        }

        // Créditos: o que vem de terceiros e a licença de cada um.
        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: qsTr("Built with Qt (LGPL), OpenCV (Apache-2.0), ONNX Runtime (MIT) and LibRaw (LGPL-2.1/CDDL). "
                       + "Categories and AI search: OpenAI CLIP (MIT). Places: GeoNames (CC BY 4.0).")
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Text {
            Layout.fillWidth: true
            text: qsTr("Face recognition: insightface buffalo_sc models, licensed for non-commercial research use only.")
            font.pixelSize: Theme.fontSm
            color: Theme.warning
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Repeater {
            model: [
                { label: qsTr("Data"), value: App.dataDir },
                { label: qsTr("Logs"), value: App.logsDir }
            ]
            ColumnLayout {
                id: entry
                required property var modelData
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: entry.modelData.label
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Medium
                    color: Theme.mutedForeground
                    horizontalAlignment: Text.AlignHCenter
                }
                // Selecionável: dá para copiar o caminho.
                TextEdit {
                    Layout.fillWidth: true
                    text: entry.modelData.value
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    font.pixelSize: Theme.fontSm
                    color: Theme.foreground
                    selectionColor: Theme.alpha(Theme.brand400, 0.45)
                    horizontalAlignment: TextEdit.AlignHCenter
                }
            }
        }
    }

    footer: [
        Button {
            text: qsTr("Open Logs Folder")
            variant: "outline"
            iconName: "folder-open"
            onClicked: App.openLogsFolder()
        },
        Button {
            text: qsTr("Close")
            onClicked: about.close()
        }
    ]
}

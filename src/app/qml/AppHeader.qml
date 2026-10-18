import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Cabeçalho do app: à esquerda o logo; ao centro a busca de fotos (texto ou
// IA) e o atalho da paleta de comandos; à direita o andamento da indexação
// (abre Tarefas) e "Adicionar pasta". Baixo de propósito: cada pixel que ele
// não usa vai para as fotos.
Rectangle {
    id: header

    signal paletteRequested()
    signal addFolderRequested()

    /// O campo de busca (o atalho Ctrl+F põe o foco nele).
    property alias searchField: searchField

    implicitHeight: Theme.headerHeight + 1
    color: Theme.card

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Image {
        id: logo
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        height: 22
        width: 22 * 3114 / 575
        source: Theme.dark ? "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-dark.png"
                           : "qrc:/qt/qml/PhotoIDX/assets/logo/photoidx-light.png"
        sourceSize.height: 22 * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: App.page = "library"
        }
    }

    // Busca de fotos, centrada; encolhe para não encostar nas pontas.
    RowLayout {
        id: center
        readonly property real room: header.width - 2 * Math.max(logo.x + logo.width, right.width + 16) - 32
        anchors.centerIn: parent
        width: Math.min(560, room)
        visible: room >= 220
        spacing: 6

        T.TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            leftPadding: 30
            rightPadding: clear.visible ? 28 : 8
            verticalAlignment: TextInput.AlignVCenter
            font.pixelSize: Theme.fontSm
            color: Theme.foreground
            selectionColor: Theme.alpha(Theme.brand400, 0.45)
            selectByMouse: true
            text: App.library.search
            onTextEdited: {
                App.library.search = text;
                if (App.page !== "library")
                    App.page = "library";
            }
            Keys.onEscapePressed: {
                text = "";
                App.library.search = "";
                focus = false;
            }
            Accessible.name: qsTr("Search photos")

            background: Rectangle {
                radius: Theme.radiusLg
                color: Theme.background
                border.color: searchField.activeFocus ? Theme.brand400
                            : App.library.aiSearch ? Theme.alpha(Theme.brand400, 0.6) : Theme.border
                Behavior on border.color { ColorAnimation { duration: Theme.animationFast } }
            }

            Icon {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                name: App.library.aiSearch ? "sparkles" : "search"
                size: 14
                color: App.library.aiSearch ? Theme.brandForeground : Theme.mutedForeground
                visible: !App.library.searching
            }
            Spinner {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                size: 14
                visible: App.library.searching
            }
            Text {
                x: searchField.leftPadding
                width: searchField.width - searchField.leftPadding - searchField.rightPadding
                anchors.verticalCenter: parent.verticalCenter
                visible: !searchField.length && !searchField.preeditText
                text: App.library.aiSearch ? qsTr("Describe the photo (in English): “sunset at the beach”…")
                                           : qsTr("Search: name, folder, camera, place, category, person…")
                font: searchField.font
                color: Theme.mutedForeground
                elide: Text.ElideRight
            }
            IconButton {
                id: clear
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                visible: searchField.length > 0
                diameter: 24
                iconSize: 14
                iconName: "x"
                tooltip: qsTr("Clear search")
                onClicked: {
                    searchField.text = "";
                    App.library.search = "";
                }
            }
        }

        // Busca por IA (CLIP): liga/desliga.
        IconButton {
            diameter: 28
            iconSize: 15
            iconName: "sparkles"
            visible: App.library.aiAvailable
            active: App.library.aiSearch
            tooltip: App.library.aiSearch ? qsTr("AI search on: describe what is in the photo")
                                          : qsTr("AI search: find photos by describing them")
            shortcut: Keymap.label("aiSearch")
            onClicked: App.library.aiSearch = !App.library.aiSearch
        }

        T.AbstractButton {
            id: paletteButton
            Layout.preferredHeight: 28
            implicitWidth: paletteRow.implicitWidth + 16
            hoverEnabled: true
            onClicked: header.paletteRequested()
            Accessible.name: qsTr("Search actions…")
            background: Rectangle {
                radius: Theme.radiusLg
                color: paletteButton.hovered ? Theme.muted : "transparent"
                border.color: Theme.border
            }
            contentItem: RowLayout {
                id: paletteRow
                spacing: 6
                Icon {
                    Layout.leftMargin: 8
                    name: "keyboard"
                    size: 13
                    color: Theme.mutedForeground
                }
                Keycap {
                    Layout.rightMargin: 4
                    text: Keymap.label("palette")
                }
            }
            Tip {
                text: qsTr("Search actions…")
                visible: paletteButton.hovered
            }
        }
    }

    // Direita: andamento da indexação e "Adicionar pasta".
    RowLayout {
        id: right
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        // Versão nova publicada: abre a recomendação de atualizar.
        T.AbstractButton {
            id: updateChip
            visible: App.updates.available
            implicitHeight: 28
            implicitWidth: updateRow.implicitWidth + 20
            hoverEnabled: true
            onClicked: window.recommendUpdate()
            Accessible.name: updateText.text
            background: Rectangle {
                radius: height / 2
                color: updateChip.hovered ? Theme.muted : Theme.alpha(Theme.success, 0.16)
            }
            contentItem: RowLayout {
                id: updateRow
                spacing: 6
                Icon {
                    Layout.leftMargin: 10
                    name: "download"
                    size: 14
                    color: Theme.success
                }
                Text {
                    id: updateText
                    Layout.rightMargin: 10
                    text: qsTr("Update available")
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Medium
                    color: Theme.foreground
                }
            }
            Tip {
                text: qsTr("Version %1 is available — you have %2").arg(App.updates.latestVersion).arg(App.version)
                visible: updateChip.hovered
                x: updateChip.width - implicitWidth
            }
        }

        T.AbstractButton {
            id: progress
            visible: App.tasks.pending > 0
            implicitHeight: 28
            implicitWidth: progressRow.implicitWidth + 20
            hoverEnabled: true
            onClicked: App.page = "tasks"
            background: Rectangle {
                radius: height / 2
                color: progress.hovered ? Theme.muted : Theme.brandSoft
            }
            contentItem: RowLayout {
                id: progressRow
                spacing: 6
                Spinner {
                    Layout.leftMargin: 10
                    size: 14
                    color: Theme.brandForeground
                    visible: App.tasks.busy
                }
                Text {
                    Layout.rightMargin: 10
                    text: qsTr("%n item(s) to process", "", App.tasks.pending)
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.Medium
                    color: Theme.foreground
                }
            }
            Tip {
                text: qsTr("Open the tasks")
                visible: progress.hovered
                x: progress.width - implicitWidth
            }
        }

        Button {
            id: addButton
            implicitHeight: 30
            size: "sm"
            iconName: "folder-plus"
            text: qsTr("Add folder")
            onClicked: header.addFolderRequested()
            Tip {
                text: Keymap.label("addFolder") !== "" ? qsTr("Index a folder and its subfolders") + "   " + Keymap.label("addFolder")
                                                       : qsTr("Index a folder and its subfolders")
                visible: addButton.hovered
                x: addButton.width - implicitWidth
            }
        }
    }
}

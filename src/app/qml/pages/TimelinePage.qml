pragma ComponentBehavior: Bound
import QtCore
import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Uma pessoa ao longo dos anos (App.timeline): uma foto por período (mês,
// trimestre, semestre ou ano), como as montagens das redes, exportada numa
// imagem só. A grade mostra a montagem como vai sair; clicar num período abre
// ao lado as outras fotos dele para trocar (ou tirar o período da montagem).
// As escolhas ficam no catálogo: voltar aqui mostra a mesma montagem.
Rectangle {
    id: page
    color: Theme.background

    readonly property var selected: App.timeline.selectedEntry
    readonly property bool panelOpen: App.timeline.selected !== ""

    /// Nome do arquivo sugerido (sem os caracteres que os SOs recusam).
    function suggestedName() {
        return (qsTr("%1 over the years").arg(App.timeline.personLabel)).replace(/[\\/:*?"<>|]+/g, " ").trim() + ".jpg";
    }

    Shortcut {
        sequences: Keymap.keys("clearSelection")
        enabled: page.visible && Dialogs.current === null
        onActivated: page.panelOpen ? App.timeline.selected = "" : App.page = "people"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ------------------------------------------------------------------ //
        // Cabeçalho
        // ------------------------------------------------------------------ //
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 72
            color: Theme.background
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 16
                spacing: 12
                IconButton {
                    iconName: "arrow-left"
                    tooltip: App.timeline.personLabel
                    onClicked: App.page = "people"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%1 over the years").arg(App.timeline.personLabel)
                        font.pixelSize: Theme.fontLg
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%n period(s)", "", App.timeline.entries.length) + " · "
                            + qsTr("%n in the image", "", App.timeline.shownCount)
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                        elide: Text.ElideRight
                    }
                }
                Select {
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSm
                    model: [
                        { value: "month", text: qsTr("Every month") },
                        { value: "quarter", text: qsTr("Every quarter") },
                        { value: "semester", text: qsTr("Every semester") },
                        { value: "year", text: qsTr("Every year") }
                    ]
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: model.findIndex((m) => m.value === App.timeline.frequency)
                    onActivated: App.timeline.frequency = currentValue
                }
                Select {
                    implicitHeight: 30
                    font.pixelSize: Theme.fontSm
                    model: [{ value: 0, text: qsTr("Columns: auto") }].concat(
                        [2, 3, 4, 5, 6, 8, 10].map((n) => ({ value: n, text: qsTr("%n column(s)", "", n) })))
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: Math.max(0, model.findIndex((m) => m.value === App.timeline.columns))
                    onActivated: App.timeline.columns = currentValue
                }
                Toggle {
                    label: qsTr("Labels")
                    checked: App.timeline.labels
                    onToggled: (value) => App.timeline.labels = value
                }
                Button {
                    id: resetButton
                    size: "sm"
                    variant: "ghost"
                    iconName: "rotate-ccw"
                    text: qsTr("Choose again")
                    enabled: App.timeline.entries.length > 0
                    onClicked: Dialogs.confirm({
                        title: qsTr("Choose the photos again?"),
                        description: qsTr("The photos you picked and the periods you removed go back to the automatic choice."),
                        variant: "warning",
                        iconName: "rotate-ccw",
                        confirmLabel: qsTr("Choose again")
                    }, () => App.timeline.reset())
                    Tip {
                        text: qsTr("Pick the best photo of each period automatically again")
                        visible: resetButton.hovered
                    }
                }
                Button {
                    size: "sm"
                    iconName: "download"
                    text: qsTr("Export image…")
                    loading: App.timeline.exporting
                    disabled: App.timeline.shownCount === 0 || App.timeline.exporting
                    onClicked: {
                        saveDialog.selectedFile = saveDialog.currentFolder + "/" + page.suggestedName();
                        saveDialog.open();
                    }
                }
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // -------------------------------------------------------------- //
            // A montagem
            // -------------------------------------------------------------- //
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridView {
                    id: grid
                    readonly property int columns: Math.max(1, App.timeline.effectiveColumns)
                    // A prévia tem as proporções da imagem: com poucas colunas
                    // os recortes não passam de maxCell e a grade fica no meio.
                    readonly property int maxCell: 320
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, columns * maxCell + 32)
                    leftMargin: 16
                    rightMargin: 16
                    topMargin: 16
                    bottomMargin: 16
                    cellWidth: Math.floor((width - 32) / columns)
                    cellHeight: Math.round((cellWidth - 8) * 1.2) + 8
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    T.ScrollBar.vertical: ThinScrollBar {}
                    model: App.timeline.entries
                    reuseItems: true

                    ScrollWatch {
                        id: scroll
                        target: grid
                    }

                    delegate: Item {
                        id: cell
                        required property var modelData
                        readonly property bool current: App.timeline.selected === modelData.period
                        width: grid.cellWidth
                        height: grid.cellHeight

                        Rectangle {
                            id: frame
                            anchors.fill: parent
                            anchors.margins: 4
                            color: Theme.muted
                            clip: true
                            opacity: cell.modelData.hidden ? 0.35 : 1
                            Image {
                                property string loadedUrl: ""
                                anchors.fill: parent
                                source: scroll.fast && loadedUrl !== cell.modelData.imageUrl ? "" : cell.modelData.imageUrl
                                onStatusChanged: if (status === Image.Ready) loadedUrl = String(source)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                sourceSize: Qt.size(400, 480)
                            }
                            // A faixa com o período, como sai na imagem.
                            Rectangle {
                                visible: App.timeline.labels
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: Math.round(parent.height * 0.05)
                                readonly property int px: Math.max(10, Math.round(frame.width * 0.085))
                                width: label.implicitWidth + px * 1.1
                                height: Math.round(px * 1.5)
                                radius: height * 0.22
                                color: Theme.white
                                Text {
                                    id: label
                                    anchors.centerIn: parent
                                    text: cell.modelData.label
                                    font.pixelSize: parent.px
                                    font.weight: Font.Bold
                                    color: Theme.neutral900
                                }
                            }
                        }
                        Rectangle {
                            anchors.fill: frame
                            color: "transparent"
                            border.width: cell.current ? 3 : (mouse.containsMouse ? 1 : 0)
                            border.color: cell.current ? Theme.brand500 : Theme.border
                        }
                        Badge {
                            visible: cell.modelData.hidden
                            x: frame.x + 8
                            y: frame.y + 8
                            text: qsTr("Not in the image")
                            variant: "dark"
                        }
                        Badge {
                            visible: cell.modelData.user && !cell.modelData.hidden
                            x: frame.x + 8
                            y: frame.y + 8
                            text: qsTr("Your pick")
                            variant: "dark"
                        }
                        MouseArea {
                            id: mouse
                            anchors.fill: frame
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: App.timeline.selected = cell.current ? "" : cell.modelData.period
                            onDoubleClicked: App.openPhoto(cell.modelData.photoId)
                        }
                        Tip {
                            text: cell.modelData.photoName + "\n"
                                + qsTr("%n photo(s) in this period", "", cell.modelData.candidates) + " · "
                                + qsTr("click to change the photo")
                            visible: mouse.containsMouse
                        }
                    }
                }

                Spinner {
                    anchors.centerIn: parent
                    visible: App.timeline.loading && App.timeline.entries.length === 0
                }
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 460)
                    visible: !App.timeline.loading && App.timeline.entries.length === 0
                    spacing: 8
                    Icon {
                        Layout.alignment: Qt.AlignHCenter
                        name: "history"
                        size: 32
                        color: Theme.mutedForeground
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("No dated photos of this person yet. Photos need a date (from the camera or the file) to show up here.")
                        font.pixelSize: Theme.fontSm
                        color: Theme.mutedForeground
                    }
                }
            }

            // -------------------------------------------------------------- //
            // Trocar a foto do período aberto
            // -------------------------------------------------------------- //
            Rectangle {
                visible: page.panelOpen
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.card
                Rectangle {
                    anchors.left: parent.left
                    width: 1
                    height: parent.height
                    color: Theme.border
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                Layout.fillWidth: true
                                text: page.selected.label || ""
                                font.pixelSize: Theme.fontLg
                                font.weight: Font.DemiBold
                                color: Theme.foreground
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: qsTr("%n photo(s) in this period", "", page.selected.candidates || 0)
                                font.pixelSize: Theme.fontXs
                                color: Theme.mutedForeground
                            }
                        }
                        IconButton {
                            iconName: "x"
                            tooltip: qsTr("Close")
                            onClicked: App.timeline.selected = ""
                        }
                    }

                    Toggle {
                        Layout.fillWidth: true
                        label: qsTr("Show in the image")
                        checked: !page.selected.hidden
                        onToggled: (value) => App.timeline.setHidden(page.selected.period, !value)
                    }

                    Button {
                        Layout.fillWidth: true
                        size: "sm"
                        variant: "outline"
                        iconName: "image"
                        text: qsTr("Open the chosen photo")
                        onClicked: App.openPhoto(page.selected.photoId)
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Pick another photo for this period:")
                        font.pixelSize: Theme.fontSm
                        color: Theme.mutedForeground
                        wrapMode: Text.WordWrap
                    }

                    GridView {
                        id: candidates
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        cellWidth: Math.floor(width / 3)
                        cellHeight: cellWidth
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        T.ScrollBar.vertical: ThinScrollBar {}
                        model: App.timeline.candidates
                        reuseItems: true

                        ScrollWatch {
                            id: candidateScroll
                            target: candidates
                        }

                        delegate: Item {
                            id: candidate
                            required property var modelData
                            width: candidates.cellWidth
                            height: candidates.cellHeight
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 3
                                radius: Theme.radiusLg
                                color: Theme.muted
                                clip: true
                                border.width: candidate.modelData.current ? 3 : 0
                                border.color: Theme.brand500
                                Image {
                                    property string loadedUrl: ""
                                    anchors.fill: parent
                                    anchors.margins: candidate.modelData.current ? 3 : 0
                                    source: candidateScroll.fast && loadedUrl !== candidate.modelData.cropUrl ? "" : candidate.modelData.cropUrl
                                    onStatusChanged: if (status === Image.Ready) loadedUrl = String(source)
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    sourceSize: Qt.size(220, 220)
                                }
                                SelectionCheck {
                                    visible: candidate.modelData.current
                                    x: 6
                                    y: 6
                                    checked: true
                                    color: Theme.primary
                                    border.color: Theme.primary
                                }
                            }
                            MouseArea {
                                id: candidateMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.timeline.choose(App.timeline.selected, candidate.modelData.faceId)
                                onDoubleClicked: App.openPhoto(candidate.modelData.photoId)
                            }
                            Tip {
                                text: candidate.modelData.photoName + "\n" + qsTr("Click to use this photo · double-click to open it")
                                visible: candidateMouse.containsMouse
                            }
                        }
                    }
                }
            }
        }
    }

    // Salvar a imagem: seletor de arquivo do sistema (regra 1 permite).
    FileDialog {
        id: saveDialog
        title: qsTr("Export image")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "jpg"
        nameFilters: [qsTr("JPEG image (*.jpg *.jpeg)"), qsTr("PNG image (*.png)")]
        currentFolder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        onAccepted: App.timeline.exportImage(selectedFile)
    }
}

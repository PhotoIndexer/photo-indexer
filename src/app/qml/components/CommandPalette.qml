pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Paleta de comandos (a do clivcut): busca qualquer ação pelo nome e mostra o
// atalho dela. Abre pela busca do cabeçalho ou pelo atalho (Keymap "palette").
//
//   CommandPalette {
//       commands: [{ name: qsTr("Crop"), shortcut: Keymap.label("crop"),
//                    icon: "crop", run: () => ... }, ...]
//   }
//   palette.open()
//
// O filtro ignora acentos e maiúsculas e exige TODAS as palavras digitadas
// ("cor exp" acha "Exportar capa"). ↑/↓ escolhem, Enter executa, Esc fecha.
// O comando roda depois de a paleta fechar (Qt.callLater): pode abrir outro
// diálogo sem brigar pelo foco.
T.Popup {
    id: palette

    /// [{name, shortcut, icon, run}]
    property var commands: []

    function normalize(text) {
        return String(text).toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "");
    }

    readonly property var filtered: {
        const words = normalize(field.text.trim()).split(/\s+/).filter((w) => w !== "");
        if (words.length === 0)
            return commands;
        // Casa o nome traduzido e as palavras-chave em inglês (o id da ação:
        // "crop", "export"...): quem usa o app em português acha "crop" também.
        return commands.filter((c) => {
            const text = normalize(c.name + " " + (c.keywords || ""));
            return words.every((w) => text.indexOf(w) >= 0);
        });
    }

    function runAt(index) {
        if (index < 0 || index >= filtered.length)
            return;
        const command = filtered[index];
        close();
        Qt.callLater(command.run);
    }

    parent: T.Overlay.overlay
    modal: true
    dim: true
    focus: true
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round(parent.height * 0.14) : 0
    width: parent ? Math.min(560, parent.width - 32) : 560
    height: Math.min(440, Math.max(list.contentHeight, 38) + header.implicitHeight + 1 + topPadding + bottomPadding + 8,
                     parent ? parent.height * 0.8 : 440)
    padding: 8
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    onOpened: {
        field.text = "";
        list.currentIndex = 0;
        field.forceActiveFocus();
    }

    T.Overlay.modal: Rectangle {
        color: Theme.backdrop
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.animationFast }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
    }

    background: Rectangle {
        color: Theme.popover
        radius: Theme.radiusXl
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            id: header
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 6
            Layout.bottomMargin: 4
            spacing: 10

            Icon {
                name: "search"
                size: 18
                color: Theme.mutedForeground
            }
            T.TextField {
                id: field
                Layout.fillWidth: true
                implicitHeight: 40
                verticalAlignment: TextInput.AlignVCenter
                font.pixelSize: Theme.fontMd
                color: Theme.foreground
                selectionColor: Theme.alpha(Theme.brand400, 0.45)
                selectedTextColor: Theme.foreground
                selectByMouse: true
                Accessible.name: qsTr("Search actions…")

                Text {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("What do you want to do? (e.g. add folder, people, filter)")
                    font: field.font
                    color: Theme.mutedForeground
                    elide: Text.ElideRight
                    visible: !field.length && !field.preeditText
                }

                Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
                Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
                Keys.onReturnPressed: palette.runAt(list.currentIndex)
                Keys.onEnterPressed: palette.runAt(list.currentIndex)
                onTextChanged: list.currentIndex = 0
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.bottomMargin: 6
            implicitHeight: 1
            color: Theme.border
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: Math.max(contentHeight, 38)
            clip: true
            model: palette.filtered
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool current: ListView.isCurrentItem

                width: ListView.view.width
                height: 38
                radius: Theme.radiusMd
                color: current ? Theme.brandSoft : (rowHover.hovered ? Theme.muted : "transparent")

                Accessible.role: Accessible.MenuItem
                Accessible.name: modelData.name

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 10

                    Icon {
                        name: row.modelData.icon || "chevron-right"
                        size: 16
                        color: row.current ? Theme.brandForeground : Theme.mutedForeground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        font.pixelSize: Theme.fontBase
                        color: Theme.popoverForeground
                        elide: Text.ElideRight
                    }
                    Keycap {
                        text: row.modelData.shortcut || ""
                    }
                }

                HoverHandler {
                    id: rowHover
                }
                TapHandler {
                    onTapped: palette.runAt(row.index)
                }
            }

            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No commands found")
                font.pixelSize: Theme.fontBase
                color: Theme.mutedForeground
            }
        }
    }
}

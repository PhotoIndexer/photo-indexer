pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Editor de atalhos de teclado (o ShortcutsDialog do clivcut): lista as
// ações do Keymap por grupo, filtra por nome ou tecla e grava atalhos novos.
//
//   ShortcutsDialog { id: shortcutsDialog }
//   shortcutsDialog.open()
//
// "+" entra no modo de gravação da linha: a próxima tecla (com os
// modificadores) vira atalho da ação; Esc cancela. Uma tecla que já era de
// outra ação passa para esta, e o aviso diz quem a perdeu. O × de cada tecla
// a remove; "Restaurar" volta a linha ao padrão, "Restaurar tudo" o Keymap
// inteiro. Com o diálogo aberto, os atalhos das telas ficam desligados
// (Keymap.recording).
T.Popup {
    id: dialog

    /// Ação gravando a próxima tecla ("" = nenhuma).
    property string capturing: ""
    /// Aviso da última troca (quem perdeu a tecla).
    property string notice: ""

    function normalize(text) {
        return String(text).toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "");
    }

    readonly property var filtered: {
        const q = normalize(search.text.trim());
        return Keymap.actions.filter((a) => q === ""
            || normalize(a.name).indexOf(q) >= 0
            || normalize(a.group).indexOf(q) >= 0
            || Keymap.keys(a.id).some((k) => normalize(App.shortcutText(k)).indexOf(q) >= 0
                                             || normalize(k).indexOf(q) >= 0));
    }

    function startCapture(id) {
        notice = "";
        capturing = capturing === id ? "" : id;
        if (capturing !== "")
            catcher.forceActiveFocus();
    }

    function resetAll() {
        Dialogs.confirm({
            title: qsTr("Reset all shortcuts?"),
            description: qsTr("Every action goes back to its default keys. Your changes are lost."),
            variant: "warning",
            confirmLabel: qsTr("Reset all")
        }, () => {
            Keymap.resetAll();
            dialog.notice = "";
        });
    }

    parent: T.Overlay.overlay
    modal: true
    dim: true
    focus: true
    anchors.centerIn: parent
    width: parent ? Math.min(640, parent.width - 32) : 640
    height: parent ? Math.min(680, parent.height - 48) : 680
    padding: 20
    // Gravando, o Esc é da gravação (cancela), não do diálogo.
    closePolicy: capturing !== "" ? T.Popup.CloseOnPressOutside
                                  : (T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside)

    onOpened: {
        capturing = "";
        notice = "";
        search.text = "";
        search.forceActiveFocus();
        Keymap.recording = true;
    }
    onClosed: {
        capturing = "";
        Keymap.recording = false;
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
        color: Theme.card
        radius: Theme.radius2xl
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Icon {
                name: "keyboard"
                size: 20
                color: Theme.brandForeground
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
                color: Theme.cardForeground
                elide: Text.ElideRight
            }
            Button {
                size: "sm"
                variant: "ghost"
                iconName: "rotate-ccw"
                text: qsTr("Reset all")
                disabled: Object.keys(Keymap.overrides).length === 0
                onClicked: dialog.resetAll()
            }
            IconButton {
                diameter: 32
                iconSize: 16
                iconName: "x"
                tooltip: qsTr("Close")
                onClicked: dialog.close()
            }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            iconName: "search"
            placeholderText: qsTr("Filter by action or key…")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Icon {
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 1
                name: dialog.notice !== "" ? "triangle-alert" : "info"
                size: 14
                color: dialog.notice !== "" ? Theme.warning : Theme.mutedForeground
            }
            Text {
                Layout.fillWidth: true
                text: dialog.notice !== "" ? dialog.notice
                    : qsTr("Click + to add a shortcut and press the keys. Esc cancels. Keys already in use move to the new action.")
                font.pixelSize: Theme.fontSm
                color: dialog.notice !== "" ? Theme.foreground : Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.filtered
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: ThinScrollBar {}

            delegate: Column {
                id: entry
                required property var modelData
                required property int index
                // Cabeçalho do grupo quando ele muda em relação à linha anterior.
                readonly property bool firstOfGroup: index === 0 || dialog.filtered[index - 1].group !== modelData.group
                readonly property bool editing: dialog.capturing === modelData.id
                readonly property bool custom: Keymap.overrides[modelData.id] !== undefined
                readonly property var keys: Keymap.keys(modelData.id)

                width: ListView.view.width - 10

                Text {
                    visible: entry.firstOfGroup
                    text: entry.modelData.group
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.5
                    color: Theme.brandForeground
                    topPadding: entry.index === 0 ? 0 : 12
                    bottomPadding: 4
                    leftPadding: 10
                }

                Rectangle {
                    width: entry.width
                    height: 38
                    radius: Theme.radiusMd
                    color: entry.editing ? Theme.brandSoft : (rowHover.hovered ? Theme.muted : "transparent")

                    HoverHandler {
                        id: rowHover
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 4
                        spacing: 6

                        Icon {
                            name: entry.modelData.icon || "chevron-right"
                            size: 15
                            color: entry.editing ? Theme.brandForeground : Theme.mutedForeground
                        }
                        Text {
                            Layout.fillWidth: true
                            text: entry.modelData.name
                            font.pixelSize: Theme.fontBase
                            font.weight: entry.custom ? Font.DemiBold : Font.Normal
                            color: Theme.cardForeground
                            elide: Text.ElideRight
                        }
                        Repeater {
                            model: entry.keys
                            Keycap {
                                id: chip
                                required property string modelData
                                text: App.shortcutText(chip.modelData)
                                removable: true
                                removeTooltip: qsTr("Remove shortcut")
                                onRemoveClicked: Keymap.remove(entry.modelData.id, chip.modelData)
                            }
                        }
                        Text {
                            visible: entry.editing
                            text: qsTr("Press the keys…")
                            font.pixelSize: Theme.fontSm
                            color: Theme.brandForeground
                        }
                        IconButton {
                            diameter: 28
                            iconSize: 14
                            iconName: "plus"
                            tooltip: qsTr("Add shortcut")
                            active: entry.editing
                            onClicked: dialog.startCapture(entry.modelData.id)
                        }
                        IconButton {
                            diameter: 28
                            iconSize: 14
                            iconName: "rotate-ccw"
                            tooltip: qsTr("Restore default")
                            opacity: entry.custom ? 1 : 0
                            enabled: entry.custom
                            onClicked: Keymap.reset(entry.modelData.id)
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No shortcuts found")
                font.pixelSize: Theme.fontBase
                color: Theme.mutedForeground
            }
        }
    }

    // Recebe as teclas durante a gravação. O ShortcutOverride aceito impede
    // que um Shortcut/Action da janela pegue a tecla antes.
    Item {
        id: catcher
        focus: dialog.capturing !== ""

        Keys.onShortcutOverride: (event) => {
            event.accepted = dialog.capturing !== "";
        }
        Keys.onPressed: (event) => {
            if (dialog.capturing === "")
                return;
            event.accepted = true;
            const k = event.key;
            if (k === Qt.Key_Shift || k === Qt.Key_Control || k === Qt.Key_Alt || k === Qt.Key_Meta
                    || k === Qt.Key_AltGr || k === Qt.Key_CapsLock || k === Qt.Key_unknown)
                return; // espera a tecla principal
            if (k === Qt.Key_Escape && event.modifiers === Qt.NoModifier) {
                dialog.capturing = "";
                search.forceActiveFocus();
                return;
            }
            // Keypad: o "Enter" do teclado numérico vem com KeypadModifier.
            const seq = Keymap.sequenceFor(k, event.modifiers & ~Qt.KeypadModifier);
            if (seq === "")
                return;
            const moved = Keymap.assign(dialog.capturing, seq);
            dialog.notice = moved !== ""
                ? qsTr("%1 was removed from “%2”.").arg(App.shortcutText(seq)).arg(moved) : "";
            dialog.capturing = "";
            search.forceActiveFocus();
        }
    }
}

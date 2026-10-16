import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Pede um texto curto num modal do app (regra 1: nada de diálogo do SO):
// nome de álbum, categoria, pessoa ou filtro salvo; frases do CLIP.
//
//   names.ask({ title: qsTr("New album"), label: qsTr("Album name"),
//               text: "", help: "", confirmLabel: qsTr("Create"),
//               allowEmpty: false }, (value) => ...)
Modal {
    id: dialog

    property var _callback: null
    property bool _allowEmpty: false
    property string _label: ""
    property string _help: ""

    function ask(options, callback) {
        const o = options || {};
        title = o.title || "";
        description = o.description || "";
        _label = o.label || "";
        _help = o.help || "";
        _allowEmpty = o.allowEmpty === true;
        confirm.text = o.confirmLabel || qsTr("Save");
        field.text = o.text || "";
        _callback = callback;
        open();
        field.forceActiveFocus();
        field.selectAll();
    }

    function accept() {
        const value = field.text.trim();
        if (value === "" && !_allowEmpty)
            return;
        const cb = _callback;
        _callback = null;
        close();
        if (cb)
            cb(value);
    }

    size: "md"
    showIcon: false
    textAlignment: Text.AlignLeft

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6

        FieldLabel {
            text: dialog._label
            visible: text !== ""
            buddy: field
        }
        TextField {
            id: field
            Layout.fillWidth: true
            onAccepted: dialog.accept()
            Keys.onEscapePressed: dialog.close()
        }
        Text {
            Layout.fillWidth: true
            visible: dialog._help !== ""
            text: dialog._help
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
        }
    }

    footer: [
        Button {
            text: qsTr("Cancel")
            variant: "ghost"
            onClicked: dialog.close()
        },
        Button {
            id: confirm
            disabled: field.text.trim() === "" && !dialog._allowEmpty
            onClicked: dialog.accept()
        }
    ]
}

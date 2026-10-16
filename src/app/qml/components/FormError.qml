pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Mensagem de erro — SEMPRE caixa preenchida + ícone (CLAUDE.md 2.1;
// form-error.jsx). O vermelho da marca fica a ΔE 7,6 do red-500: sem a
// forma, "confirmado" e "falhou" seriam a mesma cor; o ícone é o que fala
// com quem não distingue vermelho de verde.
//
//   FormError { Layout.fillWidth: true; text: qsTr("Could not load.") }
//   FormError { Layout.fillWidth: true; items: [error1, error2] }   // FormErrors
//
// Sem texto (ou com ``items`` todos vazios) o componente some sozinho.
Rectangle {
    id: box

    /// Uma mensagem.
    property string text: ""
    /// Várias mensagens na mesma caixa (as vazias são ignoradas).
    property var items: []

    readonly property var _lines: {
        const out = [];
        if (text)
            out.push(text);
        for (const item of (items || [])) {
            if (item && !out.includes(String(item)))
                out.push(String(item));
        }
        return out;
    }

    visible: _lines.length > 0
    implicitWidth: row.implicitWidth + 24
    implicitHeight: visible ? row.implicitHeight + 16 : 0
    radius: Theme.radiusMd
    color: Theme.errorBackground
    border.color: Theme.errorBorder

    Accessible.role: Accessible.AlertMessage
    Accessible.name: _lines.join("\n")

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 8

        Icon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            name: "triangle-alert"
            size: 14
            color: Theme.errorText
        }

        Column {
            Layout.fillWidth: true
            spacing: 2
            Repeater {
                model: box._lines
                Text {
                    required property string modelData
                    width: parent.width
                    text: modelData
                    font.pixelSize: Theme.fontSm
                    color: Theme.errorText
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Quem desenha os diálogos pedidos ao singleton ``Dialogs``. Instanciado UMA
// vez, no Main.qml; o resto do app só chama Dialogs.confirm/alert/choose.
Modal {
    id: host

    readonly property var dialog: Dialogs.current
    readonly property bool busy: Dialogs.busy
    readonly property bool requiresText: dialog !== null && !!dialog.requireText

    title: dialog ? dialog.title || "" : ""
    description: dialog ? dialog.description || "" : ""
    variant: dialog ? dialog.variant || "info" : "info"
    showIcon: true
    iconName: dialog && dialog.iconName ? dialog.iconName : ""
    size: dialog && dialog.size ? dialog.size : "sm"
    dismissible: !busy
    // Mais de duas opções: empilhadas, a principal em cima.
    footerVertical: dialog !== null && dialog.buttons.length > 2
    showContent: requiresText

    onDialogChanged: {
        confirmText.text = "";
        if (dialog && !opened)
            open();
        else if (!dialog && visible)
            close();
        if (requiresText && opened)
            confirmText.forceActiveFocus();
    }
    // O Popup toma o foco ao abrir: o campo pede depois disso.
    onOpened: {
        if (requiresText)
            confirmText.forceActiveFocus();
    }

    // Confirmação digitada (``requireText``): a frase e o campo; o botão de
    // confirmar só acende quando o texto confere (Dialogs.typedOk).
    Text {
        Layout.fillWidth: true
        visible: host.requiresText
        text: !host.requiresText ? ""
            : host.dialog.requirePrompt
                ? host.dialog.requirePrompt
                : qsTr("Type %1 to confirm.").arg("<b>" + Messages.escapeHtml(host.dialog.requireText) + "</b>")
        textFormat: Text.StyledText
        font.pixelSize: Theme.fontSm
        color: Theme.mutedForeground
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
    TextField {
        id: confirmText
        Layout.fillWidth: true
        visible: host.requiresText
        enabled: !host.busy
        horizontalAlignment: TextInput.AlignHCenter
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        Accessible.name: qsTr("Confirmation text")
        onTextChanged: Dialogs.typed = text
        onAccepted: {
            if (host.dialog && Dialogs.typedOk)
                Dialogs._press(host.dialog.actionButton);
        }
    }

    // Esc, clique fora ou X: é o "Cancelar".
    onClosed: Dialogs._dismiss()

    footer: Repeater {
        model: host.dialog ? host.dialog.buttons : []

        Button {
            id: dialogButton
            required property var modelData
            required property int index

            Layout.fillWidth: host.footerVertical
            text: dialogButton.modelData.label
            variant: dialogButton.modelData.variant || "default"
            iconName: dialogButton.modelData.iconName || ""
            loading: host.busy && dialogButton.modelData.id === Dialogs.busyButton
            disabled: (host.busy && dialogButton.modelData.id !== Dialogs.busyButton)
                || (host.dialog !== null && dialogButton.modelData.id === host.dialog.actionButton && !Dialogs.typedOk)
            focus: dialogButton.modelData.primary === true && !host.requiresText
            onClicked: Dialogs._press(dialogButton.modelData.id)

            Component.onCompleted: {
                // Com texto a digitar, o foco é do campo (Enter não confirma sozinho).
                if (dialogButton.modelData.primary === true && !host.requiresText)
                    forceActiveFocus();
            }
        }
    }
}

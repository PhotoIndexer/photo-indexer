pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// A senha das fotos privadas, num modal do app (regra 1). Quatro modos:
//
//   setup     primeira vez: mostra a chave gerada na instalação (copiar e
//             guardar!) e pede a senha que a protege
//   unlock    abre as privadas
//   recover   esqueceu a senha: a chave guardada + senha nova
//   change    troca a senha (pede a atual)
//
//   passwords.gate(() => App.library.makePrivate([]))   // pede o que faltar
//
// O PBKDF2 leva ~0,5 s (de propósito): o botão fica em espera enquanto o C++
// trabalha; o erro aparece aqui dentro (FormError), não noutro modal.
Modal {
    id: dialog

    property string mode: "unlock"
    property string error: ""
    property string keyText: ""
    property bool keySaved: false
    property var _after: null

    /// Roda ``callback`` quando as privadas estiverem abertas (pedindo a
    /// senha nova ou a senha, se precisar).
    function gate(callback) {
        if (App.privacy.unlocked) {
            if (callback)
                callback();
            return;
        }
        show(App.privacy.hasPassword ? "unlock" : "setup", callback);
    }

    function show(newMode, callback) {
        mode = newMode;
        _after = callback || null;
        error = "";
        keySaved = false;
        keyText = newMode === "setup" ? App.privacy.revealKey() : "";
        first.text = "";
        second.text = "";
        third.text = "";
        recoveryField.text = "";
        open();
        (mode === "recover" ? recoveryField : first).forceActiveFocus();
    }

    function submit() {
        if (App.privacy.working || !canSubmit)
            return;
        error = "";
        if (mode === "setup")
            App.privacy.setPassword(first.text, second.text);
        else if (mode === "unlock")
            App.privacy.unlock(first.text);
        else if (mode === "recover")
            App.privacy.recover(recoveryField.text, first.text, second.text);
        else
            App.privacy.changePassword(third.text, first.text, second.text);
    }

    readonly property bool canSubmit: {
        if (first.text === "")
            return false;
        if (mode === "setup")
            return keySaved && second.text !== "";
        if (mode === "recover")
            return recoveryField.text.trim() !== "" && second.text !== "";
        if (mode === "change")
            return third.text !== "" && second.text !== "";
        return true;
    }

    readonly property string monoFamily: Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas" : "monospace"

    title: mode === "setup" ? qsTr("Protect private photos")
         : mode === "unlock" ? qsTr("Unlock private photos")
         : mode === "recover" ? qsTr("Recover with your key")
         : qsTr("Change password")
    description: mode === "setup"
        ? qsTr("Private photos are encrypted with the key below. It was created for this computer when %1 was installed.").arg(App.brandName)
        : mode === "unlock" ? qsTr("Enter the password to see and manage private photos.")
        : mode === "recover" ? qsTr("Paste the encryption key you saved and choose a new password.")
        : qsTr("The key does not change, only the password that protects it.")
    iconName: mode === "recover" ? "key-round" : "lock"
    variant: "info"
    size: mode === "setup" ? "lg" : "md"
    textAlignment: Text.AlignLeft
    dismissible: !App.privacy.working

    Connections {
        target: App.privacy
        function onSucceeded(operation) {
            if (!dialog.opened)
                return;
            const after = dialog._after;
            dialog._after = null;
            dialog.close();
            if (after)
                after();
        }
        function onFailed(key, params) {
            if (dialog.opened)
                dialog.error = Messages.error(key, params);
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        // A chave (só na primeira vez).
        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.mode === "setup"
            spacing: 8

            FieldLabel { text: qsTr("Encryption key") }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: keyRow.implicitHeight + 16
                radius: Theme.radiusMd
                color: Theme.muted
                border.color: Theme.border
                RowLayout {
                    id: keyRow
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    TextEdit {
                        Layout.fillWidth: true
                        text: dialog.keyText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        font.family: dialog.monoFamily
                        font.pixelSize: Theme.fontBase
                        color: Theme.foreground
                        selectionColor: Theme.alpha(Theme.brand400, 0.45)
                    }
                    Button {
                        size: "sm"
                        variant: "outline"
                        iconName: "copy"
                        text: qsTr("Copy")
                        onClicked: App.privacy.copyKey()
                    }
                }
            }
            // Aviso: perder a chave e a senha = perder as fotos.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: warnRow.implicitHeight + 20
                radius: Theme.radiusMd
                color: Theme.alpha(Theme.warning, 0.12)
                border.color: Theme.alpha(Theme.warning, 0.45)
                RowLayout {
                    id: warnRow
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10
                    Icon {
                        Layout.alignment: Qt.AlignTop
                        name: "triangle-alert"
                        size: 18
                        color: Theme.warning
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Store this key somewhere safe, outside this computer (a password manager or printed). It is the only way back to your private photos if you forget the password or this computer fails — without the key and the password, the encrypted photos cannot be recovered by anyone.")
                        font.pixelSize: Theme.fontSm
                        color: Theme.foreground
                        wrapMode: Text.Wrap
                    }
                }
            }
            Toggle {
                Layout.fillWidth: true
                label: qsTr("I saved the key in a safe place")
                checked: dialog.keySaved
                onToggled: (value) => dialog.keySaved = value
            }
            Separator { Layout.fillWidth: true }
        }

        // A chave guardada (recuperação).
        FieldLabel {
            visible: dialog.mode === "recover"
            text: qsTr("Encryption key")
            buddy: recoveryField
        }
        TextField {
            id: recoveryField
            Layout.fillWidth: true
            visible: dialog.mode === "recover"
            placeholderText: "XXXX-XXXX-XXXX-…"
            font.family: dialog.monoFamily
            onAccepted: first.forceActiveFocus()
        }

        // Senha atual (trocar).
        FieldLabel {
            visible: dialog.mode === "change"
            text: qsTr("Current password")
            buddy: third
        }
        TextField {
            id: third
            Layout.fillWidth: true
            visible: dialog.mode === "change"
            echoMode: TextInput.Password
            onAccepted: first.forceActiveFocus()
        }

        FieldLabel {
            text: dialog.mode === "unlock" ? qsTr("Password") : qsTr("New password")
            buddy: first
        }
        TextField {
            id: first
            Layout.fillWidth: true
            echoMode: TextInput.Password
            onAccepted: dialog.mode === "unlock" ? dialog.submit() : second.forceActiveFocus()
            Keys.onEscapePressed: dialog.close()
        }
        FieldLabel {
            visible: dialog.mode !== "unlock"
            text: qsTr("Repeat the password")
            buddy: second
        }
        TextField {
            id: second
            Layout.fillWidth: true
            visible: dialog.mode !== "unlock"
            echoMode: TextInput.Password
            onAccepted: dialog.submit()
            Keys.onEscapePressed: dialog.close()
        }
        Text {
            Layout.fillWidth: true
            visible: dialog.mode !== "unlock"
            text: qsTr("At least %1 characters. Longer is better: a sentence you remember works well.").arg(App.privacy.minPasswordLength)
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
            wrapMode: Text.Wrap
        }

        FormError {
            Layout.fillWidth: true
            text: dialog.error
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.mode === "unlock"
            text: "<a href='#'>" + qsTr("Forgot the password? Use your encryption key") + "</a>"
            textFormat: Text.StyledText
            linkColor: Theme.brandForeground
            font.pixelSize: Theme.fontSm
            onLinkActivated: dialog.show("recover", dialog._after)
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    footer: [
        Button {
            text: qsTr("Cancel")
            variant: "ghost"
            disabled: App.privacy.working
            onClicked: dialog.close()
        },
        Button {
            iconName: dialog.mode === "unlock" ? "lock-open" : "shield-check"
            text: dialog.mode === "setup" ? qsTr("Create password")
                : dialog.mode === "unlock" ? qsTr("Unlock")
                : dialog.mode === "recover" ? qsTr("Recover")
                : qsTr("Change password")
            disabled: !dialog.canSubmit
            loading: App.privacy.working
            onClicked: dialog.submit()
        }
    ]
}

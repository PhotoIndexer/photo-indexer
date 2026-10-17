pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Fotos privadas (App.privacy): o estado do cofre, a chave de criptografia
// (ver, copiar, gerar outra), a senha e o trancamento automático. Explica,
// sem rodeio, o que fica cifrado e o que não fica.
Item {
    id: page

    /// PasswordDialog do Main.
    required property var passwords

    property bool revealed: false
    readonly property var st: App.privacy.status
    readonly property bool canSeeKey: App.privacy.unlocked || !App.privacy.hasPassword
    readonly property string monoFamily: Qt.platform.os === "osx" ? "Menlo"
                                       : Qt.platform.os === "windows" ? "Consolas" : "monospace"

    // Trancou: a chave some da tela.
    Connections {
        target: App.privacy
        function onChanged() {
            if (!page.canSeeKey)
                page.revealed = false;
        }
    }

    function maskedKey() {
        const key = App.privacy.revealKey();
        if (page.revealed && key !== "")
            return key;
        return "••••-••••-••••-••••-••••-••••-••••-••••-••••-••••-••••-••••-••••-••••";
    }

    function rotateKey() {
        Dialogs.confirm({
            title: qsTr("Generate a new key?"),
            description: qsTr("Every private photo is encrypted again with the new key (originals and thumbnails), in the background, while private photos are unlocked. When it finishes, the current key <b>stops working</b>: save the new one.")
                + "<br><br>" + qsTr("Photos on disks that are not connected now are re-encrypted when the disk comes back; until then the old key is kept."),
            variant: "warning",
            iconName: "key-round",
            confirmLabel: qsTr("Generate new key")
        }, () => {
            const key = App.privacy.rotateKey();
            if (key === "")
                return;
            page.revealed = true;
            Dialogs.choose({
                title: qsTr("Your new encryption key"),
                description: "<tt>" + key + "</tt><br><br>"
                    + qsTr("Store it somewhere safe, outside this computer. The old key no longer opens the photos once they are re-encrypted."),
                variant: "warning",
                iconName: "key-round",
                buttons: [{ id: "copy", label: qsTr("Copy key"), variant: "default", primary: true },
                          { id: "done", label: qsTr("Done"), variant: "outline" }]
            }, (id) => {
                if (id === "copy")
                    App.privacy.copyKey();
            });
        });
    }

    ScrollPage {
        anchors.fill: parent
        spacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                text: qsTr("Private photos")
                font.pixelSize: Theme.fontLg
                font.weight: Font.DemiBold
                color: Theme.foreground
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("A private photo disappears from the whole library and only shows up in the Private album, with the password. Its original file is encrypted (AES-256-GCM) into a .photoidxe file in the same folder, and its thumbnail is encrypted too.")
                font.pixelSize: Theme.fontSm
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
        }

        // Estado do cofre.
        Card {
            Layout.fillWidth: true
            padding: 18
            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: Theme.radiusLg
                    color: App.privacy.unlocked ? Theme.alpha(Theme.emerald400, 0.16) : Theme.muted
                    Icon {
                        anchors.centerIn: parent
                        name: App.privacy.unlocked ? "lock-open" : "lock"
                        size: 22
                        color: App.privacy.unlocked ? Theme.emerald600 : Theme.mutedForeground
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: !App.privacy.hasPassword ? qsTr("Not set up yet")
                            : App.privacy.unlocked ? qsTr("Unlocked") : qsTr("Locked")
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("%n private photo(s)", "", Number(page.st.photos || 0))
                            + (page.st.pending > 0 ? " · " + qsTr("%n waiting to be encrypted or decrypted", "", Number(page.st.pending)) : "")
                            + (page.st.errors > 0 ? " · " + qsTr("%n with errors (see Tasks)", "", Number(page.st.errors)) : "")
                        font.pixelSize: Theme.fontSm
                        color: Theme.mutedForeground
                        wrapMode: Text.Wrap
                    }
                }
                Button {
                    visible: App.privacy.unlocked
                    variant: "outline"
                    iconName: "book-image"
                    text: qsTr("Open the Private album")
                    onClicked: {
                        App.library.tab = "albums";
                        App.library.selectNode(App.privacy.albumNode);
                        App.page = "library";
                    }
                }
                Button {
                    iconName: !App.privacy.hasPassword ? "shield-check" : App.privacy.unlocked ? "lock" : "lock-open"
                    text: !App.privacy.hasPassword ? qsTr("Create password")
                        : App.privacy.unlocked ? qsTr("Lock now") : qsTr("Unlock")
                    onClicked: {
                        if (!App.privacy.hasPassword)
                            page.passwords.show("setup");
                        else if (App.privacy.unlocked)
                            App.privacy.lock();
                        else
                            page.passwords.show("unlock");
                    }
                }
            }
        }

        // A chave.
        Card {
            Layout.fillWidth: true
            padding: 18
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Icon {
                        name: "key-round"
                        size: 18
                        color: Theme.mutedForeground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Encryption key")
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                    }
                    Badge {
                        text: qsTr("id %1").arg(String(page.st.keyId || "").substring(0, 8))
                        variant: "muted"
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Created when %1 was installed, kept in the catalog protected by your password. With it you get your private photos back if you forget the password or move to another computer. Keep a copy outside this computer.").arg(App.brandName)
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                    wrapMode: Text.Wrap
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: keyText.implicitHeight + 18
                    radius: Theme.radiusMd
                    color: Theme.muted
                    border.color: Theme.border
                    TextEdit {
                        id: keyText
                        anchors.fill: parent
                        anchors.margins: 9
                        text: page.canSeeKey ? (App.privacy.unlocked, page.maskedKey()) : qsTr("Unlock private photos to see the key.")
                        readOnly: true
                        selectByMouse: page.revealed
                        wrapMode: TextEdit.WrapAnywhere
                        font.family: page.canSeeKey ? page.monoFamily : Theme.fontFamily
                        font.pixelSize: Theme.fontBase
                        color: page.canSeeKey ? Theme.foreground : Theme.mutedForeground
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        size: "sm"
                        variant: "outline"
                        iconName: page.revealed ? "eye-off" : "eye"
                        text: page.revealed ? qsTr("Hide") : qsTr("Show")
                        disabled: !page.canSeeKey
                        onClicked: page.revealed = !page.revealed
                    }
                    Button {
                        size: "sm"
                        variant: "outline"
                        iconName: "copy"
                        text: qsTr("Copy")
                        disabled: !page.canSeeKey
                        onClicked: App.privacy.copyKey()
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        size: "sm"
                        variant: "outline"
                        iconName: "refresh-cw"
                        text: qsTr("Generate new key…")
                        disabled: !App.privacy.unlocked
                        onClicked: page.rotateKey()
                    }
                }
            }
        }

        // Senha e trancamento automático.
        Card {
            Layout.fillWidth: true
            padding: 18
            visible: App.privacy.hasPassword
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Password")
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.DemiBold
                            color: Theme.foreground
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Protects the key in the catalog. Changing it does not re-encrypt any photo.")
                            font.pixelSize: Theme.fontSm
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                        }
                    }
                    Button {
                        size: "sm"
                        variant: "outline"
                        text: qsTr("Change password…")
                        onClicked: page.passwords.show("change")
                    }
                    Button {
                        size: "sm"
                        variant: "ghost"
                        text: qsTr("Forgot it…")
                        onClicked: page.passwords.show("recover")
                    }
                }
                Separator { Layout.fillWidth: true }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: qsTr("Lock automatically")
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.DemiBold
                            color: Theme.foreground
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("After this time without using the app. Closing the app always locks.")
                            font.pixelSize: Theme.fontSm
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                        }
                    }
                    Select {
                        implicitHeight: 30
                        font.pixelSize: Theme.fontSm
                        model: [0, 5, 10, 15, 30, 60].map((m) => ({ value: m,
                            text: m === 0 ? qsTr("Never") : qsTr("%n minute(s)", "", m) }))
                        textRole: "text"
                        valueRole: "value"
                        currentIndex: model.findIndex((m) => m.value === App.privacy.autoLockMinutes)
                        onActivated: App.privacy.autoLockMinutes = currentValue
                    }
                }
            }
        }

        // O que fica protegido e o que não fica.
        Card {
            Layout.fillWidth: true
            padding: 18
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: qsTr("What is protected")
                    font.pixelSize: Theme.fontMd
                    font.weight: Font.DemiBold
                    color: Theme.foreground
                }
                Repeater {
                    model: [
                        { icon: "lock", text: qsTr("The original file: encrypted into <b>name.photoidxe</b> in the same folder; the original is deleted only after the encrypted copy is checked (SHA-256 before and after are stored).") },
                        { icon: "lock", text: qsTr("The thumbnail in %1: encrypted.").arg(App.dataDir) },
                        { icon: "user-x", text: qsTr("Faces leave People and face search (they are detected again if the photo is made public).") },
                        { icon: "eye-off", text: qsTr("Hidden from folders, dates, categories, places, search and duplicates.") },
                        { icon: "info", text: qsTr("Not encrypted: the catalog still knows the file name, date, camera, location and EXIF of private photos, so they can be searched inside the Private album.") },
                        { icon: "triangle-alert", text: qsTr("Opening a .photoidxe file needs %1 and the key; other programs cannot read it. To get the normal file back, make the photo public.").arg(App.brandName) }
                    ]
                    RowLayout {
                        id: line
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10
                        Icon {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 1
                            name: line.modelData.icon
                            size: 15
                            color: Theme.mutedForeground
                        }
                        Text {
                            Layout.fillWidth: true
                            text: line.modelData.text
                            textFormat: Text.StyledText
                            font.pixelSize: Theme.fontSm
                            color: Theme.foreground
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }
    }
}

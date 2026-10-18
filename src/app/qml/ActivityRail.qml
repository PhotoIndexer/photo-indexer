pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC
import PhotoIDX

// Trilho vertical à esquerda (o ActivityRail do photo-editor), em todas as
// telas. Não há barra de menus:
//
//   Biblioteca  a grade com o painel lateral (pastas, datas, categorias...)
//   Pessoas     o catálogo de pessoas e a busca por rosto
//   Pastas      as pastas indexadas e os discos de origem
//   Tarefas     as etapas da indexação (com o total pendente)
//   Idioma      menu com App.languages; o atual com ✓
//   Sobre       menu: Sobre, pasta de logs, tela cheia, sair
//   (embaixo)   atalhos de teclado e tema claro/escuro
Rectangle {
    id: rail

    signal aboutRequested()
    signal shortcutsRequested()
    signal fullScreenRequested()
    signal quitRequested()

    readonly property var items: [
        { key: "library", icon: "images", label: qsTr("Library") },
        { key: "people", icon: "users", label: qsTr("People") },
        { key: "folders", icon: "hard-drive", label: qsTr("Folders") },
        { key: "tasks", icon: "activity", label: qsTr("Tasks") },
        { key: "privacy", icon: App.privacy.unlocked ? "lock-open" : "lock", label: qsTr("Private") },
        { key: "language", icon: "languages", label: qsTr("Language") },
        { key: "about", icon: "info", label: qsTr("About") }
    ]

    function isActive(key) {
        switch (key) {
        case "library": return App.page === "library" || App.page === "viewer";
        case "people": return App.page === "people" || App.page === "faceSearch" || App.page === "timeline";
        case "folders": return App.page === "folders";
        case "tasks": return App.page === "tasks";
        case "privacy": return App.page === "privacy";
        case "language": return languageMenu.visible;
        case "about": return aboutMenu.visible;
        }
        return false;
    }

    function activate(key, item) {
        switch (key) {
        case "language": languageMenu.popup(item, item.width + 4, 0); break;
        case "about": aboutMenu.popup(item, item.width + 4, 0); break;
        default: App.page = key; break;
        }
    }

    implicitWidth: 56 + 1
    color: Theme.card

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: 1
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        spacing: 2

        Repeater {
            model: rail.items

            RailItem {
                id: entry
                required property var modelData
                Layout.alignment: Qt.AlignHCenter
                iconName: modelData.icon
                label: modelData.label
                shortcut: Keymap.label(modelData.key)
                active: rail.isActive(modelData.key)
                badge: modelData.key === "tasks" && App.tasks.pending > 0 ? App.tasks.pending : 0
                busy: modelData.key === "tasks" && App.tasks.busy
                onClicked: rail.activate(modelData.key, entry)
            }
        }

        Item { Layout.fillHeight: true }

        IconButton {
            Layout.alignment: Qt.AlignHCenter
            iconName: "keyboard"
            tooltip: qsTr("Keyboard shortcuts")
            shortcut: Keymap.label("shortcuts")
            tooltipSide: "right"
            onClicked: rail.shortcutsRequested()
        }
        IconButton {
            Layout.alignment: Qt.AlignHCenter
            iconName: Theme.dark ? "sun" : "moon"
            tooltip: Theme.dark ? qsTr("Light theme") : qsTr("Dark theme")
            shortcut: Keymap.label("theme")
            tooltipSide: "right"
            onClicked: App.darkTheme = !App.darkTheme
        }
    }

    AppMenu {
        id: aboutMenu

        AppMenuItem {
            text: qsTr("About %1").arg(App.brandName)
            onTriggered: rail.aboutRequested()
        }
        AppMenuItem {
            text: qsTr("Open logs folder")
            onTriggered: App.openLogsFolder()
        }
        AppMenuSeparator {}
        AppMenuItem {
            text: qsTr("Toggle full screen")
            shortcutText: Keymap.label("fullScreen")
            onTriggered: rail.fullScreenRequested()
        }
        AppMenuItem {
            text: qsTr("Quit")
            shortcutText: Keymap.label("quit")
            onTriggered: rail.quitRequested()
        }
    }

    // Idioma: a escolha é salva pelo C++ (QSettings) e aplicada na hora.
    AppMenu {
        id: languageMenu

        Instantiator {
            model: App.languages
            delegate: AppMenuItem {
                id: languageItem
                required property var modelData
                text: languageItem.modelData.name
                checkable: true
                checked: App.language === languageItem.modelData.code
                onTriggered: App.setLanguage(languageItem.modelData.code)
            }
            onObjectAdded: (index, object) => languageMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => languageMenu.removeItem(object)
        }
    }

    // Um item do trilho: ícone + rótulo pequeno; ativo = barra à esquerda e
    // fundo suave no acento. Selo com número (pendentes) no canto.
    component RailItem: QQC.AbstractButton {
        id: button

        property string iconName: ""
        property string label: ""
        property string shortcut: ""
        property bool active: false
        property int badge: 0
        property bool busy: false

        implicitWidth: 52
        implicitHeight: 52
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        opacity: enabled ? 1 : 0.4
        Accessible.name: label

        background: Item {
            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: -2
                anchors.verticalCenter: parent.verticalCenter
                width: 3
                height: button.active ? 26 : 0
                radius: 2
                color: Theme.brand500
                Behavior on height { NumberAnimation { duration: 120 } }
            }
            Rectangle {
                anchors.centerIn: parent
                width: 46
                height: 46
                radius: Theme.radiusLg + 2
                color: button.active ? Theme.brandSoft : (button.hovered || button.down ? Theme.muted : "transparent")
                Behavior on color { ColorAnimation { duration: Theme.animationFast } }
            }
            Rectangle {
                anchors.centerIn: parent
                width: 48
                height: 48
                radius: Theme.radiusLg + 3
                color: "transparent"
                border.width: 2
                border.color: Theme.focusRing
                visible: button.visualFocus
            }
        }

        contentItem: Item {
            Column {
                anchors.centerIn: parent
                spacing: 3

                Item {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 19
                    height: 19
                    Icon {
                        anchors.centerIn: parent
                        visible: !button.busy
                        name: button.iconName
                        size: 19
                        color: button.active ? Theme.brandForeground
                             : button.hovered ? Theme.foreground : Theme.mutedForeground
                    }
                    Spinner {
                        anchors.centerIn: parent
                        visible: button.busy
                        size: 17
                        color: Theme.brand400
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(implicitWidth, 50)
                    horizontalAlignment: Text.AlignHCenter
                    text: button.label
                    font.pixelSize: Theme.fontTiny - 1
                    font.weight: button.active ? Font.DemiBold : Font.Normal
                    color: button.active ? Theme.foreground : Theme.mutedForeground
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                visible: button.badge > 0
                x: parent.width - width - 2
                y: 3
                height: 15
                width: Math.max(15, badgeText.implicitWidth + 8)
                radius: 7.5
                color: Theme.brand500
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: button.badge > 999 ? Math.round(button.badge / 1000) + "k" : button.badge
                    font.pixelSize: Theme.fontTiny - 1
                    font.weight: Font.DemiBold
                    color: Theme.white
                }
            }
        }

        Tip {
            text: button.shortcut !== "" ? button.label + "   " + button.shortcut : button.label
            visible: button.hovered
            x: button.width + 6
            y: Math.round((button.height - implicitHeight) / 2)
        }
    }
}

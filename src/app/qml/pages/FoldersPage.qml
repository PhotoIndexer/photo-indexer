pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// As pastas indexadas, agrupadas pelo disco de origem. O disco é reconhecido
// pela identidade dele (UUID do volume, GUID do Windows, servidor e
// compartilhamento de rede), não pelo caminho: se voltar em outra letra ou
// ponto de montagem, o catálogo se ajusta sozinho.
Item {
    id: page

    signal addFolderRequested()

    function diskIcon(kind) {
        return kind === "network" ? "server" : kind === "removable" ? "usb" : "hard-drive";
    }

    ScrollPage {
        anchors.fill: parent
        spacing: 20

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: qsTr("Folders and disks")
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    color: Theme.foreground
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Each folder is scanned with all its subfolders. Thumbnails stay in %1, so photos of disconnected disks remain visible.")
                        .arg(App.dataDir)
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                    wrapMode: Text.Wrap
                }
            }
            Button {
                size: "sm"
                variant: "outline"
                iconName: "refresh-cw"
                text: qsTr("Rescan all")
                disabled: App.folders.length === 0
                onClicked: App.rescanFolder("")
            }
            Button {
                size: "sm"
                variant: "outline"
                iconName: "hard-drive"
                text: qsTr("Detect disks")
                onClicked: App.refreshDisks()
            }
            Button {
                size: "sm"
                iconName: "folder-plus"
                text: qsTr("Add folder")
                onClicked: page.addFolderRequested()
            }
        }

        Text {
            visible: App.disks.length === 0
            Layout.fillWidth: true
            Layout.topMargin: 40
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No folders yet. Add one to start your catalog.")
            font.pixelSize: Theme.fontMd
            color: Theme.mutedForeground
        }

        Repeater {
            model: App.disks

            Card {
                id: diskCard
                required property var modelData
                readonly property var disk: modelData
                Layout.fillWidth: true
                padding: 18

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: Theme.radiusLg
                        color: diskCard.disk.online ? Theme.brandSoft : Theme.muted
                        Icon {
                            anchors.centerIn: parent
                            name: page.diskIcon(diskCard.disk.kind)
                            size: 20
                            color: diskCard.disk.online ? Theme.brandForeground : Theme.mutedForeground
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: diskCard.disk.label
                            font.pixelSize: Theme.fontMd
                            font.weight: Font.DemiBold
                            color: Theme.foreground
                            elide: Text.ElideRight
                        }
                        Text {
                            text: Messages.diskKind(diskCard.disk.kind)
                                  + (diskCard.disk.protocol ? " · " + diskCard.disk.protocol.toUpperCase() : "")
                                  + (diskCard.disk.totalBytes > 0 ? " · " + App.formatBytes(diskCard.disk.totalBytes) : "")
                                  + " · " + qsTr("%n photo(s)", "", diskCard.disk.photoCount)
                            font.pixelSize: Theme.fontSm
                            color: Theme.mutedForeground
                        }
                    }
                    Badge {
                        text: diskCard.disk.online ? qsTr("Online") : qsTr("Offline")
                        variant: diskCard.disk.online ? "success" : "muted"
                    }
                }

                // Identidade do disco.
                GridLayout {
                    Layout.fillWidth: true
                    columns: width > 720 ? 4 : 2
                    columnSpacing: 16
                    rowSpacing: 4
                    Repeater {
                        model: [
                            { label: qsTr("Disk ID"), value: diskCard.disk.key },
                            { label: qsTr("Mounted at"), value: diskCard.disk.mountPoint },
                            { label: qsTr("Server"), value: diskCard.disk.server },
                            { label: qsTr("Share"), value: diskCard.disk.share },
                            { label: qsTr("File system"), value: diskCard.disk.fsType },
                            { label: qsTr("Device"), value: diskCard.disk.device },
                            { label: qsTr("Serial number"), value: diskCard.disk.serial },
                            { label: qsTr("Model"), value: diskCard.disk.model }
                        ].filter((x) => x.value)
                        ColumnLayout {
                            id: idItem
                            required property var modelData
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                text: idItem.modelData.label
                                font.pixelSize: Theme.fontXs
                                color: Theme.mutedForeground
                            }
                            TextEdit {
                                Layout.fillWidth: true
                                text: idItem.modelData.value
                                readOnly: true
                                selectByMouse: true
                                wrapMode: TextEdit.WrapAnywhere
                                font.pixelSize: Theme.fontSm
                                color: Theme.foreground
                                selectionColor: Theme.alpha(Theme.brand400, 0.45)
                            }
                        }
                    }
                }

                Separator { Layout.fillWidth: true }

                // As pastas indexadas deste disco.
                Repeater {
                    model: App.folders.filter((f) => f.diskId === diskCard.disk.id)
                    RowLayout {
                        id: folderRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 10
                        Icon {
                            name: "folder-open"
                            size: 16
                            color: Theme.mutedForeground
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Text {
                                Layout.fillWidth: true
                                text: folderRow.modelData.path
                                font.pixelSize: Theme.fontBase
                                color: Theme.foreground
                                elide: Text.ElideMiddle
                            }
                            Text {
                                text: qsTr("%n photo(s)", "", folderRow.modelData.photoCount)
                                      + " · " + qsTr("%1 indexed").arg(folderRow.modelData.indexed)
                                      + (folderRow.modelData.errors > 0 ? " · " + qsTr("%1 with errors").arg(folderRow.modelData.errors) : "")
                                      + (folderRow.modelData.scannedAt ? " · " + qsTr("scanned %1").arg(Qt.formatDateTime(folderRow.modelData.scannedAt, Qt.DefaultLocaleShortDate)) : "")
                                font.pixelSize: Theme.fontXs
                                color: Theme.mutedForeground
                            }
                        }
                        IconButton {
                            diameter: 30
                            iconSize: 15
                            iconName: "images"
                            tooltip: qsTr("Show photos")
                            onClicked: {
                                App.library.tab = "folders";
                                App.library.selectNode("dir:" + folderRow.modelData.path.replace(/\\/g, "/"));
                                App.page = "library";
                            }
                        }
                        IconButton {
                            diameter: 30
                            iconSize: 15
                            iconName: "refresh-cw"
                            enabled: folderRow.modelData.online
                            tooltip: qsTr("Rescan")
                            onClicked: App.rescanFolder(folderRow.modelData.id)
                        }
                        IconButton {
                            diameter: 30
                            iconSize: 15
                            iconName: "folder-search"
                            enabled: folderRow.modelData.online
                            tooltip: qsTr("Show in file manager")
                            onClicked: App.revealInFolder(folderRow.modelData.path)
                        }
                        IconButton {
                            diameter: 30
                            iconSize: 15
                            iconName: "trash-2"
                            tooltip: qsTr("Remove from catalog")
                            onClicked: Dialogs.confirm({
                                title: qsTr("Remove folder?"),
                                description: qsTr("Remove <b>%1</b> from the catalog? Its thumbnails, categories, albums and faces are deleted from %2. The photos on disk are not touched.")
                                    .arg(Messages.escapeHtml(folderRow.modelData.path)).arg(App.brandName),
                                variant: "danger",
                                confirmLabel: qsTr("Remove")
                            }, () => App.removeFolder(folderRow.modelData.id))
                        }
                    }
                }
            }
        }
    }
}

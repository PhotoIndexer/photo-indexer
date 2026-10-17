pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// As tarefas de indexação (App.tasks): cada etapa do Pipeline com o
// andamento e os controles, a manutenção, os ajustes da IA e os erros.
Item {
    id: page

    Component.onCompleted: App.tasks.refreshErrors()

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
                    text: qsTr("Tasks")
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    color: Theme.foreground
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Each stage runs in parallel threads. Closing the app does not lose anything: the queue is the catalog itself.")
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                    wrapMode: Text.Wrap
                }
            }
            Button {
                size: "sm"
                variant: "outline"
                iconName: "play"
                text: qsTr("Start all")
                onClicked: App.tasks.startAll()
            }
            Button {
                size: "sm"
                variant: "outline"
                iconName: "pause"
                text: qsTr("Pause all")
                onClicked: App.tasks.pauseAll()
            }
            Button {
                size: "sm"
                iconName: "refresh-cw"
                text: qsTr("Rescan folders")
                onClicked: App.tasks.scanAll()
            }
        }

        // Teto de tarefas simultâneas (todas as etapas somadas).
        Card {
            Layout.fillWidth: true
            padding: 18
            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: 40
                    radius: Theme.radiusLg
                    color: Theme.muted
                    Icon {
                        anchors.centerIn: parent
                        name: "gauge"
                        size: 20
                        color: Theme.mutedForeground
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        text: qsTr("Simultaneous tasks")
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.foreground
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Limit for all stages together, so indexing never takes over the computer. Running now: %1. Image memory: %2 of %3 MB.")
                              .arg(App.tasks.runningTasks).arg(App.tasks.memory.used).arg(App.tasks.memory.capacity)
                        font.pixelSize: Theme.fontSm
                        color: Theme.mutedForeground
                        wrapMode: Text.Wrap
                    }
                }
                IconButton {
                    diameter: 26
                    iconSize: 13
                    iconName: "zoom-out"
                    enabled: App.tasks.maxTasks > 1
                    tooltip: qsTr("Fewer tasks")
                    onClicked: App.tasks.setMaxTasks(App.tasks.maxTasks - 1)
                }
                Text {
                    Layout.preferredWidth: 22
                    horizontalAlignment: Text.AlignHCenter
                    text: App.tasks.maxTasks
                    font.pixelSize: Theme.fontBase
                    font.weight: Font.Medium
                    color: Theme.foreground
                }
                IconButton {
                    diameter: 26
                    iconSize: 13
                    iconName: "zoom-in"
                    enabled: App.tasks.maxTasks < 64
                    tooltip: qsTr("More tasks")
                    onClicked: App.tasks.setMaxTasks(App.tasks.maxTasks + 1)
                }
            }
        }

        // Totais do catálogo.
        GridLayout {
            Layout.fillWidth: true
            columns: width > 900 ? 6 : 3
            columnSpacing: 12
            rowSpacing: 12
            Repeater {
                model: [
                    { label: qsTr("Photos"), value: App.tasks.stats.photos, icon: "images" },
                    { label: qsTr("RAW"), value: App.tasks.stats.raw, icon: "image" },
                    { label: qsTr("Categorized"), value: App.tasks.stats.categorized, icon: "sparkles" },
                    { label: qsTr("Faces"), value: App.tasks.stats.faces, icon: "scan-face" },
                    { label: qsTr("People"), value: App.tasks.stats.people, icon: "users" },
                    { label: qsTr("With errors"), value: App.tasks.stats.errors, icon: "circle-alert" }
                ]
                Rectangle {
                    id: stat
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 64
                    radius: Theme.radiusLg
                    color: Theme.card
                    border.color: Theme.border
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        Icon {
                            name: stat.modelData.icon
                            size: 18
                            color: Theme.mutedForeground
                        }
                        ColumnLayout {
                            spacing: 0
                            Text {
                                text: Number(stat.modelData.value || 0).toLocaleString(Qt.locale(), "f", 0)
                                font.pixelSize: Theme.fontLg
                                font.weight: Font.DemiBold
                                color: Theme.foreground
                            }
                            Text {
                                text: stat.modelData.label
                                font.pixelSize: Theme.fontXs
                                color: Theme.mutedForeground
                            }
                        }
                    }
                }
            }
        }

        // Etapas. O modelo é a QUANTIDADE de etapas (fixa): os cartões nascem
        // uma vez e só os valores mudam. Com a lista inteira como modelo, o
        // Repeater recriava todos a cada atualização (4x por segundo) e a
        // barra renascia do zero — piscando.
        Repeater {
            model: App.tasks.stages.length

            Card {
                id: stageCard
                required property int index
                readonly property var s: App.tasks.stages[index] || ({})
                readonly property bool unavailable: s.state === "unavailable"
                readonly property bool running: s.state === "running" || s.state === "idle"
                readonly property bool scanning: s.key === "search" && (s.inFlight > 0 || s.pending > 0)
                /// Fração feita, contra o catálogo (não contra a sessão): só
                /// anda para a frente. -1 = sem fração (o Searcher não sabe o
                /// total antes de terminar).
                readonly property real progress: {
                    const base = Number(s.total === undefined ? -1 : s.total);
                    if (base <= 0)
                        return -1;  // nada a fazer nesta etapa: sem barra
                    return Math.max(0, Math.min(1, 1 - (Number(s.pending || 0) + Number(s.inFlight || 0)) / base));
                }
                Layout.fillWidth: true
                padding: 18

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        radius: Theme.radiusLg
                        color: stageCard.s.state === "running" ? Theme.brandSoft : Theme.muted
                        Icon {
                            anchors.centerIn: parent
                            visible: stageCard.s.state !== "running"
                            name: Messages.stageIcon(stageCard.s.key)
                            size: 20
                            color: Theme.mutedForeground
                        }
                        Spinner {
                            anchors.centerIn: parent
                            visible: stageCard.s.state === "running"
                            size: 20
                            color: Theme.brandForeground
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        RowLayout {
                            spacing: 8
                            Text {
                                text: Messages.stageName(stageCard.s.key)
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                                color: Theme.foreground
                            }
                            Badge {
                                text: Messages.stageState(stageCard.s.state)
                                variant: stageCard.s.state === "running" ? "brand"
                                       : stageCard.s.state === "idle" ? "success" : "muted"
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Messages.stageDescription(stageCard.s.key)
                            font.pixelSize: Theme.fontSm
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                        }
                    }

                    // Threads: − n +
                    RowLayout {
                        spacing: 2
                        visible: !stageCard.unavailable
                        Text {
                            text: qsTr("Threads")
                            font.pixelSize: Theme.fontXs
                            color: Theme.mutedForeground
                            Layout.rightMargin: 4
                        }
                        IconButton {
                            diameter: 26
                            iconSize: 13
                            iconName: "zoom-out"
                            enabled: stageCard.s.threads > 1
                            tooltip: qsTr("Fewer threads")
                            onClicked: App.tasks.setThreads(stageCard.s.key, stageCard.s.threads - 1)
                        }
                        Text {
                            Layout.preferredWidth: 22
                            horizontalAlignment: Text.AlignHCenter
                            text: stageCard.s.threads
                            font.pixelSize: Theme.fontBase
                            font.weight: Font.Medium
                            color: Theme.foreground
                        }
                        IconButton {
                            diameter: 26
                            iconSize: 13
                            iconName: "zoom-in"
                            enabled: stageCard.s.threads < 64
                            tooltip: qsTr("More threads")
                            onClicked: App.tasks.setThreads(stageCard.s.key, stageCard.s.threads + 1)
                        }
                    }

                    IconButton {
                        visible: !stageCard.unavailable
                        iconName: stageCard.running ? "pause" : "play"
                        tooltip: stageCard.running ? qsTr("Pause") : qsTr("Start")
                        onClicked: stageCard.running ? App.tasks.pause(stageCard.s.key) : App.tasks.start(stageCard.s.key)
                    }
                    IconButton {
                        visible: !stageCard.unavailable
                        iconName: "square"
                        enabled: stageCard.s.inFlight > 0 || stageCard.running
                        tooltip: qsTr("Stop and clear the queue in progress")
                        onClicked: App.tasks.cancel(stageCard.s.key)
                    }
                }

                // Progresso.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 6
                    radius: 3
                    color: Theme.muted
                    visible: !stageCard.unavailable
                    clip: true
                    Rectangle {
                        visible: stageCard.progress >= 0
                        height: parent.height
                        radius: 3
                        color: stageCard.s.failed > 0 && stageCard.s.pending === 0 ? Theme.warning : Theme.brand500
                        width: parent.width * Math.max(0, stageCard.progress)
                        Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                    }
                    // Searcher varrendo: um trecho que corre (sem total ainda).
                    Rectangle {
                        id: sweep
                        visible: stageCard.scanning
                        height: parent.height
                        width: parent.width * 0.25
                        radius: 3
                        color: Theme.brand500
                        NumberAnimation on x {
                            running: sweep.visible
                            loops: Animation.Infinite
                            from: -sweep.width
                            to: sweep.parent.width
                            duration: 1400
                            easing.type: Easing.InOutQuad
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 18
                    visible: !stageCard.unavailable
                    // Também por quantidade fixa: os números trocam, os itens ficam.
                    Repeater {
                        model: stageCard.s.key === "search" ? 6 : 5
                        ColumnLayout {
                            id: num
                            required property int index
                            readonly property var modelData: {
                                const s = stageCard.s;
                                switch (index) {
                                case 0: return { label: s.key === "search" ? qsTr("Folders in queue") : qsTr("Pending"), value: s.pending };
                                case 1: return { label: qsTr("In progress"), value: s.inFlight };
                                case 2: return { label: qsTr("Done"), value: s.done };
                                case 3: return { label: qsTr("Failed"), value: s.failed };
                                case 4: return { label: qsTr("Per minute"), value: s.rate };
                                default: return { label: qsTr("Images found"), value: Number(s.detail || 0) };
                                }
                            }
                            spacing: 0
                            Text {
                                text: Number(num.modelData.value || 0).toLocaleString(Qt.locale(), "f", 0)
                                font.pixelSize: Theme.fontMd
                                font.weight: Font.DemiBold
                                color: Theme.foreground
                            }
                            Text {
                                text: num.modelData.label
                                font.pixelSize: Theme.fontXs
                                color: Theme.mutedForeground
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: stageCard.s.current
                        font.pixelSize: Theme.fontXs
                        color: Theme.mutedForeground
                        elide: Text.ElideMiddle
                    }
                }

                FormError {
                    Layout.fillWidth: true
                    visible: stageCard.unavailable
                    text: stageCard.unavailable ? qsTr("The AI model of this stage is not installed in this copy of the app.") : ""
                }

                // Manutenção da etapa.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    visible: stageCard.s.key !== "search" && !stageCard.unavailable
                    Item { Layout.fillWidth: true }
                    Button {
                        size: "sm"
                        variant: "ghost"
                        iconName: "rotate-ccw"
                        text: qsTr("Retry failed")
                        disabled: (App.tasks.stats.errors || 0) === 0
                        onClicked: App.tasks.requeue(stageCard.s.key, "errors")
                    }
                    Button {
                        size: "sm"
                        variant: "ghost"
                        iconName: "refresh-cw"
                        text: stageCard.s.key === "index" ? qsTr("Reindex everything…")
                            : stageCard.s.key === "categorize" ? qsTr("Recategorize everything…")
                            : qsTr("Detect faces again…")
                        onClicked: Dialogs.confirm({
                            title: qsTr("Redo “%1” for every photo?").arg(Messages.stageName(stageCard.s.key)),
                            description: stageCard.s.key === "faces"
                                ? qsTr("Faces are detected again in every photo. Faces you confirmed by hand are replaced by the new detections.")
                                : qsTr("Every photo goes back to the queue of this stage. It may take a while on a large catalog."),
                            variant: "warning",
                            confirmLabel: qsTr("Redo")
                        }, () => App.tasks.requeue(stageCard.s.key, "all"))
                    }
                    Button {
                        visible: stageCard.s.key === "faces"
                        size: "sm"
                        variant: "ghost"
                        iconName: "users"
                        text: qsTr("Regroup people")
                        onClicked: App.tasks.reclusterPeople()
                    }
                    Button {
                        visible: stageCard.s.key === "faces"
                        size: "sm"
                        variant: "ghost"
                        iconName: "merge"
                        text: qsTr("Merge similar people now")
                        onClicked: App.tasks.mergePeopleNow()
                    }
                }
            }
        }

        // Ajustes da IA.
        Card {
            Layout.fillWidth: true
            title: qsTr("AI settings")
            description: qsTr("Changes to the categories apply to photos categorized from now on; use “Recategorize everything” to apply them to the whole catalog (fast: the AI reuses what it already computed).")

            GridLayout {
                Layout.fillWidth: true
                columns: width > 820 ? 2 : 1
                columnSpacing: 32
                rowSpacing: 18
                Repeater {
                    model: [
                        { key: "minProbability", from: 0.1, to: 0.9, step: 0.01, decimals: 2 },
                        { key: "maxCategories", from: 1, to: 5, step: 1, decimals: 0 },
                        { key: "parentProbability", from: 0.2, to: 0.95, step: 0.01, decimals: 2 },
                        { key: "siblingMargin", from: 0.2, to: 1, step: 0.05, decimals: 2 },
                        { key: "blurThreshold", from: 0, to: 300, step: 5, decimals: 0 },
                        { key: "minPersonScore", from: 0.35, to: 0.9, step: 0.01, decimals: 2 },
                        { key: "matchSimilarity", from: 0.25, to: 0.8, step: 0.01, decimals: 2 },
                        { key: "mergeSimilarity", from: 0.3, to: 0.9, step: 0.01, decimals: 2 },
                        { key: "mergeMinFaces", from: 1, to: 20, step: 1, decimals: 0 },
                        { key: "mergeMinScore", from: 0.35, to: 0.95, step: 0.01, decimals: 2 }
                    ]
                    ColumnLayout {
                        id: setting
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4
                        ValueSlider {
                            Layout.fillWidth: true
                            label: Messages.settingName(setting.modelData.key)
                            from: setting.modelData.from
                            to: setting.modelData.to
                            stepSize: setting.modelData.step
                            decimals: setting.modelData.decimals
                            showSign: false
                            defaultValue: value
                            value: App.tasks.settings[setting.modelData.key] || 0
                            onReleased: App.tasks.setSetting(setting.modelData.key, value)
                            onMoved: (v) => value = v
                        }
                        Text {
                            Layout.fillWidth: true
                            text: Messages.settingHelp(setting.modelData.key)
                            font.pixelSize: Theme.fontXs
                            color: Theme.mutedForeground
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        // Erros.
        Card {
            Layout.fillWidth: true
            title: qsTr("Errors")
            description: App.tasks.errors.length === 0 ? qsTr("No photo with errors.")
                                                       : qsTr("Files that a stage could not process (damaged, unsupported or unreadable).")

            RowLayout {
                Layout.fillWidth: true
                visible: App.tasks.errors.length > 0
                Item { Layout.fillWidth: true }
                Button {
                    size: "sm"
                    variant: "ghost"
                    iconName: "images"
                    text: qsTr("Show in the library")
                    onClicked: {
                        App.library.selectNode("errors");
                        App.page = "library";
                    }
                }
                Button {
                    size: "sm"
                    variant: "ghost"
                    iconName: "refresh-cw"
                    text: qsTr("Refresh")
                    onClicked: App.tasks.refreshErrors()
                }
            }

            Repeater {
                model: App.tasks.errors.slice(0, 100)
                RowLayout {
                    id: err
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 10
                    Badge {
                        text: Messages.stageName(err.modelData.stage)
                        variant: "muted"
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            Layout.fillWidth: true
                            text: err.modelData.path
                            font.pixelSize: Theme.fontSm
                            color: Theme.foreground
                            elide: Text.ElideMiddle
                        }
                        Text {
                            Layout.fillWidth: true
                            text: err.modelData.error
                            font.pixelSize: Theme.fontXs
                            color: Theme.danger
                            elide: Text.ElideRight
                        }
                    }
                    IconButton {
                        diameter: 28
                        iconSize: 14
                        iconName: "external-link"
                        tooltip: qsTr("Open")
                        onClicked: App.openPhoto(err.modelData.id)
                    }
                }
            }
        }
    }
}

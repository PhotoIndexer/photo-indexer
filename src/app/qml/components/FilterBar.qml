pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import PhotoIDX

// Filtro personalizado da grade (App.library.rules): condições sobre qualquer
// campo do catálogo — arquivo, imagem, datas, câmera, local, organização e
// qualquer tag EXIF — encadeadas com "todas" (E) ou "qualquer" (OU). Junta-se
// ao nó da árvore e à busca. Pode ser salvo com um nome.
Rectangle {
    id: bar

    /// NameDialog do Main (nome do filtro salvo).
    required property var names

    readonly property var fields: App.library.filterFields
    readonly property var groups: ["file", "image", "date", "camera", "location", "organize", "exif"]

    function field(key) {
        return fields.find((f) => f.key === key) || null;
    }

    function choicesFor(type) {
        switch (type) {
        case "category": return App.library.categoryChoices;
        case "album": return App.library.albumChoices;
        case "person": return App.library.personChoices;
        }
        return [];
    }

    implicitHeight: content.implicitHeight + 16
    color: Theme.card

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    ColumnLayout {
        id: content
        x: 12
        y: 8
        width: parent.width - 24
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Icon {
                name: "sliders-horizontal"
                size: 14
                color: Theme.mutedForeground
            }
            Text {
                text: qsTr("Show photos that match")
                font.pixelSize: Theme.fontSm
                color: Theme.foreground
            }
            Select {
                implicitHeight: 28
                font.pixelSize: Theme.fontSm
                model: [{ value: true, text: qsTr("all conditions") }, { value: false, text: qsTr("any condition") }]
                textRole: "text"
                valueRole: "value"
                currentIndex: App.library.matchAll ? 0 : 1
                onActivated: App.library.matchAll = currentValue
            }

            Item { Layout.fillWidth: true }

            Button {
                id: addButton
                size: "sm"
                implicitHeight: 28
                variant: "outline"
                iconName: "plus"
                text: qsTr("Add condition")
                onClicked: fieldMenu.popup(addButton, 0, addButton.height + 4)
            }
            Button {
                id: savedButton
                size: "sm"
                implicitHeight: 28
                variant: "ghost"
                iconName: "chevron-down"
                text: qsTr("Saved filters")
                onClicked: savedMenu.popup(savedButton, 0, savedButton.height + 4)
            }
            Button {
                size: "sm"
                implicitHeight: 28
                variant: "ghost"
                iconName: "check"
                text: qsTr("Save…")
                disabled: App.library.rules.length === 0
                onClicked: bar.names.ask({ title: qsTr("Save filter"), label: qsTr("Filter name"), confirmLabel: qsTr("Save") },
                                         (name) => App.library.saveFilter(name))
            }
            IconButton {
                diameter: 28
                iconSize: 15
                iconName: "x"
                tooltip: qsTr("Remove all conditions")
                enabled: App.library.rules.length > 0
                onClicked: App.library.clearRules()
            }
        }

        Text {
            visible: App.library.rules.length === 0
            text: qsTr("No conditions yet. Add one to filter by size, dimensions, camera, place, date, EXIF…")
            font.pixelSize: Theme.fontSm
            color: Theme.mutedForeground
        }

        Repeater {
            model: App.library.rules

            RowLayout {
                id: ruleRow

                required property var modelData
                required property int index

                readonly property var meta: bar.field(modelData.field) || ({ type: "text", ops: [] })
                readonly property bool takesValue: Messages.opTakesValue(modelData.op)
                readonly property bool between: modelData.op === "between"

                function update(change) {
                    App.library.updateRule(ruleRow.index, change);
                }

                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.preferredWidth: 22
                    horizontalAlignment: Text.AlignRight
                    text: ruleRow.index === 0 ? "" : (App.library.matchAll ? qsTr("and") : qsTr("or"))
                    font.pixelSize: Theme.fontXs
                    color: Theme.mutedForeground
                }

                Select {
                    implicitHeight: 28
                    Layout.preferredWidth: 200
                    font.pixelSize: Theme.fontSm
                    model: bar.fields.map((f) => ({ value: f.key, text: Messages.fieldName(f.key) }))
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: bar.fields.findIndex((f) => f.key === ruleRow.modelData.field)
                    onActivated: {
                        const f = bar.field(currentValue);
                        ruleRow.update({ field: currentValue, op: f.ops[0], value: "", value2: "" });
                    }
                }

                Select {
                    implicitHeight: 28
                    Layout.preferredWidth: 150
                    font.pixelSize: Theme.fontSm
                    model: ruleRow.meta.ops.map((o) => ({ value: o, text: Messages.opName(o) }))
                    textRole: "text"
                    valueRole: "value"
                    currentIndex: ruleRow.meta.ops.indexOf(ruleRow.modelData.op)
                    onActivated: ruleRow.update({ op: currentValue })
                }

                // Tag EXIF: o nome da tag (e o valor, se o operador pede).
                TextField {
                    visible: ruleRow.meta.type === "exif"
                    implicitHeight: 28
                    Layout.preferredWidth: 170
                    font.pixelSize: Theme.fontSm
                    placeholderText: qsTr("Tag, e.g. LensModel")
                    text: ruleRow.modelData.value || ""
                    onTextEdited: ruleRow.update({ value: text })
                }

                // Escolha numa lista: enum, categoria, álbum, pessoa.
                Select {
                    readonly property var options: ruleRow.meta.type === "enum"
                        ? (ruleRow.meta.choices || []).map((c) => ({ value: c, text: Messages.choiceName(c) }))
                        : bar.choicesFor(ruleRow.meta.type).map((c) => ({ value: c.id, text: "  ".repeat(c.depth || 0) + c.name }))
                    visible: ruleRow.takesValue && ["enum", "category", "album", "person"].indexOf(ruleRow.meta.type) >= 0
                    implicitHeight: 28
                    Layout.preferredWidth: 220
                    font.pixelSize: Theme.fontSm
                    model: options
                    textRole: "text"
                    valueRole: "value"
                    displayText: currentIndex < 0 ? qsTr("Choose…") : currentText
                    currentIndex: options.findIndex((o) => String(o.value) === String(ruleRow.modelData.value))
                    onActivated: ruleRow.update({ value: currentValue })
                }

                // Texto, número, data (um ou dois valores).
                TextField {
                    visible: ruleRow.takesValue && ["text", "number", "date"].indexOf(ruleRow.meta.type) >= 0
                             || (ruleRow.meta.type === "exif" && ["eq", "contains"].indexOf(ruleRow.modelData.op) >= 0)
                    implicitHeight: 28
                    Layout.preferredWidth: ruleRow.between ? 110 : 200
                    font.pixelSize: Theme.fontSm
                    placeholderText: ruleRow.meta.type === "date" ? qsTr("YYYY-MM-DD")
                                   : ruleRow.meta.type === "number" ? qsTr("Number") : qsTr("Text")
                    text: ruleRow.meta.type === "exif" ? (ruleRow.modelData.value2 || "") : (ruleRow.modelData.value || "")
                    inputMethodHints: ruleRow.meta.type === "number" ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
                    onTextEdited: ruleRow.meta.type === "exif" ? ruleRow.update({ value2: text }) : ruleRow.update({ value: text })
                }
                Text {
                    visible: ruleRow.between
                    text: qsTr("and")
                    font.pixelSize: Theme.fontSm
                    color: Theme.mutedForeground
                }
                TextField {
                    visible: ruleRow.between
                    implicitHeight: 28
                    Layout.preferredWidth: 110
                    font.pixelSize: Theme.fontSm
                    placeholderText: ruleRow.meta.type === "date" ? qsTr("YYYY-MM-DD") : qsTr("Number")
                    text: ruleRow.modelData.value2 || ""
                    onTextEdited: ruleRow.update({ value2: text })
                }

                Item { Layout.fillWidth: true }

                IconButton {
                    diameter: 26
                    iconSize: 14
                    iconName: "trash-2"
                    tooltip: qsTr("Remove this condition")
                    onClicked: App.library.removeRule(ruleRow.index)
                }
            }
        }
    }

    // Campos agrupados (Arquivo, Imagem, Datas, Câmera, Local, Organização, EXIF).
    AppMenu {
        id: fieldMenu

        Instantiator {
            model: bar.groups
            delegate: AppMenu {
                id: groupMenu
                required property string modelData
                title: Messages.fieldGroup(modelData)

                Instantiator {
                    model: bar.fields.filter((f) => f.group === groupMenu.modelData)
                    delegate: AppMenuItem {
                        required property var modelData
                        text: Messages.fieldName(modelData.key)
                        onTriggered: App.library.addRule(modelData.key)
                    }
                    onObjectAdded: (index, object) => groupMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => groupMenu.removeItem(object)
                }
            }
            onObjectAdded: (index, object) => fieldMenu.insertMenu(index, object)
            onObjectRemoved: (index, object) => fieldMenu.removeMenu(object)
        }
    }

    AppMenu {
        id: savedMenu

        Instantiator {
            model: App.library.savedFilters.length > 0 ? App.library.savedFilters : [null]
            delegate: AppMenu {
                id: savedItem
                required property var modelData
                title: savedItem.modelData ? savedItem.modelData.name : qsTr("No saved filters")
                enabled: savedItem.modelData !== null

                AppMenuItem {
                    text: qsTr("Apply")
                    onTriggered: App.library.applySavedFilter(savedItem.modelData.id)
                }
                AppMenuItem {
                    text: qsTr("Delete")
                    onTriggered: App.library.deleteSavedFilter(savedItem.modelData.id)
                }
            }
            onObjectAdded: (index, object) => savedMenu.insertMenu(index, object)
            onObjectRemoved: (index, object) => savedMenu.removeMenu(object)
        }
    }
}

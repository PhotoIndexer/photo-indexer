pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import PhotoIDX

// Busca por rosto: a partir de uma foto escolhida no disco (ou de um rosto
// já catalogado), as fotos em que a pessoa aparece — o mesmo cosseno do
// ../photoe (ArcFace, limiar 0,45), contra todas as faces do catálogo.
// Clicar em quem pode ser abre a pessoa À DIREITA (PersonDetail), com a
// busca intacta à esquerda: dá para ir juntando, nomeando e tirando faces
// candidato por candidato.
Item {
    id: page

    required property var names

    signal chooseFileRequested()

    /// Uma pessoa da lista aberta à direita (no lugar das fotos encontradas).
    property bool showPerson: false
    readonly property bool personOpen: showPerson && Object.keys(App.people.person).length > 0

    // Outra foto: volta para as fotos encontradas.
    readonly property string query: App.people.queryUrl
    onQueryChanged: showPerson = false

    // Esquerda: a foto, os rostos dela, o limiar e as pessoas que casam.
    Rectangle {
        id: side
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 320
        color: Theme.card

        ScrollPage {
            anchors.fill: parent
            padding: 16
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                IconButton {
                    iconName: "arrow-left"
                    tooltip: qsTr("People")
                    onClicked: {
                        if (page.personOpen)
                            App.people.closePerson();
                        page.showPerson = false;
                        App.page = "people";
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: App.people.queryPersonId !== "" ? qsTr("Faces similar to %1").arg(App.people.queryPersonLabel)
                                                          : qsTr("Search by face")
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    color: Theme.foreground
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                radius: Theme.radiusLg
                color: Theme.imagePane
                clip: true
                Image {
                    anchors.fill: parent
                    source: App.people.queryUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    sourceSize: Qt.size(640, 640)
                    autoTransform: true
                }
                Spinner {
                    anchors.centerIn: parent
                    size: 26
                    color: Theme.white
                    visible: App.people.detecting
                }
            }

            Button {
                Layout.fillWidth: true
                variant: "outline"
                iconName: "upload"
                text: qsTr("Choose another photo…")
                onClicked: page.chooseFileRequested()
            }

            FormError {
                Layout.fillWidth: true
                text: App.people.searchError !== "" ? Messages.error(App.people.searchError, {}) : ""
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: App.people.queryFaces.length > 1
                spacing: 8
                SectionTitle { text: qsTr("Faces in this photo — pick one") }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: App.people.queryFaces
                        Rectangle {
                            id: qf
                            required property var modelData
                            width: 64
                            height: 64
                            radius: 32
                            clip: true
                            color: Theme.muted
                            border.width: App.people.queryFace === modelData.index ? 3 : 0
                            border.color: Theme.brand400
                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                source: qf.modelData.cropUrl
                                fillMode: Image.PreserveAspectCrop
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.people.queryFace = qf.modelData.index
                            }
                        }
                    }
                }
            }

            ValueSlider {
                Layout.fillWidth: true
                label: qsTr("Minimum similarity")
                from: 0.2
                to: 0.9
                stepSize: 0.01
                decimals: 2
                showSign: false
                defaultValue: 0.45
                value: App.people.threshold
                onMoved: (v) => App.people.threshold = v
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Lower finds more photos (and more mistakes); higher, fewer and surer.")
                font.pixelSize: Theme.fontXs
                color: Theme.mutedForeground
                wrapMode: Text.Wrap
            }
            // Só quem ainda não tem nome: o que falta atribuir (e o que o
            // "Juntar todas" junta).
            Toggle {
                Layout.fillWidth: true
                label: qsTr("Unnamed people only")
                checked: App.people.unnamedOnly
                onToggled: (value) => App.people.unnamedOnly = value
            }
            Button {
                visible: App.people.hiddenCount > 0
                size: "sm"
                variant: "ghost"
                iconName: "eye"
                text: qsTr("Show removed (%1)").arg(App.people.hiddenCount)
                onClicked: App.people.showHidden()
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: App.people.matchedPeople.length > 0
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    SectionTitle {
                        Layout.fillWidth: true
                        text: qsTr("Who it may be")
                    }
                    // Busca por pessoa: juntar de uma vez todas as sem nome da lista.
                    Button {
                        readonly property int count: App.people.matchedPeople.filter((m) => !m.named).length
                        visible: App.people.queryPersonId !== "" && count > 0
                        size: "sm"
                        variant: "outline"
                        iconName: "merge"
                        text: qsTr("Merge all (%1)").arg(count)
                        onClicked: Dialogs.confirm({
                            title: qsTr("Merge all into %1?").arg(App.people.queryPersonLabel),
                            description: qsTr("The %n unnamed person(s) in this list become <b>%1</b>. People with a name are left out. Check the list first: raise the minimum similarity to leave out doubtful ones.", "", count)
                                .arg(Messages.escapeHtml(App.people.queryPersonLabel)),
                            variant: "warning",
                            confirmLabel: qsTr("Merge all")
                        }, () => {
                            page.showPerson = false;
                            App.people.mergeAllMatched();
                        })
                    }
                }
                Repeater {
                    model: App.people.matchedPeople
                    T.ItemDelegate {
                        id: who
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 48
                        hoverEnabled: true
                        readonly property bool current: page.personOpen && App.people.personId === modelData.id
                        onClicked: {
                            App.people.openPerson(modelData.id);
                            page.showPerson = true;
                        }
                        // Botão direito: tirar da lista (e do "Juntar todas").
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: {
                                matchMenu.personId = who.modelData.id;
                                matchMenu.popup(who, who.width / 2, who.height / 2);
                            }
                        }
                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: who.current ? Theme.accent : who.hovered ? Theme.muted : "transparent"
                            border.width: who.current ? 1 : 0
                            border.color: Theme.brand400
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: 18
                                clip: true
                                color: Theme.muted
                                Image {
                                    anchors.fill: parent
                                    source: who.modelData.coverUrl
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                Text {
                                    Layout.fillWidth: true
                                    text: who.modelData.label
                                    font.pixelSize: Theme.fontBase
                                    color: Theme.foreground
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: qsTr("%1% similar · %n face(s)", "", who.modelData.faces)
                                        .arg(Math.round(who.modelData.similarity * 100))
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.mutedForeground
                                }
                            }
                            // Busca a partir de uma pessoa: juntar o candidato
                            // nela sem sair da lista.
                            IconButton {
                                visible: App.people.queryPersonId !== ""
                                diameter: 28
                                iconSize: 14
                                iconName: "merge"
                                tooltip: qsTr("Merge into %1").arg(App.people.queryPersonLabel)
                                onClicked: Dialogs.confirm({
                                    title: qsTr("Merge people?"),
                                    description: qsTr("All faces of <b>%1</b> go to <b>%2</b>.")
                                        .arg(Messages.escapeHtml(who.modelData.label)).arg(Messages.escapeHtml(App.people.queryPersonLabel)),
                                    variant: "warning",
                                    confirmLabel: qsTr("Merge")
                                }, () => {
                                    if (page.personOpen && App.people.personId === who.modelData.id)
                                        page.showPerson = false;
                                    App.people.merge(App.people.queryPersonId, who.modelData.id);
                                })
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.border
        }
    }

    // Direita: a pessoa escolhida na lista…
    PersonDetail {
        anchors.left: side.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: page.personOpen
        names: page.names
        backTooltip: qsTr("Back to the photos found")
        onBack: {
            App.people.closePerson();
            page.showPerson = false;
        }
    }

    // …ou as fotos encontradas.
    ColumnLayout {
        visible: !page.personOpen
        anchors.left: side.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            color: Theme.background
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: App.people.detecting ? qsTr("Looking for faces…") : qsTr("%n photo(s) found", "", App.people.results.count)
                font.pixelSize: Theme.fontMd
                font.weight: Font.DemiBold
                color: Theme.foreground
            }
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        PhotoGrid {
            Layout.fillWidth: true
            Layout.fillHeight: true
            photos: App.people.results
            showScore: true
            onOpened: (photoId) => App.openPhoto(photoId)
        }
    }

    // Menu de um candidato de "quem pode ser".
    AppMenu {
        id: matchMenu
        property string personId: ""
        AppMenuItem {
            text: qsTr("Open")
            onTriggered: {
                App.people.openPerson(matchMenu.personId);
                page.showPerson = true;
            }
        }
        AppMenuItem {
            text: qsTr("Remove from list")
            onTriggered: {
                if (page.personOpen && App.people.personId === matchMenu.personId)
                    page.showPerson = false;
                App.people.hideMatch(matchMenu.personId);
            }
        }
    }
}

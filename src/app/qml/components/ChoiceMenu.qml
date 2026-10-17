pragma ComponentBehavior: Bound
import QtQuick
import PhotoIDX

// Menu de escolha a partir de uma lista [{id, name, depth}] (álbuns,
// categorias, pessoas): a profundidade vira recuo. Vazio, mostra ``emptyText``.
//
//   ChoiceMenu {
//       title: qsTr("Add to album")
//       choices: App.library.albumChoices
//       onPicked: (id) => App.library.addToAlbum([], id)
//   }
AppMenu {
    id: menu

    property var choices: []
    property string emptyText: qsTr("Nothing here yet")

    signal picked(string id)

    // Os itens só existem com o menu aberto: fechado, uma lista de milhares
    // (pessoas) não cria nada nem se refaz a cada mudança dela.
    property bool _built: false
    onAboutToShow: _built = true
    onClosed: _built = false

    Instantiator {
        active: menu._built
        model: menu.choices.length > 0 ? menu.choices : [null]
        delegate: AppMenuItem {
            id: item
            required property var modelData
            text: item.modelData ? "    ".repeat(item.modelData.depth || 0) + item.modelData.name : menu.emptyText
            enabled: item.modelData !== null
            onTriggered: menu.picked(item.modelData.id)
        }
        onObjectAdded: (index, object) => menu.insertItem(index, object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}

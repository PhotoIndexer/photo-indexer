import QtQuick
import PhotoIDX

// Marca de seleção de uma linha de lista: círculo de 20px que recebe o ✓
// (CLAUDE.md 2.3). NÃO é o Toggle: diz "esta linha está no conjunto que vou
// operar agora". Só desenha — o clique e o Accessible.checked vão na LINHA.
//
//   SelectionCheck { checked: picked.includes(model.id) }
Rectangle {
    id: check

    property bool checked: false

    implicitWidth: 20
    implicitHeight: 20
    radius: 10
    color: checked ? Theme.primary : "transparent"
    border.width: 1
    border.color: checked ? Theme.primary : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.animationFast } }

    Icon {
        anchors.centerIn: parent
        visible: check.checked
        name: "check"
        size: 12
        color: Theme.primaryForeground
    }
}

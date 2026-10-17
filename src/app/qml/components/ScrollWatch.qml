import QtQuick

// Rolagem rápida numa lista/grade (barra arrastada, arremesso forte): as
// imagens das células que só passam não são pedidas; quando a rolagem para
// (120 ms parada), só as visíveis carregam. Sem isso, varrer milhares de
// fotos enfileirava milhares de leituras de arquivo.
//
//   ScrollWatch { id: watch; target: grid }
//   Image { source: watch.fast && loadedUrl !== url ? "" : url }   // ver PhotoGrid
QtObject {
    id: watch

    property Flickable target: null
    /// Mais rápido que isto (px/ms) conta como "só passando".
    property real threshold: 3
    readonly property bool fast: _fast

    property bool _fast: false
    property real _lastY: 0
    property double _lastT: 0

    property Timer _settle: Timer {
        interval: 120
        onTriggered: watch._fast = false
    }

    property Connections _scroll: Connections {
        target: watch.target
        function onContentYChanged() {
            const now = Date.now();
            const speed = Math.abs(watch.target.contentY - watch._lastY) / Math.max(1, now - watch._lastT);
            watch._lastY = watch.target.contentY;
            watch._lastT = now;
            if (speed > watch.threshold)
                watch._fast = true;
            watch._settle.restart();
        }
    }
}

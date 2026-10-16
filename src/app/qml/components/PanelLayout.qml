import QtQuick
import QtCore

// Tamanhos dos painéis do editor, lembrados entre sessões (o
// usePanelLayout.js do React; antes no localStorage, agora no QSettings).
//
//   top    fração da altura para a área de cima (fotos + painel); o resto
//          é da filmstrip. Padrão 0,70.
//   split  fração da largura das fotos que fica com a Original.
//   panel  largura do painel de edição, em px.
//
//   PanelLayout { id: panels }
//   Splitter {
//       mode: "fraction"; container: content
//       value: panels.top; from: panels.limits.top[0]; to: panels.limits.top[1]
//       stepSize: panels.steps.top; defaultValue: panels.defaults.top
//       onMoved: (v) => panels.set("top", v)
//   }
QtObject {
    id: layout

    property real top: defaults.top
    property real split: defaults.split
    property real panel: defaults.panel

    readonly property var defaults: ({ top: 0.7, split: 0.5, panel: 320 })
    readonly property var limits: ({ top: [0.35, 0.85], split: [0.2, 0.8], panel: [240, 560] })
    /// Passo das setas do teclado sobre a divisória.
    readonly property var steps: ({ top: 0.02, split: 0.02, panel: 16 })

    function clamp(key, value) {
        const l = limits[key];
        if (typeof value !== "number" || !isFinite(value))
            return defaults[key];
        return Math.min(l[1], Math.max(l[0], value));
    }
    function set(key, value) {
        layout[key] = clamp(key, value);
    }
    function step(key, direction) {
        set(key, layout[key] + direction * steps[key]);
    }
    function reset(key) {
        layout[key] = defaults[key];
    }

    property Settings store: Settings {
        category: "editorLayout"
        property alias top: layout.top
        property alias split: layout.split
        property alias panel: layout.panel
    }

    Component.onCompleted: {
        // Valor salvo fora dos limites (versão antiga, edição à mão) volta ao intervalo.
        for (const key of ["top", "split", "panel"])
            layout[key] = clamp(key, Number(layout[key]));
    }
}

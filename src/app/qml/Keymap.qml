pragma Singleton
import QtQuick
import QtCore
import PhotoIDX

// Registro dos atalhos de teclado (o Keymap do clivcut): o padrão de cada ação
// e as trocas da pessoa, salvas nas preferências (QSettings, "shortcuts").
// Quem dispara continua sendo cada tela (Shortcut/Action com as condições
// dela); daqui sai só a SEQUÊNCIA, então trocar um atalho vale na hora.
//
//   Shortcut { sequences: Keymap.keys("nextPhoto"); onActivated: ... }
//   Text { text: Keymap.label("palette") }          // "Ctrl+K" / "⌘K"
//   Keymap.assign("nextPhoto", "N")                 // tira o N de quem o usava
//
// Sequências no formato portável do QKeySequence ("Ctrl+Shift+Z"; no macOS o
// Ctrl é o ⌘). Para mostrar: App.shortcutText(seq); para comparar:
// App.sameShortcut(a, b).
QtObject {
    id: keymap

    /// {idDaAção: ["Ctrl+B", ...]} — só o que difere do padrão.
    property string custom: "{}"

    property Settings store: Settings {
        category: "shortcuts"
        property alias custom: keymap.custom
    }

    readonly property var overrides: {
        try {
            const o = JSON.parse(keymap.custom);
            return o && typeof o === "object" ? o : {};
        } catch (e) {
            return {};
        }
    }

    /// Gravando um atalho no editor de atalhos: as telas desligam os seus.
    property bool recording: false

    readonly property bool _mac: Qt.platform.os === "osx"

    // Ordem = ordem dos grupos e das linhas no editor de atalhos.
    readonly property var actions: [
        { id: "library", group: qsTr("Screens"), name: qsTr("Library"), icon: "images", keys: ["Ctrl+1"] },
        { id: "people", group: qsTr("Screens"), name: qsTr("People"), icon: "users", keys: ["Ctrl+2"] },
        { id: "folders", group: qsTr("Screens"), name: qsTr("Folders and disks"), icon: "hard-drive", keys: ["Ctrl+3"] },
        { id: "tasks", group: qsTr("Screens"), name: qsTr("Tasks"), icon: "activity", keys: ["Ctrl+4"] },
        { id: "privacy", group: qsTr("Screens"), name: qsTr("Private photos"), icon: "lock", keys: ["Ctrl+5"] },

        { id: "search", group: qsTr("Library"), name: qsTr("Search photos"), icon: "search", keys: ["Ctrl+F"] },
        { id: "aiSearch", group: qsTr("Library"), name: qsTr("AI search on or off"), icon: "sparkles", keys: ["Ctrl+I"] },
        { id: "filter", group: qsTr("Library"), name: qsTr("Show or hide the filter"), icon: "sliders-horizontal", keys: ["Ctrl+Shift+F"] },
        { id: "selectAll", group: qsTr("Library"), name: qsTr("Select all photos"), icon: "check", keys: ["Ctrl+A"] },
        { id: "clearSelection", group: qsTr("Library"), name: qsTr("Clear selection / back"), icon: "x", keys: ["Esc"] },
        { id: "trash", group: qsTr("Library"), name: qsTr("Move selected photos to the Trash"), icon: "trash-2", keys: _mac ? ["Ctrl+Backspace", "Del"] : ["Del"] },
        { id: "zoomIn", group: qsTr("Library"), name: qsTr("Larger thumbnails"), icon: "zoom-in", keys: ["Ctrl+="] },
        { id: "zoomOut", group: qsTr("Library"), name: qsTr("Smaller thumbnails"), icon: "zoom-out", keys: ["Ctrl+-"] },
        { id: "newAlbum", group: qsTr("Library"), name: qsTr("New album"), icon: "book-image", keys: ["Ctrl+Shift+N"] },

        { id: "nextPhoto", group: qsTr("Photo"), name: qsTr("Next photo"), icon: "chevron-right", keys: ["Right"] },
        { id: "previousPhoto", group: qsTr("Photo"), name: qsTr("Previous photo"), icon: "chevron-left", keys: ["Left"] },
        { id: "faceBoxes", group: qsTr("Photo"), name: qsTr("Show or hide faces"), icon: "scan-face", keys: ["F"] },

        { id: "addFolder", group: qsTr("Catalog"), name: qsTr("Add folder…"), icon: "folder-plus", keys: ["Ctrl+O"] },
        { id: "rescan", group: qsTr("Catalog"), name: qsTr("Rescan all folders"), icon: "refresh-cw", keys: ["F5"] },
        { id: "faceSearch", group: qsTr("Catalog"), name: qsTr("Search by a person's photo…"), icon: "scan-face", keys: ["Ctrl+Shift+P"] },
        { id: "quit", group: qsTr("Catalog"), name: qsTr("Quit"), icon: "log-out", keys: ["Ctrl+Q"] },

        { id: "palette", group: qsTr("Interface"), name: qsTr("Command palette"), icon: "search", keys: ["Ctrl+K"] },
        { id: "shortcuts", group: qsTr("Interface"), name: qsTr("Keyboard shortcuts"), icon: "keyboard", keys: ["Ctrl+/"] },
        { id: "fullScreen", group: qsTr("Interface"), name: qsTr("Full screen"), icon: "maximize", keys: [_mac ? "Ctrl+Meta+F" : "F11"] },
        { id: "theme", group: qsTr("Interface"), name: qsTr("Light or dark theme"), icon: "sun", keys: [] }
    ]

    function action(id) {
        return actions.find((a) => a.id === id) || null;
    }

    function defaults(id) {
        const a = action(id);
        return a ? a.keys : [];
    }

    /// Atalhos da ação: os da pessoa ou, sem troca, os padrões.
    function keys(id) {
        const o = overrides[id];
        return o !== undefined ? o : defaults(id);
    }

    /// Só o primeiro atalho ("" se nenhum) — o da Action dos menus.
    function first(id) {
        const k = keys(id);
        return k.length > 0 ? k[0] : "";
    }

    function isCustom(id) {
        return overrides[id] !== undefined;
    }

    /// Primeiro atalho no formato da plataforma ("Ctrl+K", "⌘K"), ou "".
    function label(id) {
        const k = first(id);
        return k !== "" ? App.shortcutText(k) : "";
    }

    /// " (Ctrl+K)" para pendurar num rótulo/dica, ou "" sem atalho.
    function hint(id) {
        const l = label(id);
        return l !== "" ? " (" + l + ")" : "";
    }

    function setKeys(id, list) {
        const o = Object.assign({}, overrides);
        // Igual ao padrão: volta a seguir o padrão (mudanças futuras dele valem).
        if (JSON.stringify(list) === JSON.stringify(defaults(id)))
            delete o[id];
        else
            o[id] = list;
        keymap.custom = JSON.stringify(o);
    }

    function reset(id) {
        const o = Object.assign({}, overrides);
        delete o[id];
        keymap.custom = JSON.stringify(o);
    }

    function resetAll() {
        keymap.custom = "{}";
    }

    /// Ação que já usa ``seq`` (exceto ``exceptId``), ou null.
    function owner(seq, exceptId) {
        for (const a of actions) {
            if (a.id !== exceptId && keys(a.id).some((k) => App.sameShortcut(k, seq)))
                return a;
        }
        return null;
    }

    /// Dá ``seq`` a ``id``, tirando-o de quem já o usava. Devolve o nome da
    /// ação que perdeu o atalho, ou "".
    function assign(id, seq) {
        const other = owner(seq, id);
        if (other)
            setKeys(other.id, keys(other.id).filter((k) => !App.sameShortcut(k, seq)));
        const mine = keys(id);
        if (!mine.some((k) => App.sameShortcut(k, seq)))
            setKeys(id, mine.concat([seq]));
        return other ? other.name : "";
    }

    function remove(id, seq) {
        setKeys(id, keys(id).filter((k) => k !== seq));
    }

    /// Sequência portável de uma tecla capturada (Keys.onPressed), ou "" se
    /// a tecla não serve de atalho. No macOS o Qt já entrega o ⌘ como Control.
    function sequenceFor(key, modifiers) {
        const names = {};
        names[Qt.Key_Left] = "Left"; names[Qt.Key_Right] = "Right";
        names[Qt.Key_Up] = "Up"; names[Qt.Key_Down] = "Down";
        names[Qt.Key_Delete] = "Del"; names[Qt.Key_Backspace] = "Backspace";
        names[Qt.Key_Return] = "Return"; names[Qt.Key_Enter] = "Enter";
        names[Qt.Key_Escape] = "Esc"; names[Qt.Key_Tab] = "Tab"; names[Qt.Key_Backtab] = "Tab";
        names[Qt.Key_Space] = "Space"; names[Qt.Key_Home] = "Home"; names[Qt.Key_End] = "End";
        names[Qt.Key_PageUp] = "PgUp"; names[Qt.Key_PageDown] = "PgDown"; names[Qt.Key_Insert] = "Ins";
        let name = names[key] || "";
        if (name === "" && key >= Qt.Key_F1 && key <= Qt.Key_F35)
            name = "F" + (key - Qt.Key_F1 + 1);
        // Letras, números e pontuação: o código da tecla é o próprio ASCII.
        if (name === "" && key >= 0x21 && key <= 0x7e)
            name = String.fromCharCode(key).toUpperCase();
        if (name === "")
            return "";
        let prefix = "";
        if (modifiers & Qt.ControlModifier) prefix += "Ctrl+";
        if (modifiers & Qt.AltModifier) prefix += "Alt+";
        if (modifiers & Qt.ShiftModifier) prefix += "Shift+";
        if (modifiers & Qt.MetaModifier) prefix += "Meta+";
        return prefix + name;
    }
}

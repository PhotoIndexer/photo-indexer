pragma Singleton
import QtQuick

// Confirmações e avisos do app — o useDialog() do React (CLAUDE.md 1: nunca
// diálogos do SO/navegador para isso). Quem desenha é o DialogHost montado
// uma vez no Main.qml; daqui sai só o estado. Um pedido feito com outro
// diálogo aberto entra na fila.
//
// QML não tem await na interface: o resultado chega por callback.
//
//   Dialogs.confirm({
//       title: qsTr("Delete photo?"),
//       description: qsTr("<b>%1</b> goes to deleted/.").arg(Messages.escapeHtml(name)),
//       variant: "danger",                 // danger | warning | success | info
//       confirmLabel: qsTr("Delete"),
//       cancelLabel: qsTr("Cancel"),       // opcional
//       onConfirm: () => App.editor.deleteCurrent()   // opcional, ver abaixo
//   }, () => console.log("confirmado"), () => console.log("cancelado"))
//
//   Dialogs.alert({ title: qsTr("Could not save"), variant: "danger" }, () => ...)
//
// Ação sem volta (excluir definitivamente): ``requireText`` pede que a pessoa
// digite aquela palavra (ou número) antes de o botão de confirmar acender;
// ``requirePrompt`` troca a frase padrão ("Type <b>X</b> to confirm.").
//
//   Dialogs.confirm({ ..., variant: "danger", requireText: qsTr("delete"),
//                     onConfirm: () => App.editor.purgeCurrent() })
//
//   Dialogs.choose({
//       title: ..., description: ..., variant: "warning",
//       buttons: [{ id: "move", label: qsTr("Move"), variant: "default", primary: true },
//                 { id: "other", label: qsTr("Choose another folder"), variant: "outline" },
//                 { id: "cancel", label: qsTr("Cancel"), variant: "ghost" }]
//   }, (id) => { ... })            // id do botão; "" se fechou com Esc/X/fora
//
// ``onConfirm`` mantém o modal aberto, com o botão em loading, enquanto a
// ação roda:
//   - ``onConfirm: (done) => { ...; done(true) }`` — assíncrono: chame
//     done(true) para fechar (confirmado) ou done(false) para continuar
//     aberto (ex.: o C++ respondeu com erro);
//   - ``onConfirm: () => ...`` — síncrono: devolver ``false`` (ou lançar
//     exceção) mantém aberto; devolver uma Promise espera por ela.
QtObject {
    id: dialogs

    /// Diálogo visível: {title, description, variant, iconName, size, buttons: [...]} ou null.
    property var current: null
    /// Uma ação de ``onConfirm`` em andamento (botões travados, sem Esc).
    property bool busy: false
    property string busyButton: ""
    /// O que a pessoa digitou no campo de ``requireText`` (o DialogHost escreve).
    property string typed: ""
    /// O diálogo atual não pede texto, ou o digitado confere (sem caixa nem espaços nas pontas).
    readonly property bool typedOk: !current || !current.requireText
        || typed.trim().toLowerCase() === String(current.requireText).trim().toLowerCase()

    property var _queue: []

    function confirm(options, onAccepted, onRejected) {
        const o = options || {};
        const danger = o.variant === "danger";
        _show({
            title: o.title || "",
            description: o.description || "",
            variant: o.variant || "warning",
            iconName: o.iconName || "",
            size: o.size || "sm",
            buttons: [
                { id: "cancel", label: o.cancelLabel || qsTr("Cancel"), variant: "outline" },
                { id: "confirm", label: o.confirmLabel || qsTr("Confirm"),
                  variant: danger ? "destructive" : (o.confirmVariant || "default"), primary: true }
            ],
            action: o.onConfirm || null,
            actionButton: "confirm",
            requireText: o.requireText !== undefined ? String(o.requireText) : "",
            requirePrompt: o.requirePrompt || "",
            resolve: (id) => {
                if (id === "confirm") {
                    if (onAccepted)
                        onAccepted();
                } else if (onRejected) {
                    onRejected();
                }
            }
        });
    }

    function alert(options, onClosed) {
        const o = options || {};
        _show({
            title: o.title || "",
            description: o.description || "",
            variant: o.variant || "info",
            iconName: o.iconName || "",
            size: o.size || "sm",
            buttons: [
                { id: "ok", label: o.confirmLabel || qsTr("OK"),
                  variant: o.variant === "danger" ? "destructive" : "default", primary: true }
            ],
            action: null,
            resolve: () => {
                if (onClosed)
                    onClosed();
            }
        });
    }

    function choose(options, onChoice) {
        const o = options || {};
        _show({
            title: o.title || "",
            description: o.description || "",
            variant: o.variant || "info",
            iconName: o.iconName || "",
            size: o.size || "md",
            buttons: o.buttons || [],
            action: null,
            resolve: (id) => {
                if (onChoice)
                    onChoice(id);
            }
        });
    }

    // ------------------------------------------------------------------ //
    // Uso interno (DialogHost)
    // ------------------------------------------------------------------ //

    function _show(dialog) {
        if (current) {
            _queue.push(dialog);
            return;
        }
        busy = false;
        busyButton = "";
        typed = "";
        current = dialog;
    }

    function _press(id) {
        const d = current;
        if (!d || busy)
            return;
        // Texto de confirmação pendente: o botão está apagado, e o Enter também não passa.
        if (id === d.actionButton && !typedOk)
            return;
        if (d.action && id === d.actionButton) {
            _run(d, id);
            return;
        }
        _finish(id);
    }

    function _dismiss() {
        if (!current || busy)
            return;
        _finish("");
    }

    function _run(d, id) {
        busy = true;
        busyButton = id;
        const settle = (ok) => {
            if (current !== d)
                return;
            busy = false;
            busyButton = "";
            if (ok)
                _finish(id);
        };
        try {
            if (d.action.length >= 1) {
                d.action(settle);
                return;
            }
            const result = d.action();
            if (result && typeof result.then === "function")
                result.then(() => settle(true), () => settle(false));
            else
                settle(result !== false);
        } catch (e) {
            console.warn("Dialog action failed:", e);
            settle(false);
        }
    }

    function _finish(id) {
        const d = current;
        busy = false;
        busyButton = "";
        typed = "";
        current = null;
        if (d && d.resolve)
            d.resolve(id === "" ? (d.buttons.length === 1 ? d.buttons[0].id : "") : id);
        if (!current && _queue.length > 0)
            current = _queue.shift();
    }
}

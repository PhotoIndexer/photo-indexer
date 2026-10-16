pragma Singleton
import QtQuick

// Tokens visuais do app — o equivalente QML do index.css (variáveis do tema
// claro/escuro) e do tailwind.config.js (escala brand-*). Toda cor, raio,
// fonte e espaçamento sai daqui: os componentes só referenciam o Theme, nunca
// um "#rrggbb" solto.
//
// Regra de cor (CLAUDE.md 2.1): vermelho da marca = acento; esmeralda =
// confirmação; erro = FormError (caixa + ícone), nunca só texto colorido.
QtObject {
    id: theme

    // Tema escuro é o padrão (App.darkTheme, persistido pelo C++).
    readonly property bool dark: App.darkTheme

    // ------------------------------------------------------------------ //
    // Paleta fixa (Tailwind) — independe do tema
    // ------------------------------------------------------------------ //

    // Vermelho da marca, derivado do #ef3236 do logotipo (mesma matiz,
    // luminosidade variando). Num fork, troque a escala inteira.
    readonly property color brand50: "#fcf0f0"
    readonly property color brand100: "#fbdedf"
    readonly property color brand200: "#fac0c1"
    readonly property color brand300: "#f99698"
    readonly property color brand400: "#f85e62"
    readonly property color brand500: "#ef3236"
    readonly property color brand600: "#e21418"
    readonly property color brand700: "#b91518"
    readonly property color brand800: "#921619"
    readonly property color brand900: "#791719"

    readonly property color emerald100: "#d1fae5"
    readonly property color emerald400: "#34d399"
    readonly property color emerald600: "#059669"
    readonly property color emerald900: "#064e3b"

    readonly property color amber100: "#fef3c7"
    readonly property color amber400: "#fbbf24"
    readonly property color amber600: "#d97706"
    readonly property color amber900: "#78350f"

    readonly property color red100: "#fee2e2"
    readonly property color red400: "#f87171"
    readonly property color red500: "#ef4444"
    readonly property color red600: "#dc2626"
    readonly property color red900: "#7f1d1d"

    readonly property color blue100: "#dbeafe"
    readonly property color blue400: "#60a5fa"
    readonly property color blue600: "#2563eb"
    readonly property color blue900: "#1e3a8a"

    readonly property color indigo600: "#4f46e5"
    readonly property color indigo700: "#4338ca"

    readonly property color neutral800: "#262626"
    readonly property color neutral900: "#171717"
    readonly property color neutral950: "#0a0a0a"
    readonly property color zinc950: "#09090b"

    readonly property color white: "#ffffff"
    readonly property color black: "#000000"

    // ------------------------------------------------------------------ //
    // Tokens do tema (index.css :root / .dark)
    // ------------------------------------------------------------------ //

    readonly property color background: dark ? "#171717" : "#ffffff"
    readonly property color foreground: dark ? "#f2f2f2" : "#0a0a0a"
    readonly property color card: dark ? "#1f1f1f" : "#ffffff"
    readonly property color cardForeground: foreground
    readonly property color popover: card
    readonly property color popoverForeground: foreground
    // No escuro o "primary" é o azul do botão Exportar/Concluir das telas.
    readonly property color primary: dark ? "#3c83f6" : "#171717"
    readonly property color primaryForeground: "#fafafa"
    readonly property color primaryHover: indigo600
    readonly property color primaryPressed: indigo700
    readonly property color secondary: dark ? "#292929" : "#f5f5f5"
    readonly property color secondaryForeground: dark ? "#f2f2f2" : "#171717"
    readonly property color muted: dark ? "#292929" : "#f5f5f5"
    readonly property color mutedForeground: dark ? "#999999" : "#737373"
    readonly property color accent: dark ? "#292929" : "#f5f5f5"
    readonly property color accentForeground: dark ? "#f2f2f2" : "#171717"
    readonly property color destructive: dark ? "#811d1d" : "#ef4444"
    readonly property color destructiveForeground: "#fafafa"
    readonly property color destructiveHover: red600
    readonly property color border: dark ? "#333333" : "#e5e5e5"
    readonly property color input: border
    readonly property color ring: dark ? "#3c83f6" : "#0a0a0a"
    // Item ativo (trilho lateral, linha da paleta, atalho sendo gravado):
    // fundo suave e ícone no acento da marca.
    readonly property color brandSoft: alpha(brand500, dark ? 0.16 : 0.10)
    readonly property color brandForeground: dark ? brand400 : brand600
    // Anel de foco das telas (focus-visible:ring-brand-400).
    readonly property color focusRing: brand400

    // Fundo das áreas de foto (antes/depois) e da filmstrip.
    readonly property color imagePane: neutral900
    readonly property color filmstrip: neutral900
    readonly property color imageLabel: Qt.rgba(0, 0, 0, 0.6)

    // Modal: fundo escurecido (bg-black/40).
    readonly property color backdrop: Qt.rgba(0, 0, 0, 0.4)
    readonly property color overlay: Qt.rgba(0, 0, 0, 0.6)

    // Splitter (components/ui/splitter.jsx): linhas na cor da borda; ativo
    // = acento a 60% e a pegada num branco suave.
    readonly property color splitterLine: border
    readonly property color splitterBand: background
    readonly property color splitterActive: alpha(brand400, 0.6)
    readonly property color splitterGrip: border
    readonly property color splitterGripActive: alpha(white, 0.7)

    // Barra de rolagem fina (App.css .scrollbar-thin).
    readonly property color scrollbarTrack: "transparent"
    readonly property color scrollbarThumb: alpha(mutedForeground, 0.3)
    readonly property color scrollbarThumbHover: alpha(mutedForeground, 0.5)
    readonly property int scrollbarSize: 8

    // Sucesso/erro/aviso/informação (DialogContext.jsx + form-error.jsx).
    readonly property color success: dark ? emerald400 : emerald600
    readonly property color warning: dark ? amber400 : amber600
    readonly property color info: dark ? blue400 : blue600
    readonly property color danger: dark ? red400 : red600
    readonly property color errorText: red400
    readonly property color errorBorder: alpha(red500, 0.4)
    readonly property color errorBackground: alpha(red500, 0.1)

    // Slider: trilho claro e preenchimento no acento (accent-brand-500).
    readonly property color sliderTrack: dark ? "#e5e5e5" : "#d4d4d4"
    readonly property color sliderFill: brand500
    readonly property color sliderHandle: dark ? "#f5f5f5" : "#ffffff"
    readonly property color sliderHandleBorder: dark ? "#737373" : "#a3a3a3"
    // Trilhos coloridos (ValueSlider.trackGradient): o que o slider faz com a
    // cor — temperatura azul → âmbar, matiz verde → magenta, saturação cinza → cor.
    readonly property color trackTempCool: "#3b82f6"
    readonly property color trackTempWarm: "#f2b233"
    readonly property color trackTintGreen: "#3fae5a"
    readonly property color trackTintMagenta: "#c74fc0"
    readonly property color trackSatLow: "#8a8a8a"
    readonly property color trackSatMid: "#d9a23b"
    readonly property color trackSatHigh: "#e5384f"

    // Gráficos de imagem (histograma, níveis, curvas): fundo escuro nos dois
    // temas, como os painéis de foto — os canais se somam ("lighter") e só
    // dão branco sobre o escuro.
    readonly property color graphBackground: neutral900
    readonly property color graphGrid: Qt.rgba(1, 1, 1, 0.1)
    readonly property color graphGuide: Qt.rgba(1, 1, 1, 0.28)
    readonly property color graphText: Qt.rgba(1, 1, 1, 0.7)
    readonly property color channelRed: "#ff4d4d"
    readonly property color channelGreen: "#3ddc6a"
    readonly property color channelBlue: "#4d8dff"
    readonly property color channelLuma: "#bdbdbd"
    readonly property color curveRgb: "#f5f5f5"
    /// Triângulo do gama (meio-tom) dos Níveis.
    readonly property color levelsMid: "#808080"
    /// Indicador de recorte do histograma aceso.
    readonly property color clipOn: "#ffffff"
    readonly property color clipOff: Qt.rgba(1, 1, 1, 0.22)

    // Miniaturas da biblioteca de efeitos: a cena (céu, sol, morro, chão)
    // que cada efeito redesenha do seu jeito.
    readonly property color swatchSkyTop: "#3d7ad6"
    readonly property color swatchSkyBottom: "#f0b46a"
    readonly property color swatchSun: "#ffe7a3"
    readonly property color swatchHill: "#3f7d4a"
    readonly property color swatchGround: "#7a5634"

    // ------------------------------------------------------------------ //
    // Raio, fonte, espaçamento
    // ------------------------------------------------------------------ //

    readonly property int radiusLg: 8      // rounded-lg (--radius)
    readonly property int radiusMd: 6      // rounded-md
    readonly property int radiusSm: 4      // rounded-sm
    readonly property int radiusXl: 12     // rounded-xl (cards da Home)
    readonly property int radius2xl: 16    // rounded-2xl (modal, botões da Home)

    readonly property string fontFamily: "Inter"
    readonly property int fontXs: 11       // text-[11px]
    readonly property int fontSm: 12       // text-xs
    readonly property int fontBase: 14     // text-sm (padrão do app)
    readonly property int fontMd: 16       // text-base
    readonly property int fontLg: 18       // text-lg
    readonly property int fontTiny: 10     // text-[10px] (selos)

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacing: 16
    readonly property int spacingLg: 24
    readonly property int spacingXl: 32
    // Respiro das telas que não são o editor (p-4 lg:p-6).
    readonly property int pagePadding: 24
    readonly property int headerHeight: 40     // cabeçalho (sem a borda de 1px)

    readonly property int controlHeight: 40     // h-10 (Input, Button)
    readonly property int controlHeightSm: 36   // h-9
    readonly property int controlHeightLg: 44   // h-11

    readonly property int animationFast: 150
    readonly property int animationNormal: 300

    // ------------------------------------------------------------------ //
    // Utilitários
    // ------------------------------------------------------------------ //

    /// A mesma cor com outra opacidade (o "/40" do Tailwind).
    function alpha(c: color, a: real): color {
        return Qt.rgba(c.r, c.g, c.b, a);
    }

    /// "#rrggbb" sem alfa — o que o provider de ícones entende.
    function hex(c: color): string {
        function two(v) {
            const s = Math.round(v * 255).toString(16);
            return s.length < 2 ? "0" + s : s;
        }
        return "#" + two(c.r) + two(c.g) + two(c.b);
    }

    /// "rgba(r, g, b, a)" para o Canvas (fillStyle/strokeStyle); ``a``
    /// negativo = o alfa da própria cor.
    function css(c: color, a: real): string {
        const al = a < 0 ? c.a : a;
        return "rgba(" + Math.round(c.r * 255) + ", " + Math.round(c.g * 255) + ", "
            + Math.round(c.b * 255) + ", " + al + ")";
    }

    /// Ícone Lucide recolorido (image://icon/<nome>?color=%23rrggbb).
    function iconUrl(name: string, c: color): string {
        return "image://icon/" + name + "?color=" + encodeURIComponent(hex(c));
    }
}

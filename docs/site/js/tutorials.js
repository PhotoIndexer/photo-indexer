/* ==========================================================================
   PhotoIDX — página de tutoriais

   Lista de capítulos (com filtro) à esquerda e o capítulo aberto à direita,
   no idioma do site (window.PIX_I18N). O capítulo vai na URL (?t=<slug>),
   então dá para mandar o link de um tutorial. Conteúdo em tutorials-content.js.
   ========================================================================== */
(function () {
  "use strict";

  var i18n = window.PIX_I18N;
  var chapters = window.PIX_TUTORIALS || [];
  var list = document.querySelector("[data-tut-list]");
  var content = document.querySelector("[data-tut-content]");
  var filter = document.querySelector("[data-tut-filter]");
  var count = document.querySelector("[data-tut-count]");
  if (!list || !content) return;

  function lang() { return i18n && i18n.lang === "en" ? "en" : "pt"; }
  function t(key, fallback) { return i18n ? i18n.t(key, fallback) : fallback; }
  function tf(key, params, fallback) { return i18n ? i18n.tf(key, params, fallback) : fallback; }
  function text(ch) { return ch[lang()] || ch.pt; }
  function shotDir() { return "../assets/screens/tutorials/" + (lang() === "en" ? "en" : "pt-br") + "/"; }

  function current() {
    var slug = new URLSearchParams(location.search).get("t");
    for (var i = 0; i < chapters.length; i++) if (chapters[i].slug === slug) return i;
    return 0;
  }

  function el(tag, cls, html) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (html !== undefined) e.innerHTML = html;
    return e;
  }

  // ------------------------------ Lista -------------------------------- //
  function renderList() {
    list.innerHTML = "";
    var q = (filter && filter.value || "").trim().toLowerCase();
    var active = current();
    chapters.forEach(function (ch, i) {
      var c = text(ch);
      if (q && (c.title + " " + c.summary).toLowerCase().indexOf(q) < 0) return;
      var li = el("li");
      var a = el("a", i === active ? "is-active" : "");
      a.href = "?t=" + ch.slug;
      a.innerHTML = "<span class=\"tut__num\">" + (i + 1) + "</span><span>" + c.title + "</span>";
      a.addEventListener("click", function (e) {
        e.preventDefault();
        go(i);
      });
      li.appendChild(a);
      list.appendChild(li);
    });
    if (count) count.textContent = tf("tut.chapters", { n: chapters.length }, "{n} capítulos");
  }

  // ----------------------------- Capítulo ------------------------------ //
  function renderChapter() {
    var i = current();
    var ch = chapters[i];
    if (!ch) return;
    var c = text(ch);
    document.title = c.title + " — " + t("tut.title", "Tutoriais") + " · PhotoIDX";
    content.innerHTML = "";
    content.appendChild(el("span", "tut__eyebrow", t("tut.eyebrow", "Tutorial") + " " + (i + 1)));
    content.appendChild(el("h1", "", c.title));
    content.appendChild(el("p", "tut__lead", c.summary));
    content.appendChild(el("hr", "tut__rule"));
    c.blocks.forEach(function (b) {
      var kind = b[0];
      if (kind === "p") {
        content.appendChild(el("p", "", b[1]));
      } else if (kind === "h") {
        content.appendChild(el("h2", "", b[1]));
      } else if (kind === "steps") {
        var ol = el("ol", "tut__steps");
        b[1].forEach(function (s) { ol.appendChild(el("li", "", s)); });
        content.appendChild(ol);
      } else if (kind === "tip") {
        content.appendChild(el("div", "tut__tip", "<b>" + t("tut.tip", "Dica") + "</b> " + b[1]));
      } else if (kind === "img") {
        var fig = el("figure", "tut__shot");
        var win = el("div", "window");
        win.appendChild(el("div", "window__bar", "<i></i><i></i><i></i><span>PhotoIDX</span>"));
        var img = el("img");
        img.src = shotDir() + b[1] + ".jpg?ts=" + (window.PIX_BUILD || "");
        img.alt = b[2] || "";
        img.width = 1440;
        img.height = 900;
        img.loading = "lazy";
        img.setAttribute("data-zoom", "");
        win.appendChild(img);
        fig.appendChild(win);
        if (b[2]) fig.appendChild(el("figcaption", "", b[2]));
        content.appendChild(fig);
      }
    });
    // Anterior / próximo.
    var nav = el("nav", "tut__pager");
    if (i > 0) nav.appendChild(pager(i - 1, "prev"));
    else nav.appendChild(el("span"));
    if (i + 1 < chapters.length) nav.appendChild(pager(i + 1, "next"));
    content.appendChild(nav);
    bindZoom();
  }

  function pager(i, dir) {
    var a = el("a", "tut__pagerlink tut__pagerlink--" + dir);
    a.href = "?t=" + chapters[i].slug;
    a.innerHTML = "<small>" + (dir === "prev" ? t("tut.prev", "← Anterior") : t("tut.next", "Próximo →")) + "</small><b>" +
      text(chapters[i]).title + "</b>";
    a.addEventListener("click", function (e) { e.preventDefault(); go(i); });
    return a;
  }

  function go(i) {
    history.pushState(null, "", "?t=" + chapters[i].slug);
    render();
    window.scrollTo({ top: 0, behavior: "smooth" });
  }

  function render() {
    renderList();
    renderChapter();
  }

  // ------------------------------ Lightbox ----------------------------- //
  var box = document.querySelector("[data-lightbox]");
  function bindZoom() {
    if (!box) return;
    var boxImg = box.querySelector("[data-lightbox-img]");
    var boxCap = box.querySelector("[data-lightbox-cap]");
    content.querySelectorAll("img[data-zoom]").forEach(function (img) {
      img.setAttribute("tabindex", "0");
      var open = function () {
        boxImg.src = img.currentSrc || img.src;
        boxImg.alt = img.alt;
        boxCap.textContent = img.alt;
        box.classList.add("is-open");
        document.body.style.overflow = "hidden";
        box.querySelector("[data-lightbox-close]").focus();
      };
      img.addEventListener("click", open);
      img.addEventListener("keydown", function (e) {
        if (e.key === "Enter" || e.key === " ") { e.preventDefault(); open(); }
      });
    });
  }
  if (box) {
    var close = function () {
      box.classList.remove("is-open");
      document.body.style.overflow = "";
    };
    box.addEventListener("click", function (e) {
      if (e.target === box || e.target.closest("[data-lightbox-close]")) close();
    });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape" && box.classList.contains("is-open")) close();
    });
  }

  // ------------------------------ Eventos ------------------------------ //
  if (filter) filter.addEventListener("input", renderList);
  window.addEventListener("popstate", render);
  if (i18n) i18n.onChange(render);
  var year = document.querySelector("[data-year]");
  if (year) year.textContent = new Date().getFullYear();
  render();
})();

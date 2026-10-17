/* ==========================================================================
   PhotoIDX hotsite — menu mobile, reveal-on-scroll, lightbox das
   screenshots (modal próprio, nunca janela nativa) e a área de download
   alimentada pela API de releases do GitHub.
   ========================================================================== */
(function () {
  "use strict";

  var i18n = window.PIX_I18N;

  /* ---------------------------- Mobile nav ------------------------------ */
  var header = document.querySelector(".site-header");
  var toggle = document.querySelector(".nav__toggle");
  if (toggle && header) {
    toggle.addEventListener("click", function () {
      var open = header.classList.toggle("nav-open");
      toggle.setAttribute("aria-expanded", open ? "true" : "false");
    });
    header.querySelectorAll(".nav__menu a").forEach(function (a) {
      a.addEventListener("click", function () { header.classList.remove("nav-open"); });
    });
  }

  /* -------------------------- Reveal on scroll -------------------------- */
  var reveals = document.querySelectorAll(".reveal");
  if (reveals.length && "IntersectionObserver" in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (en) {
        if (en.isIntersecting) { en.target.classList.add("is-visible"); io.unobserve(en.target); }
      });
    }, { threshold: 0.12 });
    reveals.forEach(function (el) { io.observe(el); });
  } else {
    reveals.forEach(function (el) { el.classList.add("is-visible"); });
  }

  /* ------------------------------ Lightbox ------------------------------ */
  var box = document.querySelector("[data-lightbox]");
  if (box) {
    var boxImg = box.querySelector("[data-lightbox-img]");
    var boxCap = box.querySelector("[data-lightbox-cap]");
    var lastFocus = null;
    var close = function () {
      box.classList.remove("is-open");
      document.body.style.overflow = "";
      if (lastFocus) lastFocus.focus();
    };
    document.querySelectorAll("img[data-zoom]").forEach(function (img) {
      img.setAttribute("tabindex", "0");
      var open = function () {
        lastFocus = img;
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
    box.addEventListener("click", function (e) {
      if (e.target === box || e.target.closest("[data-lightbox-close]")) close();
    });
    document.addEventListener("keydown", function (e) {
      if (e.key === "Escape" && box.classList.contains("is-open")) close();
    });
  }

  /* ------------------------------ Download ------------------------------ */
  // Entre as releases publicadas, a mais
  // recente (maior `id`) que tenha um asset do padrão pedido — por plataforma.
  // Assim, se uma release sair sem o instalador de um sistema, aquele sistema
  // continua apontando para a última versão que o tinha.
  var REPO = "PhotoIndexer/photo-indexer";
  var API = "https://api.github.com/repos/" + REPO + "/releases?per_page=30";
  var CACHE_KEY = "pix_releases_v1";
  var CACHE_TTL = 10 * 60 * 1000; // a API anônima permite 60 req/h por IP

  var PLATFORMS = {
    "macos-arm64": /-macos-arm64\.dmg$/,
    "macos-x64": /-macos-x64\.dmg$/,
    "windows-x64": /-windows-x64\.msi$/,
    "windows-arm64": /-windows-arm64\.msi$/,
    "linux-x64": /-linux-x64\.tar\.gz$/,
    "linux-arm64": /-linux-arm64\.tar\.gz$/
  };

  var resolved = null;   // { platform: {name, url, size, sha, tag, date} }
  var failed = false;

  function t(k, f) { return i18n ? i18n.t(k, f) : f; }
  function tf(k, p, f) { return i18n ? i18n.tf(k, p, f) : f; }

  // Plataforma do visitante. Mac com Apple Silicon não se distingue de Intel
  // pelo user agent; arm64 é o padrão (todo Mac vendido desde 2020).
  function detectPlatform() {
    var ua = navigator.userAgent || "";
    var uaPlat = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "";
    var s = (uaPlat + " " + ua).toLowerCase();
    if (/iphone|ipad|android/.test(s)) return null;
    if (/mac/.test(s)) return "macos-arm64";
    if (/win/.test(s)) return "windows-x64";
    if (/linux/.test(s)) return /aarch64|arm64|armv8/.test(s) ? "linux-arm64" : "linux-x64";
    return null;
  }

  function formatSize(bytes) {
    if (!bytes) return "";
    var mb = bytes / (1024 * 1024);
    return mb >= 1024 ? (mb / 1024).toFixed(2) + " GB" : Math.round(mb) + " MB";
  }

  function formatDate(iso) {
    try {
      return new Date(iso).toLocaleDateString(i18n && i18n.lang === "en" ? "en-US" : "pt-BR",
        { day: "2-digit", month: "short", year: "numeric" });
    } catch (e) { return ""; }
  }

  // Só aceitamos links do próprio GitHub: o href vem de uma resposta externa.
  function safeUrl(url) {
    return /^https:\/\/github\.com\//.test(url || "") ? url : null;
  }

  function resolve(releases) {
    var list = releases
      .filter(function (r) { return !r.draft && !r.prerelease; })
      .sort(function (a, b) { return b.id - a.id; });
    var out = {};
    Object.keys(PLATFORMS).forEach(function (key) {
      for (var i = 0; i < list.length; i++) {
        var rel = list[i];
        var assets = rel.assets || [];
        var hit = assets.filter(function (a) { return PLATFORMS[key].test(a.name); })[0];
        var url = hit && safeUrl(hit.browser_download_url);
        if (!url) continue;
        var sha = assets.filter(function (a) { return a.name === hit.name + ".sha256"; })[0];
        out[key] = {
          name: hit.name,
          url: url,
          size: hit.size,
          sha: sha ? safeUrl(sha.browser_download_url) : null,
          tag: rel.tag_name,
          version: String(rel.tag_name || "").replace(/^v/, ""),
          date: rel.published_at
        };
        break;
      }
    });
    return out;
  }

  function render() {
    var hero = document.querySelector("[data-dl-hero]");
    if (!hero) return;
    var status = hero.querySelector("[data-dl-status]");
    var title = hero.querySelector("[data-dl-hero-title]");
    var meta = hero.querySelector("[data-dl-hero-meta]");
    var btn = hero.querySelector("[data-dl-hero-btn]");
    var label = hero.querySelector("[data-dl-hero-label]");

    if (failed) {
      hero.classList.add("is-error");
      status.textContent = t("dl.error", "Não foi possível consultar as versões agora");
      meta.textContent = t("dl.errorHint", "Os botões abrem a página de Releases no GitHub, onde estão todos os instaladores.");
      return;
    }
    if (!resolved) return;

    // Cards por plataforma
    document.querySelectorAll("[data-asset]").forEach(function (a) {
      var info = resolved[a.getAttribute("data-asset")];
      var small = a.querySelector("[data-asset-meta]");
      if (!info) {
        a.classList.add("is-missing");
        small.textContent = t("dl.unavailable", "indisponível nesta versão");
        return;
      }
      a.href = info.url;
      a.removeAttribute("target");
      a.setAttribute("download", "");
      a.title = info.name;
      small.textContent = "v" + info.version + " · " + formatSize(info.size);
    });

    // Destaque para o sistema do visitante
    var current = detectPlatform();
    document.querySelectorAll("[data-dl-os]").forEach(function (card) {
      card.classList.toggle("is-current", !!current && current.indexOf(card.getAttribute("data-dl-os")) === 0);
    });

    var pick = current && resolved[current];
    var latest = Object.keys(resolved)
      .map(function (k) { return resolved[k]; })
      .sort(function (a, b) { return String(b.date).localeCompare(String(a.date)); })[0];

    if (pick) {
      // PT é o fallback (fica no HTML), mas estes nomes só aparecem aqui.
      var OS_PT = {
        "macos-arm64": "macOS (Apple Silicon)", "macos-x64": "macOS (Intel)",
        "windows-x64": "Windows", "windows-arm64": "Windows (ARM64)",
        "linux-x64": "Linux x64", "linux-arm64": "Linux arm64"
      };
      var osName = t("dl.os." + current, OS_PT[current] || current);
      status.textContent = t("dl.recommended", "Recomendado para o seu sistema");
      title.textContent = tf("dl.heroFor", { version: pick.version, os: osName }, "PhotoIDX {version} para {os}");
      meta.textContent = pick.name + " · " + formatSize(pick.size) + " · " + formatDate(pick.date);
      btn.href = pick.url;
      btn.removeAttribute("target");
      btn.setAttribute("download", "");
      label.textContent = tf("dl.getFor", { os: osName }, "Baixar para {os}");
    } else if (latest) {
      status.textContent = t("dl.latest", "Última versão");
      title.textContent = "PhotoIDX " + latest.version;
      meta.textContent = tf("dl.published", { version: latest.version, date: formatDate(latest.date) },
        "Versão {version} · publicada em {date}");
      btn.href = "#download";
      btn.removeAttribute("target");
      label.textContent = t("dl.pickBelow", "Escolha o instalador abaixo");
    }
  }

  function readCache() {
    try {
      var raw = sessionStorage.getItem(CACHE_KEY);
      if (!raw) return null;
      var data = JSON.parse(raw);
      return Date.now() - data.at < CACHE_TTL ? data.releases : null;
    } catch (e) { return null; }
  }

  function writeCache(releases) {
    try { sessionStorage.setItem(CACHE_KEY, JSON.stringify({ at: Date.now(), releases: releases })); } catch (e) { /* ok */ }
  }

  // Guardamos só o necessário da resposta (ela é grande: corpo das notas etc.).
  function slim(releases) {
    return releases.map(function (r) {
      return {
        id: r.id, tag_name: r.tag_name, draft: r.draft, prerelease: r.prerelease, published_at: r.published_at,
        assets: (r.assets || []).map(function (a) {
          return { name: a.name, size: a.size, browser_download_url: a.browser_download_url };
        })
      };
    });
  }

  // A lista gravada no build do site (packaging/site.py), refeito no fim de
  // toda release: vale quando a API anônima do GitHub não responde (limite
  // de 60 pedidos por hora por IP, rede).
  function loadBaked() {
    return fetch("assets/releases.json?v=" + (window.PIX_BUILD || ""))
      .then(function (r) { if (!r.ok) throw new Error("HTTP " + r.status); return r.json(); })
      .then(function (data) {
        if (!Array.isArray(data) || !data.length) throw new Error("empty");
        resolved = resolve(data);
        if (!Object.keys(resolved).length) throw new Error("no installers");
        render();
      });
  }

  function load() {
    var cached = readCache();
    if (cached) { resolved = resolve(cached); render(); return; }
    fetch(API, { headers: { Accept: "application/vnd.github+json" } })
      .then(function (r) { if (!r.ok) throw new Error("HTTP " + r.status); return r.json(); })
      .then(function (data) {
        if (!Array.isArray(data)) throw new Error("unexpected payload");
        var releases = slim(data);
        writeCache(releases);
        resolved = resolve(releases);
        if (!Object.keys(resolved).length) throw new Error("no installers");
        render();
      })
      .catch(function () {
        loadBaked().catch(function () { failed = true; render(); });
      });
  }

  if (document.querySelector("[data-dl-hero]")) {
    load();
    if (i18n) i18n.onChange(render);
  }

  /* --------------------------- Footer year ------------------------------ */
  document.querySelectorAll("[data-year]").forEach(function (el) {
    el.textContent = new Date().getFullYear();
  });
})();

/* ==========================================================================
   PhotoIDX hotsite — i18n (EN + PT-BR)

   - PT-BR é o padrão e o FALLBACK: o texto em português está no próprio HTML
     e é capturado ao carregar. Chave sem tradução em EN mantém o português.
   - A escolha fica num cookie (`photoidx_lang`), com allowlist fechada: só
     "en" e "pt-br" chegam ao document.cookie.

   Marcação:
     data-i18n="k"          -> textContent
     data-i18n-html="k"     -> innerHTML (apenas strings deste catálogo)
     data-i18n-alt="k"      -> atributo alt
     data-i18n-aria="k"     -> atributo aria-label
     data-i18n-content="k"  -> atributo content (meta)
     data-i18n-placeholder="k" -> atributo placeholder
   ========================================================================== */
(function () {
  "use strict";

  var SUPPORTED = ["en", "pt-br"];
  var FALLBACK = "pt-br";
  var COOKIE = "photoidx_lang";
  var ONE_YEAR = 60 * 60 * 24 * 365;

  var EN = {
    "meta.title": "PhotoIDX — all your photos in one catalog, organized by AI",
    "meta.description": "Free photo catalog for Windows, macOS and Linux: indexes local, USB and network folders (RAW included), keeps the full EXIF, categorizes with local AI, recognizes people, finds places from GPS and searches by text, by AI or by face. Offline, never touching the originals.",
    "top.license": "Free and open source · BSD 2-Clause license",
    "top.language": "Language",
    "nav.features": "Features",
    "nav.library": "Library",
    "nav.people": "People",
    "nav.search": "Search",
    "nav.tasks": "Tasks",
    "nav.download": "Download",
    "nav.cta": "Free download",
    "nav.tutorials": "Tutorials",
    "tut.metaTitle": "Tutorials — PhotoIDX",
    "tut.metaDescription": "Step-by-step PhotoIDX tutorials: adding folders, browsing, searching, categories, people, face search, copies, files and private photos.",
    "tut.listLabel": "Tutorials",
    "tut.title": "Tutorials",
    "tut.chapters": "{n} chapters",
    "tut.filter": "Filter…",
    "tut.eyebrow": "Tutorial",
    "tut.tip": "Tip",
    "tut.prev": "← Previous",
    "tut.next": "Next →",
    "tut.footerNote": "Screenshots with fictitious photos and names.",
    "nav.menu": "Open menu",
    "hero.eyebrow": "Windows · macOS · Linux",
    "hero.title": "All your photos, <span class=\"grad\">in one catalog.</span>",
    "hero.lead": "<strong>PhotoIDX</strong> indexes the folders you choose — on your computer, on an external drive or on a network share — and organizes everything by itself: dates, places, categories and people, with AI that runs on your computer. Your photos are never changed.",
    "hero.download": "Download PhotoIDX",
    "hero.features": "See the features",
    "hero.badge.offline": "offline, AI on your computer",
    "hero.badge.raw": "from Canon, Nikon, Sony, Fuji…",
    "hero.badge.originals": "originals changed",
    "flow.eyebrow": "How it works",
    "flow.title": "From the forgotten drive to an organized catalog, in four steps",
    "flow.subtitle": "One catalog for all the photos of your life. No importing, no copying, no uploading to the cloud.",
    "flow.s1.title": "Add your folders",
    "flow.s1.body": "One folder, several, or the root of a drive. Local, USB or network (SMB, NFS, AFP…): each drive is recognized by its own identity, on any system.",
    "flow.s2.title": "Indexing works on its own",
    "flow.s2.body": "Four stages in parallel: the Searcher finds the images, the Indexer reads the EXIF and makes thumbnails, the Categorizer classifies and Faces recognizes people.",
    "flow.s3.title": "Browse your way",
    "flow.s3.body": "By folder (grouped by drive), by date, by category, by album, by country, state and city, or by person.",
    "flow.s4.title": "Find any photo",
    "flow.s4.body": "Search by text, describe the photo to the AI, build filters on any field or pick a photo of someone to find every photo of that person.",
    "features.eyebrow": "Features",
    "features.title": "A catalog that understands your photos",
    "features.subtitle": "Built for tens of thousands of photos spread across several drives — and to find any of them in seconds.",
    "features.raw.title": "RAW from any camera",
    "features.raw.body": "JPEG, PNG, TIFF, WebP and RAW files from Canon, Nikon, Sony, Fuji, Olympus, Panasonic, Pentax, DNG and more — with the camera's own preview.",
    "features.exif.title": "Full EXIF",
    "features.exif.body": "Every tag of every photo goes into the catalog: camera, lens, ISO, aperture, shutter speed, GPS and the rest. The EXIF date is the photo's date.",
    "features.ai.title": "Categories by local AI",
    "features.ai.body": "16 categories and over 130 subcategories — beach, wedding, soccer, dog, food… The AI runs on your computer; create your own and teach it with phrases.",
    "features.people.title": "People",
    "features.people.body": "Faces are grouped into people automatically. Name them, merge them, remove or add faces — the app learns from your corrections.",
    "features.face.title": "Search by face",
    "features.face.body": "Pick a photo of a person and see every photo they appear in, sorted by similarity.",
    "features.places.title": "Places from GPS",
    "features.places.body": "Photos with GPS get a country, state and city — no internet needed: the 170,000-city database ships with the app.",
    "features.disks.title": "Drives and network",
    "features.disks.body": "Each folder knows which drive it came from (volume UUID, server and share). Drive disconnected? Thumbnails stay visible; new drive letter? The catalog adjusts itself.",
    "features.dupes.title": "Duplicates and look-alikes",
    "features.dupes.body": "The pixel hash finds the same photo with different EXIF; the hash tree finds resized copies and similar photos.",
    "features.safe.title": "Never changes the originals",
    "features.safe.body": "The app only reads your folders. Thumbnails, categories and people live in a separate catalog in your user folder.",
    "library.eyebrow": "Library",
    "library.title": "Six ways to look at the same collection",
    "library.body": "The side panel has a tree in each tab; the grid shows the photos of whatever you pick.",
    "library.c1": "Folders, grouped by source drive (local, USB or network)",
    "library.c2": "Dates: year, month and day, from the photo's date",
    "library.c3": "Categories and subcategories, from the AI and your own",
    "library.c4": "Albums and sub-albums you create",
    "library.c5": "Places: country, state and city",
    "library.c6": "People, with the names you give them",
    "viewer.eyebrow": "The photo",
    "viewer.title": "Everything known about each photo",
    "viewer.body": "Open a photo to see the people tagged in it, the camera and exposure, the file and drive, the place, the categories, the albums, identical or similar photos and every EXIF tag.",
    "viewer.c1": "Fix who is in the photo with one click",
    "viewer.c2": "Add and remove categories and albums",
    "viewer.c3": "Drive offline? The thumbnail is still there",
    "people.eyebrow": "People",
    "people.title": "Every face in the right place",
    "people.subtitle": "Face recognition runs on your computer. Each person has an identifier of their own; people who are similar enough, with enough faces, merge on their own.",
    "people.g1.title": "People catalog",
    "people.g1.body": "Name people, merge two of them, remove a face that is not theirs or add one that was missed.",
    "people.g2.title": "Search by face",
    "people.g2.body": "Pick a photo of someone: the app shows who it may be and every photo the person appears in.",
    "search.eyebrow": "Search and filters",
    "search.title": "Find by word, by description or by any field",
    "search.c1": "Text: name, folder, camera, lens, place, category, person, album",
    "search.c2": "AI: describe the photo (“sunset at the beach”) and see the closest ones",
    "search.c3": "Filters on any field — size, width, height, megapixels, date, ISO, lens, country, city, EXIF tag… — chained with AND or OR",
    "search.c4": "Save the filters you use all the time",
    "search.g1.title": "Places",
    "search.g1.body": "Country, state and city from GPS, with no internet. Light or dark theme.",
    "search.g2.title": "Dates",
    "search.g2.body": "Year, month and day from the EXIF date — or the file's, when the photo has no EXIF. In English or Portuguese.",
    "tasks.eyebrow": "Tasks",
    "tasks.title": "Indexing in plain sight, and under control",
    "tasks.subtitle": "Follow each stage — pending, in progress, done, failed and items per minute — and pause, resume, cancel or change its number of threads. Closing the app midway loses nothing.",
    "tasks.g1.title": "The four stages",
    "tasks.g1.body": "Searcher, Indexer, Categorizer and Faces, each with its own thread pool.",
    "tasks.g2.title": "Folders and drives",
    "tasks.g2.body": "Each drive's ID, network protocol, server and share — recognized on any system.",
    "data.eyebrow": "Your data",
    "data.title": "Everything on your computer",
    "data.body": "Your photos stay where they are. What PhotoIDX creates lives in a folder in your user account — which is why photos on a disconnected drive can still be browsed.",
    "data.body2": "The AI (categories, search and faces) runs on the computer itself: no photo ever leaves the machine.",
    "data.tree.db": "# the catalog",
    "data.tree.thumb": "# thumbnails",
    "data.tree.faces": "# faces",
    "dl.eyebrow": "Download",
    "dl.title": "Install and start indexing",
    "dl.subtitle": "Free, no account and no internet. Pick the installer for your system.",
    "dl.loading": "Checking the latest version…",
    "dl.openReleases": "Open the Releases",
    "dl.mac": "Open the <code>.dmg</code> and drag PhotoIDX to Applications.",
    "dl.macReq": "Requires macOS 14 or newer.",
    "dl.macArm": "Apple Silicon (M1 or newer)",
    "dl.win": "Open the <code>.msi</code>: the shortcut shows up in the Start Menu and on the Desktop.",
    "dl.winReq": "Requires Windows 10 or 11, 64-bit; on Windows 11 on ARM, use the ARM64 installer.",
    "dl.linux": "Extract the <code>.tar.gz</code> and run <code>bin/PhotoIDX</code>.",
    "dl.linuxReq": "Requires glibc 2.39 (Ubuntu 24.04 or newer).",
    "dl.checksum": "Every installer has a .sha256 file next to it to check its integrity.",
    "dl.allReleases": "See all versions →",
    "dl.note1": "<b>First launch:</b> the installers are not signed yet. On <b>macOS</b>, if you see “cannot verify the developer”, go to System Settings → Privacy &amp; Security → <b>Open Anyway</b>. On <b>Windows</b>, on the SmartScreen dialog, click <b>More info</b> → <b>Run anyway</b>.",
    "dl.note2": "<b>Face recognition:</b> uses insightface's <i>buffalo_sc</i> models, licensed for non-commercial research use only.",
    "dl.error": "Could not check the versions right now",
    "dl.errorHint": "The buttons open the Releases page on GitHub, where all installers are.",
    "dl.unavailable": "not available in this version",
    "dl.recommended": "Recommended for your system",
    "dl.heroFor": "PhotoIDX {version} for {os}",
    "dl.getFor": "Download for {os}",
    "dl.latest": "Latest version",
    "dl.published": "Version {version} · published on {date}",
    "dl.pickBelow": "Pick the installer below",
    "dl.os.macos-arm64": "macOS (Apple Silicon)",
    "dl.os.macos-x64": "macOS (Intel)",
    "dl.os.windows-x64": "Windows",
    "dl.os.windows-arm64": "Windows (ARM64)",
    "dl.os.linux-x64": "Linux x64",
    "dl.os.linux-arm64": "Linux arm64",
    "cta.eyebrow": "Part of the PhotoE family",
    "cta.title": "Free, open and built to last",
    "cta.body": "PhotoIDX is free software (BSD 2-Clause), by the author of PhotoEditor. Use it, study it and contribute.",
    "cta.download": "Download PhotoIDX",
    "cta.source": "Source code on GitHub",
    "footer.about": "PhotoIDX is a free photo catalog (BSD 2-Clause) with AI categories, face recognition and places, created by Helvio Junior.",
    "footer.pages": "On this page",
    "footer.links": "Links",
    "footer.license": "Distributed under the BSD 2-Clause license",
    "common.close": "Close",
    "shots.categories": "Library with the side panel of AI categories and the thumbnail grid",
    "shots.library": "Library with the folder tree grouped by drive and the thumbnail grid",
    "shots.viewer": "An open photo with the recognized face and the panel with camera, file, drive, place, categories and EXIF",
    "shots.people": "People catalog, named and unnamed",
    "shots.faceSearch": "Search by face from a photo picked on disk",
    "shots.filters": "Filter with three chained conditions: megapixels, country and camera",
    "shots.places": "Places tab, in the light theme",
    "shots.dates": "Dates tab, in English",
    "shots.tasks": "Tasks screen with the four indexing stages",
    "shots.folders": "Indexed folders grouped by source drive"
  };


  var CATALOGS = { en: EN, "pt-br": {} };

  // Atributo -> como ler/escrever o valor.
  var BINDINGS = [
    ["data-i18n", function (el) { return el.textContent; }, function (el, v) { el.textContent = v; }],
    ["data-i18n-html", function (el) { return el.innerHTML; }, function (el, v) { el.innerHTML = v; }],
    ["data-i18n-alt", function (el) { return el.getAttribute("alt"); }, function (el, v) { el.setAttribute("alt", v); }],
    ["data-i18n-aria", function (el) { return el.getAttribute("aria-label"); }, function (el, v) { el.setAttribute("aria-label", v); }],
    ["data-i18n-content", function (el) { return el.getAttribute("content"); }, function (el, v) { el.setAttribute("content", v); }],
    ["data-i18n-placeholder", function (el) { return el.getAttribute("placeholder"); }, function (el, v) { el.setAttribute("placeholder", v); }]
  ];

  function normalize(value) {
    var v = String(value || "").trim().toLowerCase().replace("_", "-");
    if (SUPPORTED.indexOf(v) !== -1) return v;
    var base = v.split("-")[0];
    if (base === "pt") return "pt-br";
    if (base === "en") return "en";
    return null;
  }

  function readCookie() {
    var row = document.cookie.split("; ").filter(function (r) { return r.indexOf(COOKIE + "=") === 0; })[0];
    if (!row) return null;
    try { return normalize(decodeURIComponent(row.slice(COOKIE.length + 1))); } catch (e) { return null; }
  }

  function writeCookie(value) {
    var lang = normalize(value);
    if (!lang || SUPPORTED.indexOf(lang) === -1) return;
    var secure = window.location.protocol === "https:" ? "; Secure" : "";
    document.cookie = COOKIE + "=" + encodeURIComponent(lang) + "; Path=/; Max-Age=" + ONE_YEAR + "; SameSite=Lax" + secure;
  }

  function detectBrowser() {
    var list = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language];
    for (var i = 0; i < list.length; i++) {
      var n = normalize(list[i]);
      if (n) return n;
    }
    return FALLBACK;
  }

  // PT-BR vem do próprio HTML: captura antes da primeira troca de idioma.
  function capturePortuguese() {
    var pt = CATALOGS["pt-br"];
    BINDINGS.forEach(function (b) {
      document.querySelectorAll("[" + b[0] + "]").forEach(function (el) {
        var key = el.getAttribute(b[0]);
        if (!(key in pt)) pt[key] = b[1](el);
      });
    });
  }

  var listeners = [];
  var api = {
    lang: FALLBACK,
    // t("chave", "fallback em português") — idioma ativo -> PT-BR -> fallback.
    t: function (key, fallback) {
      var cat = CATALOGS[api.lang] || {};
      if (key in cat) return cat[key];
      if (key in CATALOGS[FALLBACK]) return CATALOGS[FALLBACK][key];
      return fallback !== undefined ? fallback : key;
    },
    tf: function (key, params, fallback) {
      return api.t(key, fallback).replace(/\{(\w+)\}/g, function (m, p) {
        return params && p in params ? params[p] : m;
      });
    },
    apply: function () {
      BINDINGS.forEach(function (b) {
        document.querySelectorAll("[" + b[0] + "]").forEach(function (el) {
          b[2](el, api.t(el.getAttribute(b[0])));
        });
      });
      document.documentElement.setAttribute("lang", api.lang === "en" ? "en" : "pt-BR");
      document.querySelectorAll("[data-lang-select]").forEach(function (s) { s.value = api.lang; });
      listeners.forEach(function (fn) { fn(api.lang); });
    },
    set: function (value, persist) {
      api.lang = normalize(value) || FALLBACK;
      if (persist) writeCookie(api.lang);
      api.apply();
    },
    onChange: function (fn) { listeners.push(fn); }
  };

  capturePortuguese();
  api.lang = readCookie() || detectBrowser();
  if (api.lang !== FALLBACK) api.apply();
  else document.querySelectorAll("[data-lang-select]").forEach(function (s) { s.value = api.lang; });

  document.querySelectorAll("[data-lang-select]").forEach(function (s) {
    s.addEventListener("change", function () { api.set(s.value, true); });
  });

  window.PIX_I18N = api;
})();

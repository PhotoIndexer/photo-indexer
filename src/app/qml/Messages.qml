pragma Singleton
import QtQuick

// Textos das chaves que o C++ manda ao QML: erros (sinal ``failed(key,
// params)``), avisos (``notice``), nomes das etapas, dos campos do filtro e
// dos operadores. Os textos em inglês são a fonte do qsTr (o padrão e o
// fallback); a tradução vem dos .ts.
//
//   Dialogs.alert(Messages.errorDialog(key, params))
//   Text { text: Messages.fieldName("width") }
QtObject {
    id: messages

    /// Escapa texto para StyledText (nomes de arquivo com < > &).
    function escapeHtml(text) {
        return String(text === undefined || text === null ? "" : text)
            .replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    /// Último componente de um caminho ("/a/b/Evento" -> "Evento").
    function baseName(path) {
        const parts = String(path || "").split(/[\\/]+/).filter((p) => p !== "");
        return parts.length ? parts[parts.length - 1] : String(path || "");
    }

    // ------------------------------------------------------------------ //
    // Erros
    // ------------------------------------------------------------------ //

    function error(key, params) {
        const p = params || {};
        const path = p.path !== undefined ? String(p.path) : "";
        switch (key) {
        case "folder.notFound":
            return qsTr("The folder “%1” does not exist or cannot be read.").arg(path);
        case "folder.alreadyAdded":
            return qsTr("The folder “%1” is already in the catalog.").arg(path);
        case "folder.insideOther":
            return qsTr("The folder “%1” is already indexed as part of “%2”.").arg(path).arg(p.parent || "");
        case "folder.appData":
            return qsTr("“%1” is where %2 keeps its thumbnails and data, so it cannot be indexed.").arg(path).arg(App.brandName);
        case "category.emptyName":
        case "album.emptyName":
        case "filter.emptyName":
            return qsTr("Enter a name.");
        case "category.cycle":
            return qsTr("A category cannot go inside itself or one of its subcategories.");
        case "album.cycle":
            return qsTr("An album cannot go inside itself or one of its sub-albums.");
        case "image.unreadable":
            return qsTr("The image could not be opened.");
        case "faces.unavailable":
            return qsTr("Face recognition is not available in this copy of the app (the model is missing).");
        case "faces.noneFound":
            return qsTr("No face was found in this photo. Try a photo where the face is larger and well lit.");
        case "error.unexpected":
        case "error.generic":
            return qsTr("Something went wrong. Please try again.");
        }
        if (key)
            console.warn("No message for error key", key);
        return qsTr("Something went wrong. Please try again.");
    }

    function errorTitle(key) {
        const k = String(key);
        if (k.startsWith("folder."))
            return qsTr("Could not add the folder");
        if (k.startsWith("faces."))
            return qsTr("Face search");
        if (k.startsWith("category."))
            return qsTr("Categories");
        if (k.startsWith("album."))
            return qsTr("Albums");
        return qsTr("Something went wrong");
    }

    /// Opções prontas para Dialogs.alert(): título, mensagem e o detalhe técnico.
    function errorDialog(key, params) {
        const p = params || {};
        let description = escapeHtml(error(key, p));
        if (p.detail)
            description += "<br><br><small>" + escapeHtml(p.detail) + "</small>";
        return { title: errorTitle(key), description: description, variant: "danger", iconName: "circle-alert" };
    }

    // ------------------------------------------------------------------ //
    // Avisos (faixa que some sozinha)
    // ------------------------------------------------------------------ //

    function notice(key, params) {
        const p = params || {};
        switch (key) {
        case "folder.added":
            return qsTr("Folder added: %1. Looking for photos…").arg(p.path || "");
        case "scan.finished":
            return qsTr("Scan finished: %n image(s) found", "", Number(p.found || 0))
                + (p.queued ? qsTr(", %1 new or changed").arg(p.queued) : "")
                + (p.missing ? qsTr(", %1 no longer on disk").arg(p.missing) : "") + ".";
        case "tasks.requeued":
            return qsTr("%n photo(s) back in the queue of “%1”.", "", Number(p.count || 0)).arg(stageName(p.stage));
        case "people.reclustered":
            return qsTr("People regrouped.");
        case "people.merged":
            return qsTr("%n merge(s) of similar people.", "", Number(p.count || 0));
        case "filter.saved":
            return qsTr("Filter saved.");
        }
        return "";
    }

    // ------------------------------------------------------------------ //
    // Etapas (tela de Tarefas)
    // ------------------------------------------------------------------ //

    function stageName(key) {
        switch (key) {
        case "search": return qsTr("Searcher");
        case "index": return qsTr("Indexer");
        case "categorize": return qsTr("Categorizer");
        case "faces": return qsTr("Faces");
        }
        return String(key || "");
    }

    function stageDescription(key) {
        switch (key) {
        case "search": return qsTr("Walks the indexed folders and their subfolders looking for images (JPEG, PNG, HEIC, RAW…).");
        case "index": return qsTr("Opens each file: all EXIF tags, thumbnail, pixel hash, similarity hash and location.");
        case "categorize": return qsTr("Local AI (CLIP) puts each photo in categories and prepares the AI search.");
        case "faces": return qsTr("Finds faces, computes each one's biometric signature and groups them into people.");
        }
        return "";
    }

    function stageIcon(key) {
        switch (key) {
        case "search": return "folder-search";
        case "index": return "hash";
        case "categorize": return "sparkles";
        case "faces": return "scan-face";
        }
        return "activity";
    }

    function stageState(state) {
        switch (state) {
        case "running": return qsTr("Running");
        case "idle": return qsTr("Waiting for work");
        case "paused": return qsTr("Paused");
        case "unavailable": return qsTr("Model not installed");
        }
        return String(state || "");
    }

    // ------------------------------------------------------------------ //
    // Biblioteca
    // ------------------------------------------------------------------ //

    function tabName(tab) {
        switch (tab) {
        case "folders": return qsTr("Folders");
        case "dates": return qsTr("Dates");
        case "categories": return qsTr("Categories");
        case "albums": return qsTr("Albums");
        case "places": return qsTr("Places");
        case "people": return qsTr("People");
        }
        return String(tab || "");
    }

    function sortName(sort) {
        switch (sort) {
        case "date-desc": return qsTr("Newest first");
        case "date-asc": return qsTr("Oldest first");
        case "name": return qsTr("File name");
        case "added": return qsTr("Recently added");
        case "size": return qsTr("Largest files");
        case "resolution": return qsTr("Highest resolution");
        case "folder": return qsTr("Folder");
        }
        return String(sort || "");
    }

    function diskKind(kind) {
        switch (kind) {
        case "local": return qsTr("Local disk");
        case "removable": return qsTr("Removable disk");
        case "network": return qsTr("Network share");
        }
        return String(kind || "");
    }

    function categorySource(source) {
        switch (source) {
        case "ai": return qsTr("by AI");
        case "user": return qsTr("by you");
        case "rule": return qsTr("automatic");
        }
        return "";
    }

    // ------------------------------------------------------------------ //
    // Filtro personalizado
    // ------------------------------------------------------------------ //

    function fieldGroup(group) {
        switch (group) {
        case "file": return qsTr("File");
        case "image": return qsTr("Image");
        case "date": return qsTr("Date");
        case "camera": return qsTr("Camera");
        case "location": return qsTr("Location");
        case "organize": return qsTr("Organization");
        case "exif": return qsTr("EXIF");
        }
        return String(group || "");
    }

    function fieldName(key) {
        switch (key) {
        case "name": return qsTr("File name");
        case "folder": return qsTr("Folder");
        case "format": return qsTr("Format");
        case "raw": return qsTr("Is RAW");
        case "sizeMb": return qsTr("File size (MB)");
        case "fileModified": return qsTr("File modified");
        case "added": return qsTr("Added to catalog");
        case "disk": return qsTr("Disk name");
        case "diskKind": return qsTr("Disk type");
        case "pictureHash": return qsTr("Pixel hash");
        case "width": return qsTr("Width (px)");
        case "height": return qsTr("Height (px)");
        case "megapixels": return qsTr("Megapixels");
        case "shape": return qsTr("Orientation");
        case "sharpness": return qsTr("Sharpness");
        case "faces": return qsTr("Number of faces");
        case "taken": return qsTr("Date taken");
        case "year": return qsTr("Year");
        case "month": return qsTr("Month");
        case "dateSource": return qsTr("Date source");
        case "modified": return qsTr("Date edited (EXIF)");
        case "camera": return qsTr("Camera");
        case "make": return qsTr("Camera make");
        case "model": return qsTr("Camera model");
        case "lens": return qsTr("Lens");
        case "iso": return qsTr("ISO");
        case "aperture": return qsTr("Aperture (f/)");
        case "shutter": return qsTr("Shutter speed (s)");
        case "focalLength": return qsTr("Focal length (mm)");
        case "focalLength35": return qsTr("Focal length 35mm (mm)");
        case "exposureBias": return qsTr("Exposure compensation (EV)");
        case "flashFired": return qsTr("Flash fired");
        case "software": return qsTr("Software");
        case "artist": return qsTr("Artist");
        case "copyright": return qsTr("Copyright");
        case "description": return qsTr("Description");
        case "hasGps": return qsTr("Has GPS");
        case "country": return qsTr("Country");
        case "state": return qsTr("State");
        case "city": return qsTr("City");
        case "latitude": return qsTr("Latitude");
        case "longitude": return qsTr("Longitude");
        case "altitude": return qsTr("Altitude (m)");
        case "category": return qsTr("Category");
        case "album": return qsTr("Album");
        case "person": return qsTr("Person");
        case "exif": return qsTr("EXIF tag");
        }
        return String(key || "");
    }

    function opName(op) {
        switch (op) {
        case "contains": return qsTr("contains");
        case "notContains": return qsTr("does not contain");
        case "eq": return qsTr("is");
        case "ne": return qsTr("is not");
        case "startsWith": return qsTr("starts with");
        case "endsWith": return qsTr("ends with");
        case "empty": return qsTr("is empty");
        case "notEmpty": return qsTr("is not empty");
        case "gt": return qsTr("greater than");
        case "gte": return qsTr("at least");
        case "lt": return qsTr("less than");
        case "lte": return qsTr("at most");
        case "between": return qsTr("between");
        case "before": return qsTr("before");
        case "after": return qsTr("after");
        case "isTrue": return qsTr("yes");
        case "isFalse": return qsTr("no");
        case "has": return qsTr("is in");
        case "hasNot": return qsTr("is not in");
        case "any": return qsTr("has any");
        case "none": return qsTr("has none");
        case "exists": return qsTr("exists");
        case "notExists": return qsTr("does not exist");
        }
        return String(op || "");
    }

    /// Operadores que não pedem valor.
    function opTakesValue(op) {
        return ["empty", "notEmpty", "isTrue", "isFalse", "any", "none"].indexOf(op) < 0;
    }

    function choiceName(value) {
        switch (value) {
        case "landscape": return qsTr("Landscape");
        case "portrait": return qsTr("Portrait");
        case "square": return qsTr("Square");
        case "exif": return qsTr("EXIF");
        case "file": return qsTr("File");
        case "local": return qsTr("Local disk");
        case "removable": return qsTr("Removable disk");
        case "network": return qsTr("Network share");
        }
        return String(value || "");
    }

    // ------------------------------------------------------------------ //
    // Ajustes da IA (Tarefas)
    // ------------------------------------------------------------------ //

    function settingName(key) {
        switch (key) {
        case "minProbability": return qsTr("Minimum category confidence");
        case "maxCategories": return qsTr("Categories per photo (max.)");
        case "blurThreshold": return qsTr("Blurry below sharpness");
        case "minPersonScore": return qsTr("Face confidence to join People");
        case "matchSimilarity": return qsTr("Same person from similarity");
        case "mergeSimilarity": return qsTr("Merge people from similarity");
        case "mergeMinFaces": return qsTr("Merge only with at least N faces each");
        case "mergeMinScore": return qsTr("Merge only with detection confidence of");
        }
        return String(key || "");
    }

    function settingHelp(key) {
        switch (key) {
        case "minProbability": return qsTr("How sure the AI must be to put a photo in a category. Higher: fewer, more precise categories.");
        case "maxCategories": return qsTr("At most this many categories per photo, the most likely first.");
        case "blurThreshold": return qsTr("Photos below this sharpness go to “Blurry / Low quality”.");
        case "minPersonScore": return qsTr("Faces detected with less confidence (animals, objects, tiny faces in the background) stay searchable but do not form people.");
        case "matchSimilarity": return qsTr("A new face joins the most similar person from this similarity on (0.45, as in PhotoE).");
        case "mergeSimilarity": return qsTr("Two people are merged automatically when their average faces are at least this similar.");
        case "mergeMinFaces": return qsTr("Confidence: only people with at least this many faces take part in automatic merges.");
        case "mergeMinScore": return qsTr("Confidence: only people whose faces were detected with at least this average confidence.");
        }
        return "";
    }
}

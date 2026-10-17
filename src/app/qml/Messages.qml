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
        case "group.emptyName":
        case "filter.emptyName":
            return qsTr("Enter a name.");
        case "files.busy":
            return qsTr("Another file operation is still running. Wait for it to finish.");
        case "files.scanRunning":
            return qsTr("A folder scan is running. Wait for it to finish before moving or deleting files.");
        case "files.outsideCatalog":
            return qsTr("“%1” is not inside an indexed folder. Photos can only be moved to folders of the catalog.").arg(p.name || "");
        case "files.rootFolder":
            return qsTr("“%1” is an indexed folder. Manage it in Folders and disks.").arg(p.name || "");
        case "files.intoItself":
            return qsTr("A folder cannot go inside itself or one of its subfolders.");
        case "files.exists":
            return qsTr("There is already a file or folder called “%1”.").arg(p.name || "");
        case "files.badName":
            return qsTr("“%1” is not a valid name.").arg(p.name || "");
        case "files.sourceMissing":
            return qsTr("“%1” is no longer on the disk (or the disk is disconnected).").arg(p.name || "");
        case "files.targetMissing":
            return qsTr("The destination folder is not available (is the disk connected?).");
        case "files.readFailed":
        case "files.writeFailed":
        case "files.moveFailed":
            return qsTr("“%1” could not be moved. Check that the disk is connected and writable.").arg(p.name || "");
        case "files.verifyFailed":
            return qsTr("The copy of “%1” did not match the original, so the original was kept where it was.").arg(p.name || "");
        case "files.noTrash":
            return qsTr("“%1” is on a disk without a Trash (a network share, for example). %2 never deletes files permanently, so nothing was deleted.")
                .arg(p.name || "").arg(App.brandName);
        case "group.notFound":
            return qsTr("This group no longer exists.");
        case "category.cycle":
            return qsTr("A category cannot go inside itself or one of its subcategories.");
        case "album.cycle":
            return qsTr("An album cannot go inside itself or one of its sub-albums.");
        case "album.privateRoot":
            return qsTr("The Private album cannot be renamed, moved or deleted.");
        case "album.privateBoundary":
            return qsTr("An album cannot move into or out of the Private album. To protect photos, make them private.");
        case "vault.noPassword":
            return qsTr("Create the password for private photos first.");
        case "vault.locked":
            return qsTr("Private photos are locked. Enter the password to continue.");
        case "vault.wrongPassword":
            return qsTr("Wrong password.");
        case "vault.passwordShort":
            return qsTr("The password needs at least %1 characters.").arg(p.min || 8);
        case "vault.passwordMismatch":
            return qsTr("The passwords do not match.");
        case "vault.passwordExists":
            return qsTr("A password is already set. Use “Change password”.");
        case "vault.invalidKey":
            return qsTr("This is not a valid key. Check it was copied completely (14 groups of 4 characters).");
        case "vault.wrongKey":
            return qsTr("This key is valid but is not the key of this catalog.");
        case "vault.failed":
            return qsTr("The password could not be saved.");
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
        if (k.startsWith("vault."))
            return qsTr("Private photos");
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
        case "people.regrouping":
            return qsTr("Regrouping people with the improved rule… This runs in the background.");
        case "people.reclustered":
            return p.count !== undefined ? qsTr("People regrouped: %n person(s).", "", Number(p.count)) : qsTr("People regrouped.");
        case "people.merged":
            return qsTr("%n merge(s) of similar people.", "", Number(p.count || 0));
        case "filter.saved":
            return qsTr("Filter saved.");
        case "disk.online":
            return qsTr("“%1” is connected again. Looking for new and changed photos…").arg(p.label || "");
        case "disk.offline":
            return qsTr("“%1” was disconnected. Its photos stay browsable through the thumbnails.").arg(p.label || "");
        case "vault.keyCopied":
            return qsTr("Key copied. The clipboard is cleared in 2 minutes — paste it somewhere safe now.");
        case "files.moving":
            return qsTr("Moving %n photo(s)…", "", Number(p.count || 0));
        case "files.moved":
            return Number(p.count) === Number(p.total) ? qsTr("%n photo(s) moved.", "", Number(p.count || 0))
                : qsTr("%1 of %n photo(s) moved.", "", Number(p.total || 0)).arg(p.count || 0);
        case "files.trashed":
            return Number(p.count) === Number(p.total) ? qsTr("%n photo(s) moved to the Trash.", "", Number(p.count || 0))
                : qsTr("%1 of %n photo(s) moved to the Trash.", "", Number(p.total || 0)).arg(p.count || 0);
        case "files.folderMoved":
            return qsTr("Folder “%1” moved.").arg(p.name || "");
        case "files.folderTrashed":
            return qsTr("Folder “%1” moved to the Trash.").arg(p.name || "");
        case "vault.autoLocked":
            return qsTr("Private photos were locked after a period without use.");
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
        case "privacy": return qsTr("Private photos");
        }
        return String(key || "");
    }

    function stageDescription(key) {
        switch (key) {
        case "search": return qsTr("Walks the indexed folders and their subfolders looking for images (JPEG, PNG, HEIC, RAW…).");
        case "index": return qsTr("Opens each file: all EXIF tags, thumbnail, pixel hash, similarity hash and location.");
        case "categorize": return qsTr("Local AI (CLIP) puts each photo in categories and prepares the AI search.");
        case "faces": return qsTr("Finds faces, computes each one's biometric signature and groups them into people.");
        case "privacy": return qsTr("Encrypts the original file and the thumbnail of photos made private (and decrypts the ones made public). Runs only while private photos are unlocked.");
        }
        return "";
    }

    function stageIcon(key) {
        switch (key) {
        case "search": return "folder-search";
        case "index": return "hash";
        case "categorize": return "sparkles";
        case "faces": return "scan-face";
        case "privacy": return "lock";
        }
        return "activity";
    }

    function stageState(state) {
        switch (state) {
        case "running": return qsTr("Running");
        case "idle": return qsTr("Waiting for work");
        case "paused": return qsTr("Paused");
        case "unavailable": return qsTr("Model not installed");
        case "locked": return qsTr("Locked");
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
        case "parentProbability": return qsTr("Use the main group when unsure");
        case "siblingMargin": return qsTr("Unsure between similar categories from");
        case "blurThreshold": return qsTr("Blurry below sharpness");
        case "minPersonScore": return qsTr("Face confidence to join People");
        case "minPersonPhotos": return qsTr("People without a name: show from N photos");
        case "matchSimilarity": return qsTr("Same person from similarity");
        case "mergeSimilarity": return qsTr("Merge people from similarity");
        case "mergeMinFaces": return qsTr("Merge only with at least N faces each");
        case "mergeMinScore": return qsTr("Merge only with detection confidence of");
        }
        return String(key || "");
    }

    function settingHelp(key) {
        switch (key) {
        case "minProbability": return qsTr("How sure the AI must be to put a photo in a category. Higher: fewer, more precise categories. Photos that match nothing stay uncategorized.");
        case "maxCategories": return qsTr("At most this many categories per photo, the most likely first, never two from the same group.");
        case "parentProbability": return qsTr("When the AI cannot tell similar categories apart (basketball or volleyball?), the photo goes to the group (Sports) if the group is at least this likely.");
        case "siblingMargin": return qsTr("Two categories of the same group count as a tie when the second has at least this fraction of the first's confidence. Lower: more photos go to the group.");
        case "blurThreshold": return qsTr("Photos below this sharpness go to “Blurry / Low quality”.");
        case "minPersonScore": return qsTr("Faces detected with less confidence (the back of a head, profiles, blur, animals, objects) stay searchable but do not form people.");
        case "minPersonPhotos": return qsTr("Unnamed people with fewer photos stay out of the People screen and the side panel (they still show up in “Search by photo”). People with a name always show, first.");
        case "matchSimilarity": return qsTr("A new face joins the most similar person from this similarity on. Two faces of the same photo are never the same person.");
        case "mergeSimilarity": return qsTr("Two people are merged automatically when their faces are, on average, at least this similar to each other — and never if they appear together in a photo.");
        case "mergeMinFaces": return qsTr("Confidence: only people with at least this many faces take part in automatic merges.");
        case "mergeMinScore": return qsTr("Confidence: only people whose faces were detected with at least this average confidence.");
        }
        return "";
    }
}

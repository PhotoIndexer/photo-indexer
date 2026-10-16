-- Catálogo do PhotoIDX, passo 1. Nunca edite um passo publicado: mudança
-- de esquema é um NNNN.sql novo (regra 8). Um comando pode ocupar várias
-- linhas; termina na linha que acaba em ';'. Sem triggers (o ';' do corpo
-- quebraria a separação).

-- Volume de onde vêm as pastas indexadas: disco local, removível ou
-- compartilhamento de rede. ``disk_key`` é a identidade estável (UUID do
-- volume, GUID do Windows, smb://servidor/compartilhamento) — é por ela que o
-- mesmo disco é reconhecido em outro ponto de montagem ou letra.
CREATE TABLE disk (
    id TEXT PRIMARY KEY,
    disk_key TEXT NOT NULL UNIQUE,
    label TEXT NOT NULL DEFAULT '',
    kind TEXT NOT NULL DEFAULT 'local',
    protocol TEXT NOT NULL DEFAULT '',
    server TEXT NOT NULL DEFAULT '',
    share TEXT NOT NULL DEFAULT '',
    fs_type TEXT NOT NULL DEFAULT '',
    device TEXT NOT NULL DEFAULT '',
    serial TEXT NOT NULL DEFAULT '',
    model TEXT NOT NULL DEFAULT '',
    mount_point TEXT NOT NULL DEFAULT '',
    total_bytes INTEGER NOT NULL DEFAULT 0,
    online INTEGER NOT NULL DEFAULT 1,
    first_seen_at TEXT NOT NULL,
    last_seen_at TEXT NOT NULL
);

-- Pasta raiz que a pessoa mandou indexar (o Searcher desce por ela).
CREATE TABLE folder (
    id TEXT PRIMARY KEY,
    disk_id TEXT NOT NULL REFERENCES disk(id),
    path TEXT NOT NULL UNIQUE,
    rel_path TEXT NOT NULL DEFAULT '',
    added_at TEXT NOT NULL,
    scanned_at TEXT
);

-- Cidade/estado/país da geolocalização reversa (GeoNames, offline).
CREATE TABLE place (
    id TEXT PRIMARY KEY,
    country_code TEXT NOT NULL,
    country TEXT NOT NULL,
    state TEXT NOT NULL DEFAULT '',
    city TEXT NOT NULL DEFAULT '',
    UNIQUE (country_code, state, city)
);

-- Uma foto. ``taken_at`` é a data de criação: a do EXIF (relógio da câmera)
-- ou, sem EXIF, a de criação do arquivo; ``year/month/day`` saem dela.
-- Estados: ``state`` (Indexer) pending | indexed | error | missing;
-- ``ai_state`` (Categorizer) e ``faces_state`` (Faces) pending | done |
-- error | skipped.
CREATE TABLE photo (
    id TEXT PRIMARY KEY,
    folder_id TEXT NOT NULL REFERENCES folder(id) ON DELETE CASCADE,
    path TEXT NOT NULL UNIQUE,
    dir TEXT NOT NULL,
    name TEXT NOT NULL,
    ext TEXT NOT NULL DEFAULT '',
    format TEXT NOT NULL DEFAULT '',
    is_raw INTEGER NOT NULL DEFAULT 0,
    size INTEGER NOT NULL DEFAULT 0,
    file_mtime INTEGER NOT NULL DEFAULT 0,
    file_created_at TEXT,
    file_modified_at TEXT,
    state TEXT NOT NULL DEFAULT 'pending',
    error TEXT NOT NULL DEFAULT '',
    width INTEGER NOT NULL DEFAULT 0,
    height INTEGER NOT NULL DEFAULT 0,
    orientation INTEGER NOT NULL DEFAULT 1,
    taken_at TEXT,
    taken_source TEXT NOT NULL DEFAULT '',
    taken_offset TEXT NOT NULL DEFAULT '',
    year INTEGER,
    month INTEGER,
    day INTEGER,
    modified_at TEXT,
    make TEXT NOT NULL DEFAULT '',
    model TEXT NOT NULL DEFAULT '',
    camera TEXT NOT NULL DEFAULT '',
    lens TEXT NOT NULL DEFAULT '',
    software TEXT NOT NULL DEFAULT '',
    artist TEXT NOT NULL DEFAULT '',
    copyright TEXT NOT NULL DEFAULT '',
    description TEXT NOT NULL DEFAULT '',
    iso INTEGER,
    aperture REAL,
    shutter REAL,
    focal_length REAL,
    focal_length_35 INTEGER,
    exposure_bias REAL,
    flash INTEGER,
    latitude REAL,
    longitude REAL,
    altitude REAL,
    place_id TEXT REFERENCES place(id) ON DELETE SET NULL,
    thumb_path TEXT NOT NULL DEFAULT '',
    thumb_width INTEGER NOT NULL DEFAULT 0,
    thumb_height INTEGER NOT NULL DEFAULT 0,
    picture_hash TEXT NOT NULL DEFAULT '',
    phash TEXT NOT NULL DEFAULT '',
    sharpness REAL,
    exif_count INTEGER NOT NULL DEFAULT 0,
    ai_state TEXT NOT NULL DEFAULT 'pending',
    faces_state TEXT NOT NULL DEFAULT 'pending',
    face_count INTEGER NOT NULL DEFAULT 0,
    added_at TEXT NOT NULL,
    indexed_at TEXT,
    categorized_at TEXT,
    faces_at TEXT
);
CREATE INDEX photo_folder ON photo(folder_id);
CREATE INDEX photo_dir ON photo(dir);
CREATE INDEX photo_state ON photo(state);
CREATE INDEX photo_date ON photo(year, month, day);
CREATE INDEX photo_taken ON photo(taken_at);
CREATE INDEX photo_picture_hash ON photo(picture_hash);
CREATE INDEX photo_phash ON photo(phash);
CREATE INDEX photo_place ON photo(place_id);
CREATE INDEX photo_ai_state ON photo(ai_state);
CREATE INDEX photo_faces_state ON photo(faces_state);

-- TODAS as tags EXIF da foto (IFD0, IFD1, Exif, GPS, Interop...).
CREATE TABLE exif_tag (
    photo_id TEXT NOT NULL REFERENCES photo(id) ON DELETE CASCADE,
    ifd TEXT NOT NULL,
    tag INTEGER NOT NULL,
    name TEXT NOT NULL,
    type INTEGER NOT NULL,
    value TEXT NOT NULL,
    PRIMARY KEY (photo_id, ifd, tag)
) WITHOUT ROWID;
CREATE INDEX exif_tag_name ON exif_tag(name);

-- Categorias (árvore). ``builtin_key`` = das que vêm com o app (o nome delas
-- é traduzido na interface, salvo se a pessoa renomeou); ``prompt`` = textos
-- em inglês para o CLIP, separados por '|'; ``auto`` = a IA pode atribuir.
CREATE TABLE category (
    id TEXT PRIMARY KEY,
    parent_id TEXT REFERENCES category(id) ON DELETE CASCADE,
    builtin_key TEXT UNIQUE,
    name TEXT NOT NULL,
    renamed INTEGER NOT NULL DEFAULT 0,
    prompt TEXT NOT NULL DEFAULT '',
    auto INTEGER NOT NULL DEFAULT 1,
    position INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);
CREATE INDEX category_parent ON category(parent_id);

-- Foto <-> categoria. ``source``: ai (o Categorizer pôs), user (a pessoa
-- pôs) ou rejected (a pessoa tirou: a IA não põe de novo).
CREATE TABLE photo_category (
    photo_id TEXT NOT NULL REFERENCES photo(id) ON DELETE CASCADE,
    category_id TEXT NOT NULL REFERENCES category(id) ON DELETE CASCADE,
    score REAL NOT NULL DEFAULT 0,
    source TEXT NOT NULL DEFAULT 'ai',
    PRIMARY KEY (photo_id, category_id)
) WITHOUT ROWID;
CREATE INDEX photo_category_category ON photo_category(category_id);

-- Álbuns da pessoa (árvore).
CREATE TABLE album (
    id TEXT PRIMARY KEY,
    parent_id TEXT REFERENCES album(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    position INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);
CREATE INDEX album_parent ON album(parent_id);

CREATE TABLE album_photo (
    album_id TEXT NOT NULL REFERENCES album(id) ON DELETE CASCADE,
    photo_id TEXT NOT NULL REFERENCES photo(id) ON DELETE CASCADE,
    added_at TEXT NOT NULL,
    PRIMARY KEY (album_id, photo_id)
) WITHOUT ROWID;
CREATE INDEX album_photo_photo ON album_photo(photo_id);

-- Embedding CLIP da foto (busca por IA).
CREATE TABLE photo_embedding (
    photo_id TEXT PRIMARY KEY REFERENCES photo(id) ON DELETE CASCADE,
    model TEXT NOT NULL,
    vector BLOB NOT NULL
);

-- Pessoa: um agrupamento de faces. Nasce sozinha (sem nome) quando uma face
-- não casa com ninguém; o nome é opcional. ``centroid`` = média normalizada
-- das faces.
CREATE TABLE person (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL DEFAULT '',
    cover_face_id TEXT,
    centroid BLOB,
    face_count INTEGER NOT NULL DEFAULT 0,
    photo_count INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL
);

-- Um rosto numa foto. Caixa normalizada (0..1) na imagem orientada.
-- ``assignment``: auto (o agrupamento pôs) ou user (a pessoa confirmou/pôs).
CREATE TABLE face (
    id TEXT PRIMARY KEY,
    photo_id TEXT NOT NULL REFERENCES photo(id) ON DELETE CASCADE,
    person_id TEXT REFERENCES person(id) ON DELETE SET NULL,
    x REAL NOT NULL,
    y REAL NOT NULL,
    w REAL NOT NULL,
    h REAL NOT NULL,
    score REAL NOT NULL,
    embedding BLOB NOT NULL,
    model TEXT NOT NULL,
    crop_path TEXT NOT NULL DEFAULT '',
    assignment TEXT NOT NULL DEFAULT 'auto',
    similarity REAL NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);
CREATE INDEX face_photo ON face(photo_id);
CREATE INDEX face_person ON face(person_id);

-- "Esta face NÃO é esta pessoa" (falso positivo corrigido): o agrupamento
-- automático nunca a devolve para lá.
CREATE TABLE face_rejection (
    face_id TEXT NOT NULL REFERENCES face(id) ON DELETE CASCADE,
    person_id TEXT NOT NULL REFERENCES person(id) ON DELETE CASCADE,
    created_at TEXT NOT NULL,
    PRIMARY KEY (face_id, person_id)
) WITHOUT ROWID;

-- Preferências do catálogo (limiares da IA, threads de cada etapa...).
CREATE TABLE setting (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

-- Filtros salvos da grade: regras encadeadas (JSON: {match: all|any,
-- rules: [{field, op, value, value2}]}).
CREATE TABLE saved_filter (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    definition TEXT NOT NULL,
    created_at TEXT NOT NULL
);

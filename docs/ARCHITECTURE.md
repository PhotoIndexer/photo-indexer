# PhotoIDX — arquitetura

App desktop nativo para Windows, macOS e Linux (x64 e arm64): **C++20 + Qt 6
(Qt Quick/QML)**, OpenCV estático, LibRaw e ONNX Runtime. Um processo só, sem
servidor. As convenções obrigatórias estão no [`CLAUDE.md`](../CLAUDE.md).

## Stack

| Peça | Versão | Para quê |
|---|---|---|
| Qt (Core, Gui, Quick, QuickControls2, Sql, Concurrent, Svg) | 6.8.3 | interface, SQLite, threads |
| OpenCV (estático: core, imgproc, imgcodecs) | 4.12.0 | JPEG/PNG, redimensionar, alinhar rostos |
| LibRaw (estático, compilado com o app) | 0.21.4 | RAW: metadados, EXIF completo, prévia embutida, sensor |
| ONNX Runtime (pacote oficial) | 1.30.0 (1.23.2 no Mac Intel) | CLIP e insightface em CPU |
| CLIP ViT-B/32 (int8) | Xenova/clip-vit-base-patch32 | categorias e busca por IA |
| insightface buffalo_sc | v0.7 | faces (o mesmo do ../photoe) |
| GeoNames cities1000 | gerado por `packaging/geonames.py` | GPS → cidade/estado/país, offline |
| Mbed TLS (só mbedcrypto, estático) | 3.6.4 | AES-256-GCM e PBKDF2-HMAC-SHA512 das fotos privadas |

## Camadas

```
src/imaging   sem banco e sem interface
     │          Formats, Exif (todas as tags), ImageIO, Raw (LibRaw), Hashes,
     │          Inspect (o trabalho do Indexer), Clip, Faces, Onnx, Embedding
src/core      o catálogo e as etapas
     │          Database (+schema/), Catalog*.cpp, Pipeline, Disks, Places,
     │          HashTree (BK-tree), FaceIndex, Taxonomy, Paths,
     │          Crypto (formato .photoidxe), Vault (chaveiro), Privacy
src/app       main, controllers (App, library, viewer, people, tasks, privacy),
              PhotoGridModel, TreeModel, image providers, qml/
```

## As etapas (Pipeline)

Cada etapa tem o seu `QThreadPool` (threads ajustáveis na tela de Tarefas). A
fila de cada uma é o estado da foto no banco — fechar o app no meio não perde
nada; o banco é só da thread principal.

| Etapa | Faz | Estado da foto |
|---|---|---|
| **Searcher** | desce pelas pastas indexadas (uma por thread), pula ocultas, `@eaDir`, lixeiras e a pasta de dados do app; arquivo novo ou alterado (tamanho/data) → pendente; o que sumiu → `missing` | — |
| **Indexer** | `imaging::inspect`: EXIF completo, data de criação (EXIF; sem EXIF, a do arquivo), pixels orientados, `picture_hash`, hash perceptual, nitidez, miniatura (480 px) em `thumbnail/YYYY/MM/<uuid>.jpg`, GPS → lugar | `state` |
| **Categorizer** | CLIP: embedding da miniatura (guardado para a busca) e zero-shot contra as frases das categorias (softmax com escala 100; até 3 acima de 15%, ou o melhor palpite); regras: borrada (nitidez), duplicata (hashes), sem categoria | `ai_state` |
| **Faces** | SCRFD + ArcFace na imagem a 1920 px; recorte de cada rosto em `faces/YYYY/MM/<uuid>.jpg`; agrupamento em pessoas | `faces_state` |
| **Privacy** | fotos privadas: cifra o original em `<nome>.photoidxe` (mesma pasta) e a miniatura em `.pidx`, decifra as que voltam a ser públicas, recifra com a chave nova; só com o cofre aberto | `private_state` |

Acima das threads de cada etapa há um **teto global** (`tasks/max`), repartido
em rodízio; a decodificação completa reserva memória no
`imaging::MemoryBudget`; exceção num worker vira o erro da foto (`guarded()`).

### Fotos privadas

- **Chave:** 256 bits aleatórios, gerada no primeiro `Catalog::open`. Em claro
  (`setting vault/plain`) só até a pessoa definir a senha; depois, o chaveiro
  (a chave atual e, durante uma troca, as antigas) fica cifrado com
  AES-256-GCM sob a chave derivada da senha (PBKDF2-HMAC-SHA512, 600 mil
  iterações, sal de 16 bytes) em `vault/wrapped`. `vault/currentId` (id
  público da chave) confere a chave digitada na recuperação.
- **Formato `.photoidxe`** (`core/Crypto.h`): cabeçalho de 40 bytes (magia,
  bloco, id da chave, prefixo do nonce, tamanho) e registros AES-256-GCM de
  1 MiB; registro 0 = metadados (nome original); a AAD amarra cada bloco ao
  cabeçalho, à posição e ao "último" — corte, troca ou bit alterado não passam.
- **Cifrar:** `QSaveFile` (temporário + rename), decifra de novo e compara o
  SHA-256, só então apaga o original; queda no meio é retomada
  (`core/Privacy.cpp`). `file_sha256`/`encrypted_sha256` ficam na foto.
- **Visibilidade:** `photo.private = 1` some de todos os nós, contagens,
  busca, IA, pessoas e duplicadas; só os nós de álbum privado a mostram, e só
  com o chaveiro aberto (`KeyRing`, na memória da sessão). O original e a
  miniatura são decifrados na memória (`image://private`, visualizador,
  Indexer) — nunca em claro no disco.

### Categorias

Árvore de 2 níveis embutida (`Taxonomy.cpp`, 16 raízes), com frases em inglês
para o CLIP; a pessoa cria, renomeia, aninha e apaga as dela (o nome vira a
frase, ou ela define frases próprias). Foto ↔ categoria é N:N com origem:
`ai`, `user`, `rule` e `rejected` (a pessoa tirou: a IA não põe de volta).
Reclassificar é barato: o embedding de cada foto fica guardado e só a
comparação é refeita. Sem nenhuma categoria acima do limiar (15%), fica o melhor palpite, se tiver ao menos 5%.

### Pessoas

O mesmo agrupamento do `_assign_person` do photoe, mas global: cada face nova
vai para a pessoa de centroide mais parecido (cosseno ≥ 0,45) ou cria uma
pessoa nova (UUID). Faces com confiança de detecção abaixo de 0,5 ficam
buscáveis, mas não formam pessoas (animais, objetos). Merge automático:
centroides ≥ 0,55 **e** as duas pessoas confiáveis (≥ 3 faces, confiança
média ≥ 0,6); nomes diferentes nunca se juntam. Correções da pessoa:
`assignFaces` (falso negativo), `rejectFaces` (falso positivo — gravado em
`face_rejection`, o agrupamento nunca devolve). A busca por foto detecta os
rostos da imagem enviada e compara com todas as faces (`FaceIndex`, busca
exata em memória).

### Hashes

- `picture_hash`: SHA-256 dos pixels (o do photoe) — mesmas imagens com EXIF
  diferente casam; RAW usa os dados do sensor.
- Hash perceptual (dHash 64 bits) numa **BK-tree** (`HashTree`, distância de
  Hamming): fotos parecidas (redimensionadas, recomprimidas) sem comparar com
  todas. Duplicata = mesmo `picture_hash`, ou hash perceptual igual com a
  mesma proporção.

## Catálogo

`~/.photo_indexer/data.sqlite3`, esquema em `src/core/schema/`:

```
disk ─< folder ─< photo ─< exif_tag
                    │  ├─< photo_category >─ category (árvore)
                    │  ├─< album_photo >─ album (árvore)
                    │  ├── photo_embedding (CLIP)
                    │  ├─< face >─ person
                    │  └── place
face_rejection, saved_filter, setting
```

### Discos

`identifyDisk` (`Disks.cpp`) dá a identidade do volume em cada SO e se é
local, removível ou de rede (SMB/CIFS, NFS, AFP, WebDAV, SSHFS, VM). A pasta
guarda o caminho relativo ao ponto de montagem; ao abrir (e em "Detectar
discos") o app reconhece os discos montados e, se um voltou em outro lugar,
refaz os caminhos das fotos. Disco offline: as fotos continuam na grade (as
miniaturas são locais) e o visualizador mostra a miniatura com um aviso.

### Busca e filtros

- **Texto:** todas as palavras precisam casar em nome, pasta, câmera, lente,
  autor, formato, ano, ou nos nomes (sem acento) de categorias, lugares,
  pessoas e álbuns.
- **IA:** o texto (em inglês) vira embedding do CLIP; as fotos mais parecidas
  que também passam pelo nó da árvore e pelo filtro.
- **Filtro personalizado:** regras sobre qualquer campo (lista fechada em
  `CatalogFilter.cpp`: arquivo, imagem, datas, câmera, local, organização,
  qualquer tag EXIF), encadeadas com E/OU e salvas com nome.

## Interface (QML)

`Main.qml` (janela, atalhos globais, paleta, diálogos), `AppHeader.qml`
(busca, andamento), `ActivityRail.qml` (Biblioteca, Pessoas, Pastas, Tarefas,
Idioma, Sobre) e as páginas: `LibraryPage` (painel lateral com as abas
Pastas/Datas/Categorias/Álbuns/Lugares/Pessoas + grade + filtro),
`ViewerPage`, `PeoplePage`, `FaceSearchPage`, `FoldersPage`, `TasksPage`.

## Build e pacotes

O mesmo modelo do photo-editor: o `Makefile` é a porta de entrada e cada
plataforma tem o seu builder Docker em `packaging/<plataforma>/`; o
`packaging/build.py` resolve a versão (Releases do GitHub), baixa os modelos
com sha256 fixo e desenha a arte dos instaladores. O LibRaw é baixado e
compilado pelo próprio CMake (`src/cmake/LibRaw.cmake`), igual nos seis
alvos. O `.github/workflows/release.yml` gera os instaladores ao publicar uma
Release; o `pages.yml` publica o hotsite (`docs/site`) em
<https://photoindexer.github.io/>, pelo repositório
`PhotoIndexer/photoindexer.github.io`.

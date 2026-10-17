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
| **Categorizer** | CLIP: embedding da miniatura (guardado para a busca) e zero-shot contra as frases das categorias (`core/Classifier`: softmax com escala 100 junto de frases de "nenhuma"; até 2 de grupos diferentes acima de 45%; na dúvida entre irmãs, o grupo pai); regras: borrada (nitidez), duplicata (hashes), sem categoria | `ai_state` |
| **Faces** | SCRFD + ArcFace na imagem a 1920 px; recorte de cada rosto em `faces/YYYY/MM/<uuid>.jpg`; agrupamento em pessoas | `faces_state` |
| **Privacy** | fotos privadas: cifra o original em `<nome>.photoidxe` (mesma pasta) e a miniatura em `.pidx`, decifra as que voltam a ser públicas, recifra com a chave nova; só com o cofre aberto | `private_state` |

Acima das threads de cada etapa há um **teto global** (`tasks/max`), repartido
em rodízio; a decodificação completa reserva memória no
`imaging::MemoryBudget`; exceção num worker vira o erro da foto (`guarded()`).

A thread principal (banco + interface) fica leve: o despacho é agrupado
(`schedulePump`, 100 ms), os resultados do Indexer vão em lote numa transação
(transações aninham por SAVEPOINT), as filas usam índices parciais
(`schema/0003.sql`: < 1 ms com 70 mil fotos, antes ~40 ms por pedido) e a
grade é lida numa conexão só de leitura em outra thread (`core/GridReader`).

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
comparação é refeita.

A regra (`core/Classifier`) foi calibrada com 100 fotos de um catálogo real
rotuladas à mão (de 24 para 64 inteiramente certas; precisão de 0,33 para
0,86), reclassificando offline pelos embeddings guardados:

- frases de **"nenhuma"** ("a photo", "a blank image"…) entram na softmax;
  quando vencem, a foto fica sem categoria — não há mais "melhor palpite";
- limiar de **45%** e no máximo 2 categorias, **nunca duas irmãs**;
- **na dúvida entre irmãs** (a segunda com ≥ 30% da primeira), ou com a
  melhor fraca sozinha mas o grupo forte (≥ 50%), vai o **pai**: o CLIP
  int8 confunde basquete, vôlei e futsal numa quadra (cossenos a ~0,01), mas
  acerta "Esportes";
- frases que descrevem o que distingue cada esporte/evento, e a categoria
  "Logo / Ícone / Gráfico" (ícones e logos iam para "exame médico").

Os quatro números são ajustes na tela de Tarefas (`ai/minProbability`,
`ai/maxCategories`, `ai/parentProbability`, `ai/siblingMargin`). Frase
embutida que a pessoa não mudou é atualizada ao abrir (`previousPrompts`).

### Pessoas

A regra inteira mora em `core/FaceClusterer` (sem banco, roda fora da thread
principal), calibrada com 43 mil faces de um catálogo real:

1. **Uma pessoa por foto**: duas faces da mesma foto nunca são a mesma pessoa.
2. **Entrar**: a face nova vai para a pessoa de centroide (normalizado) mais
   parecido, cosseno ≥ 0,50 (`faces/matchSimilarity`), ou cria uma nova (UUID).
3. **Juntar**: duas pessoas se juntam quando a **média** das similaridades
   entre as faces das duas é ≥ 0,40 (`faces/mergeSimilarity`), recalculada a
   cada junção — sem cadeia (A~B e B~C não junta A com C) — e nunca se
   aparecem juntas numa foto. O cosseno entre centroides inchava com grupos
   mistos e criava "pessoas" de milhares de rostos diferentes.
4. Faces com confiança de detecção < 0,65 (`faces/minPersonScore`: nuca,
   perfil, borrão, animais) ficam buscáveis, mas não formam pessoas.
5. Pessoas com nome e faces confirmadas são fixas: recebem faces, absorvem
   grupos, mas duas fixas nunca se juntam.

"Reagrupar pessoas" (Tarefas) refaz tudo com essa regra mantendo nomes,
confirmações e `face_rejection`; roda uma vez sozinho ao abrir um catálogo da
regra antiga (`faces/clusterRule`). Correções da pessoa: `assignFaces` (falso
negativo), `rejectFaces` (falso positivo — gravado em `face_rejection`, o
agrupamento nunca devolve). A tela Pessoas mostra primeiro as com nome e
esconde as com menos de 20 fotos (`people/minPhotos`), salvo na busca por foto.
A busca por foto detecta os
rostos da imagem enviada e compara com todas as faces (`FaceIndex`, busca
exata em memória).

### Hashes

- `picture_hash`: SHA-256 dos pixels (o do photoe) — mesmas imagens com EXIF
  diferente casam; RAW usa os dados do sensor.
- Hash perceptual (dHash 64 bits) numa **BK-tree** (`HashTree`, distância de
  Hamming): fotos parecidas (redimensionadas, recomprimidas) sem comparar com
  todas. Duplicata = mesmo `picture_hash`, ou hash perceptual igual com a
  mesma proporção.
- Orientação: a tag `Orientation`; quando ela diz "normal", a do fabricante
  (`imaging::makerNoteOrientation`: Canon, ShotInfo/AutoRotate — câmeras com
  "Girar auto" desligado). A miniatura, o visualizador e as faces usam a
  efetiva (`ShotInfo::displayOrientation`); o `picture_hash`, só a da tag
  (igual ao Pillow/photoe).

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

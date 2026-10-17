# Instruções do Projeto

**PhotoIDX**: catálogo de fotos com categorias por IA, reconhecimento facial e
lugares, distribuído como **app desktop nativo** (Windows, macOS e Linux, cada
um em x64 e arm64): **C++20 + Qt 6 (Qt Quick/QML)**, OpenCV estático, LibRaw e
ONNX Runtime — sem navegador embarcado e sem servidor local. A estrutura, o
design e o modelo de build (CMake, Docker para compilar e empacotar, QML com
tema em singleton) são os do `../photo-editor`; o reconhecimento facial e o
hash de imagem são os do `../photoe`. As convenções abaixo são obrigatórias.

**Não é um editor**: as fotos da pessoa **nunca** são alteradas, movidas nem
apagadas. O app só lê as pastas indexadas; tudo o que ele gera fica em
`~/.photo_indexer/`. Não há login, conta nem permissionamento.

## Estrutura

Na raiz ficam **só** estas pastas (mais `CMakeLists.txt`, `Makefile`,
`README.md`, `LICENSE`, `CLAUDE.md` e os gerados/ignorados `build/`, `dist/`,
`.cache/`):

```
.github/  docs/  i18n/  packaging/  src/  tests/
```

```
src/imaging/    leitura de imagem e IA (OpenCV, LibRaw, ONNX, QtCore/QtGui):
                formatos, EXIF completo, RAW, hashes, inspeção do Indexer,
                CLIP (categorias e busca), faces (SCRFD + ArcFace)
src/core/       catálogo SQLite (Catalog*.cpp), Pipeline (as etapas), discos,
                lugares (GeoNames), árvore de hashes, índice de faces,
                taxonomia; schema/ (migrations) e data/places.qz
src/app/        main, controllers expostos ao QML, modelos de lista, image
                providers, qml/ (Theme, componentes, páginas), icons/ (Lucide),
                assets/, fonts/
src/cmake/      módulos CMake (ONNX Runtime, LibRaw, Packaging)
packaging/      builders Docker por plataforma, build.py, geonames.py, deps,
                arte dos instaladores
i18n/           traduções Qt Linguist (.ts)
tests/          QtTest; tests/data/ com referências do insightface/Pillow
docs/           ARCHITECTURE.md, TODO.md, site/ (hotsite no GitHub Pages)
```

Coisa nova vai para dentro de `src/` (ou `packaging/`, `tests/`, `docs/`,
`i18n/`) — nunca uma pasta nova na raiz.

## UI / UX

### 1. Modais próprios — nunca diálogos do sistema
Toda confirmação, alerta ou pedido de texto usa os modais do app
(`Dialogs.qml` + `components/Modal.qml`; texto curto: `dialogs/NameDialog.qml`).

- **Proibido:** `MessageDialog`, `QMessageBox` ou qualquer popup nativo para
  confirmar/avisar. Seletor de pasta/arquivo nativo (`FolderDialog`,
  `FileDialog`) é permitido: é escolha de arquivo, não aviso.
- Erros das ações chegam do C++ pelo sinal `failed(key, params)` (chave de
  i18n); avisos curtos por `notice(key, params)`. O `Main.qml` mostra o modal
  (`Messages.errorDialog`) ou a faixa de aviso (`Messages.notice`).

### 2. Telas ocupam 100% da área disponível
Nada de cartões estreitos centralizados: `ScrollPage`/layouts com
`Layout.fillWidth`; a grade de miniaturas reparte a sobra entre as colunas.

### 2.1. Cor: vermelho é acento, esmeralda é confirmação, erro tem forma própria
Cores só no `src/app/qml/Theme.qml` (claro e escuro); componentes nunca
escrevem hex. Vermelho da marca (`Theme.brand*`) = acento; sucesso em
`Theme.emerald*`; erro sempre em `components/FormError.qml` (caixa + ícone).

### 2.2. Booleano é o Toggle; seleção em lista é o círculo com ✓
`components/Toggle.qml` para todo booleano; `components/SelectionCheck.qml`
para marcar linhas/miniaturas que entram numa operação. Nada de `CheckBox`
cru do Controls.

### 3. Detalhe de objeto é uma página própria
A foto aberta (`ViewerPage`), a pessoa aberta (`PeoplePage`) e a busca por
rosto (`FaceSearchPage`) são telas (`App.page`), nunca modais.

### 4. A interface nunca trava
O banco é só da thread principal (regra 9); imagens grandes vêm de providers
assíncronos; miniaturas e recortes são arquivos lidos por `file://` com
`asynchronous: true`. Mudanças do catálogo durante a indexação são agrupadas
(`LibraryController`: refaz a grade no máximo a cada ~5 s). As etapas rodam
com prioridade baixa e padrões de threads contidos; o OpenCV e o ONNX não
abrem threads próprias dentro de cada foto.

## Internacionalização (i18n)

### 5. Todo texto visível é traduzível; inglês é o padrão e o fallback
Idiomas: **EN** e **PT-BR** (lista em `src/app/Language.cpp`).

- **QML:** `qsTr("English text")`; plurais com `qsTr("%n photos", "", n)`.
  **C++:** `tr()` em QObject, `QCoreApplication::translate()` fora deles.
  Texto-fonte **sempre em inglês**.
- Nomes das categorias embutidas: `QT_TRANSLATE_NOOP("Category", ...)` em
  **cada linha** de `src/core/Taxonomy.cpp` (o `lupdate` não expande macros).
- Traduções em `i18n/photoidx_pt_BR.ts`; `photoidx_en.ts` guarda só os
  plurais. Atualize com `make translations` e traduza as entradas novas no
  mesmo commit.
- Dados da pessoa (nomes de álbum, pessoa, categoria criada por ela) e os do
  mundo (país/cidade do GeoNames, nomes de tag EXIF) não são traduzidos.

## Identidade visual

### 6. Logo, ícones e fonte no binário — o app funciona offline
Logo `src/app/assets/logo/photoidx-{light,dark}.png` (a marca da família, o
"S" hexagonal, + PHOTO**IDX**), ícones Lucide em `src/app/icons/*.svg`
(servidos recoloridos por `image://icon/<nome>?color=...`; ícone novo = SVG
oficial do Lucide) e a fonte **Inter**. Os ícones dos instaladores são
**desenhados em código** a partir do logo (`packaging/art.py`).

## Dados

### 7. Tudo da máquina fica em `~/.photo_indexer/`
Igual nos três SOs (sobrescrevível por `PHOTOIDX_DATA_DIR`):

```
~/.photo_indexer/data.sqlite3                 o catálogo
~/.photo_indexer/thumbnail/YYYY/MM/<uuid>.jpg miniatura (uuid = id da foto,
                                              YYYY/MM = data de criação)
~/.photo_indexer/faces/YYYY/MM/<uuid>.jpg     recorte de cada rosto
~/.photo_indexer/logs/                        app.log rotativo
```

- Caminhos sempre por `pix::AppPaths` (`src/core/Paths.h`).
- A pasta de dados **nunca é indexada**, mesmo que a pessoa indexe a raiz do
  disco (`C:\`, `/`): o Searcher a pula e `addFolder` a recusa.
- Miniaturas são o que permite navegar pelas fotos de um disco desconectado:
  o visualizador cai na miniatura quando o original está offline.
- **Sem `.env`:** ajuste de desenvolvimento por variável de ambiente
  (`PHOTOIDX_DATA_DIR`, `PHOTOIDX_MODELS_DIR`, `PHOTOIDX_LOG_LEVEL`) ou
  argumento (`--debug`). Preferências de interface no `QSettings`; as do
  catálogo (threads, limiares da IA) na tabela `setting`.

### 8. Migrations incrementais
O catálogo é dado real e persiste entre versões: o esquema mora em
`src/core/schema/NNNN.sql` e migra pelo `PRAGMA user_version`.

- **Proibido:** editar ou apagar um passo já publicado.
- **Como aplicar:** mudou o esquema → arquivo `NNNN.sql` novo, `kSchemaSteps`
  em `src/core/Database.cpp` + 1, teste em `tst_core`, tudo no mesmo commit.
- Um comando pode ocupar várias linhas (termina na linha que acaba em `;`);
  nada de trigger.

### 9. Banco só na thread principal; o pesado roda fora dela
`QSqlDatabase` não cruza threads. O `Catalog` monta na thread principal os
pedidos (`IndexJob`, `CategorizeJob`, `FaceJob`), o `Pipeline` os roda no
pool de cada etapa e grava o resultado de volta por `invokeMethod` em fila.
A fila de cada etapa **é o estado da foto no banco** (`state`, `ai_state`,
`faces_state`): fechar o app no meio não perde nada.

### 10. Discos: identidade, não caminho
Cada pasta indexada pertence a um disco (`src/core/Disks.cpp`), reconhecido
pela identidade estável — UUID do volume (Linux `/dev/disk/by-uuid`, macOS
DiskArbitration), GUID do volume (Windows) ou `protocolo://servidor/
compartilhamento` (SMB, NFS, AFP, WebDAV, SSHFS). O caminho relativo ao ponto
de montagem fica guardado: se o disco volta em outro lugar, `refreshDisks()`
refaz os caminhos. Código de SO novo vai em `Disks.cpp`, atrás de `#if`.

### 11. Hash e faces: os do `../photoe`
- `picture_hash` = SHA-256 de `"<modo>:<L>x<A>:"` + pixels decodificados e
  orientados (o `picture_hash` do photoe; RAW = dados do sensor). O teste
  `tst_imaging::pictureHashMatchesPhotoe` compara com o Pillow.
- Faces = insightface `buffalo_sc` (SCRFD 0,35 + ArcFace, cosseno 0,45 para a
  mesma pessoa), comparado com o insightface em `tst_ai` (referências geradas
  por `tests/data/faces/gen_faces.py`). Mudou o pipeline de faces? Regere as
  referências e rode o teste.
- Os modelos do insightface são licenciados **só para pesquisa não
  comercial**: o aviso aparece no Sobre e no README; não remova.

### 12. Log vai para `~/.photo_indexer/logs/`
`app.log` rotativo (5 MB × 3) e, com terminal, também o stderr. Nos módulos:
`Q_LOGGING_CATEGORY(lcX, "photoidx.x")` + `qCInfo`/`qCWarning`. **Proibido**
`printf`/`std::cout` para diagnóstico.

## Convenções de código e versionamento

### 13. Estilo do código
- Todo arquivo começa com `// SPDX-License-Identifier: BSD-2-Clause`; headers
  com `#pragma once`; includes do projeto, depois Qt, depois std.
- Namespaces `pix`, `pix::imaging`, `pix::app`; classes PascalCase, funções e
  variáveis camelCase, membros `m_`, constantes `k`. Imagens são `cv::Mat`
  **RGB** (nunca BGR na API).
- **Identificadores em inglês; comentários em português.**
- Caminhos com `QString`/`QFile` (+ `cv::imdecode`) — nunca `cv::imread` com
  `std::string`, que quebra com acentos no Windows.
- Filtros da grade: só campos e operadores da lista fechada de
  `CatalogFilter.cpp`; valores sempre como parâmetros ligados. A interface
  nunca manda SQL.

### 14. Ferramenta não instalada na máquina? Docker
Docker é ferramenta de **desenvolvimento e build**, nunca de execução do app.
**Compile e teste sempre pelo Docker**: `make build`, `make test`, `make run`
rodam no builder Linux (`packaging/linux/Dockerfile`); `make shell` abre um
shell nele. Para outros runtimes (Python das referências, geração do
GeoNames), imagem oficial descartável com `--rm` e volume no projeto.

### 15. O GitHub manda na versão
Igual ao photo-editor: publicar uma Release `vX.Y.Z` cria a versão; entre
Releases, todo build é `X.Y.Z-dev+<commit>`. Quem resolve é
`packaging/build.py version`, que grava o `VERSION` da raiz (ignorado pelo
git). **Proibido** versionar o `VERSION` ou gravar versão fixa no código.

### 16. Builders em Docker e versões fixas
Os instaladores saem dos builders em `packaging/<plataforma>/` chamados pelo
`Makefile` (`make help`): `.tar.gz` (Linux x64/arm64), `.dmg` (macOS
x64/arm64, cross-compile), `.msi` (Windows x64 cross-compile; arm64 nativo).

- **Versões fixas:** Qt, OpenCV (`packaging/deps/opencv.sh`), ONNX Runtime
  (`src/cmake/OnnxRuntime.cmake`), LibRaw (`src/cmake/LibRaw.cmake`), modelos
  CLIP e buffalo_sc (revisão + sha256 em `packaging/build.py`). A tabela do
  GeoNames é gerada por `packaging/geonames.py` e versionada. Trocar é
  decisão explícita, nunca efeito colateral de um build.
- **Nunca troque os GUIDs de `packaging/windows/msi.py`** nem o `BUNDLE_ID`
  `br.com.photoidx.desktop`.
- Dependência nova precisa existir para os **seis** alvos.

### 17. Hotsite
`docs/site/` é o hotsite (HTML/CSS/JS estáticos, EN e PT-BR), em
<https://photoindexer.github.io/>. O `.github/workflows/pages.yml`, a cada
push na `main` que mexe em `docs/`, monta o site (`make site`) e o publica no
repositório `PhotoIndexer/photoindexer.github.io` (o único que o GitHub serve
na raiz do domínio), com a deploy key do secret `SITE_DEPLOY_KEY`. As
capturas de tela moram em `docs/images/` (o README também as usa), geradas
pelo próprio app (`--screenshot`).

### 18. Como validar
- `make test` — todos os testes (QtTest).
- `make run ARGS="<pasta> --screenshot /tmp/x.png --size 1440x900"` abre o
  app sem tela, indexa a pasta, fotografa e sai (`--page`, `--open N`,
  `--wait MS`).

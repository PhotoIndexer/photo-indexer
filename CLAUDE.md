# Instruções do Projeto

**PhotoIDX**: catálogo de fotos com categorias por IA, reconhecimento facial e
lugares, distribuído como **app desktop nativo** (Windows, macOS e Linux, cada
um em x64 e arm64): **C++20 + Qt 6 (Qt Quick/QML)**, OpenCV estático, LibRaw e
ONNX Runtime — sem navegador embarcado e sem servidor local. A estrutura, o
design e o modelo de build (CMake, Docker para compilar e empacotar, QML com
tema em singleton) são os do `../photo-editor`; o reconhecimento facial e o
hash de imagem são os do `../photoe`. As convenções abaixo são obrigatórias.

**Não é um editor**: o conteúdo das fotos da pessoa **nunca** é alterado. O
app lê as pastas indexadas; tudo o que ele gera fica em `~/.photo_indexer/`.
Não há login, conta nem permissionamento. **Exceções, só a pedido explícito
da pessoa:** "tornar privada" — o original vira `<nome>.photoidxe` (cifrado)
na mesma pasta (regra 11.1) — e as operações de arquivo do modo Pastas
(regra 10.1): mover, renomear, criar pasta e **excluir sempre para a
Lixeira** do sistema. O app nunca apaga de vez por conta própria.

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

- **Teto de tarefas:** além das threads de cada etapa, o `Pipeline` tem um
  teto somando todas (`tasks/max`, padrão metade dos núcleos entre 2 e 8),
  repartido em rodízio entre as etapas. Nada de pré-enfileirar no pool.
- **Memória:** decodificação completa reserva antes o que vai ocupar no
  `imaging::MemoryBudget` (25% da RAM, 512 MB–4 GB) e espera se não couber;
  acima de `kMaxDecodePixels` a imagem não é decodificada.
- **Exceção nunca escapa de um worker** (o `QThreadPool` derruba o app):
  todo trabalho de foto passa por `guarded()` e a exceção vira o erro da foto.
- **Thread principal leve:** o despacho das etapas é agrupado
  (`schedulePump`, no máximo a cada 100 ms), os resultados do Indexer são
  gravados em lote numa transação (`applyIndexBatch`), e as filas e contagens
  usam índices parciais (`schema/0003.sql`) — consulta que roda a cada
  resultado tem de responder em ~1 ms com 100 mil fotos. A grade é lida fora
  da thread principal (`core/GridReader`) e uma atualização de fundo com a
  mesma ordem não refaz o modelo (`PhotoGridModel::setRows`).
- **Telemetria:** `PIX_PERF("rótulo")` (`core/Perf.h`) nas funções pesadas;
  o vigia registra em `logs/perf.log` cada travada da interface (> 250 ms)
  com o que rodava, e um resumo por minuto. Travou? Comece por lá.
  `tests/bench_catalog <cópia do banco>` mede cada consulta num catálogo real.
- **Listas grandes no QML:** grade/lista com modelo C++ (nunca `Repeater`/
  `Flow` nem `.filter()` em JavaScript sobre milhares de itens); menus
  (`ChoiceMenu`) só criam os itens ao abrir; imagens esperam a rolagem
  rápida parar (`components/ScrollWatch.qml`).

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
                                              YYYY/MM = data de criação;
                                              .pidx = cifrada, foto privada)
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
`QSqlDatabase` não cruza threads. Exceção: **leituras da navegação** em
conexões **só de leitura** (WAL: veem o último commit, não esperam nem travam
a gravação):

- `core/CatalogReader`: uma thread com um `Catalog` só de leitura
  (`openReaderOf`) — árvores do painel, listas dos menus (categorias,
  álbuns, pessoas), pastas/discos, totais e erros das Tarefas, contagens do
  Pipeline. `reader->read<T>(contexto, trabalho, pronto)`; o resultado volta
  na thread principal. O checkpoint do WAL também roda nela (a principal
  desliga o automático).
- `core/GridReader`: a grade, com o SQL montado pelo `Catalog` (`photosSql`).

Leitura nova que a interface pede (lista, contagem, árvore) vai pelo
`CatalogReader`, nunca por um getter de `Q_PROPERTY` que consulta o banco —
o QML avalia bindings muitas vezes. Gravações continuam só na principal. O `Catalog` monta na thread principal os
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
Arquivo que some no meio de uma etapa (disco saiu antes de o app perceber)
**não é erro**: o resultado vem com `unreachable`, a foto volta para a fila
(`Pipeline` `Stage::parked`, até os discos mudarem) e o app sonda os discos.

### 10.1 Modo Pastas: mover, renomear, Lixeira — nunca apagar de vez
- Operações de arquivo só em `src/core/FileOps.cpp`, fora da thread
  principal; o catálogo (caminho, pasta, `folder_id`) é atualizado depois, na
  principal. Mover entre volumes = copiar, conferir (tamanho + SHA-256) e só
  então remover o original.
- Excluir = `QFile::moveToTrash` (o app nunca chama remoção definitiva). Sem
  Lixeira: no macOS/Linux o sistema recusa e o app mostra o erro; no Windows,
  num disco de rede, o próprio sistema pode apagar de vez — a confirmação
  **avisa** (decisão da pessoa: avisar, não bloquear).
- Destino sempre dentro de uma pasta indexada (as fotos não saem do
  catálogo); pasta indexada (raiz) e disco não se movem nem se excluem por
  aqui; nada roda com uma varredura em andamento.

### 11. Hash e faces: os do `../photoe`
- `picture_hash` = SHA-256 de `"<modo>:<L>x<A>:"` + pixels decodificados e
  orientados (o `picture_hash` do photoe; RAW = dados do sensor). O teste
  `tst_imaging::pictureHashMatchesPhotoe` compara com o Pillow.
- Faces = insightface `buffalo_sc` (SCRFD 0,35 + ArcFace), comparado com o
  insightface em `tst_ai` (referências geradas por
  `tests/data/faces/gen_faces.py`). Mudou o pipeline de faces? Regere as
  referências e rode o teste.
- Agrupar em pessoas só em `src/core/FaceClusterer.cpp`: uma pessoa por foto,
  entrar com cosseno ≥ 0,50 ao centroide, juntar pela média das
  similaridades ≥ 0,40 (nunca cadeia, nunca quem aparece junto), faces com
  confiança < 0,65 fora. Mudou a regra? Suba `faces/clusterRule` em
  `AppController` (reagrupa uma vez ao abrir) e teste em
  `tst_core::faceClustererRules`.
- Os modelos do insightface são licenciados **só para pesquisa não
  comercial**: o aviso aparece no Sobre e no README; não remova.

### 11.1 Fotos privadas: só a etapa "privacy" mexe nos arquivos
- Cifrar/decifrar arquivo da pessoa só em `src/core/Privacy.cpp`: sempre por
  `QSaveFile`, conferindo (decifrar + SHA-256) **antes** de apagar o
  original; nunca deixar a foto em dois lugares nem pela metade.
- O original e a miniatura de uma privada são decifrados **na memória**
  (`crypto::decryptFile`/`decryptBytes`) — nunca em claro no disco.
- Consulta nova sobre fotos (árvore, grade, contagem, busca, IA, faces)
  precisa filtrar `p.private = 0` (salvo os nós de álbum privado com o cofre
  aberto). Teste em `tst_core::privatePhotoLifecycle`.
- A chave nasce no `Catalog::open`; com senha, só existe cifrada no banco
  (`core/Vault.h`). Formato e parâmetros (`core/Crypto.h`) não mudam sem
  versão nova do formato — arquivos antigos precisam continuar abrindo.

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
  (`src/cmake/OnnxRuntime.cmake`), LibRaw (`src/cmake/LibRaw.cmake`), Mbed TLS
  (`src/cmake/MbedTLS.cmake`), modelos
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

- **Tutoriais** em `docs/site/tutorials/` (o texto, EN e PT-BR, em
  `docs/site/js/tutorials-content.js`; um capítulo por função). As telas são
  do próprio app com fotos e nomes **fictícios** (`packaging/tutorials/`):
  mudou a interface de uma função, refaça as capturas e o texto dela.
- **Toda release termina republicando o site:** o `release.yml`, depois do
  job dos instaladores, chama o `pages.yml` (`workflow_call`). O build grava
  `assets/releases.json` com as releases e os instaladores publicados (com o
  token do workflow); a página consulta a API do GitHub ao vivo e, se ela não
  responder (limite anônimo de 60/h por IP), usa esse arquivo. O site sai
  sempre da `main`, mesmo chamado por uma release de tag antiga.

### 18. Como validar
- `make test` — todos os testes (QtTest).
- `make run ARGS="<pasta> --screenshot /tmp/x.png --size 1440x900"` abre o
  app sem tela, indexa a pasta, fotografa e sai (`--page`, `--open N`,
  `--wait MS`).

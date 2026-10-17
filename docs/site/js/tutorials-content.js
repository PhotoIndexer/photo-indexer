/* ==========================================================================
   PhotoIDX — conteúdo dos tutoriais (EN + PT-BR)

   Cada capítulo: slug (o ?t= da página), en e pt com title, summary e blocks.
   Blocos: ["p", html] · ["h", texto] · ["steps", [html, ...]] ·
           ["img", "arquivo", legenda] · ["tip", html]
   As capturas ficam em assets/screens/tutorials/<en|pt-br>/<arquivo>.jpg e
   são geradas pelo próprio app (--screenshot) com fotos e nomes fictícios.
   ========================================================================== */
window.PIX_TUTORIALS = [
  {
    slug: "getting-started",
    en: {
      title: "Getting started",
      summary: "Install PhotoIDX, add your first folder and follow the indexing.",
      blocks: [
        ["p", "PhotoIDX builds a catalog of the folders you choose — on your computer, on an external drive or on a network share. Your photos stay where they are: the app reads them and keeps everything it creates (thumbnails, faces, the catalog) in a folder of your user."],
        ["h", "Add a folder"],
        ["steps", [
          "Open PhotoIDX. With an empty catalog the library shows <b>Add a folder to start your catalog</b>.",
          "Click <b>Add folder</b> (top right) and pick a folder — or simply <b>drag a folder</b> from your file manager onto the window.",
          "Add as many folders as you like: local disks, USB drives and network shares (SMB, NFS, AFP…). Subfolders are included."
        ]],
        ["img", "01-empty", "An empty catalog, ready for the first folder."],
        ["h", "Watch the indexing"],
        ["p", "Indexing runs on its own, in four stages: the <b>Searcher</b> finds the images, the <b>Indexer</b> reads the EXIF and makes the thumbnails, the <b>Categorizer</b> sorts them with AI and <b>Faces</b> recognizes people. You can browse while it works."],
        ["img", "03-folders-page", "Folders and disks: each indexed folder, grouped by the drive it lives on."],
        ["tip", "Closing the app in the middle of the indexing loses nothing: the next time it continues where it stopped."]
      ]
    },
    pt: {
      title: "Primeiros passos",
      summary: "Instale o PhotoIDX, adicione a primeira pasta e acompanhe a indexação.",
      blocks: [
        ["p", "O PhotoIDX monta um catálogo das pastas que você escolher — no computador, num HD externo ou num compartilhamento de rede. As fotos ficam onde estão: o app lê os arquivos e guarda o que gera (miniaturas, rostos, o catálogo) numa pasta do seu usuário."],
        ["h", "Adicione uma pasta"],
        ["steps", [
          "Abra o PhotoIDX. Com o catálogo vazio, a biblioteca mostra <b>Adicione uma pasta para começar o catálogo</b>.",
          "Clique em <b>Adicionar pasta</b> (no canto superior direito) e escolha uma pasta — ou simplesmente <b>arraste uma pasta</b> do gerenciador de arquivos para a janela.",
          "Adicione quantas quiser: discos locais, pen drives e HDs externos, compartilhamentos de rede (SMB, NFS, AFP…). As subpastas entram junto."
        ]],
        ["img", "01-empty", "Catálogo vazio, pronto para a primeira pasta."],
        ["h", "Acompanhe a indexação"],
        ["p", "A indexação trabalha sozinha, em quatro etapas: o <b>Searcher</b> acha as imagens, o <b>Indexer</b> lê o EXIF e gera as miniaturas, o <b>Categorizer</b> classifica com IA e o <b>Faces</b> reconhece as pessoas. Dá para navegar enquanto ela trabalha."],
        ["img", "03-folders-page", "Pastas e discos: cada pasta indexada, agrupada pelo disco onde está."],
        ["tip", "Fechar o app no meio da indexação não perde nada: na próxima vez, ela continua de onde parou."]
      ]
    }
  },
  {
    slug: "library",
    en: {
      title: "Browsing the library",
      summary: "Six ways to look at the same photos: folders, dates, categories, albums, places and people.",
      blocks: [
        ["p", "The side panel has a tree in each tab; the grid shows the photos of whatever node you pick. The counts next to each node are the number of photos inside it."],
        ["steps", [
          "<b>Folders</b> — grouped by drive (local, USB or network). A disconnected drive stays browsable through its thumbnails.",
          "<b>Dates</b> — year, month and day, from the date the photo was taken.",
          "<b>Categories</b> — the ones the AI chose and the ones you created.",
          "<b>Albums</b> — your own albums and sub-albums.",
          "<b>Places</b> — country, state and city, from the GPS of the photo (no internet needed).",
          "<b>People</b> — your groups of people, the people with a name and the ones still unnamed."
        ]],
        ["img", "04-library", "Folders tab with a subfolder selected."],
        ["img", "06-dates", "Dates tab: one month of photos."],
        ["img", "07-places", "Places tab: country, state and city."],
        ["tip", "Use the menu at the top of the grid to sort (newest first, name, size, resolution…) and the slider to change the thumbnail size. Double-click a photo to open it."]
      ]
    },
    pt: {
      title: "Navegando pela biblioteca",
      summary: "Seis jeitos de ver as mesmas fotos: pastas, datas, categorias, álbuns, lugares e pessoas.",
      blocks: [
        ["p", "O painel lateral tem uma árvore em cada aba; a grade mostra as fotos do nó escolhido. O número ao lado de cada nó é a quantidade de fotos dentro dele."],
        ["steps", [
          "<b>Pastas</b> — agrupadas pelo disco (local, USB ou rede). Um disco desconectado continua navegável pelas miniaturas.",
          "<b>Datas</b> — ano, mês e dia, pela data em que a foto foi tirada.",
          "<b>Categorias</b> — as que a IA escolheu e as que você criou.",
          "<b>Álbuns</b> — os seus álbuns e sub-álbuns.",
          "<b>Lugares</b> — país, estado e cidade, pelo GPS da foto (sem internet).",
          "<b>Pessoas</b> — os seus grupos de pessoas, as pessoas com nome e as que ainda não têm nome."
        ]],
        ["img", "04-library", "Aba Pastas com uma subpasta escolhida."],
        ["img", "06-dates", "Aba Datas: as fotos de um mês."],
        ["img", "07-places", "Aba Lugares: país, estado e cidade."],
        ["tip", "Use o menu no alto da grade para ordenar (mais recentes, nome, tamanho, resolução…) e o controle deslizante para mudar o tamanho das miniaturas. Clique duas vezes numa foto para abri-la."]
      ]
    }
  },
  {
    slug: "viewer",
    en: {
      title: "Everything about a photo",
      summary: "Open a photo to see the people in it, the camera, the file, the place, the categories and the full EXIF.",
      blocks: [
        ["p", "The photo opens on its own screen, with the faces outlined and named. The side panel shows the date, camera and exposure, the file and its disk, the place, the categories, the albums, the people, similar photos and every EXIF tag."],
        ["img", "05-viewer", "A group photo: two people recognized, one face still unknown."],
        ["steps", [
          "Click a face (on the photo or in <b>People in this photo</b>) to name it, move it to another person or search for that person in other photos.",
          "Use the <b>+</b> next to <b>Categories</b> and <b>Albums</b> to add the photo; the <b>×</b> on a chip removes it.",
          "Photo still in the faces queue? Click <b>Find faces now</b> and it jumps the queue.",
          "Use the arrows at the top to go to the previous or next photo of the grid."
        ]],
        ["tip", "If the drive is disconnected, the viewer shows the thumbnail and warns you — the information about the photo is still all there."]
      ]
    },
    pt: {
      title: "Tudo sobre uma foto",
      summary: "Abra uma foto para ver quem está nela, a câmera, o arquivo, o lugar, as categorias e o EXIF completo.",
      blocks: [
        ["p", "A foto abre numa tela própria, com os rostos marcados e com nome. O painel lateral mostra a data, a câmera e a exposição, o arquivo e o disco, o lugar, as categorias, os álbuns, as pessoas, as fotos parecidas e cada tag do EXIF."],
        ["img", "05-viewer", "Uma foto de grupo: duas pessoas reconhecidas e um rosto ainda desconhecido."],
        ["steps", [
          "Clique num rosto (na foto ou em <b>Pessoas nesta foto</b>) para dar nome, passar para outra pessoa ou procurar a pessoa em outras fotos.",
          "Use o <b>+</b> ao lado de <b>Categorias</b> e <b>Álbuns</b> para pôr a foto; o <b>×</b> de cada etiqueta tira.",
          "Foto ainda na fila de rostos? Clique em <b>Procurar rostos agora</b> e ela fura a fila.",
          "Use as setas no alto para ir à foto anterior ou à próxima da grade."
        ]],
        ["tip", "Se o disco estiver desconectado, o visualizador mostra a miniatura e avisa — as informações da foto continuam todas lá."]
      ]
    }
  },
  {
    slug: "search",
    en: {
      title: "Search and filters",
      summary: "Find any photo by a word, by describing it to the AI or by filtering on any field.",
      blocks: [
        ["h", "Text search"],
        ["p", "Type in the search box at the top: it looks at the file name, folder, camera, lens, place, category, person and album."],
        ["img", "10-search", "Searching for a family name finds all photos of that family."],
        ["h", "AI search"],
        ["p", "Turn on the AI button (the sparkle next to the search box) and describe the photo — <i>a dog playing outside</i>, <i>sunset at the beach</i>. The results come sorted by similarity. The AI understands English best."],
        ["img", "11-ai-search", "AI search: the photos most similar to the description, with the score."],
        ["h", "Filters"],
        ["steps", [
          "Click the filter button (next to the sort menu).",
          "Click <b>Add condition</b> and choose a field: size, width, megapixels, date, camera, ISO, lens, country, city, an EXIF tag…",
          "Choose whether photos must match <b>all conditions</b> or <b>any condition</b>.",
          "Use <b>Save</b> to keep the filters you use often."
        ]],
        ["img", "12-filters", "Two conditions: more than 2 megapixels and taken in Curitiba."]
      ]
    },
    pt: {
      title: "Busca e filtros",
      summary: "Ache qualquer foto por uma palavra, descrevendo-a para a IA ou filtrando por qualquer campo.",
      blocks: [
        ["h", "Busca por texto"],
        ["p", "Digite na caixa de busca, no alto: ela procura no nome do arquivo, na pasta, na câmera, na lente, no lugar, na categoria, na pessoa e no álbum."],
        ["img", "10-search", "Buscar por um sobrenome acha as fotos da família inteira."],
        ["h", "Busca por IA"],
        ["p", "Ligue o botão da IA (as estrelinhas ao lado da busca) e descreva a foto — <i>a dog playing outside</i>, <i>sunset at the beach</i>. Os resultados vêm ordenados pela semelhança. A IA entende melhor em inglês."],
        ["img", "11-ai-search", "Busca por IA: as fotos mais parecidas com a descrição, com a nota."],
        ["h", "Filtros"],
        ["steps", [
          "Clique no botão de filtro (ao lado do menu de ordem).",
          "Clique em <b>Adicionar condição</b> e escolha o campo: tamanho, largura, megapixels, data, câmera, ISO, lente, país, cidade, uma tag do EXIF…",
          "Escolha se as fotos devem atender a <b>todas as condições</b> ou a <b>qualquer condição</b>.",
          "Use <b>Salvar</b> para guardar os filtros que você usa sempre."
        ]],
        ["img", "12-filters", "Duas condições: mais de 2 megapixels e tirada em Curitiba."]
      ]
    }
  },
  {
    slug: "categories-albums",
    en: {
      title: "Categories and albums",
      summary: "Fix what the AI got wrong, create your own categories and organize albums.",
      blocks: [
        ["h", "Categories"],
        ["p", "The local AI puts each photo in up to two categories (beach, wedding, soccer, dog, food…). When it gets one wrong:"],
        ["steps", [
          "Select the photos in the grid (click, <b>Shift</b>-click, <b>Ctrl</b>/<b>⌘</b>-click).",
          "<b>Drag</b> them onto the right category in the side panel — or right-click → <b>Move to category</b>.",
          "To add without removing, hold <b>Alt</b>/<b>⌥</b> while dropping, or use <b>Add to category</b>."
        ]],
        ["p", "Create your own with the <b>+</b> of the Categories tab. Right-click a category for <b>New subcategory…</b>, <b>Rename…</b> and <b>AI phrases…</b> — English phrases that teach the AI what to look for."],
        ["img", "08-categories", "The Categories tab, with the AI's categories and subcategories."],
        ["h", "Albums"],
        ["steps", [
          "Create an album with the <b>+</b> of the Albums tab, or select photos and right-click → <b>New album with the selection…</b>.",
          "Drag photos onto an album to add them; drag from one album to another to move.",
          "Albums can have sub-albums. The <b>Private</b> album is special: see the Private photos tutorial."
        ]],
        ["img", "09-albums", "Albums tab with two albums."]
      ]
    },
    pt: {
      title: "Categorias e álbuns",
      summary: "Corrija o que a IA errou, crie as suas categorias e organize álbuns.",
      blocks: [
        ["h", "Categorias"],
        ["p", "A IA local põe cada foto em até duas categorias (praia, casamento, futebol, cachorro, comida…). Quando ela erra:"],
        ["steps", [
          "Selecione as fotos na grade (clique, <b>Shift</b>+clique, <b>Ctrl</b>/<b>⌘</b>+clique).",
          "<b>Arraste</b> para a categoria certa no painel lateral — ou botão direito → <b>Mover para a categoria</b>.",
          "Para acrescentar sem tirar, segure <b>Alt</b>/<b>⌥</b> ao soltar, ou use <b>Adicionar à categoria</b>."
        ]],
        ["p", "Crie as suas com o <b>+</b> da aba Categorias. Com o botão direito numa categoria: <b>Nova subcategoria…</b>, <b>Renomear…</b> e <b>Frases da IA…</b> — frases em inglês que ensinam à IA o que procurar."],
        ["img", "08-categories", "A aba Categorias, com as categorias e subcategorias da IA."],
        ["h", "Álbuns"],
        ["steps", [
          "Crie um álbum com o <b>+</b> da aba Álbuns, ou selecione fotos e use botão direito → <b>Novo álbum com a seleção…</b>.",
          "Arraste fotos para um álbum para pôr; de um álbum para outro, para mover.",
          "Álbuns podem ter sub-álbuns. O álbum <b>Privado</b> é especial: veja o tutorial de Fotos privadas."
        ]],
        ["img", "09-albums", "Aba Álbuns com dois álbuns."]
      ]
    }
  },
  {
    slug: "people",
    en: {
      title: "People",
      summary: "Name people, merge duplicates, fix wrong faces and organize people into groups.",
      blocks: [
        ["p", "Faces are grouped into people automatically, on your computer. The People screen shows the people with a name and the unnamed ones with enough photos; the others are found through the face search."],
        ["img", "13-people", "The People screen: six people with a name and two still unnamed."],
        ["h", "Open a person"],
        ["steps", [
          "Click a person. At the top: <b>Rename</b> (or <b>Name</b>), <b>Merge with…</b>, <b>Find similar faces</b> and <b>Photos</b>.",
          "Click faces to select them, then <b>Not this person</b> (it leaves and never comes back), <b>This is…</b> (moves to another person) or <b>New person…</b>.",
          "Right-click a face for the same actions. Turn on <b>Least similar first</b> to spot mistakes quickly."
        ]],
        ["img", "14-person", "A person with the similarity of each face."],
        ["h", "Groups of people"],
        ["p", "In the People tab of the library, create groups — <i>Family</i>, <i>Friends</i>, <i>Team</i> — with the <b>+</b>. Drag a person onto a group, or right-click → <b>Move to group…</b>. Whoever has no group stays in <b>Named people</b> or <b>Unnamed people</b>."],
        ["img", "15-people-tab", "A group selected: the photos of everybody in it."],
        ["tip", "Giving the same name to two people offers to merge them. People in a group are kept as they are when the app regroups faces."]
      ]
    },
    pt: {
      title: "Pessoas",
      summary: "Dê nome às pessoas, junte as repetidas, corrija rostos errados e organize as pessoas em grupos.",
      blocks: [
        ["p", "Os rostos são agrupados em pessoas sozinhos, no seu computador. A tela Pessoas mostra as pessoas com nome e as sem nome com fotos suficientes; as outras são achadas pela busca por rosto."],
        ["img", "13-people", "A tela Pessoas: seis pessoas com nome e duas ainda sem nome."],
        ["h", "Abra uma pessoa"],
        ["steps", [
          "Clique numa pessoa. No alto: <b>Renomear</b> (ou <b>Nome</b>), <b>Juntar com…</b>, <b>Procurar rostos parecidos</b> e <b>Fotos</b>.",
          "Clique nas faces para selecioná-las e use <b>Não é esta pessoa</b> (sai e nunca mais volta), <b>Esta é…</b> (passa para outra pessoa) ou <b>Nova pessoa…</b>.",
          "O botão direito numa face tem as mesmas ações. Ligue <b>Menos parecidas primeiro</b> para achar os erros rapidinho."
        ]],
        ["img", "14-person", "Uma pessoa, com a semelhança de cada face."],
        ["h", "Grupos de pessoas"],
        ["p", "Na aba Pessoas da biblioteca, crie grupos — <i>Família</i>, <i>Amigos</i>, <i>Time</i> — com o <b>+</b>. Arraste uma pessoa para o grupo, ou botão direito → <b>Mover para o grupo…</b>. Quem não tem grupo fica em <b>Pessoas com nome</b> ou <b>Pessoas sem nome</b>."],
        ["img", "15-people-tab", "Um grupo escolhido: as fotos de todos dele."],
        ["tip", "Dar a duas pessoas o mesmo nome oferece juntá-las. Quem está num grupo fica como está quando o app reagrupa os rostos."]
      ]
    }
  },
  {
    slug: "face-search",
    en: {
      title: "Search by face",
      summary: "Pick a photo of someone and find every photo that person appears in.",
      blocks: [
        ["steps", [
          "On the People screen, click <b>Search by photo…</b> and choose an image — it doesn't need to be in the catalog.",
          "If the photo has several faces, pick the one you want.",
          "Move <b>Minimum similarity</b>: lower finds more photos (and more mistakes); higher, fewer and surer.",
          "<b>Who it may be</b> lists the people that match. Click one to open it on the right, without losing the search: name it, merge it, remove wrong faces — then go to the next."
        ]],
        ["img", "16-face-search", "The photos of Ana found from one portrait, and who it may be."],
        ["tip", "From any photo of the library: right-click → <b>People</b> → <b>Find this person in other photos</b>."]
      ]
    },
    pt: {
      title: "Busca por rosto",
      summary: "Escolha a foto de alguém e ache todas as fotos em que essa pessoa aparece.",
      blocks: [
        ["steps", [
          "Na tela Pessoas, clique em <b>Buscar por foto…</b> e escolha uma imagem — ela não precisa estar no catálogo.",
          "Se a foto tiver vários rostos, escolha o que você quer.",
          "Mexa na <b>Similaridade mínima</b>: mais baixa acha mais fotos (e mais erros); mais alta, menos e mais certas.",
          "<b>Quem pode ser</b> lista as pessoas que combinam. Clique numa para abri-la à direita, sem perder a busca: dê nome, junte, tire as faces erradas — e passe para a próxima."
        ]],
        ["img", "16-face-search", "As fotos da Ana achadas a partir de um retrato, e quem pode ser."],
        ["tip", "De qualquer foto da biblioteca: botão direito → <b>Pessoas</b> → <b>Achar esta pessoa em outras fotos</b>."]
      ]
    }
  },
  {
    slug: "similar-faces",
    en: {
      title: "Similar faces and Merge all",
      summary: "Find the pieces of a person that the app grouped apart, and merge them in one go.",
      blocks: [
        ["p", "Sometimes the same person ends up split in two or three — older photos, a different haircut, glasses. <b>Find similar faces</b> searches the catalog with the person's average face and lists the candidates."],
        ["steps", [
          "Open a person (or right-click it in the side panel) and click <b>Find similar faces</b>.",
          "Turn on <b>Unnamed people only</b> to see only who still has no name.",
          "Right-click a candidate that is someone else → <b>Remove from list</b>. <b>Show removed</b> brings them back.",
          "Merge one candidate with its merge button, or all of them with <b>Merge all</b> — people with a name are never included."
        ]],
        ["img", "17-similar", "Faces similar to Ana: an unnamed person with her photos, ready to merge."],
        ["tip", "Check the list before <b>Merge all</b>: raise the minimum similarity to leave the doubtful ones out."]
      ]
    },
    pt: {
      title: "Rostos parecidos e Juntar todas",
      summary: "Ache os pedaços de uma pessoa que o app agrupou separados e junte tudo de uma vez.",
      blocks: [
        ["p", "Às vezes a mesma pessoa fica dividida em duas ou três — fotos antigas, outro corte de cabelo, óculos. <b>Procurar rostos parecidos</b> busca no catálogo com o rosto médio da pessoa e lista os candidatos."],
        ["steps", [
          "Abra uma pessoa (ou use o botão direito nela, no painel lateral) e clique em <b>Procurar rostos parecidos</b>.",
          "Ligue <b>Só pessoas sem nome</b> para ver só quem ainda não tem nome.",
          "Botão direito num candidato que é outra pessoa → <b>Tirar da lista</b>. <b>Mostrar as tiradas</b> traz de volta.",
          "Junte um candidato pelo botão dele, ou todos com <b>Juntar todas</b> — pessoas com nome nunca entram."
        ]],
        ["img", "17-similar", "Rostos parecidos com a Ana: uma pessoa sem nome com fotos dela, pronta para juntar."],
        ["tip", "Confira a lista antes do <b>Juntar todas</b>: aumente a similaridade mínima para deixar as duvidosas de fora."]
      ]
    }
  },
  {
    slug: "copies",
    en: {
      title: "Copies in other places",
      summary: "Find the same photo elsewhere — resized, recompressed, exported with another name or with a watermark.",
      blocks: [
        ["steps", [
          "Right-click a photo → <b>Find copies in other places</b>.",
          "Choose the tolerance: <b>Identical</b>; <b>Close: resized, recompressed, watermark</b>; or <b>Loose: also crops and color edits</b>.",
          "The copies come grouped by folder: the path on top and the photos below, with the match and the resolution of each. The photo you picked is marked <b>Original</b>.",
          "Right-click a copy to open it, show it in the file manager or move it to the Trash."
        ]],
        ["img", "18-copies", "The original and three copies in another folder: smaller, with a watermark and with another name."],
        ["tip", "The search uses the perceptual hash of each photo and the AI to separate photos that only look alike — a watermark in a corner still counts as the same photo."]
      ]
    },
    pt: {
      title: "Cópias em outros lugares",
      summary: "Ache a mesma foto em outro lugar — redimensionada, recomprimida, exportada com outro nome ou com marca d'água.",
      blocks: [
        ["steps", [
          "Botão direito numa foto → <b>Procurar cópias em outros lugares</b>.",
          "Escolha a tolerância: <b>Idêntica</b>; <b>Parecida: redimensionada, recomprimida, marca d'água</b>; ou <b>Ampla: também cortes e ajustes de cor</b>.",
          "As cópias vêm agrupadas por pasta: o caminho em cima e as fotos embaixo, com a semelhança e a resolução de cada uma. A foto escolhida aparece como <b>Original</b>.",
          "Botão direito numa cópia para abrir, mostrar no gerenciador de arquivos ou mandar para a Lixeira."
        ]],
        ["img", "18-copies", "O original e três cópias em outra pasta: menor, com marca d'água e com outro nome."],
        ["tip", "A busca usa o hash perceptual de cada foto e a IA para separar fotos que só se parecem — uma marca d'água no canto continua sendo a mesma foto."]
      ]
    }
  },
  {
    slug: "folders-mode",
    en: {
      title: "Managing files in Folders",
      summary: "Move photos and folders, create and rename folders and send files to the Trash — like a file manager.",
      blocks: [
        ["p", "In the Folders tab, the grid and the side panel also act on the files themselves. Every action asks for confirmation."],
        ["steps", [
          "<b>Move photos</b>: select them and drag onto a folder in the side panel — or right-click → <b>Move to folder…</b>.",
          "<b>Move a folder</b>: drag a subfolder onto another folder.",
          "<b>Folders</b>: right-click → <b>New folder…</b>, <b>Rename folder…</b> or <b>Move folder to Trash…</b>.",
          "<b>Photos</b>: right-click → <b>Rename file…</b> or <b>Move to Trash…</b>.",
          "From any tab: right-click a photo → <b>Show in folder</b> opens its folder here, with the photo selected."
        ]],
        ["img", "19-folders-mode", "The Exportadas folder, with copies ready to be organized."],
        ["tip", "PhotoIDX never deletes a file permanently: deleting always goes to the system Trash. Moving to another drive copies, checks the copy and only then removes the original. On Windows, network drives usually have no Recycle Bin — the confirmation warns you."]
      ]
    },
    pt: {
      title: "Gerenciando arquivos em Pastas",
      summary: "Mova fotos e pastas, crie e renomeie pastas e mande arquivos para a Lixeira — como num gerenciador de arquivos.",
      blocks: [
        ["p", "Na aba Pastas, a grade e o painel lateral agem também sobre os arquivos. Toda ação pede confirmação."],
        ["steps", [
          "<b>Mover fotos</b>: selecione e arraste para uma pasta do painel lateral — ou botão direito → <b>Mover para a pasta…</b>.",
          "<b>Mover uma pasta</b>: arraste uma subpasta para outra pasta.",
          "<b>Pastas</b>: botão direito → <b>Nova pasta…</b>, <b>Renomear pasta…</b> ou <b>Mandar pasta para a Lixeira…</b>.",
          "<b>Fotos</b>: botão direito → <b>Renomear arquivo…</b> ou <b>Mandar para a Lixeira…</b>.",
          "De qualquer aba: botão direito numa foto → <b>Mostrar na pasta</b> abre a pasta dela aqui, com a foto escolhida."
        ]],
        ["img", "19-folders-mode", "A pasta Exportadas, com cópias prontas para organizar."],
        ["tip", "O PhotoIDX nunca apaga um arquivo de vez: excluir sempre manda para a Lixeira do sistema. Mover para outro disco copia, confere a cópia e só então remove o original. No Windows, discos de rede costumam não ter Lixeira — a confirmação avisa."]
      ]
    }
  },
  {
    slug: "private",
    en: {
      title: "Private photos",
      summary: "Encrypt photos with a password: they disappear from the library and only show up in the Private album.",
      blocks: [
        ["steps", [
          "Open <b>Private</b> in the left bar and click <b>Create password</b>.",
          "Copy the <b>encryption key</b> and keep it somewhere safe: it recovers your photos if you forget the password.",
          "In the library, select photos and right-click → <b>Make private…</b>.",
          "Each original is encrypted into a <code>.photoidxe</code> file in the same folder, and its thumbnail is encrypted too.",
          "Private photos only show up in the <b>Private</b> album, after entering the password; they lock again after a while without use."
        ]],
        ["img", "20-privacy", "The Private photos screen: password, encryption key and what is protected."],
        ["tip", "Making a photo public again decrypts the original back to its normal file, in the same place."]
      ]
    },
    pt: {
      title: "Fotos privadas",
      summary: "Cifre fotos com uma senha: elas somem da biblioteca e só aparecem no álbum Privado.",
      blocks: [
        ["steps", [
          "Abra <b>Privado</b> na barra da esquerda e clique em <b>Criar senha</b>.",
          "Copie a <b>chave de criptografia</b> e guarde num lugar seguro: ela recupera as fotos se você esquecer a senha.",
          "Na biblioteca, selecione fotos e use botão direito → <b>Tornar privada…</b>.",
          "Cada original é cifrado num arquivo <code>.photoidxe</code> na mesma pasta, e a miniatura também é cifrada.",
          "As fotos privadas só aparecem no álbum <b>Privado</b>, com a senha; elas se trancam de novo depois de um tempo sem uso."
        ]],
        ["img", "20-privacy", "A tela Fotos privadas: senha, chave de criptografia e o que fica protegido."],
        ["tip", "Tornar uma foto pública de novo decifra o original de volta para o arquivo normal, no mesmo lugar."]
      ]
    }
  },
  {
    slug: "tasks",
    en: {
      title: "Tasks and performance",
      summary: "Follow each indexing stage, pause it, change the number of threads and retry what failed.",
      blocks: [
        ["p", "The Tasks screen shows each stage — pending, in progress, done, failed and items per minute — and the totals of the catalog."],
        ["img", "02-tasks", "Tasks: the stages of the indexing and the catalog totals."],
        ["steps", [
          "<b>Pause all</b> and <b>Start all</b> stop and resume every stage; each stage also has its own buttons.",
          "Change <b>Threads</b> to use more or less of the computer. A global limit keeps the whole app light.",
          "<b>Retry failed</b> puts the photos that failed back in the queue.",
          "<b>Rescan all</b> looks for new, changed and removed files in every folder."
        ]],
        ["tip", "A drive was disconnected in the middle? Its photos wait in the queue — nothing turns into an error — and continue when it comes back."]
      ]
    },
    pt: {
      title: "Tarefas e desempenho",
      summary: "Acompanhe cada etapa da indexação, pause, mude o número de threads e refaça o que falhou.",
      blocks: [
        ["p", "A tela Tarefas mostra cada etapa — pendentes, em andamento, concluídas, falhas e itens por minuto — e os totais do catálogo."],
        ["img", "02-tasks", "Tarefas: as etapas da indexação e os totais do catálogo."],
        ["steps", [
          "<b>Pausar tudo</b> e <b>Iniciar tudo</b> param e retomam todas as etapas; cada etapa também tem os seus botões.",
          "Mude as <b>Threads</b> para usar mais ou menos do computador. Um limite geral mantém o app leve.",
          "<b>Tentar as que falharam</b> devolve à fila as fotos que deram erro.",
          "<b>Varrer tudo de novo</b> procura arquivos novos, alterados e removidos em todas as pastas."
        ]],
        ["tip", "Um disco desconectou no meio? As fotos dele esperam na fila — nada vira erro — e continuam quando ele voltar."]
      ]
    }
  }
];

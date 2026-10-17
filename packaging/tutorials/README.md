# Capturas dos tutoriais (docs/site/tutorials)

As telas dos tutoriais (`docs/images/tutorials/<en|pt-br>/NN-*.jpg`) saem do
próprio app (`--screenshot`) num catálogo **fictício**: rostos gerados por IA
(pessoas que não existem, thispersondoesnotexist.com) e nomes inventados.
Refaça quando a interface mudar.

1. Rostos: baixe 8 imagens de `https://thispersondoesnotexist.com/random-person.jpeg`
   para `faces/face1.jpg` … `face8.jpg` (a ordem define quem é quem em `curate.py`).
2. Fotos: `dataset.py` (imagem `python:3.12` + pillow, piexif) monta
   `Fotos/` — o demo do app, retratos, fotos de grupo e cópias exportadas —
   e o `manifest.json` (quem está em cada arquivo). Semente fixa: rodar de
   novo dá os mesmos arquivos.
3. Catálogo: `index.sh` (no builder Linux) indexa num `PHOTOIDX_DATA_DIR`
   novo; `curate.py` dá os nomes, cria os grupos e os álbuns.
4. Capturas: `shots.sh` (no builder) tira as 20 telas nos dois idiomas;
   `tojpg.py` grava os JPG em `docs/images/tutorials/`.

Os caminhos (`/tut`, `/cache/tut-data`, `/shots`, `/faces`, `/demo`) são os
volumes montados no `docker run`.

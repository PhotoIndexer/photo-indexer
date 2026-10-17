#!/bin/sh
# Capturas dos tutoriais nos dois idiomas: /shots/<lang>/<id>.png
export PHOTOIDX_MODELS_DIR=/src/.cache/models QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
APP=/cache/build-core/src/app/PhotoIDX
F=/tut/Fotos
shot() {  # lang id wait args...
  lang=$1; id=$2; wait=$3; shift 3
  mkdir -p /shots/$lang
  HOME=/cache/tut-home-$lang PHOTOIDX_DATA_DIR=${DATA:-/cache/tut-data} timeout 200 $APP "$@" --wait $wait \
    --screenshot /shots/$lang/$id.png --size 1440x900 >/dev/null 2>&1
  echo "$lang $id $?"
}
for lang in en pt; do
  rm -rf /cache/tut-empty
  DATA=/cache/tut-empty shot $lang 01-empty 4000 --page library
  shot $lang 02-tasks 7000 --page tasks
  shot $lang 03-folders-page 7000 --page folders
  shot $lang 04-library 9000 --page library --tab folders --node "dir:$F/2025/Familia-Souza"
  shot $lang 05-viewer 10000 --page library --open "$F/2025/Familia-Souza/IMG_1077.jpg"
  shot $lang 06-dates 9000 --page library --tab dates --node "month:2025-06"
  shot $lang 07-places 9000 --page library --tab places
  shot $lang 08-categories 9000 --page library --tab categories
  shot $lang 09-albums 9000 --page library --tab albums
  shot $lang 10-search 9000 --page library --search "Souza"
  shot $lang 11-ai-search 14000 --page library --search "a dog playing outside" --ai-search
  shot $lang 12-filters 10000 --page library --rule "megapixels:gt:2" --rule "city:contains:Curitiba"
  shot $lang 13-people 9000 --page people
  shot $lang 14-person 10000 --person 0
  shot $lang 15-people-tab 9000 --page library --tab people --node "pgroup:e8c637be-05d3-4fbb-b149-68984b024c45"
  shot $lang 16-face-search 24000 --face-search "$F/2025/Familia-Souza/IMG_1003.jpg"
  shot $lang 17-similar 12000 --similar 0
  shot $lang 18-copies 10000 --page library --copies "$F/2025/Familia-Souza/IMG_1001.jpg"
  shot $lang 19-folders-mode 9000 --page library --tab folders --node "dir:$F/Exportadas"
  shot $lang 20-privacy 8000 --page privacy
done

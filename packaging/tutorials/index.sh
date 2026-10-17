#!/bin/sh
# Indexa as fotos dos tutoriais num catálogo novo.
export PHOTOIDX_DATA_DIR=/cache/tut-data PHOTOIDX_MODELS_DIR=/src/.cache/models QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
rm -rf /cache/tut-data /cache/tut-home-en /cache/tut-home-pt
for lang in en pt; do
  mkdir -p /cache/tut-home-$lang/.config/PhotoE
  code=en; [ $lang = pt ] && code=pt_BR
  printf '[ui]\ndark=true\nlanguage=%s\n[library]\ntab=folders\n' $code > /cache/tut-home-$lang/.config/PhotoE/PhotoIDX.conf
done
cp -r /cache/tut-home-en /cache/tut-home-idx
export HOME=/cache/tut-home-idx
timeout 400 /cache/build-core/src/app/PhotoIDX /tut/Fotos --page tasks --wait 14000 --screenshot /shots/raw-tasks-en.png --size 1440x900 >/dev/null 2>&1
echo "first run $?"
timeout 600 /cache/build-core/src/app/PhotoIDX --page tasks --wait 240000 --screenshot /shots/raw-tasks-done.png --size 1440x900 >/dev/null 2>&1
echo "second run $?"
grep -E "WARNING|CRITICAL" /cache/tut-data/logs/app.log | sed 's/^\[[^]]*\] //' | sort | uniq -c | head

#!/bin/sh
# Põe o PhotoIDX (extraído do .tar.gz) no menu de aplicativos do usuário e
# faz os arquivos .photoidxe (fotos privadas) abrirem nele com dois cliques.
# Só mexe em ~/.local/share; rode de novo se mover a pasta do app.
#
#   ./install-desktop.sh            instala
#   ./install-desktop.sh --remove   desfaz
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
DATA=${XDG_DATA_HOME:-$HOME/.local/share}
APPS="$DATA/applications"
MIME="$DATA/mime"
ICONS="$DATA/icons/hicolor/256x256/apps"

if [ "${1:-}" = "--remove" ]; then
    rm -f "$APPS/photoidx.desktop" "$MIME/packages/photoidx.xml" "$ICONS/photoidx.png"
else
    mkdir -p "$APPS" "$MIME/packages" "$ICONS"
    sed "s|^Exec=.*|Exec=\"$HERE/bin/PhotoIDX\" %F|" "$HERE/share/applications/photoidx.desktop" > "$APPS/photoidx.desktop"
    cp "$HERE/share/mime/packages/photoidx.xml" "$MIME/packages/photoidx.xml"
    [ -f "$HERE/share/icons/hicolor/256x256/apps/photoidx.png" ] && cp "$HERE/share/icons/hicolor/256x256/apps/photoidx.png" "$ICONS/"
fi
command -v update-mime-database >/dev/null && update-mime-database "$MIME" || true
command -v update-desktop-database >/dev/null && update-desktop-database "$APPS" || true
command -v xdg-mime >/dev/null && [ "${1:-}" != "--remove" ] && xdg-mime default photoidx.desktop application/x-photoidxe || true
echo "PhotoIDX: ${1:+removed from}${1:-installed in} the applications menu."

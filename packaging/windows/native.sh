#!/usr/bin/env bash
# Windows ARM64 num Windows ARM nativo (MSVC) — o único alvo que não sai de
# um builder em Docker: o Qt só publica o arm64 do Windows compilado com
# MSVC, e o llvm-mingw do builder x64 não serve para ele.
#
#   native.sh [alvo]     compila e monta a pasta do app em .cache/stage/<alvo>/
#
# O .msi NÃO sai daqui: a pasta volta para o builder Windows em Docker
# (`make msi-stage TARGET=windows-arm64`), o mesmo wixl e os mesmos GUIDs do
# x64. Roda no Git Bash com o ambiente do MSVC carregado (vcvarsall arm64; na
# CI, ilammy/msvc-dev-cmd). Uso: `make windows-native`.
#
# Tudo o que baixa fica em .cache/ (Qt, OpenCV, venv), que a CI guarda no
# cache do Actions.
set -euo pipefail

TARGET=${1:-windows-arm64}
case $TARGET in
    windows-arm64) AQT_HOST=windows_arm64; AQT_ARCH=win64_msvc2022_arm64; QT_DIR=msvc2022_arm64 ;;
    windows-x64)   AQT_HOST=windows;       AQT_ARCH=win64_msvc2022_64;    QT_DIR=msvc2022_64 ;;
    *) echo "alvo não suportado aqui: $TARGET" >&2; exit 1 ;;
esac
QT_VERSION=${QT_VERSION:-6.8.3}
AQT_VERSION=3.3.0
CACHE=.cache
cd "$(dirname "$0")/../.."

# Python do build: aqtinstall (Qt), Pillow (ícone), CMake e Ninja recentes.
VENV=$CACHE/venv-$TARGET
if [ ! -x "$VENV/Scripts/python.exe" ]; then
    python -m venv "$VENV"
    "$VENV/Scripts/python.exe" -m pip install -q --upgrade pip
fi
# Sempre: um venv restaurado do cache da CI pode estar incompleto (sem o
# Pillow, o art.py para). Com tudo instalado, o pip só confere e sai.
"$VENV/Scripts/python.exe" -m pip install -q "aqtinstall==$AQT_VERSION" "Pillow>=10" "cmake==3.31.6" ninja
PATH="$(cd "$VENV/Scripts" && pwd):$PATH"
# O python do venv pelo caminho, não pelo PATH: o do runner pode vir antes.
PY="$(cd "$VENV/Scripts" && pwd)/python.exe"

echo "==> [$TARGET] versão $($PY packaging/build.py version)"
$PY packaging/build.py model
$PY packaging/build.py art

QT=$CACHE/qt/$QT_VERSION/$QT_DIR
if [ ! -d "$QT/bin" ]; then
    # Pelo módulo, não pelo aqt.exe: no Git Bash do runner Windows ARM o
    # Scripts/ do venv no PATH não basta para achar o executável.
    "$PY" -m aqt install-qt "$AQT_HOST" desktop "$QT_VERSION" "$AQT_ARCH" -O "$CACHE/qt"
fi

# OpenCV estático com o CRT dinâmico (/MD), o mesmo do app e do Qt. No MSVC
# ele instala no formato "pacote Windows": o OpenCVConfig.cmake da raiz adivinha
# a arquitetura (e diz x64 no ARM64); o do próprio build fica em staticlib/ —
# é ele que o configure do app usa (OpenCV_DIR).
OPENCV=$CACHE/deps/$TARGET/opencv
OPENCV_WORK=$CACHE/deps/$TARGET/opencv-work bash packaging/deps/opencv.sh "$OPENCV" \
    -DBUILD_WITH_STATIC_CRT=OFF -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl

EXTRA=()

B=build/$TARGET
cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl \
    -DCMAKE_PREFIX_PATH="$(cd "$QT" && pwd -W);$(cd "$OPENCV" && pwd -W)" \
    -DOpenCV_DIR="$(cd "$OPENCV/staticlib" && pwd -W)" \
    -DPHOTOIDX_BUILD_TESTS=OFF -DPHOTOIDX_DEPLOY=ON "${EXTRA[@]}" ${CMAKE_ARGS:-}
cmake --build "$B"

STAGE=$CACHE/stage/$TARGET
rm -rf "$STAGE"
# O deploy do Qt (windeployqt) roda dentro do install.
# Prefixo ABSOLUTO: o deploy do Qt recusa caminho relativo (qt.conf).
mkdir -p "$STAGE"
cmake --install "$B" --prefix "$(cd "$STAGE" && pwd -W)"
$PY packaging/build.py build-info --target "$TARGET" --out "$STAGE/build-info.json"
# Runtime do Visual C++ (app, Qt e ONNX Runtime são MSVC): ao lado do .exe,
# sem depender do vc_redist instalado na máquina.
$PY packaging/build.py vcruntime --target "$TARGET" --out "$CACHE/vcruntime-$TARGET" >/dev/null
cp "$CACHE/vcruntime-$TARGET"/msvcp140*.dll "$CACHE/vcruntime-$TARGET"/vcruntime140*.dll \
   "$CACHE/vcruntime-$TARGET"/concrt140.dll "$STAGE/"
ls "$STAGE"
echo "==> [$TARGET] pasta pronta em $STAGE (o .msi sai com: make msi-stage TARGET=$TARGET)"

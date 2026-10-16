#!/bin/sh
# Compila o PhotoIDX para Windows x64 (llvm-mingw) e gera o .msi:
#
#   build.sh                 dist/<pacote>-windows-x64.msi
#   build.sh msi <alvo>      só o .msi de uma pasta já montada em
#                            .cache/stage/<alvo>/ (o windows-arm64, que vem
#                            do runner Windows ARM: packaging/windows/native.sh)
#
# Roda dentro do builder (packaging/windows/Dockerfile), com o repositório em
# /src e as pastas de build no volume /cache. Uso: `make windows`, `make msi-stage`.
# CMAKE_ARGS (ambiente) vai direto para o configure.
set -eu

QT_WIN=/opt/qt/$QT_VERSION/llvm-mingw_64
QT_HOST=/opt/qt/$QT_VERSION/gcc_64
MEM_GB=$(awk '/MemTotal/ { print int($2 / 1048576) }' /proc/meminfo)
JOBS=$(nproc)
[ "$MEM_GB" -lt "$JOBS" ] && JOBS=$(( MEM_GB > 1 ? MEM_GB : 1 ))

cd /src
echo "==> versão $(python3 packaging/build.py version)"
NUMBER=$(python3 packaging/build.py version --number)

msi() { # alvo pasta
    NAME=$(python3 packaging/build.py package-name --target "$1")
    echo "==> [$1] gerando o .msi"
    python3 packaging/windows/msi.py --dir "$2" --version "$NUMBER" --arch "${1#windows-}" \
        --out "dist/$NAME.msi" --work "/cache/msi-$1"
    rm -rf "/cache/msi-$1"
    ls -lh "dist/$NAME.msi"
}

if [ "${1:-}" = msi ]; then
    TARGET=${2:?uso: build.sh msi <alvo>}
    [ -f ".cache/stage/$TARGET/PhotoIDX.exe" ] || { echo "falta .cache/stage/$TARGET/PhotoIDX.exe" >&2; exit 1; }
    msi "$TARGET" ".cache/stage/$TARGET"
    exit 0
fi

TARGET=windows-x64
python3 packaging/build.py model
python3 packaging/build.py art
EXTRA=""

B=/cache/build-$TARGET
echo "==> [$TARGET] compilando (jobs: $JOBS)"
# shellcheck disable=SC2086
cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/mingw/x86_64.cmake \
    -DCMAKE_PREFIX_PATH="$QT_WIN;/opt/deps/x86_64" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DPHOTOIDX_ONNXRUNTIME_ROOT=/opt/deps/x86_64/onnxruntime \
    -DPHOTOIDX_BUILD_TESTS=OFF -DPHOTOIDX_DEPLOY=OFF $EXTRA ${CMAKE_ARGS:-}
cmake --build "$B" -j "$JOBS"

W=/cache/package-$TARGET
rm -rf "$W"
echo "==> [$TARGET] montando a pasta do app"
cmake --install "$B" --prefix "$W/app" --strip
python3 packaging/build.py build-info --target "$TARGET" --out "$W/app/build-info.json"
# Runtime do Visual C++ que o onnxruntime.dll (MSVC) importa: o bundle.py só
# copia dele as DLLs que algum binário do pacote pede.
python3 packaging/build.py vcruntime --target "$TARGET" --out "/cache/vcruntime-$TARGET" >/dev/null
python3 packaging/windows/bundle.py --dir "$W/app" --qt "$QT_WIN" --qt-host "$QT_HOST" \
    --lib-dir /opt/llvm-mingw/x86_64-w64-mingw32/bin --lib-dir /opt/deps/x86_64/onnxruntime/lib \
    --lib-dir "/cache/vcruntime-$TARGET"
msi "$TARGET" "$W/app"
rm -rf "$W"

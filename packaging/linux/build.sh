#!/bin/sh
# Compila o PhotoIDX para Linux (a arquitetura do container) e:
#
#   build.sh test       compila e roda os testes (ctest)
#   build.sh package    compila, testa e gera dist/<pacote>-linux-<x64|arm64>.tar.gz
#   build.sh configure  só o configure (depuração)
#
# Roda dentro do builder (packaging/linux/Dockerfile), com o repositório em
# /src e as pastas de build no volume /cache. Uso: `make linux`, `make test`.
# BUILD_TYPE (Release) e SKIP_TESTS=1 vêm do ambiente.
set -eu

MODE=${1:-package}
case $(uname -m) in
    x86_64)  TARGET=linux-x64 ;;
    aarch64) TARGET=linux-arm64 ;;
    *) echo "arquitetura sem suporte: $(uname -m)" >&2; exit 1 ;;
esac
BUILD_TYPE=${BUILD_TYPE:-Release}
B=/cache/build-$TARGET
# Um compilador de C++ com Qt passa de 1 GB: limita os jobs pela memória do Docker.
MEM_GB=$(awk '/MemTotal/ { print int($2 / 1048576) }' /proc/meminfo)
JOBS=$(nproc)
[ "$MEM_GB" -lt "$JOBS" ] && JOBS=$(( MEM_GB > 1 ? MEM_GB : 1 ))

cd /src
echo "==> [$TARGET] versão $(python3 packaging/build.py version)"

EXTRA=""
if [ "$MODE" = package ]; then
    # O que vai no pacote além do binário: modelo de IA e ícones (o CMake os
    # instala de onde ficaram).
    python3 packaging/build.py model
    python3 packaging/build.py art
fi

# shellcheck disable=SC2086
cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DPHOTOIDX_DEPLOY=ON $EXTRA \
    ${CMAKE_ARGS:-}
[ "$MODE" = configure ] && exit 0
echo "==> [$TARGET] compilando (jobs: $JOBS)"
cmake --build "$B" -j "$JOBS"

if [ -z "${SKIP_TESTS:-}" ] && [ -f "$B/CTestTestfile.cmake" ]; then
    echo "==> [$TARGET] testes"
    ctest --test-dir "$B" --output-on-failure -j "$JOBS"
fi
[ "$MODE" = test ] && exit 0

NAME=$(python3 packaging/build.py package-name --target "$TARGET")
W=/cache/package-$TARGET
rm -rf "$W"
echo "==> [$TARGET] instalando em $NAME/"
cmake --install "$B" --prefix "$W/$NAME" --strip
if [ ! -x "$W/$NAME/bin/PhotoIDX" ]; then
    echo "pacote sem bin/PhotoIDX: o app (src/app) não foi compilado" >&2
    exit 1
fi
# Atalho na raiz: quem extrai o .tar.gz acha o executável sem procurar.
ln -s bin/PhotoIDX "$W/$NAME/PhotoIDX"
python3 packaging/build.py build-info --target "$TARGET" --out "$W/$NAME/share/photoidx/build-info.json"

mkdir -p dist
rm -f "dist/$NAME.tar.gz"
tar -C "$W" -czf "dist/$NAME.tar.gz" "$NAME"
rm -rf "$W"
ls -lh "dist/$NAME.tar.gz"

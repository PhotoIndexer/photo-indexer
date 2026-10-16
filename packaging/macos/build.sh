#!/bin/sh
# Compila o PhotoIDX para macOS e gera um .dmg por arquitetura pedida:
#
#   dist/<pacote>-macos-<x64|arm64>.dmg      arquiteturas: x86_64 | arm64
#
# Roda dentro do builder (packaging/macos/Dockerfile), com o repositório em
# /src e as pastas de build no volume /cache. Uso: `make dmg` / `make dmg-all`.
# CMAKE_ARGS (ambiente) vai direto para o configure.
set -eu

[ $# -gt 0 ] || set -- x86_64 arm64
# Piso do app: 13.3 é onde a libc++ da Apple passa a ter std::to_chars de
# ponto flutuante (o toolchain do clivcut fica em 13.0). O Info.plist sai
# com o piso real de todos os binários do pacote (bundle.py).
MACOS_DEPLOYMENT_TARGET=${MACOS_DEPLOYMENT_TARGET:-13.3}
QT_MAC=/opt/qt/$QT_VERSION/macos
QT_HOST=/opt/qt/$QT_VERSION/gcc_64
# Um compilador de C++ com Qt passa de 1 GB: limita os jobs pela memória do Docker.
MEM_GB=$(awk '/MemTotal/ { print int($2 / 1048576) }' /proc/meminfo)
JOBS=$(nproc)
[ "$MEM_GB" -lt "$JOBS" ] && JOBS=$(( MEM_GB > 1 ? MEM_GB : 1 ))

cd /src
echo "==> versão $(python3 packaging/build.py version)"
NUMBER=$(python3 packaging/build.py version --number)
python3 packaging/build.py model
# O CMake só embute o ícone no .app se o .icns existir na configuração.
python3 packaging/build.py art

for ARCH in "$@"; do
    case $ARCH in
        x86_64) TARGET=macos-x64 ;;
        arm64)  TARGET=macos-arm64 ;;
        *) echo "arquitetura desconhecida: $ARCH (use x86_64 ou arm64)" >&2; exit 1 ;;
    esac
    EXTRA=""

    B=/cache/build-$TARGET
    # WrapOpenGL_AGL: o SDK atual não tem mais o AGL.framework, que o Qt 6.8
    # ainda pede (FindWrapOpenGL); o OpenGL.framework o substitui (clivcut idem).
    echo "==> [$TARGET] compilando (jobs: $JOBS)"
    # shellcheck disable=SC2086
    cmake -S . -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=/opt/darwin/$ARCH.cmake \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOS_DEPLOYMENT_TARGET" \
        -DCMAKE_PREFIX_PATH="$QT_MAC;/opt/deps/$ARCH" \
        -DQT_HOST_PATH="$QT_HOST" -DQT_NO_APPLE_SDK_AND_XCODE_CHECK=ON \
        -DWrapOpenGL_AGL=/opt/sdk/MacOSX.sdk/System/Library/Frameworks/OpenGL.framework \
        -DPHOTOIDX_ONNXRUNTIME_ROOT=/opt/deps/$ARCH/onnxruntime \
        -DPHOTOIDX_BUILD_TESTS=OFF -DPHOTOIDX_DEPLOY=OFF $EXTRA ${CMAKE_ARGS:-}
    cmake --build "$B" -j "$JOBS"

    NAME=$(python3 packaging/build.py package-name --target "$TARGET")
    W=/cache/package-$TARGET
    rm -rf "$W"
    echo "==> [$TARGET] montando o PhotoIDX.app"
    cmake --install "$B" --prefix "$W/stage"
    python3 packaging/build.py build-info --target "$TARGET" \
        --out "$W/stage/PhotoIDX.app/Contents/Resources/build-info.json"
    python3 packaging/macos/bundle.py --arch "$ARCH" --app "$W/stage/PhotoIDX.app" \
        --qt "$QT_MAC" --qt-host "$QT_HOST" --lib-dir "/opt/deps/$ARCH/onnxruntime/lib" \
        --version "$NUMBER"
    echo "==> [$TARGET] gerando o .dmg"
    python3 packaging/macos/dmg.py --app "$W/stage/PhotoIDX.app" --work "$W/dmg" \
        --out "dist/$NAME.dmg"
    rm -rf "$W"
done
ls -lh dist/*.dmg

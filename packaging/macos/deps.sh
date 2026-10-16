#!/bin/sh
# Dependências nativas do PhotoIDX para macOS, para UMA arquitetura
# (x86_64 | arm64) em /opt/deps/<arch>:
#
#   OpenCV estático e mínimo (packaging/deps/opencv.sh, o mesmo do Linux e do
#   Windows) e o ONNX Runtime oficial em /opt/deps/<arch>/onnxruntime (a
#   .dylib tem install name @rpath/..., pronta para o Contents/Frameworks).
#
# Roda no build da imagem (packaging/macos/Dockerfile), depois do setup.sh.
set -eu

ARCH=$1
# Mesmas versões do src/cmake/OnnxRuntime.cmake: a Microsoft parou de
# publicar o pacote do Mac Intel depois da 1.23.2.
case $ARCH in
    x86_64) ORT_VERSION=1.23.2; ORT_PKG=onnxruntime-osx-x86_64-$ORT_VERSION ;;
    arm64)  ORT_VERSION=1.30.0; ORT_PKG=onnxruntime-osx-arm64-$ORT_VERSION ;;
    *) echo "arquitetura desconhecida: $ARCH" >&2; exit 1 ;;
esac

P=/opt/deps/$ARCH
mkdir -p "$P/onnxruntime"

echo "==> [$ARCH] OpenCV"
OPENCV_WORK=/tmp/opencv-$ARCH bash /opt/darwin/opencv.sh "$P" \
    -DCMAKE_TOOLCHAIN_FILE=/opt/darwin/$ARCH.cmake
rm -rf /tmp/opencv-$ARCH

echo "==> [$ARCH] ONNX Runtime $ORT_VERSION"
# O .tgz traz "./onnxruntime-osx-<arch>-<versão>/": extrai e sobe um nível.
mkdir -p /tmp/ort-$ARCH
curl -fsSL --retry 3 "https://github.com/microsoft/onnxruntime/releases/download/v$ORT_VERSION/$ORT_PKG.tgz" \
    | tar -xz -C /tmp/ort-$ARCH
mv /tmp/ort-$ARCH/$ORT_PKG/* "$P/onnxruntime/"
rm -rf /tmp/ort-$ARCH
ls "$P/onnxruntime/lib"

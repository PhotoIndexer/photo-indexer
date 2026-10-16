#!/usr/bin/env bash
# OpenCV estático e mínimo para o PhotoIDX.
#
#   packaging/deps/opencv.sh <prefixo> [args extras do cmake...]
#
# Só os módulos que o imaging usa: core, imgproc, imgcodecs (JPEG/PNG com
# libjpeg-turbo e libpng embutidos), features2d (SIFT do Merge), calib3d
# (estimateAffinePartial2D), objdetect (HOG do Auto) e flann. Nada de vídeo,
# GUI, dnn, Python ou Java: o binário do app fica autocontido e pequeno.
#
# Roda igual no builder Linux, na CI (Linux/macOS/Windows com bash) e no Mac
# do desenvolvedor. Argumentos extras vão direto para o cmake (toolchain de
# cross-compile, CMAKE_OSX_ARCHITECTURES, gerador do Visual Studio...).
set -euo pipefail

# Versão fixa: trocar é decisão explícita (regra 18), nunca efeito de um build.
OPENCV_VERSION=4.12.0

PREFIX=${1:?uso: opencv.sh <prefixo> [args do cmake...]}
shift
WORK=${OPENCV_WORK:-$(dirname "$PREFIX")/opencv-work}
mkdir -p "$WORK"

if [ -f "$PREFIX/.opencv-$OPENCV_VERSION" ]; then
    echo "OpenCV $OPENCV_VERSION já está em $PREFIX"
    exit 0
fi

SRC="$WORK/opencv-$OPENCV_VERSION"
if [ ! -d "$SRC" ]; then
    curl -fsSL "https://github.com/opencv/opencv/archive/refs/tags/$OPENCV_VERSION.tar.gz" | tar -xz -C "$WORK"
fi

cmake -S "$SRC" -B "$WORK/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_LIST=core,imgproc,imgcodecs,features2d,calib3d,objdetect,flann \
    -DBUILD_JPEG=ON -DBUILD_PNG=ON -DBUILD_ZLIB=ON \
    -DWITH_JPEG=ON -DWITH_PNG=ON \
    -DWITH_TIFF=OFF -DWITH_WEBP=OFF -DWITH_OPENJPEG=OFF -DWITH_JASPER=OFF -DWITH_OPENEXR=OFF \
    -DWITH_IMGCODEC_HDR=OFF -DWITH_IMGCODEC_SUNRASTER=OFF -DWITH_IMGCODEC_PXM=OFF -DWITH_IMGCODEC_PFM=OFF \
    -DWITH_AVIF=OFF -DWITH_SPNG=OFF -DWITH_GDAL=OFF -DWITH_GDCM=OFF \
    -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_V4L=OFF -DWITH_MSMF=OFF -DWITH_DSHOW=OFF \
    -DWITH_GTK=OFF -DWITH_QT=OFF -DWITH_WIN32UI=OFF -DWITH_OPENGL=OFF -DWITH_VTK=OFF \
    -DWITH_OPENCL=OFF -DWITH_CUDA=OFF -DWITH_IPP=OFF -DWITH_ITT=OFF -DWITH_LAPACK=OFF \
    -DWITH_EIGEN=OFF -DWITH_PROTOBUF=OFF -DWITH_ADE=OFF -DWITH_QUIRC=OFF -DWITH_OBSENSOR=OFF \
    -DWITH_TBB=OFF -DWITH_OPENMP=OFF -DBUILD_opencv_apps=OFF \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
    -DBUILD_JAVA=OFF -DBUILD_opencv_python3=OFF -DBUILD_opencv_python_bindings_generator=OFF \
    -DOPENCV_GENERATE_PKGCONFIG=OFF -DOPENCV_FORCE_3RDPARTY_BUILD=ON \
    "$@"
cmake --build "$WORK/build" --parallel
cmake --install "$WORK/build"
touch "$PREFIX/.opencv-$OPENCV_VERSION"

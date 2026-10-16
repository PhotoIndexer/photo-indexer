# syntax=docker/dockerfile:1
# Builder do pacote Windows x64 do PhotoIDX, num container Linux — o mesmo
# modelo do builder de macOS (e do clivcut): cross-compile e instalador sem
# Windows.
#
#   compilador  llvm-mingw (clang + lld, UCRT, libc++) — a MESMA versão com
#               que o Qt oficial "win64_llvm_mingw" foi compilado (LLVM 17.0.6):
#               o app e o Qt dividem a libc++.dll do pacote
#   Qt          pacote oficial win64_llvm_mingw + o de Linux para as
#               ferramentas de host (moc, qmlcachegen...) via QT_HOST_PATH
#   OpenCV      estático (packaging/deps/opencv.sh, o mesmo dos outros alvos)
#   ONNX Rt.    pacote oficial win-x64 (DLL do MSVC; API em C, o lld liga
#               direto na .lib de importação)
#   .msi        wixl (msitools), GUIDs fixos em packaging/windows/msi.py
#
# Windows ARM64 NÃO sai daqui: o Qt só publica o arm64 compilado com MSVC
# (win64_msvc2022_arm64), sem build llvm-mingw para aarch64. Ele é compilado
# num runner Windows ARM nativo (packaging/windows/native.sh) e só o .msi
# volta para este container (`make msi-stage`).
#
# Uso: pelo Makefile da raiz — `make windows`, `make msi`.

ARG BASE=ubuntu:noble-20260905
FROM ${BASE}

ARG QT_VERSION=6.8.3
ARG AQT_VERSION=3.3.0
# llvm-mingw 20231128 = LLVM 17.0.6, o toolchain do Qt 6.8 para MinGW/LLVM.
ARG LLVM_MINGW=20231128
ARG ORT_VERSION=1.30.0

# libglib2.0: o QtCore de Linux (ferramentas de host, como o qmlimportscanner) liga nela.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils unzip git cmake ninja-build file \
        python3 python3-venv msitools wixl fonts-liberation2 libglib2.0-0t64 \
 && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW}/llvm-mingw-${LLVM_MINGW}-ucrt-ubuntu-20.04-x86_64.tar.xz" \
      | tar -xJ -C /opt \
 && mv /opt/llvm-mingw-${LLVM_MINGW}-ucrt-ubuntu-20.04-x86_64 /opt/llvm-mingw
ENV PATH=/opt/llvm-mingw/bin:$PATH

# Python do builder num venv: aqtinstall (Qt) e Pillow (ícone).
RUN python3 -m venv /opt/builder \
 && /opt/builder/bin/pip install --no-cache-dir --upgrade pip \
 && /opt/builder/bin/pip install --no-cache-dir "aqtinstall==${AQT_VERSION}" "Pillow>=10"
ENV PATH=/opt/builder/bin:$PATH \
    QT_VERSION=${QT_VERSION}

RUN aqt install-qt windows desktop ${QT_VERSION} win64_llvm_mingw -O /opt/qt \
 && aqt install-qt linux desktop ${QT_VERSION} linux_gcc_64 -O /opt/qt \
 && rm -rf /opt/qt/${QT_VERSION}/*/doc /opt/qt/${QT_VERSION}/*/examples aqtinstall.log

COPY toolchain/x86_64.cmake /opt/mingw/x86_64.cmake

COPY --from=deps opencv.sh /opt/mingw/opencv.sh
RUN OPENCV_WORK=/tmp/opencv bash /opt/mingw/opencv.sh /opt/deps/x86_64 \
        -DCMAKE_TOOLCHAIN_FILE=/opt/mingw/x86_64.cmake \
 && rm -rf /tmp/opencv

RUN mkdir -p /opt/deps/x86_64/onnxruntime \
 && curl -fsSL -o /tmp/ort.zip "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip" \
 && unzip -q /tmp/ort.zip -d /tmp/ort && mv /tmp/ort/*/* /opt/deps/x86_64/onnxruntime/ \
 && rm -rf /tmp/ort /tmp/ort.zip

ENV HOME=/tmp LANG=C.UTF-8 PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /src
ENTRYPOINT ["sh", "/src/packaging/windows/build.sh"]

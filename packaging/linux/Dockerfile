# Builder Linux do PhotoIDX (C++/Qt): compila, roda os testes e gera o
# .tar.gz do Linux. Também é o ambiente de desenvolvimento quando a máquina
# não tem Qt/OpenCV instalados (regra 15.1): `make shell`, `make test`.
#
# A mesma receita serve às duas arquiteturas: a imagem é construída para a
# plataforma pedida (`docker build --platform linux/amd64|linux/arm64`). Num
# Mac/PC x86_64 o arm64 roda emulado (qemu, lento); na CI ele roda nativo
# num runner arm64.
#
# Versões fixas (regra 18): imagem base datada, Qt e OpenCV. O ONNX Runtime
# vem pronto pelo próprio CMake (FetchContent, versão em src/cmake/OnnxRuntime.cmake).
#
# Piso do pacote: glibc 2.39 (a do Ubuntu 24.04 desta imagem).
#
# Uso: pelo Makefile da raiz — `make linux`, `make test`, `make shell`.

ARG BASE=ubuntu:noble-20260905
FROM ${BASE}

ARG QT_VERSION=6.8.3
ARG AQT_VERSION=3.3.0

# libGL/EGL/xkb/xcb/fontconfig: o Qt carrega na hora de abrir janela; o
# xvfb + mesa permite os testes de interface e o --screenshot sem tela.
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl git build-essential cmake ninja-build pkg-config file \
        python3 python3-venv \
        libgl1-mesa-dev libegl1 libgl1-mesa-dri libglib2.0-0 libfontconfig1 libfreetype6 libdbus-1-3 \
        libxkbcommon0 libxkbcommon-x11-0 libx11-xcb1 libxcb1 libxcb-cursor0 libxcb-icccm4 libxcb-image0 \
        libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1 \
        libxcb-glx0 libxcb-xfixes0 libxcb-sync1 libxrender1 libxi6 libsm6 libice6 \
        xvfb xauth fonts-liberation2 \
 && rm -rf /var/lib/apt/lists/*

# Qt oficial (o do Ubuntu é 6.4, antigo demais para o QML do app). O aqt
# chama o host Linux arm64 de "linux_arm64"; /opt/qt/current aponta para a
# pasta da arquitetura (gcc_64 | gcc_arm64).
RUN python3 -m venv /opt/aqt \
 && /opt/aqt/bin/pip install --no-cache-dir "aqtinstall==${AQT_VERSION}" \
 && case "$(uname -m)" in \
        x86_64)  host=linux;       arch=linux_gcc_64;    dir=gcc_64 ;; \
        aarch64) host=linux_arm64; arch=linux_gcc_arm64; dir=gcc_arm64 ;; \
        *) echo "arquitetura sem Qt: $(uname -m)" >&2; exit 1 ;; \
    esac \
 && /opt/aqt/bin/aqt install-qt ${host} desktop ${QT_VERSION} ${arch} -O /opt/qt \
 && ln -s /opt/qt/${QT_VERSION}/${dir} /opt/qt/current \
 && rm -rf /opt/aqt /opt/qt/${QT_VERSION}/*/doc /opt/qt/${QT_VERSION}/*/examples aqtinstall.log
ENV CMAKE_PREFIX_PATH=/opt/qt/current:/opt/opencv \
    PATH=/opt/qt/current/bin:$PATH

COPY deps/opencv.sh /tmp/opencv.sh
RUN OPENCV_WORK=/tmp/opencv-work bash /tmp/opencv.sh /opt/opencv && rm -rf /tmp/opencv-work

# Python do builder num venv (o do sistema é "externally managed"): Pillow
# para a arte (ícones).
RUN python3 -m venv /opt/builder \
 && /opt/builder/bin/pip install --no-cache-dir --upgrade pip \
 && /opt/builder/bin/pip install --no-cache-dir "Pillow>=10"
ENV PATH=/opt/builder/bin:$PATH

# HOME gravável (o container também roda com o uid do host); caches no
# repositório (.cache/) e no volume /cache (pastas de build).
ENV HOME=/tmp LANG=C.UTF-8 QT_QPA_PLATFORM=offscreen PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /src
ENTRYPOINT ["sh", "/src/packaging/linux/build.sh"]
CMD ["package"]

# PhotoIDX — base de compilação (regra 18: passo novo de build, teste ou
# empacotamento entra aqui, como alvo; os workflows só chamam o make).
#
#   make help          lista os alvos
#   make               compila e testa (nativo se houver Qt; senão no builder Linux)
#   make dist          todos os instaladores que saem de Docker, em dist/
#   make linux         .tar.gz do Linux desta arquitetura (dmg/windows: idem)
#
# Os instaladores saem de builders em Docker, como no clivcut
# (../clivcut/Makefile): um por plataforma, em packaging/<plataforma>/
# Dockerfile, com o repositório montado em /src e as pastas de build num
# volume (/cache). Além do Docker, nada é instalado no host:
#
#   Linux    .tar.gz   x86_64 nativo; arm64 com --platform linux/arm64 (qemu
#                      num host x86_64 — lento; nativo num host/runner arm64)
#   macOS    .dmg      cross-compile (osxcross + SDK exportado deste Mac)
#   Windows  .msi      x64: cross-compile (llvm-mingw); arm64: só num Windows
#                      ARM nativo (make windows-native) + make msi-stage
#
# Compatível com o GNU Make 3.81 do macOS.

# O Docker Desktop nem sempre põe o CLI no PATH.
export PATH := $(PATH):/Applications/Docker.app/Contents/Resources/bin

PYTHON       ?= python3
DOCKER       ?= docker
# Arquiteturas de `make linux` / `make dmg`: x86_64 | arm64 (separadas por
# espaço). Padrão: a desta máquina.
HOST_ARCH    := $(shell uname -m | sed -e 's/aarch64/arm64/')
ARCHS        ?= $(HOST_ARCH)
# Volume Docker com as pastas de build (rebuild incremental, fora do repositório).
CACHE_VOLUME ?= photoidx-cache
# 1 = usa as imagens dos builders que já existem (a CI as cria antes, com cache).
SKIP_BUILDER ?=
BUILD_TYPE   ?= Release
# Argumentos extras para o app no `make run` (ex.: ARGS="--screenshot /tmp/x.png").
ARGS         ?=
# Alvo do `make msi-stage` / `make windows-native` (ver packaging/windows/native.sh).
TARGET       ?= windows-arm64
SDK_DIR      := .cache/macos-sdk
ART_OUT      ?= .cache/art-preview
# Pasta do hotsite montado (`make site`).
SITE_OUT     ?= .cache/site

LINUX_BUILDER   ?= photoidx-linux-builder
MACOS_BUILDER   ?= photoidx-macos-builder
WINDOWS_BUILDER ?= photoidx-windows-builder

# Versão do build: quem manda é o GitHub (última Release; na CI, a tag) — ver
# version() em packaging/build.py. Local sai 1.2.3-dev+<commit>.
VERSION := $(shell $(PYTHON) packaging/build.py version 2>/dev/null || echo 0.0.0-dev)

# Build nativo (`make build`/`configure`/`run`): precisa de um Qt 6.8 no host
# (QT_PREFIX=<.../6.8.3/macos> ou qmake/qtpaths no PATH). Sem ele, os alvos
# de desenvolvimento rodam no builder Linux.
QT_PREFIX    ?=
HOST_QT      := $(or $(QT_PREFIX),$(shell command -v qtpaths6 2>/dev/null || command -v qmake6 2>/dev/null))
HOST_TARGET  := $(shell $(PYTHON) -c "import sys; sys.path.insert(0, 'packaging'); import build; print(build.host_target())")
NATIVE_BUILD := build/$(HOST_TARGET)
NATIVE_DEPS  := .cache/deps/$(HOST_TARGET)

# As PHOTOIDX_* (versão/canal/commit, a CI define) passam para dentro.
DOCKER_ENV  = -e PHOTOIDX_VERSION -e PHOTOIDX_CHANNEL -e PHOTOIDX_GIT_HASH -e GITHUB_TOKEN \
	-e BUILD_TYPE=$(BUILD_TYPE) -e CMAKE_ARGS -e SKIP_TESTS
BUILDER_RUN = $(DOCKER) run --rm $(DOCKER_ENV) -v "$(CURDIR)":/src -v $(CACHE_VOLUME):/cache

# Os builders rodam como root: num host Linux (a CI), dist/ e .cache/ ficariam
# do root e o passo seguinte (make checksums, fora do container) não grava
# nada. Ao fim de cada builder, a posse volta para quem chamou o make. No Mac
# o Docker Desktop já mapeia o dono; o chown é inofensivo.
HOST_OWNER := $(shell id -u):$(shell id -g)
own_outputs = $(DOCKER) run --rm -v "$(CURDIR)":/src --entrypoint sh $(1) -c \
	'for d in /src/dist /src/.cache; do [ -e $$d ] && chown -R $(HOST_OWNER) $$d; done; true'

# Linux: imagem e plataforma Docker de cada arquitetura.
linux_image    = $(if $(filter arm64,$(1)),$(LINUX_BUILDER)-arm64,$(LINUX_BUILDER))
linux_platform = $(if $(filter arm64,$(1)),linux/arm64,linux/amd64)
LINUX_RUN      = $(BUILDER_RUN) --platform $(call linux_platform,$(HOST_ARCH)) $(call linux_image,$(HOST_ARCH))

.PHONY: default local package help version version-refresh model art art-preview site checksums \
        deps configure build run test shell translations \
        linux-builder linux-build linux linux-all \
        macos-sdk macos-builder dmg dmg-all dmg-art macos-shell \
        windows-builder windows msi msi-stage windows-native windows-shell \
        dist package clean clean-all

default: build

help:			## Mostra esta ajuda
	@echo "PhotoIDX $(VERSION)"
	@echo
	@grep -E '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | sed -e 's/:[^#]*## */|/' \
		| awk -F'|' '{ printf "  make %-16s %s\n", $$1, $$2 }'
	@echo
	@echo "Variáveis: ARCHS=\"x86_64 arm64\"  SKIP_BUILDER=1  BUILD_TYPE=Release  ARGS=\"...\""
	@echo "           TARGET=windows-arm64  QT_PREFIX=<Qt do host>  CACHE_VOLUME=$(CACHE_VOLUME)"

# --------------------------------------------------------------------------
# Comum
# --------------------------------------------------------------------------

version:		## Versão do build (última Release do GitHub, cache de 24 h; regra 17)
	@$(PYTHON) packaging/build.py version

version-refresh:	## Consulta a última Release no GitHub agora (ignora o cache)
	@$(PYTHON) packaging/build.py version --refresh

model:			## Baixa os modelos de IA (CLIP e buffalo_sc) em .cache/models/
	$(PYTHON) packaging/build.py model

art: linux-builder	## Ícones do app (.icns/.ico/.png) desenhados do logo, em .cache/art
	$(BUILDER_RUN) --entrypoint python3 $(LINUX_BUILDER) packaging/build.py art

art-preview: linux-builder	## Prévia da arte dos instaladores (ícones, fundo do DMG) em ART_OUT
	$(BUILDER_RUN) --entrypoint python3 $(LINUX_BUILDER) packaging/macos/dmg.py --preview $(ART_OUT)

site:			## Hotsite (docs/site) montado em SITE_OUT, como o GitHub Pages publica
	$(PYTHON) packaging/site.py --out $(SITE_OUT)

checksums:		## dist/*.sha256 de cada instalador
	$(PYTHON) packaging/build.py checksums $(wildcard dist/*.tar.gz dist/*.dmg dist/*.msi)

# --------------------------------------------------------------------------
# Desenvolvimento (build nativo, com Qt no host; senão no builder Linux)
# --------------------------------------------------------------------------

deps:			## OpenCV estático do host em .cache/deps/<alvo>/opencv
	bash packaging/deps/opencv.sh "$(CURDIR)/$(NATIVE_DEPS)/opencv"

ifneq ($(HOST_QT),)
QT_CMAKE_PREFIX := $(if $(QT_PREFIX),$(QT_PREFIX),$(shell $(HOST_QT) -query QT_INSTALL_PREFIX 2>/dev/null))

configure: deps		## Configura o build nativo em build/<alvo do host>
	cmake -S . -B $(NATIVE_BUILD) -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_PREFIX_PATH="$(QT_CMAKE_PREFIX);$(CURDIR)/$(NATIVE_DEPS)/opencv" $(CMAKE_ARGS)

build: configure	## [padrão] Compila (nativo; sem Qt no host, no builder Linux)
	cmake --build $(NATIVE_BUILD)

test: build		## Testes (ctest; sem Qt no host, no builder Linux)
	ctest --test-dir $(NATIVE_BUILD) --output-on-failure

run: build		## Abre o app (ARGS="--screenshot /tmp/x.png" ...)
	@if [ -d "$(NATIVE_BUILD)/src/app/PhotoIDX.app" ]; then \
		"$(NATIVE_BUILD)/src/app/PhotoIDX.app/Contents/MacOS/PhotoIDX" $(ARGS); \
	else "$(NATIVE_BUILD)/src/app/PhotoIDX" $(ARGS); fi
else
configure: linux-builder
	$(LINUX_RUN) configure

build: linux-build

test: linux-builder
	$(LINUX_RUN) test

run: linux-build
	@echo "Sem Qt no host: o app roda no builder Linux, sem tela (QT_QPA_PLATFORM=offscreen)."
	$(BUILDER_RUN) --entrypoint sh $(call linux_image,$(HOST_ARCH)) -c \
		'exec /cache/build-linux-$(subst x86_64,x64,$(HOST_ARCH))/src/app/PhotoIDX $(ARGS)'
endif

shell: linux-builder	## Shell no builder Linux (depuração)
	$(BUILDER_RUN) -it --entrypoint bash $(call linux_image,$(HOST_ARCH))

translations: linux-builder	## Atualiza os .ts de i18n/ (lupdate, alvo update_translations do Qt)
	$(BUILDER_RUN) --entrypoint sh $(call linux_image,$(HOST_ARCH)) -c \
		'sh packaging/linux/build.sh configure && cmake --build /cache/build-linux-$(subst x86_64,x64,$(HOST_ARCH)) --target update_translations'

# --------------------------------------------------------------------------
# Linux (.tar.gz)
# --------------------------------------------------------------------------

linux-builder:		## Cria/atualiza a imagem do builder Linux de ARCHS
	@if [ -n "$(SKIP_BUILDER)" ]; then echo "SKIP_BUILDER: usando as imagens existentes"; \
	else for a in $(sort $(ARCHS) $(HOST_ARCH)); do \
		$(DOCKER) build --platform $$( [ $$a = arm64 ] && echo linux/arm64 || echo linux/amd64 ) \
			-t $$( [ $$a = arm64 ] && echo $(LINUX_BUILDER)-arm64 || echo $(LINUX_BUILDER) ) \
			--build-context deps=packaging/deps -f packaging/linux/Dockerfile packaging || exit 1; \
	done; fi

linux-build: linux-builder	## Só compila (sem testes) no builder Linux
	$(BUILDER_RUN) -e SKIP_TESTS=1 --platform $(call linux_platform,$(HOST_ARCH)) $(call linux_image,$(HOST_ARCH)) test

linux: linux-builder	## .tar.gz do Linux de ARCHS (padrão: esta máquina) em dist/
	@for a in $(ARCHS); do \
		$(BUILDER_RUN) --platform $$( [ $$a = arm64 ] && echo linux/arm64 || echo linux/amd64 ) \
			$$( [ $$a = arm64 ] && echo $(LINUX_BUILDER)-arm64 || echo $(LINUX_BUILDER) ) package || exit 1; \
		$(call own_outputs,--platform $$( [ $$a = arm64 ] && echo linux/arm64 || echo linux/amd64 ) $$( [ $$a = arm64 ] && echo $(LINUX_BUILDER)-arm64 || echo $(LINUX_BUILDER) )) || exit 1; \
	done

linux-all:		## .tar.gz x86_64 e arm64 (arm64 emulado num host x86_64: lento)
	$(MAKE) linux ARCHS="x86_64 arm64"

# --------------------------------------------------------------------------
# macOS (.dmg) — cross-compile no Linux com o SDK deste Mac
# --------------------------------------------------------------------------

$(SDK_DIR)/MacOSX.sdk.tar.gz:
	sh packaging/macos/export-sdk.sh $(SDK_DIR)

macos-sdk: $(SDK_DIR)/MacOSX.sdk.tar.gz	## Exporta o SDK do macOS deste Mac para o builder

macos-builder: $(SDK_DIR)/MacOSX.sdk.tar.gz	## Cria/atualiza a imagem do builder macOS
	@if [ -n "$(SKIP_BUILDER)" ]; then echo "SKIP_BUILDER: usando a imagem $(MACOS_BUILDER) existente"; \
	else $(DOCKER) build -t $(MACOS_BUILDER) --build-context sdk=$(SDK_DIR) --build-context deps=packaging/deps \
		-f packaging/macos/Dockerfile packaging/macos; fi

dmg: macos-builder	## .dmg de ARCHS (padrão: esta máquina) em dist/
	$(BUILDER_RUN) $(MACOS_BUILDER) $(ARCHS)
	$(call own_outputs,$(MACOS_BUILDER))

dmg-all: macos-builder	## .dmg x86_64 e arm64 em dist/
	$(BUILDER_RUN) $(MACOS_BUILDER) x86_64 arm64
	$(call own_outputs,$(MACOS_BUILDER))

dmg-art: macos-builder	## Prévia do fundo e dos ícones do DMG em ART_OUT
	$(BUILDER_RUN) --entrypoint python3 $(MACOS_BUILDER) packaging/macos/dmg.py --preview $(ART_OUT)

macos-shell: macos-builder	## Shell no builder macOS (depuração)
	$(BUILDER_RUN) -it --entrypoint bash $(MACOS_BUILDER)

# --------------------------------------------------------------------------
# Windows (.msi)
# --------------------------------------------------------------------------

windows-builder:	## Cria/atualiza a imagem do builder Windows
	@if [ -n "$(SKIP_BUILDER)" ]; then echo "SKIP_BUILDER: usando a imagem $(WINDOWS_BUILDER) existente"; \
	else $(DOCKER) build --platform linux/amd64 -t $(WINDOWS_BUILDER) --build-context deps=packaging/deps \
		-f packaging/windows/Dockerfile packaging/windows; fi

windows: windows-builder	## .msi do Windows x64 (cross-compile llvm-mingw) em dist/
	$(BUILDER_RUN) --platform linux/amd64 $(WINDOWS_BUILDER)
	$(call own_outputs,--platform linux/amd64 $(WINDOWS_BUILDER))

msi: windows		## Alias de `make windows`

msi-stage: windows-builder	## .msi de uma pasta pronta em .cache/stage/TARGET (windows-arm64)
	$(BUILDER_RUN) --platform linux/amd64 $(WINDOWS_BUILDER) msi $(TARGET)
	$(call own_outputs,--platform linux/amd64 $(WINDOWS_BUILDER))

windows-native:		## Só num Windows (Git Bash + MSVC): compila TARGET em .cache/stage/TARGET
	bash packaging/windows/native.sh $(TARGET)

windows-shell: windows-builder	## Shell no builder Windows (depuração)
	$(BUILDER_RUN) -it --platform linux/amd64 --entrypoint bash $(WINDOWS_BUILDER)

# --------------------------------------------------------------------------
# Tudo
# --------------------------------------------------------------------------

dist:			## Todos os instaladores que saem de Docker (Linux x64/arm64, macOS, Windows x64)
	$(MAKE) linux-all
	$(MAKE) dmg-all
	$(MAKE) windows
	$(MAKE) checksums

local:			## Instalador só do sistema e da arquitetura DESTA máquina, em dist/
	@echo "==> Alvo local: $(HOST_TARGET)"
	@case "$(HOST_TARGET)" in \
		macos-*) $(MAKE) dmg ARCHS=$(HOST_ARCH) ;; \
		linux-*) $(MAKE) linux ARCHS=$(HOST_ARCH) ;; \
		windows-arm64) $(MAKE) windows-native TARGET=windows-arm64 && $(MAKE) msi-stage TARGET=windows-arm64 ;; \
		windows-*) $(MAKE) windows ;; \
		*) echo "Plataforma desconhecida: $(HOST_TARGET)"; exit 1 ;; \
	esac

package: local		## Alias de `make local`

clean:			## Apaga dist/ e o volume com as pastas de build
	rm -rf dist $(ART_OUT) .cache/art .cache/stage
	-$(DOCKER) volume rm $(CACHE_VOLUME)

clean-all: clean	## clean + imagens dos builders e downloads (.cache/, build/)
	-$(DOCKER) image rm $(LINUX_BUILDER) $(LINUX_BUILDER)-arm64 $(MACOS_BUILDER) $(WINDOWS_BUILDER)
	rm -rf .cache build

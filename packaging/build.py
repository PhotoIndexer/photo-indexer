#!/usr/bin/env python3
"""Utilitarios do build do PhotoIDX (C++/Qt): versao, modelo de IA e arte
dos instaladores.

So usa a biblioteca padrao (roda com qualquer python3 >= 3.9). Quem compila e
empacota sao os builders em Docker de ``packaging/<plataforma>/`` (ver o
Makefile da raiz, `make help`); este script so faz o que e comum a todos:

    version [--refresh]          versao do build (Releases do GitHub) -> VERSION
    package-name --target T      nome do pacote: PhotoIDX-1.2.3-<T> /
                                 PhotoIDX-dev-v1.2.3-<T>
    model                        modelos de IA em .cache/models/: CLIP (categorias e
                                 busca) e insightface buffalo_sc (faces)
    art [--out DIR]              icones (.icns/.ico/.png) desenhados do logo
    vcruntime --target T --out DIR
                                 DLLs do runtime do Visual C++ (msvcp140...) que
                                 o ONNX Runtime pede, ao lado do PhotoIDX.exe
    build-info --target T --out F
                                 build-info.json que vai dentro do pacote
    checksums FILE...            FILE.sha256 ao lado de cada arquivo

Alvos: macos-arm64, macos-x64, windows-x64, windows-arm64, linux-x64, linux-arm64.
"""
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CACHE = ROOT / '.cache'
MODELS = CACHE / 'models'

APP_NAME = 'PhotoIDX'

# Alvos do build. ``platform`` e a tag das wheels do runtime do Visual C++
# (vcruntime, so Windows).
TARGETS = {
    'macos-arm64': {'platform': 'macosx_13_0_arm64', 'os': 'macos'},
    'macos-x64': {'platform': 'macosx_13_0_x86_64', 'os': 'macos'},
    'linux-x64': {'platform': 'manylinux_2_39_x86_64', 'os': 'linux'},
    'linux-arm64': {'platform': 'manylinux_2_39_aarch64', 'os': 'linux'},
    'windows-x64': {'platform': 'win_amd64', 'os': 'windows'},
    'windows-arm64': {'platform': 'win_arm64', 'os': 'windows'},
}

# Modelo das categorias e da busca por IA: CLIP ViT-B/32 (OpenAI, MIT) em
# ONNX quantizado (int8), ~150 MB: o encoder de imagem, o de texto e o
# vocabulario BPE do tokenizer. Revisao e hashes fixos: o pacote sempre leva o
# mesmo modelo (regra 16).
CLIP_MODEL = {
    'dir': 'clip-vit-base-patch32',
    'repo': 'Xenova/clip-vit-base-patch32',
    'revision': 'd15189d7028b43f1d3e65039190477f6af591c2a',
    'files': {
        'onnx/vision_model_quantized.onnx': '583fd1110a514667812fee7d684952aaf82a99b959760c8d7dca7e0ab9839299',
        'onnx/text_model_quantized.onnx': '73baab855d406190da9faa498cfedf65f15cf309f4cc7385b7b032e6d08e5c3a',
        'vocab.json': '5047b556ce86ccaf6aa22b3ffccfc52d391ea4accdab9c2f2407da5b742d4363',
        'merges.txt': '9fd691f7c8039210e0fced15865466c65820d09b63988b0174bfe25de299051a',
    },
}

# Reconhecimento facial: insightface buffalo_sc (SCRFD-500m + ArcFace
# MobileFaceNet), o MESMO do ../photoe. ATENÇÃO: os modelos pré-treinados do
# insightface são liberados só para pesquisa não comercial.
FACE_MODEL = {
    'dir': 'buffalo_sc',
    'url': 'https://github.com/deepinsight/insightface/releases/download/v0.7/buffalo_sc.zip',
    'sha256': '57d31b56b6ffa911c8a73cfc1707c73cab76efe7f13b675a05223bf42de47c72',
    'files': {
        'det_500m.onnx': '5e4447f50245bbd7966bd6c0fa52938c61474a04ec7def48753668a9d8b4ea3a',
        'w600k_mbf.onnx': '9cc6e4a75f0e2bf0b1aed94578f144d15175f357bdc05e815e5c4a02b319eb4f',
    },
}


def log(msg):
    print(f'==> {msg}', flush=True)


def run(cmd, **kw):
    print('    $ ' + ' '.join(str(c) for c in cmd), flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kw)


# ---------------------------------------------------------------------------
# Versao: quem manda e o GitHub (regra 17)
# ---------------------------------------------------------------------------
# A versao NAO e versionada no repositorio. Ela sai das Releases do GitHub:
#
#   canal    de onde vem o numero                  exibido / nome do pacote
#   release  a tag da Release (CI)                 1.2.3 / PhotoIDX-1.2.3-<alvo>
#   dev      a ultima Release (build local)        1.2.3-dev+<commit>  / PhotoIDX-dev-v1.2.3-<alvo>
#
# PHOTOIDX_VERSION (X.Y.Z) e PHOTOIDX_CHANNEL sobrescrevem (a CI usa).
# Sem Release publicada ainda, a versao e 0.0.0. O .app e o .msi recebem o
# numero puro X.Y.Z (o MSI so aceita numeros); o sufixo e so para exibicao.
# O arquivo VERSION da raiz e GERADO aqui (ignorado pelo git): e de onde o
# CMakeLists.txt le a versao (rotulo para a interface, numero para o projeto).

GITHUB_REPO = 'helviojunior/photo-indexer'
CHANNELS = ('release', 'dev')
LATEST_RELEASE_CACHE = CACHE / 'latest-release'
# Versao encontrada vale 24 h. "Nenhuma Release" (0.0.0) so 1 h: e o estado
# que muda quando se publica a primeira, e 24 h presas em 0.0.0 atrapalham.
LATEST_RELEASE_TTL = 24 * 3600
NO_RELEASE_TTL = 3600
_VERSION_RE = re.compile(r'^v?(\d{1,3})\.(\d{1,3})\.(\d{1,3})$')


def _parse_version(raw: str):
    """``v1.2.3``/``1.2.3`` -> ``1.2.3``, ou None fora do formato do MSI."""
    m = _VERSION_RE.match((raw or '').strip())
    if not m or int(m.group(1)) > 255:
        return None
    return '.'.join(str(int(g)) for g in m.groups())


def latest_release(refresh: bool = False) -> str:
    """Numero da ultima Release publicada no GitHub.

    A consulta a API fica em cache (``.cache/latest-release``) por 24 h — 1 h
    se ainda nao ha Release. ``refresh`` (ou PHOTOIDX_REFRESH_VERSION=1)
    ignora o cache. Sem rede, usa o ultimo valor em cache; sem Release, 0.0.0.
    """
    refresh = refresh or os.environ.get('PHOTOIDX_REFRESH_VERSION') == '1'
    cached = None
    if LATEST_RELEASE_CACHE.is_file():
        cached = _parse_version(LATEST_RELEASE_CACHE.read_text())
        ttl = NO_RELEASE_TTL if cached == '0.0.0' else LATEST_RELEASE_TTL
        age = time.time() - LATEST_RELEASE_CACHE.stat().st_mtime
        if cached and not refresh and age < ttl:
            return cached
    headers = {'Accept': 'application/vnd.github+json'}
    if os.environ.get('GITHUB_TOKEN'):
        headers['Authorization'] = f'Bearer {os.environ["GITHUB_TOKEN"]}'
    req = urllib.request.Request(
        f'https://api.github.com/repos/{GITHUB_REPO}/releases/latest', headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            found = _parse_version(json.load(r).get('tag_name', '')) or '0.0.0'
    except urllib.error.HTTPError as exc:
        if exc.code != 404:            # 404 = nenhuma Release publicada ainda
            return cached or '0.0.0'
        found = '0.0.0'
    except OSError:
        return cached or '0.0.0'
    LATEST_RELEASE_CACHE.parent.mkdir(parents=True, exist_ok=True)
    LATEST_RELEASE_CACHE.write_text(found)
    return found


def version() -> str:
    """Numero X.Y.Z do build (ver a tabela acima)."""
    forced = os.environ.get('PHOTOIDX_VERSION', '').strip()
    if forced:
        parsed = _parse_version(forced)
        if not parsed:
            sys.exit(f'PHOTOIDX_VERSION={forced!r} is not X.Y.Z (X <= 255, Y and Z <= 999).')
        return parsed
    return latest_release()


def channel() -> str:
    value = os.environ.get('PHOTOIDX_CHANNEL', '').strip() or 'dev'
    if value not in CHANNELS:
        sys.exit(f'PHOTOIDX_CHANNEL={value!r}: choose from {", ".join(CHANNELS)}.')
    return value


def git_hash() -> str:
    """Commit atual (7 caracteres), lido de .git sem precisar do git — os
    containers dos builders nao o tem. PHOTOIDX_GIT_HASH sobrescreve."""
    forced = os.environ.get('PHOTOIDX_GIT_HASH', '').strip()
    if forced:
        return forced[:7]
    git = ROOT / '.git'
    try:
        head = (git / 'HEAD').read_text().strip()
        if not head.startswith('ref: '):
            return head[:7]
        ref = head[5:]
        if (git / ref).is_file():
            return (git / ref).read_text().strip()[:7]
        for line in (git / 'packed-refs').read_text().splitlines():
            if line.endswith(' ' + ref):
                return line.split()[0][:7]
    except OSError:
        pass
    return ''


def version_label() -> str:
    """O que o app exibe: 1.2.3 na Release; 1.2.3-dev+abc1234 fora dela."""
    ver, chan = version(), channel()
    if chan == 'release':
        return ver
    commit = git_hash()
    return f'{ver}-{chan}' + (f'+{commit}' if commit else '')


def package_prefix() -> str:
    """Inicio do nome dos pacotes: PhotoIDX-1.2.3 / PhotoIDX-dev-v1.2.3."""
    ver, chan = version(), channel()
    return f'{APP_NAME}-{ver}' if chan == 'release' else f'{APP_NAME}-{chan}-v{ver}'


def msi_version(ver: str) -> str:
    """X.Y.Z do projeto -> ProductVersion do MSI.

    O MSI aceita no maximo 255.255.65535, e a versao do projeto (a tag da
    Release, regra 17) vai ate 255.999.999. Os dois numeros de baixo viram um
    contador so (Y*1000 + Z, ate 999999) repartido em 16 bits: a ordem se
    mantem, que e o que o MajorUpgrade compara.
    """
    major, minor, patch = (int(p) for p in ver.split('.'))
    counter = minor * 1000 + patch
    return f'{major}.{counter // 65536}.{counter % 65536}'


def write_version_file() -> str:
    """Grava o VERSION da raiz (gerado, fora do git) e devolve o rotulo.

    So reescreve quando muda: o CMake reconfigura quando o VERSION muda de
    data, e regravar o mesmo texto recompilaria tudo a cada `make`.
    """
    label = version_label()
    path = ROOT / 'VERSION'
    if not path.is_file() or path.read_text().strip() != label:
        path.write_text(label + '\n')
    return label


def host_target() -> str:
    system = platform.system()
    machine = platform.machine().lower()
    arch = 'arm64' if machine in ('arm64', 'aarch64') else 'x64'
    name = {'Darwin': 'macos', 'Linux': 'linux', 'Windows': 'windows'}.get(system)
    if not name:
        sys.exit(f'Unsupported host system: {system}')
    return f'{name}-{arch}'


def check_target(name: str) -> str:
    if name == 'host':
        return host_target()
    if name not in TARGETS:
        sys.exit(f'Unknown target: {name} (choose from {", ".join(TARGETS)}, host)')
    return name


# ---------------------------------------------------------------------------
# Downloads
# ---------------------------------------------------------------------------

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path, expected_sha256: str = None):
    """Baixa para ``dest`` (uma vez so; o cache fica em .cache/)."""
    if dest.is_file() and (not expected_sha256 or sha256(dest) == expected_sha256):
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + '.part')
    log(f'Downloading {url}')
    for attempt in range(3):
        try:
            with urllib.request.urlopen(url, timeout=60) as r, open(tmp, 'wb') as f:
                shutil.copyfileobj(r, f, 1 << 20)
            break
        except OSError:
            if attempt == 2:
                raise
            time.sleep(2 + attempt * 3)
    if expected_sha256 and sha256(tmp) != expected_sha256:
        tmp.unlink()
        sys.exit(f'Checksum mismatch for {url}')
    os.replace(tmp, dest)
    return dest


# ---------------------------------------------------------------------------
# Modelo
# ---------------------------------------------------------------------------

def cmd_model():
    dest = MODELS / CLIP_MODEL['dir']
    for name, digest in CLIP_MODEL['files'].items():
        download(
            f'https://huggingface.co/{CLIP_MODEL["repo"]}/resolve/{CLIP_MODEL["revision"]}/{name}',
            dest / Path(name).name, digest)
    log(f'CLIP model ready in {dest.relative_to(ROOT)}')

    import zipfile
    faces = MODELS / FACE_MODEL['dir']
    if not all((faces / n).is_file() and sha256(faces / n) == h for n, h in FACE_MODEL['files'].items()):
        archive = download(FACE_MODEL['url'], CACHE / 'downloads' / 'buffalo_sc.zip', FACE_MODEL['sha256'])
        faces.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as zf:
            for member in zf.namelist():
                name = Path(member).name
                if name in FACE_MODEL['files']:
                    (faces / name).write_bytes(zf.read(member))
        for name, digest in FACE_MODEL['files'].items():
            if sha256(faces / name) != digest:
                sys.exit(f'Checksum mismatch for {name} in {FACE_MODEL["url"]}')
    log(f'Face model ready in {faces.relative_to(ROOT)}')
    return dest


# ---------------------------------------------------------------------------
# Runtime do Visual C++ (Windows)
# ---------------------------------------------------------------------------
# O onnxruntime.dll oficial e compilado com MSVC e importa msvcp140.dll,
# msvcp140_1.dll, vcruntime140*.dll — que nem todo Windows tem. Os arquivos
# redistribuiveis da Microsoft vem do pacote ``msvc-runtime`` do PyPI (wheel
# por arquitetura, versao fixa), baixado e conferido pelo sha256 do PyPI.
VCRUNTIME_VERSION = '14.44.35112'
# Tag da wheel do pacote msvc-runtime (as DLLs sao as mesmas para todo Python).
PYTHON_ABI = 'cp312'


def cmd_vcruntime(target: str, out: Path):
    import zipfile
    if TARGETS[target]['os'] != 'windows':
        sys.exit(f'vcruntime: {target} is not a Windows target')
    platform_tag = TARGETS[target]['platform']
    with urllib.request.urlopen(f'https://pypi.org/pypi/msvc-runtime/{VCRUNTIME_VERSION}/json',
                                timeout=30) as r:
        files = json.load(r)['urls']
    wheel = next(f for f in files if f['filename'].endswith(f'-{PYTHON_ABI}-{PYTHON_ABI}-{platform_tag}.whl'))
    path = download(wheel['url'], CACHE / 'vcruntime' / wheel['filename'], wheel['digests']['sha256'])
    out.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path) as zf:
        for name in zf.namelist():
            if name.lower().endswith('.dll'):
                (out / Path(name).name).write_bytes(zf.read(name))
                log(f'VC++ runtime {Path(name).name}')


# ---------------------------------------------------------------------------
# Arte, build-info, checksums
# ---------------------------------------------------------------------------

def cmd_art(out: Path):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    try:
        import art  # noqa: E402  (packaging/art.py, precisa do Pillow)
    except ImportError as exc:
        sys.exit(f'packaging/art.py needs Pillow ({exc}); run it inside a builder (make art).')
    for path in art.write_icons(out):
        log(f'Icon {path}')


def cmd_build_info(target: str, out: Path):
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps({
        'version': version(), 'label': version_label(), 'channel': channel(),
        'commit': git_hash(), 'target': target,
        'model': f'{CLIP_MODEL["repo"]}@{CLIP_MODEL["revision"]}',
        'face_model': FACE_MODEL['url'],
        'built_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
    }, indent=2) + '\n')


def cmd_checksums(files):
    for name in files:
        path = Path(name)
        line = f'{sha256(path)}  {path.name}\n'
        Path(f'{path}.sha256').write_text(line)
        print(line, end='')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('version', help='print (and write to VERSION) the version label')
    p.add_argument('--refresh', action='store_true',
                   help='ask the GitHub API now, ignoring the 24 h cache')
    p.add_argument('--number', action='store_true', help='print only X.Y.Z')
    p.add_argument('--msi', action='store_true', help='print the MSI ProductVersion')
    p = sub.add_parser('package-name')
    p.add_argument('--target', required=True)
    sub.add_parser('model')
    p = sub.add_parser('art')
    p.add_argument('--out', type=Path, default=CACHE / 'art')
    p = sub.add_parser('vcruntime')
    p.add_argument('--target', required=True)
    p.add_argument('--out', required=True, type=Path)
    p = sub.add_parser('build-info')
    p.add_argument('--target', required=True)
    p.add_argument('--out', required=True, type=Path)
    p = sub.add_parser('checksums')
    p.add_argument('files', nargs='+')
    args = parser.parse_args()

    if args.command == 'version':
        if args.refresh:
            latest_release(refresh=True)
        label = write_version_file()
        print(msi_version(version()) if args.msi else version() if args.number else label)
    elif args.command == 'package-name':
        print(f'{package_prefix()}-{check_target(args.target)}')
    elif args.command == 'model':
        cmd_model()
    elif args.command == 'art':
        cmd_art(args.out)
    elif args.command == 'vcruntime':
        cmd_vcruntime(check_target(args.target), args.out)
    elif args.command == 'build-info':
        cmd_build_info(check_target(args.target), args.out)
    elif args.command == 'checksums':
        cmd_checksums(args.files)


if __name__ == '__main__':
    main()

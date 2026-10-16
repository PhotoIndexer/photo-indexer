"""Completa o ``PhotoIDX.app`` instalado pelo CMake, no Linux.

Faz o que o ``macdeployqt`` faria num Mac (como o bundle.py do clivcut)::

    PhotoIDX.app/Contents/
        MacOS/PhotoIDX
        Frameworks/      Qt*.framework e o ONNX Runtime (.dylib)
        PlugIns/         plugins do Qt (cocoa, imagens, SQLite...)
        Resources/qml/   módulos QML que o app importa (qmlimportscanner)
        Resources/qt.conf, PhotoIDX.icns
        Resources/models/   (CLIP e buffalo_sc, instalados pelo CMake)

Tudo que é universal (o Qt oficial) é afinado para a arquitetura do pacote;
cada binário do app fica com um único rpath, ``@executable_path/../Frameworks``,
e o pacote é assinado ad hoc com o rcodesign (o arm64 não roda sem
assinatura).

Uso (pelo build.sh): bundle.py --arch x86_64 --app <PhotoIDX.app>
                     --qt <qt macos> --qt-host <qt linux> --lib-dir <dir>...
"""
import argparse
import json
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import art  # noqa: E402  (packaging/art.py)

MACHO_64 = b'\xcf\xfa\xed\xfe'
MACHO_FAT = b'\xca\xfe\xba\xbe'
LC_RPATH = 0x8000001C
LC_DYLIB_DEPS = {0xC, 0x80000018, 0x8000001F, 0x20, 0x80000023}  # load, weak, reexport, lazy, upward
LC_VERSION_MIN_MACOSX = 0x24
LC_BUILD_VERSION = 0x32
RPATH = '@executable_path/../Frameworks'
APP_NAME = 'PhotoIDX'
BUNDLE_ID = 'br.com.photoidx.desktop'      # nunca trocar (regra 18)

# Plugins do Qt que o app usa: janela (cocoa), formatos de imagem e ícones
# SVG, o driver SQLite (catálogo do evento) e o estilo nativo.
QT_PLUGINS = {
    'platforms': ['libqcocoa.dylib'],
    'imageformats': None,      # None = a pasta inteira
    'iconengines': None,
    'sqldrivers': ['libqsqlite.dylib'],
    'styles': None,
}
# Importados em tempo de execução, invisíveis para o scanner.
EXTRA_QML_IMPORTS = ['QtQuick.Controls', 'QtQuick.Controls.Basic', 'QtQuick.Dialogs',
                     'QtQuick.Layouts', 'QtQuick.Window']
# Traduções do próprio Qt (diálogos, menus padrão) nos idiomas do app.
QT_TRANSLATIONS = ('qt_pt_BR.qm', 'qtbase_pt_BR.qm', 'qtdeclarative_pt_BR.qm')


def is_macho(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    with open(path, 'rb') as f:
        return f.read(4) in (MACHO_64, MACHO_FAT)


def is_fat(path: Path) -> bool:
    with open(path, 'rb') as f:
        return f.read(4) == MACHO_FAT


def _commands(data: bytes):
    ncmds, = struct.unpack_from('<I', data, 16)
    offset = 32
    for _ in range(ncmds):
        cmd, size = struct.unpack_from('<II', data, offset)
        yield cmd, offset, size
        offset += size


def load_commands(path: Path):
    """(dylibs carregadas, rpaths) de um Mach-O de 64 bits já afinado."""
    data = path.read_bytes()
    if data[:4] != MACHO_64:
        raise ValueError(f'{path}: não é um Mach-O de 64 bits afinado')
    deps, rpaths = [], []
    for cmd, offset, size in _commands(data):
        if cmd in LC_DYLIB_DEPS or cmd == LC_RPATH:
            name_at, = struct.unpack_from('<I', data, offset + 8)
            raw = data[offset + name_at:offset + size].split(b'\0', 1)[0].decode()
            (rpaths if cmd == LC_RPATH else deps).append(raw)
    return deps, rpaths


def min_macos(path: Path):
    """Versão mínima de macOS gravada no Mach-O (LC_BUILD_VERSION), ou None."""
    data = path.read_bytes()
    if data[:4] != MACHO_64:
        return None
    for cmd, offset, _size in _commands(data):
        if cmd == LC_BUILD_VERSION:
            value, = struct.unpack_from('<I', data, offset + 12)
        elif cmd == LC_VERSION_MIN_MACOSX:
            value, = struct.unpack_from('<I', data, offset + 8)
        else:
            continue
        return (value >> 16, (value >> 8) & 0xFF, value & 0xFF)
    return None


def run(*args):
    subprocess.run([str(a) for a in args], check=True)


class Bundler:
    def __init__(self, args):
        self.arch = args.arch
        self.qt = Path(args.qt)
        self.qt_host = Path(args.qt_host)
        self.qml_roots = [Path(p) for p in args.qml_root if Path(p).is_dir()]
        self.version = args.version
        self.app = Path(args.app)
        self.contents = self.app / 'Contents'
        self.frameworks = self.contents / 'Frameworks'
        self.resources = self.contents / 'Resources'
        # Onde procurar as @rpath/*.dylib: as já instaladas, as pedidas, o Qt.
        self.lib_dirs = [self.frameworks, *[Path(d) for d in args.lib_dir], self.qt / 'lib']

    def make(self):
        if not (self.contents / 'MacOS' / APP_NAME).is_file():
            raise SystemExit(f'{self.app}: sem Contents/MacOS/{APP_NAME} (o cmake --install rodou?)')
        self.frameworks.mkdir(exist_ok=True)
        self.copy_plugins()
        self.copy_qml()
        self.copy_translations()
        (self.resources / 'qt.conf').write_text(
            '[Paths]\nPlugins = PlugIns\nImports = Resources/qml\nQmlImports = Resources/qml\n'
            'Translations = Resources/translations\n')
        if not (self.resources / f'{APP_NAME}.icns').is_file():
            art.write_icns(self.resources / f'{APP_NAME}.icns')
        self.resolve_dependencies()
        self.patch_info_plist()
        self.sign()

    # -- conteúdo do Qt ---------------------------------------------------

    def copy_plugins(self):
        for group, names in QT_PLUGINS.items():
            src = self.qt / 'plugins' / group
            if not src.is_dir():
                continue
            dst = self.contents / 'PlugIns' / group
            dst.mkdir(parents=True, exist_ok=True)
            for lib in sorted(src.glob('*.dylib')):
                if (names is None or lib.name in names) and not lib.stem.endswith('_debug'):
                    shutil.copy2(lib, dst)

    def copy_qml(self):
        """Os módulos QML que o app importa, sem os submódulos não usados."""
        with tempfile.TemporaryDirectory() as extra:
            Path(extra, 'Extra.qml').write_text(
                ''.join(f'import {m}\n' for m in EXTRA_QML_IMPORTS) + 'Item {}\n')
            cmd = [self.qt_host / 'libexec/qmlimportscanner']
            for root in [*self.qml_roots, Path(extra)]:
                cmd += ['-rootPath', root]
            cmd += ['-importPath', self.qt / 'qml']
            out = subprocess.run([str(c) for c in cmd], check=True, capture_output=True, text=True).stdout
        qml_root = (self.qt / 'qml').resolve()
        dst_root = self.resources / 'qml'
        for entry in json.loads(out):
            path = Path(entry.get('path', ''))
            if entry.get('type') != 'module' or not path.is_dir() or qml_root not in path.resolve().parents:
                continue
            self._copy_module(path, dst_root / path.resolve().relative_to(qml_root))

    def _copy_module(self, src: Path, dst: Path):
        dst.mkdir(parents=True, exist_ok=True)
        for item in src.iterdir():
            if item.is_dir():
                # Subpasta com qmldir é outro módulo: entra só se o scanner pedir.
                if not (item / 'qmldir').exists() and not item.name.endswith('.dSYM'):
                    shutil.copytree(item, dst / item.name, symlinks=True, dirs_exist_ok=True)
            elif not item.stem.endswith('_debug'):
                shutil.copy2(item, dst / item.name)

    def copy_translations(self):
        dst = self.resources / 'translations'
        for name in QT_TRANSLATIONS:
            src = self.qt / 'translations' / name
            if src.is_file():
                dst.mkdir(exist_ok=True)
                shutil.copy2(src, dst)

    def patch_info_plist(self):
        plist = self.contents / 'Info.plist'
        info = plistlib.loads(plist.read_bytes())
        # O piso real é o do binário mais exigente do pacote (Qt, ONNX Runtime, app).
        floor = max((v for p in self.machos(own=True) if (v := min_macos(p))), default=(13, 0, 0))
        info.update({
            'CFBundleName': APP_NAME,
            'CFBundleDisplayName': APP_NAME,
            'CFBundleIdentifier': BUNDLE_ID,
            'CFBundleExecutable': APP_NAME,
            'CFBundleIconFile': APP_NAME,
            'CFBundlePackageType': 'APPL',
            'CFBundleDevelopmentRegion': 'en',
            'CFBundleLocalizations': ['en', 'pt-BR'],
            'LSMinimumSystemVersion': f'{floor[0]}.{floor[1]}',
            'LSArchitecturePriority': [self.arch],
            'LSApplicationCategoryType': 'public.app-category.photography',
            'NSHighResolutionCapable': True,
            'NSSupportsAutomaticGraphicsSwitching': True,
            'NSRequiresAquaSystemAppearance': False,
            'NSHumanReadableCopyright': 'BSD 2-Clause — Helvio Junior / PhotoE',
        })
        if self.arch == 'arm64':
            info['LSRequiresNativeExecution'] = True
        if self.version:
            info['CFBundleShortVersionString'] = self.version
            info['CFBundleVersion'] = self.version
        plist.write_bytes(plistlib.dumps(info))
        (self.contents / 'PkgInfo').write_text('APPL????')

    # -- dependências e rpaths --------------------------------------------

    def machos(self, own=False):
        return [p for p in self.app.rglob('*') if is_macho(p)]

    def resolve_dependencies(self):
        done = set()
        while True:
            pending = [p for p in self.machos(own=True) if p not in done]
            if not pending:
                break
            for path in pending:
                done.add(path)
                if is_fat(path):
                    run('llvm-lipo', path, '-thin', self.arch, '-output', path)
                deps, rpaths = load_commands(path)
                for dep in deps:
                    self.bring(dep, path)
                if rpaths != [RPATH]:
                    if rpaths:
                        run('llvm-install-name-tool', '-delete_all_rpaths', path)
                    run('llvm-install-name-tool', '-add_rpath', RPATH, path)

    def bring(self, dep: str, user: Path):
        """Copia para Contents/Frameworks o alvo de uma dependência @rpath."""
        if dep.startswith(('/usr/lib/', '/System/', '@loader_path/', '@executable_path/')):
            return
        if not dep.startswith('@rpath/'):
            raise SystemExit(f'{user}: dependência fora do pacote: {dep}')
        first = dep.split('/')[1]
        dst = self.frameworks / first
        if dst.exists():
            return
        if first.endswith('.framework'):
            shutil.copytree(self.qt / 'lib' / first, dst, symlinks=True,
                            ignore=shutil.ignore_patterns('Headers', '*.prl', '*_debug'))
            return
        for d in self.lib_dirs:
            if (d / first).exists():
                shutil.copy2((d / first).resolve(), dst)
                return
        raise SystemExit(f'{user}: {dep} não encontrada em {[str(d) for d in self.lib_dirs]}')

    def sign(self):
        """Ad hoc: cada binário (o Python do helper também), depois o pacote."""
        for path in self.machos():
            run('rcodesign', 'sign', path)
        run('rcodesign', 'sign', self.app)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--arch', required=True, choices=['x86_64', 'arm64'])
    ap.add_argument('--app', required=True, help='PhotoIDX.app instalado pelo cmake --install')
    ap.add_argument('--qt', required=True)
    ap.add_argument('--qt-host', required=True)
    ap.add_argument('--lib-dir', action='append', default=[], help='mais pastas de .dylib')
    ap.add_argument('--qml-root', action='append', default=['/src/src/app/qml'])
    ap.add_argument('--version', default='', help='X.Y.Z do Info.plist')
    Bundler(ap.parse_args()).make()


if __name__ == '__main__':
    main()

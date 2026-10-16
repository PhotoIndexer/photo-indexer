"""Completa a pasta do PhotoIDX para Windows instalada pelo CMake, no Linux.

Faz o que o ``windeployqt`` faria no Windows::

    PhotoIDX.exe, onnxruntime.dll       (cmake --install)
    Qt6*.dll, libc++.dll, libunwind.dll    dependências, achadas pelas importações do PE
    plugins/                               plataforma, imagens, SQLite, estilo
    qml/                                   módulos QML que o app importa (qmlimportscanner)
    translations/, qt.conf
    models/                                (cmake --install)

As DLLs são achadas lendo a tabela de importação de cada PE (e as importações
atrasadas) e procuradas nas pastas dadas; o que não está em nenhuma é do
Windows (KERNEL32, api-ms-win-crt-*...).

Uso (pelo build.sh): bundle.py --dir <pasta> --qt <qt windows> --qt-host <qt linux>
                     --lib-dir <dir>...
"""
import argparse
import json
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

APP_NAME = 'PhotoIDX'
QT_PLUGINS = {
    'platforms': ['qwindows.dll'],
    'imageformats': None,           # None = a pasta inteira
    'iconengines': None,
    'sqldrivers': ['qsqlite.dll'],
    'styles': None,
}
EXTRA_QML_IMPORTS = ['QtQuick.Controls', 'QtQuick.Controls.Basic', 'QtQuick.Dialogs',
                     'QtQuick.Layouts', 'QtQuick.Window']
QT_TRANSLATIONS = ('qt_pt_BR.qm', 'qtbase_pt_BR.qm', 'qtdeclarative_pt_BR.qm')


def pe_imports(path: Path):
    """Nomes das DLLs importadas (normais e atrasadas) por um PE32/PE32+."""
    data = path.read_bytes()
    if data[:2] != b'MZ':
        return []
    pe, = struct.unpack_from('<I', data, 0x3C)
    if data[pe:pe + 4] != b'PE\0\0':
        return []
    nsections, = struct.unpack_from('<H', data, pe + 6)
    opt_size, = struct.unpack_from('<H', data, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from('<H', data, opt)
    dirs = opt + (112 if magic == 0x20B else 96)
    sections = []
    for i in range(nsections):
        s = opt + opt_size + i * 40
        vsize, vaddr, rsize, raw = struct.unpack_from('<IIII', data, s + 8)
        sections.append((vaddr, max(vsize, rsize), raw))

    def offset(rva):
        for vaddr, size, raw in sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + raw
        return None

    def cstr(rva):
        off = offset(rva)
        return data[off:data.index(b'\0', off)].decode('ascii', 'replace') if off is not None else None

    names = []
    # (índice do data directory, tamanho do descritor, posição do campo Name)
    for index, size, name_at in ((1, 20, 12), (13, 32, 4)):
        rva, _ = struct.unpack_from('<II', data, dirs + index * 8)
        off = offset(rva) if rva else None
        while off is not None and off + size <= len(data):
            name_rva, = struct.unpack_from('<I', data, off + name_at)
            if not name_rva:
                break
            name = cstr(name_rva)
            if name:
                names.append(name)
            off += size
    return names


def is_debug(path: Path) -> bool:
    """``Food.dll`` com um ``Foo.dll`` ao lado: a variante de depuração do Qt (MSVC)."""
    return (path.suffix.lower() == '.dll' and path.stem.endswith('d')
            and path.with_name(path.stem[:-1] + path.suffix).exists())


class Bundler:
    def __init__(self, args):
        self.root = Path(args.dir)
        self.qt = Path(args.qt)
        self.qt_host = Path(args.qt_host)
        self.qml_roots = [Path(p) for p in args.qml_root if Path(p).is_dir()]
        self.lib_dirs = [self.root, self.qt / 'bin', *[Path(d) for d in args.lib_dir]]

    def make(self):
        if not (self.root / f'{APP_NAME}.exe').is_file():
            raise SystemExit(f'{self.root}: sem {APP_NAME}.exe (o cmake --install rodou?)')
        self.copy_plugins()
        self.copy_qml()
        self.copy_translations()
        (self.root / 'qt.conf').write_text(
            '[Paths]\nPrefix = .\nPlugins = plugins\nQmlImports = qml\nTranslations = translations\n')
        self.resolve_dependencies()

    def copy_plugins(self):
        for group, names in QT_PLUGINS.items():
            src = self.qt / 'plugins' / group
            if not src.is_dir():
                continue
            dst = self.root / 'plugins' / group
            dst.mkdir(parents=True, exist_ok=True)
            for lib in sorted(src.glob('*.dll')):
                if (names is None or lib.name in names) and not is_debug(lib):
                    shutil.copy2(lib, dst)

    def copy_qml(self):
        with tempfile.TemporaryDirectory() as extra:
            Path(extra, 'Extra.qml').write_text(
                ''.join(f'import {m}\n' for m in EXTRA_QML_IMPORTS) + 'Item {}\n')
            cmd = [self.qt_host / 'libexec/qmlimportscanner']
            for root in [*self.qml_roots, Path(extra)]:
                cmd += ['-rootPath', root]
            cmd += ['-importPath', self.qt / 'qml']
            out = subprocess.run([str(c) for c in cmd], check=True, capture_output=True, text=True).stdout
        qml_root = (self.qt / 'qml').resolve()
        for entry in json.loads(out):
            path = Path(entry.get('path', ''))
            if entry.get('type') != 'module' or not path.is_dir() or qml_root not in path.resolve().parents:
                continue
            self._copy_module(path, self.root / 'qml' / path.resolve().relative_to(qml_root))

    def _copy_module(self, src: Path, dst: Path):
        dst.mkdir(parents=True, exist_ok=True)
        for item in src.iterdir():
            if item.is_dir():
                if not (item / 'qmldir').exists():
                    shutil.copytree(item, dst / item.name, dirs_exist_ok=True)
            elif item.suffix.lower() not in ('.pdb', '.a', '.prl') and not is_debug(item):
                shutil.copy2(item, dst / item.name)

    def copy_translations(self):
        dst = self.root / 'translations'
        for name in QT_TRANSLATIONS:
            src = self.qt / 'translations' / name
            if src.is_file():
                dst.mkdir(exist_ok=True)
                shutil.copy2(src, dst)

    def binaries(self):
        return [p for p in self.root.rglob('*')
                if p.suffix.lower() in ('.exe', '.dll') and p.is_file()]

    def resolve_dependencies(self):
        present = {p.name.lower() for p in self.root.glob('*.dll')}
        done = set()
        while True:
            pending = [p for p in self.binaries() if p not in done]
            if not pending:
                break
            for path in pending:
                done.add(path)
                for dll in pe_imports(path):
                    if dll.lower() in present:
                        continue
                    for d in self.lib_dirs:
                        src = d / dll
                        if not src.is_file():       # o PE guarda o nome em qualquer caixa
                            src = next((f for f in d.glob('*.dll') if f.name.lower() == dll.lower()), None)
                        if src and src.is_file():
                            shutil.copy2(src, self.root / src.name)
                            present.add(dll.lower())
                            break
                    # Fora das pastas = do sistema (KERNEL32, api-ms-win-crt-*...).


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--dir', required=True, help='pasta instalada pelo cmake --install')
    ap.add_argument('--qt', required=True)
    ap.add_argument('--qt-host', required=True)
    ap.add_argument('--lib-dir', action='append', default=[])
    ap.add_argument('--qml-root', action='append', default=['/src/src/app/qml'])
    Bundler(ap.parse_args()).make()


if __name__ == '__main__':
    main()

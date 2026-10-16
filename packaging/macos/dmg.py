"""Instalador ``.dmg`` do PhotoIDX — montado no Linux, sem ``hdiutil``.

Recebe o ``PhotoIDX.app`` pronto (cmake --install + bundle.py) e monta a
janela "arraste para Aplicativos": fundo desenhado do logo
(``packaging/art.py``), o app à esquerda e o atalho de Aplicativos à direita.

O ``.dmg`` segue o caminho que a Mozilla usa para o Firefox (e o clivcut):
``mkfs.hfsplus`` cria o volume HFS+, o ``hfsplus`` do libdmg-hfsplus o
preenche sem montar nada, e o ``dmg`` comprime (UDZO). A janela que abre ao
montar — fundo, tamanho, posição dos ícones — é o ``.DS_Store`` da raiz,
escrito aqui com ``ds_store`` + ``mac_alias``.

Uso:
    dmg.py --app PhotoIDX.app --work <tmp> --out dist/PhotoIDX-...-macos-arm64.dmg
    dmg.py --preview <pasta>        # confere a arte em PNG
"""
import argparse
import datetime
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import art  # noqa: E402  (packaging/art.py)

APP_NAME = 'PhotoIDX'
VOLUME_NAME = APP_NAME

HFSPLUS = os.environ.get('HFSPLUS', 'hfsplus')
DMG = os.environ.get('DMG_TOOL', 'dmg')
MKFS_HFSPLUS = os.environ.get('MKFS_HFSPLUS', 'mkfs.hfsplus')

# HFS+: o cabecalho do volume fica em 1024 bytes do inicio (e uma copia a
# 1024 bytes do fim). Campos usados — ver TN1150 (HFS Plus Volume Format).
_HEADER_OFFSET = 1024
_CREATE_DATE = 16          # UInt32, segundos desde 1904 (hora local)
_NEXT_CATALOG_ID = 64      # UInt32, proximo CNID livre
_FINDER_INFO = 80          # 8 x UInt32; [2] = pasta aberta ao montar
_ROOT_CNID = 2
# O mac_alias so aceita data com fuso. O mkfs.hfsplus grava a hora local do
# container, que e UTC.
_MAC_EPOCH = datetime.datetime(1904, 1, 1, tzinfo=datetime.timezone.utc)


def _read_u32(image: Path, offset: int) -> int:
    with open(image, 'rb') as f:
        f.seek(_HEADER_OFFSET + offset)
        return struct.unpack('>I', f.read(4))[0]


def _write_u32(image: Path, offset: int, value: int):
    """Grava no cabecalho principal e na copia do fim do volume."""
    size = image.stat().st_size
    with open(image, 'r+b') as f:
        for base in (_HEADER_OFFSET, size - _HEADER_OFFSET):
            f.seek(base + offset)
            f.write(struct.pack('>I', value))


def _hfs(image: Path, *args):
    subprocess.run([HFSPLUS, '--symlinks', 'clone_link', '--special-modes', 'no',
                    str(image), *[str(a) for a in args]],
                   check=True, stdout=subprocess.DEVNULL)


# Mach-O: magia (64 bits, little-endian) e "fat"; filetype 2 = MH_EXECUTE.
_MACHO_64 = b'\xcf\xfa\xed\xfe'
_MACHO_FAT = b'\xca\xfe\xba\xbe'


def _needs_exec_bit(path: Path) -> bool:
    """Executavel Mach-O ou script — o hfsplus cria tudo como 0644.

    Biblioteca (.dylib/.so) fica de fora: o dyld carrega sem o bit, e marcar
    as milhares do runtime seriam milhares de chamadas ao hfsplus.
    """
    if path.is_symlink() or not os.access(path, os.X_OK):
        return False
    with open(path, 'rb') as f:
        head = f.read(16)
    if head[:2] == b'#!':
        return True
    if head[:4] == _MACHO_64:
        return struct.unpack('<I', head[12:16])[0] == 2
    if head[:4] == _MACHO_FAT:
        return not path.name.endswith(('.dylib', '.so'))
    return False


def _executables(root: Path):
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            path = Path(dirpath) / name
            if _needs_exec_bit(path):
                yield path


def _write_ds_store(target: Path, volume_created: datetime.datetime,
                    background_cnid: int, app_name: str):
    """A janela do volume: sem barras, do tamanho do fundo, icones no lugar."""
    from ds_store import DSStore
    from mac_alias import Alias, TargetInfo, VolumeInfo, ALIAS_FIXED_DISK, ALIAS_KIND_FILE

    width, height = art.DMG_LAYOUT['window']
    # Alias do fundo montado a mao (o Alias.for_file so roda num Mac com o
    # volume montado). O Finder o resolve pelo nome do volume + caminho; os
    # CNIDs e a data batem com o volume de verdade de qualquer forma.
    alias = Alias(
        volume=VolumeInfo(VOLUME_NAME.encode(), volume_created, b'H+', ALIAS_FIXED_DISK, 0,
                          b'\0\0', posix_path=f'/Volumes/{VOLUME_NAME}'.encode()),
        target=TargetInfo(ALIAS_KIND_FILE, b'.background.tiff', _ROOT_CNID, background_cnid,
                          volume_created, b'\0\0\0\0', b'\0\0\0\0',
                          folder_name=VOLUME_NAME.encode(), cnid_path=[],
                          carbon_path=f'{VOLUME_NAME}:.background.tiff'.encode(),
                          posix_path=b'/.background.tiff'),
    )
    with DSStore.open(str(target), 'w+') as d:
        d['.']['vSrn'] = ('long', 1)
        d['.']['bwsp'] = {
            'ShowStatusBar': False,
            'WindowBounds': f'{{{{200, 120}}, {{{width}, {height}}}}}',
            'ContainerShowSidebar': False,
            'PreviewPaneVisibility': False,
            'SidebarWidth': 0,
            'ShowTabView': False,
            'ShowToolbar': False,
            'ShowPathbar': False,
            'ShowSidebar': False,
        }
        d['.']['icvp'] = {
            'viewOptionsVersion': 1,
            'backgroundType': 2,
            'backgroundImageAlias': alias.to_bytes(),
            'backgroundColorRed': 1.0,
            'backgroundColorGreen': 1.0,
            'backgroundColorBlue': 1.0,
            'gridOffsetX': 0.0,
            'gridOffsetY': 0.0,
            'gridSpacing': 100.0,
            'arrangeBy': 'none',
            'showIconPreview': False,
            'showItemInfo': False,
            'labelOnBottom': True,
            'textSize': float(art.DMG_LAYOUT['text_size']),
            'iconSize': float(art.DMG_LAYOUT['icon_size']),
            'scrollPositionX': 0.0,
            'scrollPositionY': 0.0,
        }
        d['.']['icvl'] = (b'type', b'icnv')
        icons = art.DMG_LAYOUT['icons']
        d[app_name]['Iloc'] = icons['app']
        d['Applications']['Iloc'] = icons['applications']


def build_dmg(app: Path, out_dmg: Path, work: Path) -> Path:
    """``out_dmg`` com o ``app`` e o atalho de Aplicativos, na janela desenhada."""
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    stage = work / 'stage'
    stage.mkdir()
    # O .app entra por ultimo, via addall; mover (mesmo disco) evita copiar o pacote.
    os.replace(app, stage / app.name)
    art.write_icns(stage / '.VolumeIcon.icns')
    background = art.write_dmg_background(work / 'background.tiff')

    # Volume com folga de 20% + 64 MB: o catalogo e os extents ocupam espaco.
    payload = sum(p.stat().st_size for p in stage.rglob('*') if p.is_file() and not p.is_symlink())
    size = int(payload * 1.2) + 64 * 1024 * 1024
    size -= size % 4096
    image = work / 'volume.hfs'
    with open(image, 'wb') as f:
        f.truncate(size)
    subprocess.run([MKFS_HFSPLUS, '-v', VOLUME_NAME, str(image)], check=True,
                   stdout=subprocess.DEVNULL)

    created = _MAC_EPOCH + datetime.timedelta(seconds=_read_u32(image, _CREATE_DATE))
    # O fundo entra PRIMEIRO: o CNID dele e o proximo livre do volume recem
    # criado, e o alias no .DS_Store precisa dele.
    background_cnid = _read_u32(image, _NEXT_CATALOG_ID)
    _hfs(image, 'add', background, '/.background.tiff')
    assert _read_u32(image, _NEXT_CATALOG_ID) == background_cnid + 1, 'unexpected CNID allocation'

    ds_store = work / 'DS_Store'
    _write_ds_store(ds_store, created, background_cnid, app.name)
    _hfs(image, 'add', ds_store, '/.DS_Store')
    _hfs(image, 'symlink', '/Applications', '/Applications')

    _hfs(image, 'addall', stage, '/')
    for path in _executables(stage):
        _hfs(image, 'chmod', '755', '/' + path.relative_to(stage).as_posix())
    # C = kHasCustomIcon na raiz: o Finder usa o .VolumeIcon.icns no volume.
    _hfs(image, 'attr', '/', 'C')

    # finderInfo[2] = pasta que o Finder abre ao montar (o "bless --openfolder").
    _write_u32(image, _FINDER_INFO + 8, _ROOT_CNID)

    out_dmg.parent.mkdir(parents=True, exist_ok=True)
    out_dmg.unlink(missing_ok=True)
    subprocess.run([DMG, 'build', str(image), str(out_dmg)], check=True,
                   stdout=subprocess.DEVNULL)
    shutil.rmtree(work)
    return out_dmg


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--app', type=Path, help='PhotoIDX.app pronto (é MOVIDO para o volume)')
    ap.add_argument('--work', type=Path, help='pasta temporária')
    ap.add_argument('--out', type=Path, help='.dmg de saída')
    ap.add_argument('--preview', type=Path, help='só grava a arte em PNG nesta pasta')
    args = ap.parse_args()
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        art.write_icons(args.preview)
        art.dmg_background(1).save(args.preview / 'background-1x.png')
        art.dmg_background(2).save(args.preview / 'background-2x.png')
        print('ok ->', args.preview)
        return
    if not (args.app and args.work and args.out):
        ap.error('--app, --work e --out são obrigatórios (ou --preview)')
    print(build_dmg(args.app, args.out, args.work))


if __name__ == '__main__':
    main()

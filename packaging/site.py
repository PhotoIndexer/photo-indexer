#!/usr/bin/env python3
"""Monta o hotsite (docs/site) para publicar no GitHub Pages.

    python3 packaging/site.py --out <pasta>     # make site

Junta numa pasta só: o HTML/CSS/JS de docs/site, as capturas de tela de
docs/images (assets/screens/) e o logo e os ícones do app (assets/), de onde
eles já moram — nada é versionado duas vezes. Troca o %BUILD_TS% (cache dos
navegadores) pelo instante do build. Só biblioteca padrão.
"""
import argparse
import shutil
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / 'docs' / 'site'
SCREENS = ROOT / 'docs' / 'images'
ASSETS = ROOT / 'src' / 'app' / 'assets'


def build(out: Path):
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(SITE, out)
    (out / 'assets' / 'screens').mkdir(parents=True, exist_ok=True)
    for jpg in SCREENS.glob('*.jpg'):
        shutil.copy2(jpg, out / 'assets' / 'screens' / jpg.name)
    (out / 'assets' / 'logo').mkdir(parents=True, exist_ok=True)
    shutil.copy2(ASSETS / 'logo' / 'photoidx-dark.png', out / 'assets' / 'logo' / 'photoidx-dark.png')
    for icon in ('favicon.png', 'favicon.ico'):
        shutil.copy2(ASSETS / icon, out / 'assets' / icon)
    stamp = time.strftime('%Y%m%d%H%M%S', time.gmtime())
    for page in list(out.rglob('*.html')) + list(out.rglob('*.css')):
        page.write_text(page.read_text(encoding='utf-8').replace('%BUILD_TS%', stamp), encoding='utf-8')
    # Sem o Jekyll do Pages: a pasta vai como está.
    (out / '.nojekyll').write_text('')
    print(f'site -> {out} ({sum(1 for _ in out.rglob("*") if _.is_file())} files)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', type=Path, default=ROOT / '.cache' / 'site')
    build(parser.parse_args().out.resolve())

#!/usr/bin/env python3
"""Monta o hotsite (docs/site) para publicar no GitHub Pages.

    python3 packaging/site.py --out <pasta>     # make site

Junta numa pasta só: o HTML/CSS/JS de docs/site, as capturas de tela de
docs/images (assets/screens/) e o logo e os ícones do app (assets/), de onde
eles já moram — nada é versionado duas vezes. Troca o %BUILD_TS% (cache dos
navegadores) pelo instante do build e grava assets/releases.json com as
releases publicadas e os instaladores delas (o que a página usa se a API
do GitHub não responder; com GITHUB_TOKEN no ambiente, sem o limite da API
anônima). Só biblioteca padrão.
"""
import argparse
import json
import os
import shutil
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / 'docs' / 'site'
SCREENS = ROOT / 'docs' / 'images'
ASSETS = ROOT / 'src' / 'app' / 'assets'
REPO = 'PhotoIndexer/photo-indexer'


def releases() -> list:
    """As releases publicadas, só com o que a página usa (o mesmo ``slim`` do
    main.js). Lista vazia se o GitHub não responder: o site sai assim mesmo
    (a página ainda tenta a API ao vivo)."""
    request = urllib.request.Request(f'https://api.github.com/repos/{REPO}/releases?per_page=30',
                                     headers={'Accept': 'application/vnd.github+json'})
    token = os.environ.get('GITHUB_TOKEN') or os.environ.get('GH_TOKEN')
    if token:
        request.add_header('Authorization', f'Bearer {token}')
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            data = json.load(response)
    except Exception as error:  # noqa: BLE001 — sem rede no build local, por exemplo
        print(f'warning: releases not fetched ({error}); assets/releases.json is empty')
        return []
    return [{
        'id': r['id'], 'tag_name': r['tag_name'], 'draft': r['draft'], 'prerelease': r['prerelease'],
        'published_at': r['published_at'],
        'assets': [{'name': a['name'], 'size': a['size'], 'browser_download_url': a['browser_download_url']}
                   for a in r.get('assets', [])],
    } for r in data if not r.get('draft')]


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
    found = releases()
    (out / 'assets' / 'releases.json').write_text(json.dumps(found, separators=(',', ':')), encoding='utf-8')
    print(f'releases.json: {len(found)} release(s), {sum(len(r["assets"]) for r in found)} file(s)')
    # Sem o Jekyll do Pages: a pasta vai como está.
    (out / '.nojekyll').write_text('')
    print(f'site -> {out} ({sum(1 for _ in out.rglob("*") if _.is_file())} files)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', type=Path, default=ROOT / '.cache' / 'site')
    build(parser.parse_args().out.resolve())

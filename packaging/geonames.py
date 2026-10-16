#!/usr/bin/env python3
"""Gera src/core/data/places.qz a partir do GeoNames (CC-BY 4.0).

A geolocalização reversa (GPS -> cidade, estado, país) é offline: a tabela vai
embutida no binário (recurso Qt, comprimido pelo rcc). Ela é GERADA por este
script e versionada — o GeoNames não tem versões fixas para baixar no build
(regra 16), então o arquivo versionado é a versão.

    python3 packaging/geonames.py            # baixa e grava a tabela

O arquivo é o TSV comprimido no formato do ``qUncompress`` do Qt (tamanho em
4 bytes big-endian + zlib). Formato do TSV (uma cidade por linha, por TAB):

    latitude  longitude  cidade  estado  código do país  país

Só cidades com 1.000+ habitantes (cities1000): ~150 mil linhas.
"""
import csv
import struct
import zlib
import io
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'src' / 'core' / 'data' / 'places.qz'
BASE = 'https://download.geonames.org/export/dump/'


def fetch(name):
    with urllib.request.urlopen(BASE + name, timeout=120) as r:
        return r.read()


def main():
    countries = {}
    for line in fetch('countryInfo.txt').decode('utf-8').splitlines():
        if line.startswith('#') or not line.strip():
            continue
        parts = line.split('\t')
        countries[parts[0]] = parts[4]
    admin1 = {}
    for line in fetch('admin1CodesASCII.txt').decode('utf-8').splitlines():
        parts = line.split('\t')
        if len(parts) >= 2:
            admin1[parts[0]] = parts[1]
    with zipfile.ZipFile(io.BytesIO(fetch('cities1000.zip'))) as zf:
        text = zf.read('cities1000.txt').decode('utf-8')
    rows = []
    for parts in csv.reader(io.StringIO(text), delimiter='\t', quoting=csv.QUOTE_NONE):
        if len(parts) < 11:
            continue
        name, lat, lon, cc, a1 = parts[1], parts[4], parts[5], parts[8], parts[10]
        rows.append((round(float(lat), 4), round(float(lon), 4), name,
                     admin1.get(f'{cc}.{a1}', ''), cc, countries.get(cc, cc)))
    rows.sort()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = ['# GeoNames cities1000 (https://www.geonames.org), CC-BY 4.0 — packaging/geonames.py']
    lines += ['\t'.join(str(v) for v in r) for r in rows]
    raw = ('\n'.join(lines) + '\n').encode('utf-8')
    OUT.write_bytes(struct.pack('>I', len(raw)) + zlib.compress(raw, 9))
    print(f'{len(rows)} places -> {OUT.relative_to(ROOT)}')


if __name__ == '__main__':
    sys.exit(main())

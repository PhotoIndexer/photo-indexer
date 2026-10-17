"""Monta as fotos fictícias dos tutoriais.

/faces/face1..8.jpg -> rostos gerados por IA (pessoas que não existem).
/demo/Fotos         -> paisagens, cidade, pets, trilha (o demo do app).
/out/Fotos          -> a árvore final.
"""
import random
import shutil
from pathlib import Path

import piexif
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont, ImageOps

random.seed(7)
import json, os
OUT = Path(os.environ.get('OUT', '/out/Fotos'))
MANIFEST = {}
if OUT.exists():
    shutil.rmtree(OUT)
OUT.mkdir(parents=True)

# Demo (sem a pasta de cópias de teste).
for d in Path('/demo/Fotos').rglob('*'):
    if d.is_file() and 'Copias' not in d.parts:
        target = OUT / d.relative_to('/demo/Fotos')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(d, target)

faces = {i: Image.open(f'/faces/face{i}.jpg').convert('RGB') for i in range(1, 9)}


def exif(date, make='Canon', model='EOS R6', gps=None):
    zeroth = {piexif.ImageIFD.Make: make, piexif.ImageIFD.Model: model, piexif.ImageIFD.DateTime: date}
    ex = {piexif.ExifIFD.DateTimeOriginal: date, piexif.ExifIFD.DateTimeDigitized: date,
          piexif.ExifIFD.ISOSpeedRatings: random.choice([100, 200, 400, 800]),
          piexif.ExifIFD.FNumber: (random.choice([18, 28, 40, 56]), 10),
          piexif.ExifIFD.ExposureTime: (1, random.choice([125, 250, 500, 1000])),
          piexif.ExifIFD.FocalLength: (random.choice([35, 50, 85]), 1)}
    gps_ifd = {}
    if gps:
        def dms(v):
            v = abs(v)
            d = int(v)
            m = int((v - d) * 60)
            s = round(((v - d) * 60 - m) * 60 * 100)
            return ((d, 1), (m, 1), (s, 100))
        gps_ifd = {piexif.GPSIFD.GPSLatitudeRef: 'S' if gps[0] < 0 else 'N', piexif.GPSIFD.GPSLatitude: dms(gps[0]),
                   piexif.GPSIFD.GPSLongitudeRef: 'W' if gps[1] < 0 else 'E', piexif.GPSIFD.GPSLongitude: dms(gps[1])}
    return piexif.dump({'0th': zeroth, 'Exif': ex, 'GPS': gps_ifd})


def grade(img, kind):
    if kind == 'warm':
        r, g, b = img.split()
        return Image.merge('RGB', (r.point(lambda v: min(255, int(v * 1.08))), g, b.point(lambda v: int(v * 0.92))))
    if kind == 'cool':
        r, g, b = img.split()
        return Image.merge('RGB', (r.point(lambda v: int(v * 0.93)), g, b.point(lambda v: min(255, int(v * 1.07)))))
    if kind == 'soft':
        return ImageEnhance.Color(ImageEnhance.Contrast(img).enhance(0.9)).enhance(0.8)
    if kind == 'bright':
        return ImageEnhance.Brightness(img).enhance(1.12)
    return img


def portrait(face, zoom, mirror, kind):
    """Um recorte do retrato (quadrado), com zoom e leve deslocamento."""
    w = face.width
    side = int(w * zoom)
    x = (w - side) // 2 + random.randint(-w // 40, w // 40)
    y = (w - side) // 2 + random.randint(-w // 40, w // 40)
    x, y = max(0, min(w - side, x)), max(0, min(w - side, y))
    crop = face.crop((x, y, x + side, y + side))
    if mirror:
        crop = ImageOps.mirror(crop)
    return grade(crop, kind)


def frame(parts, size=(1800, 1200)):
    """Retratos lado a lado sobre o fundo deles desfocado."""
    W, H = size
    bg = ImageOps.fit(parts[0], (W, H)).filter(ImageFilter.GaussianBlur(40))
    bg = ImageEnhance.Brightness(bg).enhance(0.75)
    n = len(parts)
    slot = W // n if n > 1 else int(H * 1.0)
    for i, p in enumerate(parts):
        h = H if n > 1 else H
        tile = ImageOps.fit(p, (min(slot, W), h))
        x = i * slot if n > 1 else (W - tile.width) // 2 + random.randint(-200, 200)
        bg.paste(tile, (max(0, min(W - tile.width, x)), 0))
    return bg


def save(img, path, date, gps=None, model='EOS R6', whos=()):
    MANIFEST[str(path.relative_to(OUT))] = list(whos)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path, quality=90, exif=exif(date, model=model, gps=gps))


KINDS = ['none', 'warm', 'cool', 'soft', 'bright']
counter = [1000]


def name():
    counter[0] += random.randint(1, 9)
    return f'IMG_{counter[0]}.jpg'


def person_set(folder, who, count, date_prefix, gps=None, portrait_ratio=False):
    for k in range(count):
        p = portrait(faces[who], random.choice([1.0, 0.85, 0.72]), k % 2 == 1, KINDS[k % len(KINDS)])
        size = (1200, 1800) if portrait_ratio and k % 3 == 0 else (1800, 1200)
        img = frame([p], size)
        save(img, OUT / folder / name(), f'{date_prefix} {10 + k}:{15 + 3 * k:02d}:00', gps, whos=[who])


def group(folder, whos, date, gps=None):
    parts = [portrait(faces[w], 0.8, False, 'none') for w in whos]
    save(frame(parts), OUT / folder / name(), date, gps, whos=whos)


CURITIBA = (-25.4284, -49.2733)
FLORIPA = (-27.5954, -48.5480)
SP = (-23.5505, -46.6333)

# Família Souza: Ana (1), Lia (2), Helena (5), Theo (6).
for who, n in ((1, 5), (2, 5), (5, 4), (6, 4)):
    person_set('2025/Familia-Souza', who, n, '2025:06:14', CURITIBA, portrait_ratio=True)
group('2025/Familia-Souza', [1, 2], '2025:06:14 17:40:00', CURITIBA)
group('2025/Familia-Souza', [1, 6, 2], '2025:06:14 18:05:00', CURITIBA)
group('2025/Familia-Souza', [5, 1], '2025:06:14 18:20:00', CURITIBA)
# Amigos: Bruno (3), Diego (4) e um amigo ainda sem nome (7).
for who, n in ((3, 4), (4, 4), (7, 4)):
    person_set('2025/Churrasco-Amigos', who, n, '2025:03:08', FLORIPA)
group('2025/Churrasco-Amigos', [3, 4], '2025:03:08 20:10:00', FLORIPA)
group('2025/Churrasco-Amigos', [4, 7, 3], '2025:03:08 20:30:00', FLORIPA)
# Escola: um menino ainda sem nome (8) e o Theo (6).
person_set('2024/Escola-Futebol', 8, 4, '2024:09:21', SP)
person_set('2024/Escola-Futebol', 6, 2, '2024:09:21', SP)
group('2024/Escola-Futebol', [8, 6], '2024:09:21 11:00:00', SP)

# Cópias exportadas: menor, com marca d'água e idêntica com outro nome.
exported = OUT / 'Exportadas'
exported.mkdir(parents=True, exist_ok=True)
sources = sorted((OUT / '2025' / 'Familia-Souza').glob('*.jpg'))[:2] + sorted((OUT / '2024' / 'Trilha').glob('*.jpg'))[:1]
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 34)
except OSError:
    font = ImageFont.load_default()
for i, src in enumerate(sources):
    img = Image.open(src).convert('RGB')
    small = img.copy()
    small.thumbnail((1024, 1024))
    small.save(exported / f'{src.stem}_web.jpg', quality=82)
    marked = small.copy()
    d = ImageDraw.Draw(marked, 'RGBA')
    text = '© Estúdio Exemplo'
    tw = d.textlength(text, font=font)
    d.rectangle((marked.width - tw - 40, marked.height - 64, marked.width - 10, marked.height - 12), fill=(0, 0, 0, 140))
    d.text((marked.width - tw - 25, marked.height - 58), text, font=font, fill=(255, 255, 255, 230))
    marked.save(exported / f'{src.stem}_marca.jpg', quality=85)
    if i == 0:
        shutil.copy2(src, exported / f'{src.stem} (copia).jpg')

Path(os.environ.get('MANIFEST', '/out/manifest.json')).write_text(json.dumps(MANIFEST))
print(sum(1 for _ in OUT.rglob('*.jpg')), 'photos')
for d in sorted({p.parent for p in OUT.rglob('*.jpg')}):
    print(d.relative_to(OUT), len(list(d.glob('*.jpg'))))

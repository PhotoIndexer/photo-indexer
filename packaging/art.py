"""Arte dos instaladores, desenhada em codigo a partir do logo do build.

* ``mark``            — o hexagono em "S" recortado do logo, em alta resolucao;
* ``write_icns``      — icone do macOS (placa branca arredondada, padrao do
                        macOS 11+, com a marca no centro);
* ``write_ico``       — icone do Windows (a marca sobre fundo transparente);
* ``write_dmg_background`` — fundo da janela do DMG: logo, faixa
                        "INSTALLATION", instrucao em EN e PT-BR e a seta do
                        app para o atalho de Aplicativos (layout em DMG_LAYOUT).

Desenhar aqui (e nao versionar PNG pronto) faz a arte acompanhar a marca:
trocou o logo em ``src/app/assets/logo``, o proximo build ja sai certo.

Uso direto para conferir o resultado:  python3 packaging/art.py <pasta>
(ou ``packaging/build.py art``, que gera os icones que o CMake embute).
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
LOGO_LIGHT = ROOT / 'src' / 'app' / 'assets' / 'logo' / 'photoidx-light.png'

BRAND = (239, 50, 54)            # #ef3236, brand-500 do tailwind.config.js
BRAND_PALE = (250, 192, 193)     # brand-200
INK = (55, 45, 74)               # tom do "PHOTO" do logo
TEXT = (24, 24, 27)
MUTED = (113, 113, 122)

# Janela do DMG em PONTOS (o Finder mede assim; o fundo sai em 1x e 2x).
# ``icons`` = CENTRO de cada icone, como o Finder grava no .DS_Store.
# A altura util da janela varia: com a barra de abas do Finder ligada sobram
# ~424 pt dos 480. Icone em y=318 deixa o rotulo (ate ~y=405) sempre visivel.
DMG_LAYOUT = {
    'window': (640, 480),
    'icon_size': 128,
    'text_size': 14,
    'icons': {'app': (160, 318), 'applications': (480, 318)},
}

# Fonte das letras do fundo: Liberation Sans (metrica do Arial) no builder;
# as demais so para conferir a arte fora do container.
_FONTS_BOLD = (
    '/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf',
    '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
    '/System/Library/Fonts/Supplemental/Arial Bold.ttf',
    'C:/Windows/Fonts/arialbd.ttf',
)
_FONTS_REGULAR = (
    '/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf',
    '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
    '/System/Library/Fonts/Supplemental/Arial.ttf',
    'C:/Windows/Fonts/arial.ttf',
)


def _font(candidates, size):
    for path in candidates:
        if Path(path).is_file():
            return ImageFont.truetype(path, size)
    raise FileNotFoundError(f'No font found among {candidates}')


def mark() -> Image.Image:
    """O hexagono em "S" do logo, recortado rente (RGBA)."""
    logo = Image.open(LOGO_LIGHT).convert('RGBA')
    # A marca é quase quadrada e vem antes do texto "PHOTOIDX": um recorte da
    # largura de 95% da altura a pega inteira, qualquer que seja o texto.
    left = logo.crop((0, 0, int(logo.height * 0.95), logo.height))
    bbox = left.getchannel('A').point(lambda a: 255 if a > 16 else 0).getbbox()
    return left.crop(bbox)


def _fit(img, box):
    ratio = min(box / img.width, box / img.height)
    return img.resize((round(img.width * ratio), round(img.height * ratio)),
                      Image.Resampling.LANCZOS)


def _paste_center(canvas, img, center=None):
    cx, cy = center or (canvas.width // 2, canvas.height // 2)
    canvas.alpha_composite(img, (cx - img.width // 2, cy - img.height // 2))


def app_icon(size=1024) -> Image.Image:
    """Icone do macOS: placa branca arredondada com sombra e a marca."""
    s = size / 1024
    canvas = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    plate_box = (round(100 * s), round(100 * s), round(924 * s), round(924 * s))
    radius = round(185 * s)

    shadow = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (plate_box[0], plate_box[1] + round(12 * s), plate_box[2], plate_box[3] + round(12 * s)),
        radius, fill=(0, 0, 0, 90))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(round(18 * s))))

    plate = Image.new('RGBA', canvas.size, (0, 0, 0, 0))
    gradient = Image.linear_gradient('L').resize(canvas.size)
    top, bottom = (255, 255, 255), (238, 238, 242)
    fill = Image.merge('RGB', [
        gradient.point(lambda v, a=a, b=b: round(a + (b - a) * v / 255))
        for a, b in zip(top, bottom)
    ]).convert('RGBA')
    mask = Image.new('L', canvas.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(plate_box, radius, fill=255)
    plate.paste(fill, (0, 0), mask)
    canvas.alpha_composite(plate)

    _paste_center(canvas, _fit(mark(), round(560 * s)))
    return canvas


def write_icns(path: Path):
    icon = app_icon(1024)
    path.parent.mkdir(parents=True, exist_ok=True)
    icon.save(path, format='ICNS',
              append_images=[app_icon(n) for n in (16, 32, 64, 128, 256, 512)])
    return path


def write_ico(path: Path):
    """Windows: a marca sem placa (o Explorer poe o proprio fundo)."""
    canvas = Image.new('RGBA', (256, 256), (0, 0, 0, 0))
    _paste_center(canvas, _fit(mark(), 232))
    path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(path, format='ICO',
                sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    return path


def write_png(path: Path, size=256):
    """Linux (.desktop/hicolor): a mesma placa do icone do macOS."""
    path.parent.mkdir(parents=True, exist_ok=True)
    app_icon(size).save(path, format='PNG')
    return path


def write_icons(out: Path):
    """Os icones que o CMake embute no executavel (PhotoIDX.icns/.ico/.png)."""
    return [write_icns(out / 'PhotoIDX.icns'), write_ico(out / 'PhotoIDX.ico'),
            write_png(out / 'PhotoIDX.png', 256)]


def _bezier(p0, p1, p2, t):
    u = 1 - t
    return (u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
            u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1])


def _lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def _draw_arrow(draw, k, start, control, end):
    """Faixa curva que engrossa e esquenta ate a ponta (como a da referencia)."""
    head_from = 0.80
    steps = 400
    for i in range(steps + 1):
        t = head_from * i / steps
        x, y = _bezier(start, control, end, t)
        r = (3 + 8 * (t / head_from) ** 1.4) * k
        color = _lerp(BRAND_PALE, BRAND, (t / head_from) ** 0.8) + (round(255 * min(1, 0.15 + t * 1.6)),)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=color)
    # A ponta segue a TANGENTE da curva onde a faixa termina: apontar direto
    # para o fim deixava a base torta em relacao a faixa.
    bx, by = _bezier(start, control, end, head_from)
    t = head_from
    dx = 2 * (1 - t) * (control[0] - start[0]) + 2 * t * (end[0] - control[0])
    dy = 2 * (1 - t) * (control[1] - start[1]) + 2 * t * (end[1] - control[1])
    length = (dx * dx + dy * dy) ** 0.5
    ux, uy = dx / length, dy / length
    nx, ny = -uy, ux
    tx, ty = bx + ux * 34 * k, by + uy * 34 * k
    half = 21 * k
    draw.polygon([(tx, ty), (bx + nx * half, by + ny * half), (bx - nx * half, by - ny * half)],
                 fill=BRAND + (255,))


def dmg_background(scale=1) -> Image.Image:
    """Fundo da janela do DMG em ``scale`` (1 = pontos, 2 = Retina)."""
    ss = 4                                   # supersampling: bordas lisas
    k = scale * ss
    w, h = DMG_LAYOUT['window']
    img = Image.new('RGBA', (w * k, h * k), (255, 255, 255, 255))
    draw = ImageDraw.Draw(img)

    logo = Image.open(LOGO_LIGHT).convert('RGBA')
    logo = _fit(logo, 330 * k)
    img.alpha_composite(logo, ((w * k - logo.width) // 2, 34 * k))

    bar_top, bar_bottom = 128, 158
    draw.rectangle((40 * k, bar_top * k, (w - 40) * k, bar_bottom * k), fill=INK)
    draw.rectangle((40 * k, bar_top * k, 46 * k, bar_bottom * k), fill=BRAND)
    font = _font(_FONTS_BOLD, 19 * k)
    draw.text((64 * k, (bar_top + bar_bottom) / 2 * k), 'INSTALLATION', font=font,
              fill=(255, 255, 255), anchor='lm')

    draw.text((w / 2 * k, 196 * k), 'Drag PhotoIDX to the Applications folder',
              font=_font(_FONTS_BOLD, 19 * k), fill=TEXT, anchor='mm')
    draw.text((w / 2 * k, 222 * k), 'Arraste o PhotoIDX para a pasta Aplicativos',
              font=_font(_FONTS_REGULAR, 15 * k), fill=MUTED, anchor='mm')

    (ax, ay), (bx, by) = DMG_LAYOUT['icons']['app'], DMG_LAYOUT['icons']['applications']
    half_icon = DMG_LAYOUT['icon_size'] / 2
    start = ((ax + half_icon + 8) * k, (ay - 4) * k)
    end = ((bx - half_icon - 6) * k, (by - 10) * k)
    control = ((ax + bx) / 2 * k, (ay - 78) * k)
    _draw_arrow(draw, k, start, control, end)

    return img.resize((w * scale, h * scale), Image.Resampling.LANCZOS).convert('RGB')


def write_dmg_background(path: Path):
    """TIFF com as duas resolucoes (72 e 144 dpi): o Finder escolhe a da tela,
    como faz com o que o ``tiffutil -cathidpicheck`` gera no macOS."""
    path.parent.mkdir(parents=True, exist_ok=True)
    one, two = dmg_background(1), dmg_background(2)
    one.save(path, format='TIFF', dpi=(72, 72), compression='tiff_lzw',
             save_all=True, append_images=[two])
    # Pillow grava o dpi so na 1a pagina: a 2a precisa dizer 144 para o Finder.
    _set_tiff_page_dpi(path, page=1, dpi=144)
    return path


def _set_tiff_page_dpi(path: Path, page: int, dpi: int):
    frames = []
    with Image.open(path) as tif:
        for i in range(tif.n_frames):
            tif.seek(i)
            frames.append((tif.copy(), 144 if i == page else 72))
    first, rest = frames[0], frames[1:]
    first[0].save(path, format='TIFF', compression='tiff_lzw', dpi=(first[1], first[1]),
                  save_all=True, append_images=[f for f, _ in rest],
                  tiffinfo={})
    # save_all nao aceita dpi por pagina: regrava a tag de resolucao direto.
    _patch_tiff_resolution(path, [d for _, d in frames])


def _patch_tiff_resolution(path: Path, dpis):
    """Reescreve XResolution/YResolution (tags 282/283) de cada pagina."""
    import struct
    data = bytearray(path.read_bytes())
    endian = '<' if data[:2] == b'II' else '>'
    (offset,) = struct.unpack_from(endian + 'I', data, 4)
    for dpi in dpis:
        if not offset:
            break
        (count,) = struct.unpack_from(endian + 'H', data, offset)
        for i in range(count):
            entry = offset + 2 + i * 12
            tag, typ, n, value = struct.unpack_from(endian + 'HHII', data, entry)
            if tag in (282, 283) and typ == 5:          # RATIONAL -> [num, den]
                struct.pack_into(endian + 'II', data, value, dpi, 1)
        (offset,) = struct.unpack_from(endian + 'I', data, offset + 2 + count * 12)
    path.write_bytes(bytes(data))


if __name__ == '__main__':
    out = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
    write_icons(out)
    write_dmg_background(out / 'background.tiff')
    app_icon(512).save(out / 'icon-preview.png')
    dmg_background(1).save(out / 'background-1x.png')
    dmg_background(2).save(out / 'background-2x.png')
    print('ok ->', out)

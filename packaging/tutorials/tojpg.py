from PIL import Image
from pathlib import Path
for src, dst in (('en', 'en'), ('pt', 'pt-br')):
    out = Path('/docs/images/tutorials') / dst
    out.mkdir(parents=True, exist_ok=True)
    for f in sorted(Path('/shots', src).glob('*.png')):
        Image.open(f).convert('RGB').save(out / (f.stem + '.jpg'), quality=82, optimize=True, progressive=True)
total = sum(p.stat().st_size for p in Path('/docs/images/tutorials').rglob('*.jpg'))
print(len(list(Path('/docs/images/tutorials').rglob('*.jpg'))), 'files', round(total / 1e6, 1), 'MB')

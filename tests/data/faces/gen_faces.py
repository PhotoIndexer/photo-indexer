"""Referencia do reconhecimento facial (insightface buffalo_sc, o mesmo do
../photoe) e do picture_hash (Pillow) para o tst_faces / tst_imaging.

Roda num container descartavel (regra 13):

    docker run --rm -v "$PWD":/src -w /src python:3.12 sh -c \
      "pip install -q insightface==0.7.3 onnxruntime==1.23.2 opencv-python-headless==4.12.0.88 \
       "numpy<2.3" pillow==11.3.0 && python tests/data/faces/gen_faces.py <pasta do buffalo_sc>"
"""
import hashlib
import json
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageOps

HERE = Path(__file__).resolve().parent
DATA = HERE.parent


def picture_hash(img):
    """O picture_hash do ../photoe (services/images.py), linha por linha."""
    width, height = img.size
    digest = hashlib.sha256(f'{img.mode}:{width}x{height}:'.encode('ascii'))
    for top in range(0, height, 256):
        strip = img.crop((0, top, width, min(height, top + 256)))
        digest.update(strip.tobytes())
    return digest.hexdigest()


def main(models_root):
    from insightface.app import FaceAnalysis
    app = FaceAnalysis(name='buffalo_sc', root=models_root, providers=['CPUExecutionProvider'],
                       allowed_modules=['detection', 'recognition'])
    app.prepare(ctx_id=-1, det_size=(640, 640), det_thresh=0.35)

    out = {'faces': {}, 'hashes': {}}
    img = cv2.imread(str(DATA / 'sample.jpg'))
    faces = []
    for f in app.get(img):
        faces.append({'bbox': [float(v) for v in f.bbox], 'kps': [[float(a), float(b)] for a, b in f.kps],
                      'det_score': float(f.det_score),
                      'embedding': [round(float(v), 6) for v in f.normed_embedding]})
    faces.sort(key=lambda f: -f['det_score'])
    out['faces']['sample.jpg'] = faces

    for path in sorted(list((DATA / 'exif').glob('*.jpg')) + [DATA / 'sample.jpg']):
        with Image.open(path) as im:
            im = ImageOps.exif_transpose(im)
            out['hashes'][str(path.relative_to(DATA))] = picture_hash(im)

    (HERE / 'expected.json').write_text(json.dumps(out, indent=1) + '\n')
    print(len(faces), 'faces;', len(out['hashes']), 'hashes')


if __name__ == '__main__':
    main(sys.argv[1])

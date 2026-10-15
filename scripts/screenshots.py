from pathlib import Path
from PIL import Image
for path in Path("build/screenshots").glob("*.ppm"):
    Image.open(path).save(path.with_suffix(".png"))
    path.unlink()

"""Original abstract sample covers used only by the desktop preview."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

destination = Path("build/demo")
destination.mkdir(parents=True, exist_ok=True)
names = ["DOOM", "HORIZON", "JOURNEY", "CONTROL", "CELESTE", "NO MAN'S SKY", "DEATH STRANDING", "THE LAST GUARDIAN", "ABZU", "BLOODBORNE", "GOD OF WAR", "GRIS", "HOLLOW KNIGHT", "INSIDE", "RATCHET & CLANK", "SHADOW"]
palette = [(116, 34, 20), (21, 91, 110), (136, 75, 35), (140, 24, 30), (31, 51, 117), (12, 107, 102), (87, 100, 100), (84, 111, 73)]
font = ImageFont.truetype("assets/font.ttf", 39)
small = ImageFont.truetype("assets/font.ttf", 15)
for index, name in enumerate(names):
    image = Image.new("RGB", (512, 512))
    r, g, b = palette[index % len(palette)]
    pixels = image.load()
    for y in range(512):
        for x in range(512):
            light = max(.25, 1 - math.hypot(x - 240, y - 180) / 650)
            pixels[x, y] = (int(r * light), int(g * light), int(b * light))
    draw = ImageDraw.Draw(image, "RGBA")
    for i in range(5):
        base = 65 + i * 93
        draw.polygon([(base-150, 395), (base+125, 85 + i*23), (base+310, 395)], fill=(150, 219, 240, 18 + i*5))
    draw.ellipse((160, 113, 352, 305), outline=(224, 248, 255, 150), width=3)
    draw.ellipse((180, 133, 332, 285), outline=(224, 248, 255, 35), width=13)
    draw.text((36, 370), name, fill=(255, 255, 255), font=font)
    draw.text((38, 457), "DESKTOP PREVIEW / SAMPLE ARTWORK", fill=(255, 255, 255, 150), font=small)
    image.save(destination / f"{index}.png")

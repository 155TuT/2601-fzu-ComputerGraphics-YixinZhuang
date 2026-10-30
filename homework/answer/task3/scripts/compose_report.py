"""Arrange rendered bounce images for the report without changing their pixels."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

TASK = Path(__file__).resolve().parents[1]
OUT = TASK / "docs/screenshots"
font = ImageFont.truetype("C:/Windows/Fonts/consola.ttf", 18)
for accum in (0, 1):
    canvas = Image.new("RGB", (752, 436), "white")
    draw = ImageDraw.Draw(canvas)
    for depth in range(6):
        x = 8 + (depth % 3) * 248
        y = 8 + (depth // 3) * 214
        draw.text((x, y), f"m = {depth}", font=font, fill="black")
        source = Image.open(OUT / f"bunny_o{accum}_m{depth}.png").convert("RGB")
        assert source.size == (240, 180)
        canvas.paste(source, (x, y + 26))
    canvas.save(OUT / f"bounce_o{accum}.png")

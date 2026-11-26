# Pictogram PP Kiosk: een wit kioskscherm met "PP" op een groen-blauwe achtergrond.
# Getekend op 1024 px en daarna verkleind. Maakt ppkiosk.ico (programma, startmenu, installer),
# ppkiosk-256.png en de pictogrammen voor de beheerpagina (web-192/512, maskable).
# Gebruik: python make_icon.py  (vanuit de map assets; lettertype Inter Black)
import sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/opentype/inter/Inter-Black.otf"
S = 1024
GREEN = (46, 139, 87)
NAVY = (31, 42, 68)


def gradient(size, c1, c2):
    top = Image.new("RGBA", (size, size), c1 + (255,))
    bottom = Image.new("RGBA", (size, size), c2 + (255,))
    m = Image.linear_gradient("L").resize((size, size))
    return Image.composite(bottom, top, m)


def artwork(size, full_bleed=False):
    """full_bleed: vierkant zonder afgeronde hoeken (maskable), tekening iets kleiner."""
    bg = gradient(size, (58, 170, 110), (24, 74, 92))
    mask = Image.new("L", (size, size), 0)
    if full_bleed:
        mask.paste(255, (0, 0, size, size))
    else:
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius=int(size * 0.23), fill=255)
    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    icon.paste(bg, (0, 0), mask)

    k = 0.78 if full_bleed else 1.0          # schaal van de tekening
    o = size * (1 - k) / 2                   # verschuiving naar het midden
    def P(x, y):
        return (o + x * size * k, o + y * size * k)

    fg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(fg)
    white = (255, 255, 255, 255)
    # scherm
    x0, y0 = P(0.14, 0.15)
    x1, y1 = P(0.86, 0.66)
    d.rounded_rectangle([x0, y0, x1, y1], radius=size * k * 0.07, fill=white)
    # voet: hals en plaat
    hx0, hy0 = P(0.44, 0.64)
    hx1, hy1 = P(0.56, 0.78)
    d.rectangle([hx0, hy0, hx1, hy1], fill=white)
    bx0, by0 = P(0.28, 0.77)
    bx1, by1 = P(0.72, 0.85)
    d.rounded_rectangle([bx0, by0, bx1, by1], radius=size * k * 0.04, fill=white)

    # schaduw onder het witte deel
    alpha = fg.split()[3]
    shadow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).bitmap((0, int(size * 0.014)), alpha, fill=(0, 30, 30, 110))
    shadow = shadow.filter(ImageFilter.GaussianBlur(size * 0.014))
    icon = Image.alpha_composite(icon, Image.composite(shadow, Image.new("RGBA", (size, size)), mask))
    icon = Image.alpha_composite(icon, fg)

    # "PP" in het scherm, groen
    d = ImageDraw.Draw(icon)
    font = ImageFont.truetype(FONT, int(size * k * 0.36))
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    d.text((cx, cy + size * k * 0.005), "PP", font=font, fill=GREEN + (255,), anchor="mm")
    # klein accent: groene "aan"-stip rechtsonder in het scherm
    r = size * k * 0.025
    sx, sy = P(0.80, 0.60)
    d.ellipse([sx - r, sy - r, sx + r, sy + r], fill=(242, 179, 61, 255))
    return icon


icon = artwork(S)
icon.resize((256, 256), Image.LANCZOS).save("ppkiosk-256.png")
icon.resize((512, 512), Image.LANCZOS).save("web-512.png")
icon.resize((192, 192), Image.LANCZOS).save("web-192.png")
artwork(S, full_bleed=True).resize((512, 512), Image.LANCZOS).save("web-maskable-512.png")
sizes = [(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)]
icon.save("ppkiosk.ico", sizes=sizes)
# voorbeeld van kleine formaten naast elkaar
prev = Image.new("RGBA", (16 + 24 + 32 + 48 + 64 + 128 + 256 + 8 * 20, 276), (40, 40, 44, 255))
x = 10
for s in [16, 24, 32, 48, 64, 128, 256]:
    prev.alpha_composite(icon.resize((s, s), Image.LANCZOS), (x, 266 - s))
    x += s + 20
prev.convert("RGB").save("preview.png")
print("ok")

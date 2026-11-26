# Pictogram PP Kiosk: afspeelknop (kijken) met geluidsgolven (luisteren)
# op een afgeronde vierkant met verloop. Getekend op 1024 px, daarna verkleind.
import math, sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024
def gradient(size, c1, c2):
    img = Image.new("RGBA", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (x * 0.45 + y * 0.55) / size
            px[x, y] = tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3)) + (255,)
    return img

bg = gradient(S, (255, 122, 69), (184, 42, 120))   # oranje naar framboos
mask = Image.new("L", (S, S), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.23), fill=255)
icon = Image.new("RGBA", (S, S), (0, 0, 0, 0))
icon.paste(bg, (0, 0), mask)

# zachte schaduw onder de vormen
shadow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(shadow)
fg = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(fg)
white = (255, 255, 255, 255)

# afspeeldriehoek, iets links van het midden, met afgeronde hoeken
cx, cy = S * 0.355, S * 0.50
h = S * 0.46
pts = [(cx - h * 0.38, cy - h / 2), (cx - h * 0.38, cy + h / 2), (cx + h * 0.52, cy)]
def rounded_triangle(draw, pts, r, fill):
    draw.polygon(pts, fill=fill)
    for p in pts:
        draw.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=fill)
# driehoek een fractie verkleinen zodat de afronding binnen de vorm valt
r = S * 0.045
cxm = sum(p[0] for p in pts) / 3; cym = sum(p[1] for p in pts) / 3
inner = [(cxm + (p[0] - cxm) * 0.86, cym + (p[1] - cym) * 0.86) for p in pts]
d.polygon(pts, fill=white)
d.line(pts + [pts[0]], fill=white, width=int(r * 2), joint="curve")
for p in pts:
    d.ellipse([p[0] - r, p[1] - r, p[0] + r, p[1] + r], fill=white)

# geluidsgolven rechts
ox, oy = S * 0.40, S * 0.50
w = int(S * 0.055)
for i, rad in enumerate([S * 0.30, S * 0.42]):
    box = [ox - rad, oy - rad, ox + rad, oy + rad]
    d.arc(box, start=-38, end=38, fill=white, width=w)
    # ronde uiteinden
    for ang in (-38, 38):
        a = math.radians(ang)
        # PIL tekent de lijndikte naar binnen: midden van de lijn ligt op rad - w/2
        rm = rad - w / 2
        ex, ey = ox + rm * math.cos(a), oy + rm * math.sin(a)
        rr = w / 2
        d.ellipse([ex - rr, ey - rr, ex + rr, ey + rr], fill=white)

alpha = fg.split()[3]
sd.bitmap((0, int(S * 0.012)), alpha, fill=(90, 0, 40, 110))
shadow = shadow.filter(ImageFilter.GaussianBlur(S * 0.012))
icon = Image.alpha_composite(icon, Image.composite(shadow, Image.new("RGBA", (S, S)), mask))
icon = Image.alpha_composite(icon, fg)

icon.save("kijkluister-1024.png")
icon.resize((512, 512), Image.LANCZOS).save("kijkluister-512.png")
icon.resize((256, 256), Image.LANCZOS).save("ppkiosk-256.png")
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

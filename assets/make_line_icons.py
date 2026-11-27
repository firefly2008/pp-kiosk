# Lijn-pictogrammen voor het volwassen thema: witte lijnen op een doorzichtige achtergrond.
# Getekend op 512 px en verkleind naar 160 px. Gebruik: python make_line_icons.py (vanuit de map assets)
import math
from PIL import Image, ImageDraw

S = 512
W = 26                     # lijndikte
C = (255, 255, 255, 255)


def new():
    im = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def save(im, name):
    im.resize((160, 160), Image.LANCZOS).save("icons/" + name + ".png")


# Presentaties: lijst met berg en zon
im, d = new()
d.rounded_rectangle([60, 100, 452, 412], radius=24, outline=C, width=W)
d.line([(100, 372), (210, 240), (290, 320), (340, 270), (412, 372)], fill=C, width=W, joint="curve")
d.ellipse([320, 150, 380, 210], outline=C, width=W)
save(im, "presentaties")

# Kijk & luister: koptelefoon
im, d = new()
d.arc([96, 90, 416, 410], 180, 360, fill=C, width=W)
d.line([(109, 250), (109, 330)], fill=C, width=W)
d.line([(403, 250), (403, 330)], fill=C, width=W)
d.rounded_rectangle([90, 270, 170, 420], radius=28, outline=C, width=W)
d.rounded_rectangle([342, 270, 422, 420], radius=28, outline=C, width=W)
save(im, "stream")

# Snelkoppelingen: ster
im, d = new()
pts = []
for i in range(10):
    a = -math.pi / 2 + i * math.pi / 5
    r = 190 if i % 2 == 0 else 82
    pts.append((256 + r * math.cos(a), 270 + r * math.sin(a)))
d.line(pts + [pts[0], pts[1]], fill=C, width=W, joint="curve")
save(im, "snelkoppelingen")

# Spelletjes: vier kaartjes in een raster
im, d = new()
for x, y in [(80, 80), (276, 80), (80, 276), (276, 276)]:
    d.rounded_rectangle([x, y, x + 156, y + 156], radius=22, outline=C, width=W)
d.ellipse([318, 318, 390, 390], fill=C)
save(im, "spelletjes")

# Muziek: noot
im, d = new()
d.line([(220, 360), (220, 110), (400, 80), (400, 320)], fill=C, width=W, joint="curve")
d.line([(220, 170), (400, 140)], fill=C, width=W)
d.ellipse([130, 320, 230, 410], fill=C)
d.ellipse([310, 290, 410, 380], fill=C)
save(im, "muziek")
print("ok")

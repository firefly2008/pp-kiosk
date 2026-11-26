# Zachte, gloeiende kleurvlekken voor het Glas-thema (donkere achtergrond met lichtgevende accenten).
# Vooraf getekend en vervaagd, zodat de kiosk zelf geen zware blur hoeft te doen.
# Gebruik: python make_glow.py (vanuit de map assets)
from PIL import Image, ImageDraw, ImageFilter

S = 512
COLORS = {"violet": (139, 92, 246), "blauw": (59, 130, 246), "turquoise": (20, 184, 166), "roze": (236, 72, 153)}
for name, rgb in COLORS.items():
    im = Image.new("RGBA", (S, S), rgb + (0,))
    a = Image.new("L", (S, S), 0)
    ImageDraw.Draw(a).ellipse([S * 0.2, S * 0.2, S * 0.8, S * 0.8], fill=200)
    a = a.filter(ImageFilter.GaussianBlur(S * 0.11))
    im.putalpha(a)
    im.save("glass/" + name + ".png")
print("ok")

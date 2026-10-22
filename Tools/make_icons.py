#!/usr/bin/env python3
"""Draw Resources/icon_60x60.png, icon_30x30.png and icon.png (store) in the
UNA launcher style: a flat teal glyph on transparency. The glyph is a peak
with a ski track carved down its face."""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TEAL = (0, 128, 128, 255)
CLEAR = (0, 0, 0, 0)


def icon(size):
    s = 16                      # supersample
    N = size * s
    im = Image.new("RGBA", (N, N), CLEAR)
    d = ImageDraw.Draw(im)
    # A smaller peak behind, then the main one.
    d.polygon([(N * 0.46, N * 0.86), (N * 0.72, N * 0.36), (N * 0.98, N * 0.86)], fill=TEAL)
    d.polygon([(N * 0.02, N * 0.86), (N * 0.42, N * 0.12), (N * 0.82, N * 0.86)], fill=TEAL)
    # Separate the peaks with a thin gap along the front one's right flank.
    d.line([(N * 0.42, N * 0.12), (N * 0.82, N * 0.86)], fill=CLEAR, width=int(N * 0.035))
    d.polygon([(N * 0.02, N * 0.86), (N * 0.42, N * 0.12), (N * 0.80, N * 0.86)], fill=TEAL)
    # The track: a zigzag of turns from the summit to the foot.
    pts = [(N * 0.42, N * 0.24), (N * 0.33, N * 0.38), (N * 0.47, N * 0.50),
           (N * 0.30, N * 0.63), (N * 0.48, N * 0.76), (N * 0.38, N * 0.88)]
    d.line(pts, fill=CLEAR, width=int(N * 0.06), joint="curve")
    return im.resize((size, size), Image.LANCZOS)


out = os.path.join(ROOT, "Resources")
os.makedirs(out, exist_ok=True)
for sz in (60, 30):
    icon(sz).save(os.path.join(out, f"icon_{sz}x{sz}.png"))
icon(256).save(os.path.join(out, "icon.png"))   # store listing
print("icons written to", out)

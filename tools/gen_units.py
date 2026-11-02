# ==============================================================================
# tools/gen_units.py
#
# Generates the placeholder unit sprites: the "figure" unit, a white
# rectangle with a black border, an arrow showing the way it faces, and a
# label under the arrow naming the animation and frame (I1..I8 idle, W1..W8
# walk, A1..A8 attack, D1..D8 death).
#
#   data/base/graphics/entity/unit/figure/<animation>_mask.png
#
# Each is a mask: the white fill takes the color the game tints it with
# (Sprite::tint), and black stays black, so one set of sprites serves every
# unit (blue warriors, red undead, the white hero, ...).
#
# A sheet has a column per frame and a row per direction, all 8 stored so the
# labels never read mirrored: down, down-right, right, up-right, up,
# up-left, left, down-left (counter-clockwise on screen, as heading_index
# counts). Frames are 64x128 px with the feet at (32, 96);
# data/base/configs/entity/units.json names them.
#
#   python3 tools/gen_units.py                    # write data/base/graphics/entity/unit/figure
#   python3 tools/gen_units.py --out somewhere    # somewhere else, for a look
#
# Needs Pillow. Deterministic.
# ==============================================================================

import argparse
import math
import os
from PIL import Image, ImageDraw, ImageFont

W, H       = 64, 128  # frame (px)
BORDER     = 8        # black edge around the white fill
ARROW_C    = (32, 32) # arrow center
LABEL_Y    = 70       # label center
S          = 4        # supersampling for the arrow
DIRECTIONS = 8
FRAMES     = 8
ANIMS      = { "idle": "I", "walk": "W", "attack": "A", "death": "D" }

def arrow(draw, d):
    """An arrow pointing the way of direction d (0 down, counter-clockwise),
    drawn at S x scale."""
    a = d * math.pi / 4                  # 0 = down the screen
    fx, fy = math.sin(a), math.cos(a)    # forward, y down
    rx, ry = fy, -fx                     # across
    cx, cy = ARROW_C
    def p(f, r):
        return ((cx + fx * f + rx * r) * S, (cy + fy * f + ry * r) * S)
    # Shaft 8 px wide from -20 to +4, head 32 px wide from +4 to +20
    draw.polygon([p(-20, -4), p(4, -4), p(4, -16), p(20, 0), p(4, 16), p(4, 4), p(-20, 4)], fill=0)

def frame(d, label, font):
    img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    ImageDraw.Draw(img).rectangle([BORDER, BORDER, W - BORDER - 1, H - BORDER - 1], fill=(255, 255, 255, 255))

    # The arrow, supersampled and thresholded so its edges stay hard
    m = Image.new("L", (W * S, H * S), 255)
    arrow(ImageDraw.Draw(m), d)
    m = m.resize((W, H), Image.BOX).point(lambda v: 0 if v < 128 else 255)
    black = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    img = Image.composite(img, black, m)

    ImageDraw.Draw(img).text((W / 2, LABEL_Y), label, fill=(0, 0, 0, 255), font=font, anchor="mm")
    return img

def main():
    ap = argparse.ArgumentParser(description="Generate the placeholder unit masks")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                  "..", "data", "base", "graphics", "entity", "unit", "figure"))
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    font = ImageFont.load_default(size=22)   # Pillow's bundled font
    for anim, letter in ANIMS.items():
        sheet = Image.new("RGBA", (W * FRAMES, H * DIRECTIONS), (0, 0, 0, 0))
        for d in range(DIRECTIONS):
            for f in range(FRAMES):
                sheet.paste(frame(d, f"{letter}{f + 1}", font), (f * W, d * H))
        path = os.path.join(args.out, f"{anim}_mask.png")
        sheet.save(path, optimize=True)
        print("wrote", os.path.normpath(path))

if __name__ == "__main__":
    main()

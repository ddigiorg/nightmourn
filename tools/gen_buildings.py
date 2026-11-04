# ==============================================================================
# tools/gen_buildings.py
#
# Generates the placeholder building sprites, in the style of the unit
# figures (gen_units.py): a mask with a white fill that takes the owner's
# color (Sprite::tint), a black outline, and the building's name in black.
#
#   data/base/graphics/entity/building/banner_mask.png   64 x 256   a pole and flag
#   data/base/graphics/entity/building/wall_mask.png     16 x (128 x 128)
#   data/base/graphics/entity/building/house_mask.png    256 x 256
#   data/base/graphics/entity/building/crafter_mask.png  384 x 384
#   data/base/graphics/gui/item/<name>.png               72 x 72 item icons
#
# Art is 128 px per tile (SPRITE_PIXELS_PER_TILE), so a house's 2 x 2 tile
# footprint is 256 px square. The game draws a building with the image's
# bottom-center on its footprint's bottom-center.
#
# The wall sheet has a frame per set of connected sides, the frame index
# being the side bits (north 1, east 2, south 4, west 8, as
# Building::connections): a square post with an arm out to each connected
# side, so a row of walls reads as one wall.
#
# data/base/configs/entity/buildings.json names them.
#
#   python3 tools/gen_buildings.py                 # write into data/base/graphics
#   python3 tools/gen_buildings.py --out somewhere # somewhere else, for a look
#
# Needs Pillow. Deterministic.
# ==============================================================================

import argparse
import os
from PIL import Image, ImageDraw, ImageFont

TILE   = 128 # px per tile
BORDER = 8   # black edge around the white fill, as the figures'
BLACK  = (0, 0, 0, 255)
WHITE  = (255, 255, 255, 255)
CLEAR  = (0, 0, 0, 0)

def outlined_box(draw, box):
    """A white box with a black BORDER inside its edge."""
    x0, y0, x1, y1 = box
    draw.rectangle([x0, y0, x1 - 1, y1 - 1], fill=BLACK)
    draw.rectangle([x0 + BORDER, y0 + BORDER, x1 - 1 - BORDER, y1 - 1 - BORDER], fill=WHITE)

def label(draw, center, text, size):
    font = ImageFont.load_default(size=size)
    draw.text(center, text, fill=BLACK, font=font, anchor="mm")

# ------------------------------------------------------------------------------
# Buildings
# ------------------------------------------------------------------------------

def gen_box(name, tiles, font_size):
    w = h = tiles * TILE
    img = Image.new("RGBA", (w, h), CLEAR)
    d = ImageDraw.Draw(img)
    outlined_box(d, (0, 0, w, h))
    # An inner line, a hint of a roof edge, so it doesn't read as a hole
    inset = 24 * tiles // 2
    d.rectangle([inset, inset, w - 1 - inset, h - 1 - inset], outline=BLACK, width=4)
    label(d, (w / 2, h / 2), name, font_size)
    return img

def gen_banner():
    """A 0.5 x 2 tile pole with a flag: the pole's foot is the image's
    bottom-center."""
    w, h = 64, 256
    img = Image.new("RGBA", (w, h), CLEAR)
    d = ImageDraw.Draw(img)
    # Pole: white with a black edge, full height
    d.rectangle([24, 0, 39, h - 1], fill=BLACK)
    d.rectangle([28, 4, 35, h - 5], fill=WHITE)
    # Flag hanging from the top, a swallowtail at its foot, the word down it
    flag = [(0, 8), (w - 1, 8), (w - 1, 172), (w / 2, 148), (0, 172)]
    d.polygon(flag, fill=BLACK)
    inner = [(BORDER, 8 + BORDER), (w - 1 - BORDER, 8 + BORDER), (w - 1 - BORDER, 158),
             (w / 2, 138), (BORDER, 158)]
    d.polygon(inner, fill=WHITE)
    font = ImageFont.load_default(size=20)
    for i, ch in enumerate("BANNER"):
        d.text((w / 2, 30 + i * 20), ch, fill=BLACK, font=font, anchor="mm")
    return img

def gen_wall_frame(bits, font):
    """One wall tile: a post, with an arm to each side in `bits`."""
    s = TILE
    POST = 20 # inset of the post and arms from the tile edge
    img = Image.new("RGBA", (s, s), CLEAR)
    d = ImageDraw.Draw(img)
    # Image y runs down, so north (+y in the world) is the top
    boxes = [(POST, POST, s - POST, s - POST)]
    if bits & 1: boxes.append((POST, 0, s - POST, s - POST))   # north
    if bits & 2: boxes.append((POST, POST, s, s - POST))       # east
    if bits & 4: boxes.append((POST, POST, s - POST, s))       # south
    if bits & 8: boxes.append((0, POST, s - POST, s - POST))   # west
    for b in boxes: d.rectangle([b[0], b[1], b[2] - 1, b[3] - 1], fill=BLACK)
    for b in boxes:
        # Shrink by the border, except where the box runs off the tile edge
        x0 = b[0] + (BORDER if b[0] > 0 else 0)
        y0 = b[1] + (BORDER if b[1] > 0 else 0)
        x1 = b[2] - (BORDER if b[2] < s else 0)
        y1 = b[3] - (BORDER if b[3] < s else 0)
        d.rectangle([x0, y0, x1 - 1, y1 - 1], fill=WHITE)
    d.text((s / 2, s / 2), "wall", fill=BLACK, font=font, anchor="mm")
    return img

def gen_wall():
    font = ImageFont.load_default(size=22)
    sheet = Image.new("RGBA", (TILE * 16, TILE), CLEAR)
    for bits in range(16):
        sheet.paste(gen_wall_frame(bits, font), (bits * TILE, 0))
    return sheet

# ------------------------------------------------------------------------------
# Item icons: the building's mask, tinted the player's blue, on a dark tile
# like the other item icons (gen_ui.py)
# ------------------------------------------------------------------------------

ICON      = 72
ICON_TINT = (110, 160, 235)

def gen_icon(mask):
    bg = Image.new("RGBA", (ICON, ICON), (20, 26, 36, 255))
    d = ImageDraw.Draw(bg)
    for i in range(ICON // 2):
        a = int(18 * (1 - i / (ICON / 2)))
        d.rectangle([i, i, ICON - 1 - i, ICON - 1 - i], outline=(20 + a, 26 + a, 36 + a, 255))
    m = mask.copy()
    m.thumbnail((ICON - 12, ICON - 12), Image.LANCZOS)
    r, g, b, a = m.split()
    tinted = Image.merge("RGBA", (
        r.point(lambda v: v * ICON_TINT[0] // 255),
        g.point(lambda v: v * ICON_TINT[1] // 255),
        b.point(lambda v: v * ICON_TINT[2] // 255),
        a))
    bg.alpha_composite(tinted, ((ICON - m.width) // 2, (ICON - m.height) // 2))
    # Bevel, lighter top-left and darker bottom-right
    d = ImageDraw.Draw(bg)
    d.line([(0, ICON - 1), (0, 0), (ICON - 1, 0)], fill=(90, 100, 115, 255), width=2)
    d.line([(1, ICON - 1), (ICON - 1, ICON - 1), (ICON - 1, 1)], fill=(8, 10, 14, 255), width=2)
    return bg

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "base", "graphics")
    ap = argparse.ArgumentParser(description="Generate the placeholder building masks and item icons")
    ap.add_argument("--out", default=root, help="graphics folder to write entity/building and gui/item into")
    args = ap.parse_args()

    banner  = gen_banner()
    wall    = gen_wall()
    house   = gen_box("house", 2, 44)
    crafter = gen_box("crafter", 3, 56)

    bdir = os.path.join(args.out, "entity", "building")
    idir = os.path.join(args.out, "gui", "item")
    os.makedirs(bdir, exist_ok=True)
    os.makedirs(idir, exist_ok=True)
    for name, img in [("banner", banner), ("wall", wall), ("house", house), ("crafter", crafter)]:
        path = os.path.join(bdir, f"{name}_mask.png")
        img.save(path, optimize=True)
        print("wrote", os.path.normpath(path))

    # The wall's icon is a straight east-west piece
    icons = {
        "banner":  banner,
        "wall":    wall.crop((10 * TILE, 0, 11 * TILE, TILE)),
        "house":   house,
        "crafter": crafter,
    }
    for name, img in icons.items():
        path = os.path.join(idir, f"{name}.png")
        gen_icon(img).save(path, optimize=True)
        print("wrote", os.path.normpath(path))

if __name__ == "__main__":
    main()

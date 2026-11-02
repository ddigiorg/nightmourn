# ==============================================================================
# tools/pack_sprites.py
#
# Packs loose sprite images into texture pages and writes the manifest Nezumi's
# SpriteSheet loads (format v1, see Nezumi/Render/2D/Sprite/SpriteSheet.hpp).
#
#   python3 tools/pack_sprites.py                      # assets-src/sprites -> assets/graphics/sprites*
#   python3 tools/pack_sprites.py SRC OUT_DIR --name sprites --page-size 1024 --mip-levels 4
#   python3 tools/pack_sprites.py --format png         # plain PNG pages, to look at
#
# or `cmake --build <build dir> --target pack-sprites`. Needs numpy, Pillow and
# etcpak. The output is committed, so building the game needs none of them.
#
# Pages are written as KTX2 (tools/ktx2.py): BC7, with mip_levels premultiplied
# mips baked in, so the game uploads them as they are: 1 byte per pixel in VRAM
# instead of 4, and no PNG decode or mip generation at startup.
#
# Source layout: one folder per object, holding its frames as PNGs and a
# sprite.json describing them. Names are the folder's path under SRC:
#
#   units/red/sprite.json
#   units/red/down.png, down-right.png, ...
#
#   {
#       "pivot": [32, 96],                 // default pivot, px in the frame, y down
#       "animations": [                    // -> animation "units/red/idle"
#           { "name": "idle", "directions": 5, "headings": 8, "frames": 1,
#             "fps": 0, "loop": true,
#             "files": ["down", "down-right", "right", "up-right", "up"] }
#       ],
#       "variants": [                      // -> animation "trees/oak/looks":
#           { "name": "looks", "files": ["01", "02"] }   // one frame per look, fps 0
#       ],
#       "sprites": [                       // static sprites (default: every PNG
#           { "file": "icon", "pivot": [8, 8] }          // no animation uses)
#       ]
#   }
#
# `files` are listed direction-major: every frame of direction 0, then of
# direction 1, ... Each file becomes the sprite "<folder>/<file>". With
# mirrored headings (headings > directions), store the directions from down
# counter-clockwise to up: down, down-right, right, up-right, up; the left
# ones are drawn mirrored.
#
# Each frame is trimmed to its opaque pixels (the manifest keeps the trimmed
# box's offset in the frame, so pivots stay put), identical frames are stored
# once, and frames are shelf-packed onto pages of page_size, each placed on a
# 2^(mip_levels - 1) px grid with at least that much transparent space around
# it, so no mip level bleeds one sprite into another.
# ==============================================================================

import argparse
import hashlib
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ktx2

def fail(msg):
    sys.exit(f"pack_sprites: {msg}")

def round_up(x, a):
    return (x + a - 1) // a * a

# ------------------------------------------------------------------------------
# Sources
# ------------------------------------------------------------------------------

def load_sources(src):
    """Sprites (name, image, pivot) in manifest order, and animations whose
    'first' indexes those sprites."""
    sprites, animations = [], []

    for meta_path in sorted(src.rglob("sprite.json")):
        folder = meta_path.parent
        prefix = folder.relative_to(src).as_posix()
        meta   = json.loads(meta_path.read_text())
        pivot  = meta.get("pivot")
        used   = set()

        def add(file, pivot_override=None):
            path = folder / f"{file}.png"
            if not path.exists(): fail(f"{path} not found")
            img = Image.open(path).convert("RGBA")
            p = pivot_override or pivot or [img.width / 2, img.height]
            sprites.append({ "name": f"{prefix}/{file}", "image": img, "pivot": p })
            used.add(file)

        def add_animation(a, defaults):
            files = a["files"]
            first = len(sprites)
            for f in files: add(f, a.get("pivot"))
            anim = { "name": f"{prefix}/{a['name']}", "first": first, **defaults }
            for k in ("frames", "directions", "headings", "fps", "loop"):
                if k in a: anim[k] = a[k]
            anim.setdefault("frames", len(files) // anim.get("directions", 1))
            if anim["frames"] * anim.get("directions", 1) != len(files):
                fail(f"{meta_path}: '{a['name']}' lists {len(files)} files, not frames x directions")
            animations.append(anim)

        for a in meta.get("animations", []):
            add_animation(a, {})
        for v in meta.get("variants", []):
            add_animation({ **v, "frames": len(v["files"]) }, { "directions": 1, "fps": 0, "loop": False })

        if "sprites" in meta:
            for s in meta["sprites"]: add(s["file"], s.get("pivot"))
        else:
            for png in sorted(folder.glob("*.png")):
                if png.stem not in used: add(png.stem)

    return sprites, animations

# ------------------------------------------------------------------------------
# Packing
# ------------------------------------------------------------------------------

def pack(sprites, page_size, grid):
    """Trims, dedupes and places every sprite. Returns the pages."""
    # Trim, then dedupe identical trimmed pixels
    unique = {} # hash -> placement dict shared by duplicates
    for s in sprites:
        img  = s["image"]
        bbox = img.getchannel("A").getbbox() or (0, 0, 1, 1) # keep fully transparent frames
        cut  = img.crop(bbox)
        key  = hashlib.sha1(cut.tobytes() + bytes(str(cut.size), "ascii")).hexdigest()
        s["offset"] = [bbox[0], bbox[1]]
        s["slot"]   = unique.setdefault(key, { "image": cut })

    # Shelf packing, tallest first. A slot's cell is its size rounded up to the
    # grid, plus one grid of gap; the first row and column start one grid in.
    slots = sorted(unique.values(), key=lambda u: (-u["image"].height, -u["image"].width))
    pages, page, x, y, shelf = [], None, grid, grid, 0
    for u in slots:
        w, h = round_up(u["image"].width, grid), round_up(u["image"].height, grid)
        if w + 2 * grid > page_size or h + 2 * grid > page_size:
            fail(f"a {u['image'].width}x{u['image'].height} sprite doesn't fit a {page_size} px page")
        if page is not None and x + w + grid > page_size:
            x, y, shelf = grid, y + shelf + grid, 0
        if page is None or y + h + grid > page_size:
            page = Image.new("RGBA", (page_size, page_size), (0, 0, 0, 0))
            pages.append(page)
            x, y, shelf = grid, grid, 0
        page.paste(u["image"], (x, y))
        u["page"] = len(pages) - 1
        u["box"]  = [x, y, x + u["image"].width, y + u["image"].height]
        x += w + grid
        shelf = max(shelf, h)
    return pages

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Pack loose sprites into a SpriteSheet.")
    parser.add_argument("src", nargs="?", default=root / "assets-src/sprites", type=Path)
    parser.add_argument("out", nargs="?", default=root / "assets/graphics", type=Path)
    parser.add_argument("--name",       default="sprites", help="manifest <name>.json, pages <name>-<n>.png")
    parser.add_argument("--page-size",  default=1024, type=int)
    parser.add_argument("--mip-levels", default=4,    type=int, help="mip levels in each page (KTX2) or the game creates (PNG)")
    parser.add_argument("--format",     default="bc7", choices=["bc7", "rgba8", "png"],
                        help="KTX2 page format, or png for plain images")
    args = parser.parse_args()

    sprites, animations = load_sources(args.src)
    if not sprites: fail(f"no sprites under {args.src}")
    names = [s["name"] for s in sprites]
    if len(set(names)) != len(names): fail("duplicate sprite names")

    grid  = 2 ** (args.mip_levels - 1)
    pages = pack(sprites, args.page_size, grid)

    args.out.mkdir(parents=True, exist_ok=True)
    # Pages from an earlier pack (any format) would be left behind
    for old in list(args.out.glob(f"{args.name}-*.png")) + list(args.out.glob(f"{args.name}-*.ktx2")):
        old.unlink()

    page_files = []
    for i, page in enumerate(pages):
        if args.format == "png":
            page_files.append(f"{args.name}-{i}.png")
            page.save(args.out / page_files[-1], optimize=True)
        else:
            page_files.append(f"{args.name}-{i}.ktx2")
            ktx2.save(np.asarray(page), args.out / page_files[-1], args.mip_levels, args.format)

    manifest = {
        "version":   1,
        "page_size": [args.page_size, args.page_size],
        "pages":     page_files,
        "sprites": [
            { "name": s["name"], "page": s["slot"]["page"], "box": s["slot"]["box"],
              "offset": s["offset"], "pivot": s["pivot"] }
            for s in sprites
        ],
        "animations": animations,
    }

    # One sprite or animation per line: diffs stay readable
    def rows(items):
        return ",\n".join("        " + json.dumps(i, separators=(", ", ": ")) for i in items)
    text = (
        "{\n"
        f'    "version": 1,\n'
        f'    "page_size": {json.dumps(manifest["page_size"])},\n'
        f'    "pages": {json.dumps(page_files)},\n'
        f'    "sprites": [\n{rows(manifest["sprites"])}\n    ],\n'
        f'    "animations": [\n{rows(animations)}\n    ]\n'
        "}\n"
    )
    (args.out / f"{args.name}.json").write_text(text)

    unique = len({ id(s["slot"]) for s in sprites })
    print(f"pack_sprites: {len(sprites)} sprites ({unique} unique), {len(animations)} animations, "
          f"{len(pages)} page(s) of {args.page_size} px -> {args.out / (args.name + '.json')}")

if __name__ == "__main__":
    main()

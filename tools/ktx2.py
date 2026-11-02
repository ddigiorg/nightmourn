# ==============================================================================
# tools/ktx2.py
#
# Writes KTX2 textures for Nezumi (Render/Core/Image/Ktx2.hpp): a premultiplied
# mip chain, block-compressed, in the file the engine uploads as is.
#
#   python3 tools/ktx2.py in.png out.ktx2 --levels 0 --format bc7   # full chain
#
# or, from another tool:
#
#   import ktx2
#   ktx2.save(rgba, "out.ktx2", levels=4, fmt="bc7")   # rgba: HxWx4 uint8, straight alpha
#
# Mips match Nezumi's own (Image.cpp): premultiply x * a / 255 rounded to
# nearest, then 2x2 box filters in premultiplied alpha. Formats:
#   bc7    RGBA, 1 byte per pixel (etcpak's BC7 encoder, linear weights)
#   bc4    one channel (the red one), 0.5 byte per pixel; for masks
#   rgba8  uncompressed, 4 bytes per pixel
#
# The data format descriptor flags RGBA data as premultiplied, which Nezumi
# requires of sprite sheet pages. No supercompression. Needs numpy, Pillow and
# etcpak (pip install etcpak).
# ==============================================================================

import argparse
import struct
import sys

import numpy as np

IDENTIFIER = b"\xabKTX 20\xbb\r\n\x1a\n"

# vkFormat, bytes per block, block size (px)
FORMATS = {
    "rgba8": (37,  4, 1),  # VK_FORMAT_R8G8B8A8_UNORM
    "bc4":   (139, 8, 4),  # VK_FORMAT_BC4_UNORM_BLOCK
    "bc7":   (145, 16, 4), # VK_FORMAT_BC7_UNORM_BLOCK
}

# ------------------------------------------------------------------------------
# Mips
# ------------------------------------------------------------------------------

def premultiply(rgba):
    """x * a / 255 rounded to nearest, exactly as Nezumi's premultiply_alpha."""
    a = rgba[..., 3:4].astype(np.uint32)
    t = rgba[..., :3].astype(np.uint32) * a + 128
    out = rgba.copy()
    out[..., :3] = ((t + (t >> 8)) >> 8).astype(np.uint8)
    return out

def half_size(img):
    """2x2 box filter, sizes floor-halved (never below 1), an odd last row or
    column folded into the last output one: Nezumi's half_size."""
    h, w = img.shape[:2]
    oh, ow = max(h // 2, 1), max(w // 2, 1)
    src = img.astype(np.uint32)

    def spans(n, on):
        out = []
        for i in range(on):
            a = min(2 * i, n - 1)
            b = n if i + 1 == on else min(2 * i + 2, n)
            out.append((a, max(b, a + 1)))
        return out

    if h % 2 == 0 and w % 2 == 0:
        s = src[0::2, 0::2] + src[1::2, 0::2] + src[0::2, 1::2] + src[1::2, 1::2]
        return ((s + 2) // 4).astype(np.uint8)

    out = np.zeros((oh, ow, img.shape[2]), np.uint8)
    for y, (y0, y1) in enumerate(spans(h, oh)):
        for x, (x0, x1) in enumerate(spans(w, ow)):
            block = src[y0:y1, x0:x1].reshape(-1, img.shape[2])
            n = block.shape[0]
            out[y, x] = (block.sum(axis=0) + n // 2) // n
    return out

def full_chain_levels(w, h):
    n, s = 1, max(w, h)
    while s > 1:
        s //= 2
        n += 1
    return n

def mip_chain(rgba, levels):
    """Premultiplied levels, largest first. levels = 0: the full chain."""
    h, w = rgba.shape[:2]
    full = full_chain_levels(w, h)
    levels = full if levels == 0 else min(levels, full)
    chain = [premultiply(rgba)]
    for _ in range(1, levels):
        chain.append(half_size(chain[-1]))
    return chain

# ------------------------------------------------------------------------------
# Encoding
# ------------------------------------------------------------------------------

def _pad_to_blocks(img, block):
    """Repeat the last row/column out to whole blocks (only the in-bounds
    pixels are ever sampled)."""
    h, w = img.shape[:2]
    ph, pw = -h % block, -w % block
    if ph or pw:
        img = np.pad(img, ((0, ph), (0, pw), (0, 0)), mode="edge")
    return np.ascontiguousarray(img)

def encode(level, fmt):
    import etcpak
    _, _, block = FORMATS[fmt]
    if fmt == "rgba8":
        return np.ascontiguousarray(level).tobytes()
    img = _pad_to_blocks(level, block)
    h, w = img.shape[:2]
    if fmt == "bc4":
        red = np.repeat(img[..., :1], 4, axis=2) # etcpak reads RGBA; BC4 takes red
        return etcpak.compress_bc4(np.ascontiguousarray(red).tobytes(), w, h)
    params = etcpak.BC7CompressBlockParams()
    params.init_linear_weights()   # premultiplied data: no perceptual weighting
    params.m_uber_level = 4
    params.m_max_partitions = 64
    return etcpak.compress_bc7(img.tobytes(), w, h, params)

# ------------------------------------------------------------------------------
# File
# ------------------------------------------------------------------------------

def _dfd(fmt, premultiplied):
    """Khronos basic data format descriptor (KTX2 requires one)."""
    vk, bytes_per_block, block = FORMATS[fmt]
    if fmt == "rgba8":
        model = 1 # KHR_DF_MODEL_RGBSDA
        samples = [(i * 8, 7, ch, 0, 255) for i, ch in enumerate((0, 1, 2, 15))] # R G B A
    else:
        model = 131 if fmt == "bc4" else 134 # KHR_DF_MODEL_BC4 / BC7
        samples = [(0, bytes_per_block * 8 - 1, 0, 0, 0xFFFFFFFF)]

    flags = 1 if (premultiplied and fmt != "bc4") else 0 # KHR_DF_FLAG_ALPHA_PREMULTIPLIED
    block_size = 24 + 16 * len(samples)
    body = struct.pack("<IHHBBBB", 0, 2, block_size, model, 1, 1, flags) # BT709, linear
    body += bytes([block - 1, block - 1, 0, 0])                           # texel block dimensions - 1
    body += bytes([bytes_per_block, 0, 0, 0, 0, 0, 0, 0])                 # bytesPlane0..7
    for offset, length, channel, lower, upper in samples:
        body += struct.pack("<HBB4BII", offset, length, channel, 0, 0, 0, 0, lower, upper)
    return struct.pack("<I", 4 + len(body)) + body

def write(path, fmt, width, height, level_data, premultiplied=True):
    """level_data: encoded bytes per level, largest first."""
    vk, bytes_per_block, _ = FORMATS[fmt]
    n = len(level_data)
    align = int(np.lcm(bytes_per_block, 4))

    dfd = _dfd(fmt, premultiplied)
    dfd_offset = 80 + 24 * n
    kvd = b""
    kvd_offset = dfd_offset + len(dfd)

    # Level data, smallest first as the spec recommends, each aligned
    offset = kvd_offset + len(kvd)
    placed = [None] * n
    blob = b""
    for m in reversed(range(n)):
        pad = -(offset + len(blob)) % align
        blob += b"\0" * pad
        placed[m] = (offset + len(blob), len(level_data[m]))
        blob += level_data[m]

    header = IDENTIFIER + struct.pack(
        "<9I", vk, 1, width, height, 0, 0, 1, n, 0) # typeSize 1, 2D, one face, no supercompression
    header += struct.pack("<4I2Q", dfd_offset, len(dfd), kvd_offset if kvd else 0, len(kvd), 0, 0)
    index = b"".join(struct.pack("<3Q", o, l, l) for o, l in placed)

    with open(path, "wb") as f:
        f.write(header + index + dfd + kvd + blob)

def save(rgba, path, levels=0, fmt="bc7"):
    """Straight-alpha HxWx4 uint8 -> premultiplied mip chain -> KTX2."""
    chain = mip_chain(rgba, levels)
    h, w = rgba.shape[:2]
    write(path, fmt, w, h, [encode(level, fmt) for level in chain])
    return len(chain)

# ------------------------------------------------------------------------------
# CLI
# ------------------------------------------------------------------------------

def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description="PNG -> premultiplied, mipmapped, compressed KTX2.")
    parser.add_argument("src")
    parser.add_argument("dst")
    parser.add_argument("--levels", type=int, default=0, help="mip levels; 0 = the full chain")
    parser.add_argument("--format", default="bc7", choices=sorted(FORMATS))
    args = parser.parse_args()

    rgba = np.asarray(Image.open(args.src).convert("RGBA"))
    n = save(rgba, args.dst, args.levels, args.format)
    print(f"ktx2: {args.src} -> {args.dst} ({args.format}, {n} levels)")

if __name__ == "__main__":
    sys.exit(main())

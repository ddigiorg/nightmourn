# ==============================================================================
# tools/figlib/render.py
#
# A tiny offline ray caster for pre-rendered figures (gen_figures.py). A
# figure is a list of solid primitives (spheres, boxes and round cones, each
# under an affine map, so ellipsoids, flattened blades and plates come free),
# cast with an orthographic camera looking down 30 degrees (the game's 2:1
# iso), the way the Age of Empires and Diablo II sprites were pre-rendered.
#
# Per frame: primary rays (depth, normal, primitive, its local point), one
# shadow ray per lit pixel toward the key light, a cheap depth-based ambient
# occlusion, shading (wrapped diffuse, a sky/ground ambient, metal
# reflections, a rim light, emission), a ground shadow cast from the feet,
# and an SS x SS box filter down to the frame.
#
# Model space: meters, y up, the feet on y = 0, the figure faces +z and its
# right hand is on -x. A stored direction turns the camera, not the model.
# ==============================================================================

import math
import numpy as np

np.seterr(invalid="ignore", divide="ignore", over="ignore")

# Frame layout (px) and scale
FW, FH  = 128, 160         # one frame
PIVOT   = (64.0, 128.0)    # the feet, px from the frame's top left
PPM     = 54.0             # px per meter, upright (a 1.75 m human is ~82 px tall)
SS      = 3                # supersampling
ELEV    = math.radians(30) # camera pitch: 2:1 dimetric

def nz(v):
    v = np.asarray(v, dtype=np.float64)
    n = np.linalg.norm(v)
    return v / n if n > 1e-12 else v

def rot_x(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]], dtype=np.float64)

def rot_y(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]], dtype=np.float64)

def rot_z(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]], dtype=np.float64)

def rot_axis(axis, a):
    k = nz(axis)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + math.sin(a) * K + (1 - math.cos(a)) * (K @ K)

def frame_y(y, zhint):
    """Rotation whose columns are x, y, z: y along `y`, z as close to `zhint`
    as it can be."""
    y = nz(y)
    z = np.asarray(zhint, dtype=np.float64)
    z = z - (z @ y) * y
    if np.linalg.norm(z) < 1e-6:
        z = np.array([0.0, 0.0, 1.0]) if abs(y[2]) < 0.9 else np.array([1.0, 0.0, 0.0])
        z = z - (z @ y) * y
    z = nz(z)
    x = np.cross(y, z)
    return np.column_stack([x, y, z])

def frame_z(z, yhint):
    """Rotation with z along `z` and y as close to `yhint` as it can be."""
    z = nz(z)
    y = np.asarray(yhint, dtype=np.float64)
    y = y - (y @ z) * z
    if np.linalg.norm(y) < 1e-6:
        y = np.array([0.0, 1.0, 0.0]) if abs(z[1]) < 0.9 else np.array([0.0, 0.0, 1.0])
        y = y - (y @ z) * z
    y = nz(y)
    x = np.cross(y, z)
    return np.column_stack([x, y, z])

def srgb(h):
    """'#rrggbb' -> linear rgb"""
    c = np.array([int(h[i:i + 2], 16) for i in (1, 3, 5)], dtype=np.float64) / 255.0
    return c ** 2.2

# ------------------------------------------------------------------------------
# Materials and primitives
# ------------------------------------------------------------------------------

class Mat:
    def __init__(self, color, metal=0.0, spec=0.15, power=18.0, emissive=None,
                 noise=0.18, nfreq=9.0, pattern=None, rim=1.0, shadow=True, glow=0.0):
        self.color    = srgb(color) if isinstance(color, str) else np.asarray(color, dtype=np.float64)
        self.metal    = metal
        self.spec     = spec
        self.power    = power
        self.emissive = srgb(emissive) if isinstance(emissive, str) else (np.zeros(3) if emissive is None else np.asarray(emissive))
        self.noise    = noise
        self.nfreq    = nfreq
        self.pattern  = pattern   # f(q (N,3) local point) -> linear rgb (N,3), or None
        self.rim      = rim
        self.shadow   = shadow    # casts shadows
        self.glow     = glow      # bloom strength of the emission

class Prim:
    """kind 'sphere' (unit), 'box' (half extents 1), or 'rcone' (round cone
    from the origin up y to (0, L, 0), radii ra, rb), mapped by M then o."""
    __slots__ = ("kind", "o", "M", "Minv", "MinvT", "params", "mat", "seed", "bc", "br")

    def __init__(self, kind, o, M, mat, params=()):
        self.kind   = kind
        self.o      = np.asarray(o, dtype=np.float64)
        self.M      = np.asarray(M, dtype=np.float64)
        self.params = params
        self.mat    = mat
        self.seed   = 0.0
        self.update()

    def update(self):
        self.Minv  = np.linalg.inv(self.M)
        self.MinvT = self.Minv.T
        if self.kind == "sphere":
            cc, cr = np.zeros(3), 1.0
        elif self.kind == "box":
            cc, cr = np.zeros(3), math.sqrt(3.0)
        else:
            L, ra, rb = self.params
            cc, cr = np.array([0.0, L * 0.5, 0.0]), L * 0.5 + max(ra, rb)
        self.bc = self.o + self.M @ cc
        self.br = cr * max(np.linalg.norm(self.M[:, i]) for i in range(3))

    def transformed(self, R, t):
        """x -> R x + t"""
        p = Prim(self.kind, R @ self.o + t, R @ self.M, self.mat, self.params)
        p.seed = self.seed
        return p

def sphere(P, c, R, radii, mat):
    P.append(Prim("sphere", c, np.asarray(R) @ np.diag(radii), mat))

def ball(P, c, r, mat):
    P.append(Prim("sphere", c, np.eye(3) * r, mat))

def box(P, c, R, half, mat):
    P.append(Prim("box", c, np.asarray(R) @ np.diag(half), mat))

def seg(P, a, b, ra, rb, mat, zhint=(0, 0, 1), fx=1.0, fz=1.0):
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    L = float(np.linalg.norm(b - a))
    if L < 1e-5:
        ball(P, a, max(ra, rb), mat)
        return
    R = frame_y(b - a, zhint)
    P.append(Prim("rcone", a, R @ np.diag([fx, 1.0, fz]), mat, (L, ra, rb)))

def plank(P, a, b, across, width, thick, mat, overlap=0.0):
    """A thin box from a to b, `width` wide along `across`."""
    a = np.asarray(a, dtype=np.float64)
    b = np.asarray(b, dtype=np.float64)
    y = b - a
    L = float(np.linalg.norm(y))
    if L < 1e-5:
        return
    z = np.cross(np.asarray(across, dtype=np.float64), y / L)
    R = frame_y(y, z)
    box(P, (a + b) * 0.5, R, (width * 0.5, L * 0.5 + overlap, thick * 0.5), mat)

# ------------------------------------------------------------------------------
# Ray intersection, vectorized over rays
# ------------------------------------------------------------------------------

def intersect(p, ro, rd, tmin=1e-4, any_hit=False):
    """ro (N,3), rd (N,3) or (3,): t (inf on a miss), world normal, local point."""
    o = (ro - p.o) @ p.MinvT
    d = np.broadcast_to(np.asarray(rd, dtype=np.float64) @ p.MinvT, o.shape)
    dl = np.linalg.norm(d, axis=1)
    dn = d / dl[:, None]
    n = len(o)

    if p.kind == "sphere":
        b = np.einsum("ij,ij->i", o, dn)
        c = np.einsum("ij,ij->i", o, o) - 1.0
        h = b * b - c
        ok = h > 0
        sq = np.sqrt(np.maximum(h, 0))
        tc = -b - sq
        if any_hit:
            tc = np.where(tc > tmin, tc, -b + sq)
        nrm = o + tc[:, None] * dn

    elif p.kind == "box":
        ds = np.where(np.abs(dn) < 1e-9, 1e-9, dn)
        t1 = (-1.0 - o) / ds
        t2 = ( 1.0 - o) / ds
        tlo = np.minimum(t1, t2)
        thi = np.maximum(t1, t2)
        tn = tlo.max(axis=1)
        tf = thi.min(axis=1)
        ok = tn < tf
        ax = tlo.argmax(axis=1)
        tc = tn
        if any_hit:
            tc = np.where(tn > tmin, tn, tf)
        nrm = np.zeros((n, 3))
        nrm[np.arange(n), ax] = -np.sign(ds[np.arange(n), ax])

    else:
        L, ra, rb = p.params
        ba = np.array([0.0, L, 0.0])
        oa = o
        ob = o - ba
        rr = ra - rb
        m0 = L * L
        m1 = oa[:, 1] * L
        m2 = dn[:, 1] * L
        m3 = np.einsum("ij,ij->i", dn, oa)
        m5 = np.einsum("ij,ij->i", oa, oa)
        m6 = np.einsum("ij,ij->i", ob, dn)
        m7 = np.einsum("ij,ij->i", ob, ob)
        d2 = m0 - rr * rr
        k2 = d2 - m2 * m2
        k1 = d2 * m3 - m1 * m2 + m2 * rr * ra
        k0 = d2 * m5 - m1 * m1 + m1 * rr * ra * 2.0 - m0 * ra * ra
        h = k1 * k1 - k0 * k2
        ok = h >= 0
        k2s = np.where(np.abs(k2) < 1e-12, 1e-12, k2)
        tb = (-np.sqrt(np.maximum(h, 0)) - k1) / k2s
        y = m1 - ra * rr + tb * m2
        body = ok & (y > 0) & (y < d2)
        nb = d2 * (oa + tb[:, None] * dn) - ba[None, :] * y[:, None]
        h1 = m3 * m3 - m5 + ra * ra
        h2 = m6 * m6 - m7 + rb * rb
        t1 = np.where(h1 > 0, -m3 - np.sqrt(np.maximum(h1, 0)), np.inf)
        t2 = np.where(h2 > 0, -m6 - np.sqrt(np.maximum(h2, 0)), np.inf)
        use2 = t2 < t1
        tcap = np.where(use2, t2, t1)
        ncap = np.where(use2[:, None], ob + t2[:, None] * dn, oa + t1[:, None] * dn)
        tc = np.where(body, tb, tcap)
        nrm = np.where(body[:, None], nb, ncap)
        ok = ok & np.isfinite(tc)

    t = tc / dl
    ok = ok & (t > tmin)
    t = np.where(ok, t, np.inf)
    tc = np.where(ok, tc, 0.0)
    nrm = np.where(ok[:, None], nrm, np.array([0.0, 0.0, 1.0]))
    q = o + tc[:, None] * dn
    nw = nrm @ p.Minv
    nw /= np.maximum(np.linalg.norm(nw, axis=1), 1e-12)[:, None]
    return t, nw, q

def occluded(prims, ro, rd, skip=None):
    """Any hit along each ray (ro (N,3), rd (3,))."""
    hit = np.zeros(len(ro), dtype=bool)
    rd = np.asarray(rd, dtype=np.float64)
    for i, p in enumerate(prims):
        if not p.mat.shadow:
            continue
        # Bounding sphere test first
        oc = ro - p.bc
        b = oc @ rd
        c = np.einsum("ij,ij->i", oc, oc) - p.br * p.br
        cand = (~hit) & (b * b - c > 0) & ((b < 0) | (c < 0))
        if not cand.any():
            continue
        idx = np.nonzero(cand)[0]
        t, _, _ = intersect(p, ro[idx], rd, tmin=2e-3, any_hit=True)
        hit[idx[np.isfinite(t)]] = True
    return hit

# ------------------------------------------------------------------------------
# Noise
# ------------------------------------------------------------------------------

def _hash3(ix, iy, iz):
    h = np.sin(ix * 127.1 + iy * 311.7 + iz * 74.7) * 43758.5453
    return h - np.floor(h)

def vnoise(p):
    i = np.floor(p)
    f = p - i
    u = f * f * (3 - 2 * f)
    x, y, z = i[:, 0], i[:, 1], i[:, 2]
    def h(dx, dy, dz):
        return _hash3(x + dx, y + dy, z + dz)
    a = h(0, 0, 0) * (1 - u[:, 0]) + h(1, 0, 0) * u[:, 0]
    b = h(0, 1, 0) * (1 - u[:, 0]) + h(1, 1, 0) * u[:, 0]
    c = h(0, 0, 1) * (1 - u[:, 0]) + h(1, 0, 1) * u[:, 0]
    d = h(0, 1, 1) * (1 - u[:, 0]) + h(1, 1, 1) * u[:, 0]
    e = a * (1 - u[:, 1]) + b * u[:, 1]
    g = c * (1 - u[:, 1]) + d * u[:, 1]
    return e * (1 - u[:, 2]) + g * u[:, 2]

def fbm(p, octaves=3):
    s, a, tot = 0.0, 0.5, 0.0
    for _ in range(octaves):
        s = s + a * vnoise(p)
        tot += a
        p = p * 2.03 + 17.1
        a *= 0.5
    return s / tot

# ------------------------------------------------------------------------------
# Lighting
# ------------------------------------------------------------------------------

# Key light in screen terms (right, up, toward the viewer), the trees' light:
# from the upper left and a little in front
KEY_SCREEN    = nz([-0.55, 0.65, 0.52])
# The ground shadow's light: high, from the upper left and behind, so the
# shadow falls short to the lower right as the trees' do
SHADOW_WORLD  = nz([-0.42, 1.55, -0.30])

SUN        = srgb("#fff1dc") * 2.1
SKY_AMB    = srgb("#5d6878") * 0.95
GROUND_AMB = srgb("#2a2620") * 0.7
ENV_SKY    = srgb("#c9d3dd")
ENV_HORZ   = srgb("#7d8790")
ENV_GROUND = srgb("#1d1b17")

def camera(yaw):
    """Screen right, screen up and toward-the-camera, in model space, for a
    figure turned `yaw` (0 faces the camera, pi/2 faces screen right)."""
    e = ELEV
    r0 = np.array([1.0, 0.0, 0.0])
    u0 = np.array([0.0, math.cos(e), -math.sin(e)])
    c0 = np.array([0.0, math.sin(e), math.cos(e)])
    R = rot_y(-yaw)
    return R @ r0, R @ u0, R @ c0

def box_blur(a, r):
    if r <= 0:
        return a
    k = 2 * r + 1
    pad = [(r, r), (r, r)] + [(0, 0)] * (a.ndim - 2)
    p = np.pad(a, pad, mode="constant")
    c = np.cumsum(np.cumsum(p, axis=0), axis=1)
    c = np.pad(c, [(1, 0), (1, 0)] + [(0, 0)] * (a.ndim - 2))
    s = c[k:, k:] - c[:-k, k:] - c[k:, :-k] + c[:-k, :-k]
    return s / (k * k)

def downsample(a, s):
    h, w = a.shape[0] // s, a.shape[1] // s
    return a.reshape(h, s, w, s, *a.shape[2:]).mean(axis=(1, 3))

def render(prims, yaw, rim_color, shadow_alpha=0.42):
    """One frame (FH, FW, 4) float straight-alpha sRGB in [0, 1]."""
    r, u, c = camera(yaw)
    L  = nz(KEY_SCREEN[0] * r + KEY_SCREEN[1] * u + KEY_SCREEN[2] * c)
    Ls = rot_y(-yaw) @ SHADOW_WORLD   # the shadow light turns with the camera
    W, H = FW * SS, FH * SS
    far = 30.0

    depth = np.full((H, W), np.inf)
    nrm   = np.zeros((H, W, 3))
    loc   = np.zeros((H, W, 3))
    pid   = np.full((H, W), -1, dtype=np.int32)

    def pix_rays(x0, x1, y0, y1):
        xs = ((np.arange(x0, x1) + 0.5) / SS - PIVOT[0]) / PPM
        ys = (PIVOT[1] - (np.arange(y0, y1) + 0.5) / SS) / PPM
        gx, gy = np.meshgrid(xs, ys)
        ro = gx[..., None] * r + gy[..., None] * u + far * c
        return ro.reshape(-1, 3)

    # Primary rays, each primitive over its bounding sphere's screen box
    for i, p in enumerate(prims):
        cx = (PIVOT[0] + (p.bc @ r) * PPM) * SS
        cy = (PIVOT[1] - (p.bc @ u) * PPM) * SS
        rad = p.br * PPM * SS + 2
        x0, x1 = max(0, int(cx - rad)), min(W, int(cx + rad) + 1)
        y0, y1 = max(0, int(cy - rad)), min(H, int(cy + rad) + 1)
        if x0 >= x1 or y0 >= y1:
            continue
        ro = pix_rays(x0, x1, y0, y1)
        t, n, q = intersect(p, ro, -c)
        t = t.reshape(y1 - y0, x1 - x0)
        sub = depth[y0:y1, x0:x1]
        upd = t < sub
        if not upd.any():
            continue
        sub[upd] = t[upd]
        nrm[y0:y1, x0:x1][upd] = n.reshape(y1 - y0, x1 - x0, 3)[upd]
        loc[y0:y1, x0:x1][upd] = q.reshape(y1 - y0, x1 - x0, 3)[upd]
        pid[y0:y1, x0:x1][upd] = i

    hit = np.isfinite(depth)
    rgb = np.zeros((H, W, 3))
    emis = np.zeros((H, W, 3))

    if hit.any():
        ys, xs = np.nonzero(hit)
        ro = pix_rays(0, W, 0, H).reshape(H, W, 3)[ys, xs]
        P  = ro - depth[ys, xs][:, None] * c
        N  = nrm[ys, xs]
        Q  = loc[ys, xs]
        ids = pid[ys, xs]

        col  = np.zeros((len(ids), 3))
        em   = np.zeros((len(ids), 3))
        met  = np.zeros(len(ids))
        spc  = np.zeros(len(ids))
        pw   = np.ones(len(ids))
        rimk = np.zeros(len(ids))
        for i in np.unique(ids):
            m = ids == i
            p = prims[i]
            mt = p.mat
            base = mt.pattern(Q[m]) if mt.pattern else np.broadcast_to(mt.color, (m.sum(), 3))
            if mt.noise > 0:
                nn = fbm(Q[m] * mt.nfreq + p.seed * 13.7, 3)
                base = base * (1.0 + mt.noise * (nn - 0.5) * 2.4)[:, None]
            col[m] = base
            em[m]  = mt.emissive
            met[m] = mt.metal
            spc[m] = mt.spec
            pw[m]  = mt.power
            rimk[m] = mt.rim

        # Self shadow toward the key light
        lit = ~occluded(prims, P + N * 0.006, L)
        sh  = np.where(lit, 1.0, 0.28)

        ndl  = N @ L
        diff = np.clip(ndl * 0.85 + 0.15, 0, 1) ** 1.3 * sh
        hemi = 0.5 + 0.5 * N[:, 1]
        amb  = GROUND_AMB[None] * (1 - hemi)[:, None] + SKY_AMB[None] * hemi[:, None]
        V    = c
        ndv  = np.clip(N @ V, 0, 1)
        Hh   = nz(L + V)
        spec = np.clip(N @ Hh, 0, 1) ** pw * spc * sh
        Rv   = 2 * ndv[:, None] * N - V[None]
        ry   = Rv[:, 1]
        env  = np.where((ry > 0)[:, None],
                        ENV_HORZ[None] * (1 - np.clip(ry * 1.6, 0, 1))[:, None] + ENV_SKY[None] * np.clip(ry * 1.6, 0, 1)[:, None],
                        ENV_HORZ[None] * np.clip(1 + ry * 4, 0, 1)[:, None] + ENV_GROUND[None] * np.clip(-ry * 4, 0, 1)[:, None])
        env  = env * (0.45 + 0.55 * sh)[:, None]

        dif_col = col * (SUN[None] * diff[:, None] + amb)
        met_col = col * (env * 1.25 + SUN[None] * diff[:, None] * 0.35)
        out = dif_col * (1 - met)[:, None] + met_col * met[:, None]
        out += SUN[None] * spec[:, None] * (0.6 + 0.4 * met)[:, None]
        fres = (1 - ndv) ** 3
        out += np.asarray(rim_color)[None] * (fres * rimk)[:, None]
        out += em
        rgb[ys, xs] = out
        emis[ys, xs] = em

        # Depth-based ambient occlusion: nearer neighbors darken a pixel
        dz = np.where(hit, depth, np.inf)
        occ = np.zeros((H, W))
        cnt = 0
        for rr in (2 * SS, 4 * SS):
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                sh_ = np.roll(np.roll(dz, dy * rr, axis=0), dx * rr, axis=1)
                d = depth - sh_
                occ += np.clip((d - 0.015) / 0.12, 0, 1) * np.isfinite(sh_)
                cnt += 1
        ao = 1.0 - 0.55 * np.clip(occ / cnt * 2.2, 0, 1)
        rgb *= ao[..., None]

        # Ink: a thin dark line where a part ends in front of another
        edge = np.zeros((H, W), dtype=bool)
        for dx, dy in ((1, 0), (0, 1), (-1, 0), (0, -1)):
            nd = np.roll(np.roll(dz, dy, axis=0), dx, axis=1)
            edge |= hit & np.isfinite(nd) & (depth - nd > 0.06)
        rgb[edge] *= 0.45

    # Ground shadow: rays from the ground up toward the shadow light
    sh_mask = np.zeros((H, W))
    gx = ((np.arange(W) + 0.5) / SS - PIVOT[0]) / PPM
    gy = (PIVOT[1] - (np.arange(H) + 0.5) / SS) / PPM
    GX, GY = np.meshgrid(gx, gy)
    ro = GX[..., None] * r + GY[..., None] * u + far * c
    tg = ro[..., 1] / c[1]
    G  = ro - tg[..., None] * c
    near = (np.abs(G[..., 0]) < 1.6) & (np.abs(G[..., 2]) < 1.6) & ~hit
    if near.any():
        ys, xs = np.nonzero(near)
        occ = occluded(prims, G[ys, xs] + np.array([0, 1e-3, 0]), Ls)
        sh_mask[ys[occ], xs[occ]] = 1.0

    # Down to the frame
    a   = downsample(hit.astype(np.float64), SS)
    col = downsample(rgb * hit[..., None], SS) / np.maximum(a, 1e-6)[..., None]
    shd = downsample(sh_mask, SS)
    shd = box_blur(box_blur(shd, 1), 1) * shadow_alpha
    em  = downsample(emis, SS)

    # Glow around emissive bits (eyes)
    glow = box_blur(box_blur(em, 2), 2) * 2.2
    glow_a = np.clip(glow.max(axis=2), 0, 1)

    col = np.clip(col, 0, None) ** (1 / 2.2)
    col = np.clip(col, 0, 1)
    gcol = np.clip(glow / np.maximum(glow_a, 1e-6)[..., None], 0, None) ** (1 / 2.2)
    gcol = np.clip(gcol, 0, 1)

    # Figure over glow over shadow (straight alpha)
    out_a = a + glow_a * (1 - a)
    out_c = (col * a[..., None] + gcol * (glow_a * (1 - a))[..., None])
    sa = shd * (1 - out_a)
    tot_a = out_a + sa
    out_c = out_c + np.array([0.02, 0.025, 0.02]) * sa[..., None]
    out_c = out_c / np.maximum(tot_a, 1e-6)[..., None]

    # Additive glow over the figure itself too
    out_c = np.clip(out_c + gcol * glow_a[..., None] * a[..., None] * 0.6, 0, 1)

    return np.nan_to_num(np.dstack([out_c, np.clip(tot_a, 0, 1)]))

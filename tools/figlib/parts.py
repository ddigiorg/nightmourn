# ==============================================================================
# tools/figlib/parts.py
#
# Pieces the figures are dressed from: limbs on the rig's joints, cloth
# strips, and the weapons (swords, shield, bow, quiver).
# ==============================================================================

import math
import numpy as np
from .render import nz, frame_y, frame_z, rot_x, rot_y, rot_z, rot_axis, ball, sphere, box, seg, plank
from .rig import v

# ------------------------------------------------------------------------------
# Body
# ------------------------------------------------------------------------------

def leg(P, J, side, thigh, shin, foot, r=(0.075, 0.062, 0.058, 0.044), foot_size=(0.048, 0.036, 0.105), knee=None, knee_r=0.06):
    hip, kn, an = (J.hip_r, J.kn_r, J.an_r) if side == "r" else (J.hip_l, J.kn_l, J.an_l)
    Rf = J.Rf_r if side == "r" else J.Rf_l
    fwd = J.Rp @ v(0, 0, 1)
    seg(P, hip, kn, r[0], r[1], thigh, zhint=fwd)
    seg(P, kn, an, r[2], r[3], shin, zhint=fwd)
    if knee is not None:
        ball(P, kn + fwd * 0.03, knee_r, knee)
    if foot is not None:
        fx, fy, fz = foot_size
        c = an + Rf @ v(0, -0.049, fz * 0.5)
        box(P, c, Rf, foot_size, foot)

def arm(P, J, side, upper, fore, hand, r=(0.055, 0.048, 0.047, 0.038), hand_r=0.044, elbow=None, elbow_r=0.05):
    sh, el, ha = (J.sh_r, J.el_r, J.ha_r) if side == "r" else (J.sh_l, J.el_l, J.ha_l)
    seg(P, sh, el, r[0], r[1], upper, zhint=J.fwd)
    seg(P, el, ha, r[2], r[3], fore, zhint=J.fwd)
    if elbow is not None:
        ball(P, el, elbow_r, elbow)
    if hand is not None:
        d = nz(ha - el)
        sphere(P, ha + d * 0.035, frame_y(d, J.fwd), (hand_r, hand_r * 1.2, hand_r * 0.9), hand)

def strip(P, top, across, width, length, hang, drag, mat, nseg=3, thick=0.014, flutter=0.0, phase=0.0):
    """Cloth hanging from `top`: segments bend from `hang` toward `drag`."""
    pts = [np.asarray(top, dtype=np.float64)]
    for k in range(nseg):
        f = (k + 1) / nseg
        wob = flutter * f * math.sin(phase + k * 1.3)
        d = nz(hang + drag * f + across * wob)
        pts.append(pts[-1] + d * (length / nseg))
    for a, b in zip(pts, pts[1:]):
        plank(P, a, b, across, width, thick, mat, overlap=0.01)
    return pts

def cape(P, J, mat, n=6, width=0.42, lengths=None, back=0.12, top_y=-0.02, drag_k=1.0, flutter=0.15, hang_back=0.25, thick=0.014):
    """Strips across the back between the shoulders."""
    p = J.p
    if lengths is None:
        lengths = [0.8] * n
    across = J.Rc @ v(1, 0, 0)
    back_dir = J.Rc @ v(0, 0, -1)
    hang = nz(v(0, -1, 0) + back_dir * hang_back)
    for i in range(n):
        f = (i + 0.5) / n - 0.5
        top = J.C(v(f * width, top_y, -back)) + v(0, -abs(f) * 0.06, 0)
        strip(P, top, across, width / n * 1.25, lengths[i], hang, p["drag"] * drag_k + back_dir * 0.1,
              mat, flutter=flutter, phase=p["sway"] * 2 * math.pi + i * 1.7, thick=thick)

def skirt(P, J, mat, n=6, radius=0.17, length=0.32, y=0.02, lengths=None, flare=0.35, thick=0.014, front_only=False):
    """Strips around the hips (tassets, tabard flaps, rags)."""
    p = J.p
    for i in range(n):
        a = 2 * math.pi * i / n
        if front_only and math.cos(a) < 0.2:
            continue
        out = J.Rp @ v(math.sin(a), 0, math.cos(a))
        across = J.Rp @ v(math.cos(a), 0, -math.sin(a))
        top = J.pelvis + v(0, y, 0) + out * radius
        L = lengths[i] if lengths else length
        hang = nz(v(0, -1, 0) + out * flare)
        strip(P, top, across, 2 * math.pi * radius / n * 1.15, L, hang, p["drag"] * 0.5, mat,
              nseg=2, thick=thick, flutter=0.1, phase=p["sway"] * 2 * math.pi + i)

# ------------------------------------------------------------------------------
# Weapons
# ------------------------------------------------------------------------------

def grip_point(J, side="r"):
    el, ha = (J.el_r, J.ha_r) if side == "r" else (J.el_l, J.ha_l)
    return ha + nz(ha - el) * 0.04

def sword(P, J, blade_len, blade_w, blade, guard, grip, pommel, guard_w=0.13, grip_len=0.11, tip_w=0.006, fuller=None, notches=None):
    g = grip_point(J, "r")
    w = J.wdir
    f = J.wflat
    R = frame_y(w, f)
    seg(P, g - w * grip_len * 0.6, g + w * grip_len * 0.5, 0.016, 0.016, grip, zhint=f)
    ball(P, g - w * (grip_len * 0.6 + 0.02), 0.028, pommel)
    box(P, g + w * (grip_len * 0.5 + 0.012), R, (guard_w, 0.014, 0.022), guard)
    base = g + w * (grip_len * 0.5 + 0.02)
    seg(P, base, base + w * blade_len, blade_w, tip_w, blade, zhint=f, fx=1.0, fz=0.2)
    if notches:
        # Dark bites out of a ruined edge
        for t, side in notches:
            c = base + w * blade_len * t + (R @ v(side, 0, 0)) * blade_w * (1 - t * 0.6)
            ball(P, c, blade_w * 0.45, notches_mat(blade))
    return base

_notch_cache = {}
def notches_mat(blade):
    from .render import Mat
    if id(blade) not in _notch_cache:
        _notch_cache[id(blade)] = Mat("#1c1612", noise=0.2, rim=0.0)
    return _notch_cache[id(blade)]

def shield(P, J, face_mat, rim_mat, boss_mat, radius=0.26):
    el, ha = J.el_l, J.ha_l
    mid = el * 0.4 + ha * 0.6
    n = J.sdir
    R = frame_z(n, J.up)
    c = mid + n * 0.05
    sphere(P, c, R, (radius, radius, 0.03), face_mat)
    sphere(P, c - n * 0.008, R, (radius * 1.06, radius * 1.06, 0.022), rim_mat)
    sphere(P, c + n * 0.022, R, (0.06, 0.06, 0.035), boss_mat)

def bow(P, J, limb, tip, string, arrow_shaft, arrow_head, fletch, half=0.62, bend=0.13, thick=0.022, recurve=False):
    hand = J.ha_l
    el = J.el_l
    up, fwd = J.bow_up, J.bow_fwd
    c = hand + nz(hand - el) * 0.02
    draw = J.p["draw"]
    k = bend + 0.12 * draw
    n = 6
    pts = []
    for i in range(-n, n + 1):
        t = i / n
        back = k * t * t
        if recurve:
            back -= 0.06 * max(0.0, abs(t) - 0.75) / 0.25
        pts.append(c + up * (t * half) - fwd * back)
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        t0 = abs((i - n) / n)
        t1 = abs((i + 1 - n) / n)
        seg(P, a, b, thick * (1 - 0.45 * t0), thick * (1 - 0.45 * t1), limb, zhint=fwd)
    ball(P, pts[0], thick * 0.75, tip)
    ball(P, pts[-1], thick * 0.75, tip)
    ball(P, c, thick * 1.3, limb)

    rest = c - fwd * k * 0.0 - fwd * 0.02
    nock = rest * (1 - draw) + grip_point(J, "r") * draw
    seg(P, pts[0], nock, 0.0065, 0.0065, string)
    seg(P, nock, pts[-1], 0.0065, 0.0065, string)

    if J.p["arrow"] > 0.5:
        # From the nock through the bow hand (or, undrawn, along the bow's aim)
        d = nz(c - nock) if np.linalg.norm(c - nock) > 0.08 else fwd
        tail = nock - d * 0.02
        head = tail + d * 0.78
        seg(P, tail, head, 0.009, 0.009, arrow_shaft)
        seg(P, head - d * 0.01, head + d * 0.06, 0.02, 0.002, arrow_head, zhint=up, fz=0.35)
        side = nz(np.cross(d, up))
        for s in (up, side):
            plank(P, tail + d * 0.02, tail + d * 0.13, s, 0.035, 0.004, fletch)

def quiver(P, J, mat, fletch, cap=None, arrows=5):
    a = J.C(v(-0.06, -0.38, -0.17))
    b = J.C(v(-0.17, 0.12, -0.2))
    seg(P, a, b, 0.05, 0.058, mat, zhint=J.Rc @ v(0, 0, -1))
    if cap is not None:
        ball(P, a, 0.05, cap)
    d = nz(b - a)
    side = nz(np.cross(d, J.Rc @ v(0, 0, 1)))
    for i in range(arrows):
        off = side * (0.03 * math.cos(i * 2.4)) + nz(np.cross(d, side)) * 0.025 * math.sin(i * 2.4)
        t = b + off + d * (0.04 + 0.02 * (i % 2))
        plank(P, t, t + d * 0.09, side if i % 2 else nz(np.cross(d, side)), 0.032, 0.004, fletch)

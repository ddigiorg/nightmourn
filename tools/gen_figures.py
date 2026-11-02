# ==============================================================================
# tools/gen_figures.py
#
# Generates the figure sprites (units and characters), pre-rendered the way
# Diablo II and Age of Empires II made theirs: each figure is a rigged 3D
# model of a few hundred primitives, posed per frame and ray cast from the
# game's 30 degree iso camera (tools/figlib). One PNG per animation,
#
#   data/base/graphics/entity/figure/<figure>/<animation>.png
#
# a column per frame and a row per stored direction (down, down-right,
# right, up-right, up; the game mirrors the other three). Each frame is
# 128x160 px with the feet at (64, 128); the packer trims the empty space.
# data/base/configs/entity/figures.json names them.
#
# Figures (original designs, dark fantasy):
#   hero             a knight in blackened plate, horned great helm, ragged
#                    crimson cape and a greatsword
#   human-warrior    a man-at-arms: mail, blue tabard, kettle hat, round
#                    shield and arming sword
#   human-archer     a hooded longbowman in leather
#   zombie-warrior   a gaunt dead soldier in rusted scraps, cold eyes
#   skeleton-archer  bones in a tattered hood with a black bow, cold eyes
#
# Animations: idle (8 frames), walk (8), attack (8, looping; frame 0 is the
# hit: the swing landing, the arrow loosed) and death (8, once).
#
#   python3 tools/gen_figures.py                      # write data/base/graphics/entity/figure
#   python3 tools/gen_figures.py --preview out.png    # a contact sheet instead
#   python3 tools/gen_figures.py --only hero --anims attack --preview out.png
#
# Needs numpy and Pillow. Deterministic. Takes a few minutes (all cores).
# ==============================================================================

import argparse
import math
import os
import sys
from multiprocessing import Pool

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figlib.render import (FW, FH, Mat, srgb, nz, render, rot_x, rot_y, rot_z, rot_axis,
                           frame_y, frame_z, ball, sphere, box, seg, plank, vnoise, fbm)
from figlib.rig import Body, pose, keyed, skeleton, walk_feet, swing_hand, blend, v
from figlib.parts import leg, arm, strip, cape, skirt, sword, shield, bow, quiver, grip_point

DIRECTIONS = 5
ANIMS = {
    #          frames  fps   loop
    "idle":   (8,      6.0,  True),
    "walk":   (8,      10.0, True),
    "attack": (8,      10.0, True),
    "death":  (8,      10.0, False),
}

# ------------------------------------------------------------------------------
# Shared pose helpers
# ------------------------------------------------------------------------------

def idle_base(s, stance, breathe=0.008, sway=0.0):
    a = 2 * math.pi * s
    p = dict(stance)
    root = np.array(p.get("root", v(0, 0, 0)), dtype=np.float64)
    p["root"] = root + v(0, -breathe * (0.5 + 0.5 * math.cos(a)), 0)
    p["lean"] = p.get("lean", 0.0) + breathe * 1.5 * math.sin(a)
    p["roll"] = p.get("roll", 0.0) + sway * math.sin(a)
    p["sway"] = s * 0.5
    return p

def walk_base(s, stance, stride=0.27, lift=0.09, limp=0.0, arm_r=None, arm_l=None, drag=0.45):
    f = walk_feet(s, stride=stride, lift=lift, limp=limp)
    p = dict(stance)
    root = np.array(p.get("root", v(0, 0, 0)), dtype=np.float64)
    p.update(f)
    p["root"] = root + f["root"]
    p["twist"] = stance.get("twist", 0.0) + f["twist"]
    p["pelvis_yaw"] = stance.get("pelvis_yaw", 0.0) + f["pelvis_yaw"]
    if arm_r is not None:
        p["hand_r"] = swing_hand(s, "r", arm_r)
    if arm_l is not None:
        p["hand_l"] = swing_hand(s, "l", arm_l)
    p["drag"] = v(0, 0.1, -drag)
    p["sway"] = s
    return p

def death_keys(stance, flail_r, flail_l, extra_end=None):
    """Hit, stagger back, knees buckle, fall on the back."""
    st = pose(**stance)
    k1 = dict(stance); k1.update(root=v(0, -0.03, -0.06), lean=-0.3, head_pitch=-0.45,
                                 hand_r=flail_r, hand_l=flail_l, drag=v(0, 0.2, 0.5))
    k2 = dict(stance); k2.update(root=v(0, -0.38, -0.1), lean=0.35, head_pitch=0.3,
                                 hand_r=v(-0.3, -0.45, 0.25), hand_l=v(0.3, -0.45, 0.2),
                                 foot_r=v(-0.12, 0.085, 0.05), foot_l=v(0.12, 0.085, 0.0))
    k3 = dict(stance); k3.update(root=v(0, -0.66, -0.28), lean=-0.85, head_pitch=-0.2,
                                 hand_r=v(-0.38, -0.2, -0.1), hand_l=v(0.4, -0.15, -0.05),
                                 foot_r=v(-0.14, 0.07, 0.28), foot_l=v(0.12, 0.07, 0.35),
                                 drag=v(0, 0.3, 0.4))
    k4 = dict(stance); k4.update(root=v(0, -0.80, -0.32), lean=-1.52, head_pitch=-0.15, head_yaw=0.4,
                                 hand_r=v(-0.45, 0.05, -0.05), hand_l=v(0.42, 0.12, 0.05),
                                 foot_r=v(-0.16, 0.06, 0.55), foot_l=v(0.1, 0.06, 0.6),
                                 toe_r=-0.6, toe_l=-0.7, drag=v(0, 0, 0), jaw=0.6)
    if extra_end:
        k4.update(extra_end)
    return [(0.0, st), (0.18, pose(**k1)), (0.45, pose(**k2)), (0.75, pose(**k3)), (1.0, pose(**k4))]

def at(stance, **kw):
    d = dict(stance)
    d.update(kw)
    return pose(**d)

# ------------------------------------------------------------------------------
# Figure base
# ------------------------------------------------------------------------------

class Figure:
    name   = ""
    scale  = 1.0
    rim    = srgb("#000000")
    body   = Body()
    impact = 0.6   # natural attack phase of the hit, shown as frame 0

    def stance(self):           return {}
    def idle(self, s):          return pose(**idle_base(s, self.stance()))
    def walk(self, s):          return pose(**walk_base(s, self.stance()))
    def attack_keys(self):      return [(0.0, pose(**self.stance()))]
    def death(self, s):         return keyed(death_keys(self.stance(), v(-0.35, 0.1, 0.1), v(0.35, 0.1, 0.1)), s, loop=False)
    def dress(self, J):         return []

    def pose_at(self, anim, i, frames):
        s = i / frames
        if anim == "idle":
            return self.idle(s)
        if anim == "walk":
            return self.walk(s)
        if anim == "attack":
            return keyed(self.attack_keys(), (s + self.impact) % 1.0)
        if anim == "death":
            return self.death(i / (frames - 1))
        raise ValueError(anim)

    def prims(self, anim, i, frames):
        p = self.pose_at(anim, i, frames)
        J = skeleton(self.body, p)
        P = self.dress(J)
        for k, q in enumerate(P):
            q.seed = (k * 0.6180339) % 1.0
        if self.scale != 1.0:
            P = [q.transformed(np.eye(3) * self.scale, np.zeros(3)) for q in P]
        return P

# ------------------------------------------------------------------------------
# Hero: blackened plate, horned great helm, crimson cape, greatsword
# ------------------------------------------------------------------------------

class Hero(Figure):
    name   = "hero"
    scale  = 1.07
    rim    = srgb("#b08a58") * 0.22
    body   = Body(sh_w=0.21)
    impact = 0.6

    steel  = Mat("#4b525c", metal=0.85, spec=0.75, power=45, noise=0.14, nfreq=6)
    dark   = Mat("#2e333a", metal=0.8, spec=0.5, power=30, noise=0.16, nfreq=6)
    gold   = Mat("#9c7a3c", metal=0.9, spec=0.9, power=55, noise=0.12)
    cloth  = Mat("#5c1414", noise=0.38, nfreq=7, spec=0.05)
    cloth2 = Mat("#3e0d0f", noise=0.4, nfreq=7, spec=0.05)
    leath  = Mat("#2b2119", noise=0.3, spec=0.1)
    fur    = Mat("#463c33", noise=0.7, nfreq=26, spec=0.0)
    blade  = Mat("#b4bcc6", metal=0.95, spec=1.1, power=90, noise=0.05)
    slit   = Mat("#040404", spec=0.0, rim=0.0, noise=0.0)
    eyes   = Mat("#000000", emissive=srgb("#ffb070") * 0.9, noise=0.0, rim=0.0)

    def stance(self):
        # Greatsword resting on the right shoulder
        return dict(hand_r=v(-0.15, -0.13, 0.2), wdir=v(-0.12, 0.62, -0.78), wflat=v(1, 0, 0.2),
                    hand_l=v(0.24, -0.5, 0.06), foot_r=v(-0.12, 0.085, -0.02), foot_l=v(0.13, 0.085, 0.05),
                    root=v(0, -0.02, 0))

    def idle(self, s):
        return pose(**idle_base(s, self.stance(), breathe=0.009))

    def walk(self, s):
        return pose(**walk_base(s, self.stance(), stride=0.32, lift=0.11, arm_l=v(0.25, -0.5, 0.06), drag=0.55))

    def two_hand(self, hand_r, wdir, **kw):
        w = nz(wdir)
        d = dict(self.stance())
        d.update(hand_r=hand_r, wdir=w, hand_l=hand_r - w * 0.12 + v(0.03, 0, 0), pole_l=v(0.3, -0.6, -0.6), **kw)
        return pose(**d)

    def attack_keys(self):
        ank = 0.085
        ready  = self.two_hand(v(-0.06, -0.36, 0.3), v(0.05, 0.62, 0.8), lean=0.08, wflat=v(1, 0, 0),
                               foot_l=v(0.13, ank, 0.12), foot_r=v(-0.13, ank, -0.08), root=v(0, -0.05, 0))
        wind   = self.two_hand(v(-0.24, 0.2, 0.0), v(-0.25, 0.45, -0.86), twist=-0.55, lean=-0.05,
                               wflat=v(0, 0, 1), foot_l=v(0.13, ank, 0.16), foot_r=v(-0.13, ank, -0.1),
                               root=v(0, -0.06, 0), drag=v(0, 0, 0.2))
        peak   = self.two_hand(v(-0.2, 0.3, -0.04), v(-0.15, 0.05, -0.99), twist=-0.7, lean=-0.08,
                               wflat=v(0, 1, 0), foot_l=v(0.14, ank, 0.2), foot_r=v(-0.13, ank, -0.12),
                               root=v(0, -0.06, 0), drag=v(0, 0, 0.3))
        hit    = self.two_hand(v(0.06, -0.26, 0.46), v(0.35, -0.28, 0.9), twist=0.45, lean=0.32,
                               wflat=v(0, 1, 0), foot_l=v(0.15, ank, 0.34), foot_r=v(-0.12, ank, -0.24),
                               root=v(0, -0.11, 0.05), drag=v(0, 0.2, -0.7))
        follow = self.two_hand(v(0.2, -0.4, 0.3), v(0.88, -0.42, 0.2), twist=0.65, lean=0.25,
                               wflat=v(0, 1, 0), foot_l=v(0.15, ank, 0.3), foot_r=v(-0.12, ank, -0.2),
                               root=v(0, -0.1, 0.04), drag=v(0, 0.1, -0.4))
        return [(0.0, ready), (0.3, wind), (0.5, peak), (0.6, hit), (0.74, follow), (0.9, ready)]

    def death(self, s):
        st = self.stance()
        return keyed(death_keys(st, v(-0.32, 0.05, 0.15), v(0.4, 0.15, 0.1),
                                extra_end=dict(wdir=v(-0.2, 0.3, 0.93))), s, loop=False)

    def dress(self, J):
        P = []
        st, dk, gd = self.steel, self.dark, self.gold
        # Legs: cuisses, poleyns, greaves, sabatons
        for sd in "rl":
            leg(P, J, sd, dk, st, dk, r=(0.08, 0.066, 0.062, 0.05), foot_size=(0.05, 0.036, 0.11), knee=st, knee_r=0.064)
        # Mail skirt and tassets, crimson tabard flaps
        seg(P, J.pelvis + v(0, 0.06, 0), J.pelvis + v(0, -0.2, 0), 0.155, 0.185, dk, zhint=J.Rp @ v(0, 0, 1), fz=0.78)
        skirt(P, J, st, n=6, radius=0.165, length=0.2, y=0.02, flare=0.3, thick=0.02, front_only=True)
        for sgn in (1, -1):
            out = J.Rp @ v(0, 0, sgn)
            top = J.pelvis + v(0, 0.03, 0) + out * 0.16
            strip(P, top, J.Rp @ v(1, 0, 0), 0.2, 0.6 if sgn > 0 else 0.55, nz(v(0, -1, 0) + out * 0.2),
                  J.p["drag"] * 0.5, self.cloth if sgn > 0 else self.cloth2, nseg=3, flutter=0.12, phase=J.p["sway"] * 6.28)
        # Torso: breastplate and plackart, belt
        Rc = J.Rc
        sphere(P, J.C(v(0, -0.13, 0.02)), Rc, (0.205, 0.2, 0.15), st)
        sphere(P, J.C(v(0, -0.3, 0.025)), Rc, (0.165, 0.13, 0.125), st)
        box(P, J.C(v(0, -0.11, 0.155)), Rc @ rot_x(-0.25), (0.012, 0.13, 0.012), gd)      # ridge
        sphere(P, J.pelvis + v(0, 0.06, 0), J.Rp, (0.17, 0.04, 0.13), self.leath)
        box(P, J.pelvis + J.Rp @ v(0, 0.06, 0.13), J.Rp, (0.035, 0.03, 0.012), gd)
        # Gorget, fur mantle
        sphere(P, J.C(v(0, 0.03, 0)), Rc, (0.11, 0.07, 0.1), st)
        sphere(P, J.C(v(0, 0.0, -0.03)), Rc, (0.235, 0.075, 0.17), self.fur)
        # Pauldrons: two lames each, a gold edge
        for sd, sh in (("r", J.sh_r), ("l", J.sh_l)):
            k = -1 if sd == "r" else 1
            R = Rc @ rot_z(k * 0.45)
            c = sh + Rc @ v(k * 0.03, 0.04, 0)
            sphere(P, c, R, (0.13, 0.085, 0.13), st)
            sphere(P, c + R @ v(k * 0.02, -0.07, 0), R, (0.12, 0.06, 0.12), dk)
            sphere(P, c + R @ v(0, 0.02, 0), R, (0.133, 0.03, 0.133), gd)
        # Arms
        for sd in "rl":
            arm(P, J, sd, dk, st, dk, r=(0.058, 0.052, 0.054, 0.044), hand_r=0.048, elbow=st, elbow_r=0.058)
        # Head: great helm, visor slit, crest and swept horns
        Rh = J.Rh
        seg(P, J.neck, J.head, 0.055, 0.05, dk)
        sphere(P, J.head, Rh, (0.118, 0.14, 0.128), st)
        box(P, J.head + Rh @ v(0, 0.005, 0.112), Rh, (0.085, 0.011, 0.03), self.slit)
        box(P, J.head + Rh @ v(0, -0.045, 0.118), Rh, (0.006, 0.035, 0.02), self.slit)
        ball(P, J.head + Rh @ v(-0.035, 0.006, 0.128), 0.007, self.eyes)
        ball(P, J.head + Rh @ v( 0.035, 0.006, 0.128), 0.007, self.eyes)
        box(P, J.head + Rh @ v(0, 0.13, -0.01), Rh, (0.012, 0.03, 0.115), gd)
        for k in (-1, 1):
            a = J.head + Rh @ v(k * 0.1, 0.06, -0.02)
            b = a + Rh @ v(k * 0.07, 0.08, -0.06)
            c = b + Rh @ v(k * 0.01, 0.07, -0.09)
            seg(P, a, b, 0.032, 0.022, dk)
            seg(P, b, c, 0.022, 0.006, dk)
        # Cape
        cape(P, J, self.cloth, n=6, width=0.44, lengths=[0.78, 0.92, 0.84, 0.95, 0.8, 0.88], back=0.13, top_y=0.0)
        # Greatsword
        sword(P, J, 1.08, 0.048, self.blade, gd, self.leath, gd, guard_w=0.17, grip_len=0.2, tip_w=0.008)
        return P

# ------------------------------------------------------------------------------
# Human warrior: mail, blue tabard, kettle hat, round shield, arming sword
# ------------------------------------------------------------------------------

def shield_face(q):
    x, y = q[:, 0], q[:, 1]
    rr = np.sqrt(x * x + y * y)
    blue = srgb("#24406e")
    bone = srgb("#b7aa8c")
    steel = srgb("#6f767e")
    cross = (np.abs(x) < 0.17) | (np.abs(y) < 0.17)
    col = np.where(cross[:, None], bone[None], blue[None])
    col = np.where((rr > 0.86)[:, None], steel[None], col)
    return col

class HumanWarrior(Figure):
    name   = "human-warrior"
    scale  = 1.0
    rim    = srgb("#b09a70") * 0.16
    body   = Body()
    impact = 0.6

    mail   = Mat("#686c70", metal=0.55, spec=0.35, power=22, noise=0.5, nfreq=42)
    steel  = Mat("#7a828b", metal=0.85, spec=0.65, power=40, noise=0.12)
    tabard = Mat("#2a3f68", noise=0.32, nfreq=8, spec=0.05)
    trim   = Mat("#b2a585", noise=0.25, spec=0.05)
    skin   = Mat("#a27a60", noise=0.18, spec=0.1)
    beard  = Mat("#3a2a1e", noise=0.4, nfreq=20)
    leath  = Mat("#4a3322", noise=0.3, spec=0.1)
    pants  = Mat("#4a4032", noise=0.3)
    boots  = Mat("#3d2b1e", noise=0.3, spec=0.15)
    wood   = Mat("#5a4027", noise=0.4)
    face   = Mat("#24406e", pattern=shield_face, noise=0.25, nfreq=6, spec=0.1)
    blade  = Mat("#a9b1ba", metal=0.95, spec=1.0, power=80, noise=0.06)

    def stance(self):
        return dict(hand_r=v(-0.25, -0.4, 0.14), wdir=v(0.05, 0.42, 0.9), wflat=v(1, 0, 0),
                    hand_l=v(0.2, -0.27, 0.22), sdir=v(0.45, 0.05, 0.9), pole_l=v(0.8, -0.4, -0.4),
                    foot_r=v(-0.12, 0.085, -0.03), foot_l=v(0.12, 0.085, 0.06), root=v(0, -0.02, 0))

    def walk(self, s):
        return pose(**walk_base(s, self.stance(), stride=0.31, lift=0.1, drag=0.4))

    def attack_keys(self):
        ank = 0.085
        st = self.stance()
        ready  = at(st, hand_r=v(-0.22, -0.3, 0.24), wdir=v(0.05, 0.55, 0.83), root=v(0, -0.05, 0),
                    foot_l=v(0.12, ank, 0.12), foot_r=v(-0.12, ank, -0.08))
        wind   = at(st, hand_r=v(-0.26, 0.18, -0.06), wdir=v(-0.1, 0.42, -0.9), twist=-0.4,
                    hand_l=v(0.12, -0.18, 0.32), sdir=v(0.2, 0.05, 1.0), root=v(0, -0.05, 0),
                    foot_l=v(0.12, ank, 0.12), foot_r=v(-0.12, ank, -0.08))
        peak   = at(st, hand_r=v(-0.22, 0.27, -0.02), wdir=v(-0.05, 0.85, -0.5), twist=-0.45,
                    hand_l=v(0.12, -0.16, 0.33), sdir=v(0.2, 0.05, 1.0), root=v(0, -0.05, 0),
                    foot_l=v(0.12, ank, 0.12), foot_r=v(-0.12, ank, -0.08))
        hit    = at(st, hand_r=v(-0.06, -0.18, 0.46), wdir=v(0.15, -0.22, 0.97), twist=0.3, lean=0.25,
                    hand_l=v(0.27, -0.26, 0.1), sdir=v(0.85, 0, 0.5), root=v(0, -0.09, 0.05),
                    foot_l=v(0.12, ank, 0.05), foot_r=v(-0.12, ank, 0.26), drag=v(0, 0, -0.4))
        follow = at(st, hand_r=v(0.04, -0.4, 0.3), wdir=v(0.5, -0.75, 0.4), twist=0.4, lean=0.22,
                    hand_l=v(0.27, -0.27, 0.1), sdir=v(0.85, 0, 0.5), root=v(0, -0.08, 0.04),
                    foot_l=v(0.12, ank, 0.05), foot_r=v(-0.12, ank, 0.22))
        return [(0.0, ready), (0.32, wind), (0.5, peak), (0.6, hit), (0.74, follow), (0.9, ready)]

    def dress(self, J):
        P = []
        for sd in "rl":
            leg(P, J, sd, self.pants, self.boots, self.boots, r=(0.082, 0.066, 0.064, 0.052), knee=None)
        Rc = J.Rc
        # Mail hauberk, belt
        sphere(P, J.C(v(0, -0.14, 0.01)), Rc, (0.19, 0.2, 0.14), self.mail)
        sphere(P, J.C(v(0, -0.32, 0.015)), Rc, (0.16, 0.14, 0.12), self.mail)
        seg(P, J.pelvis + v(0, 0.04, 0), J.pelvis + v(0, -0.3, 0), 0.15, 0.19, self.mail, zhint=J.Rp @ v(0, 0, 1), fz=0.78)
        # Tabard: front and back panels over the chest, flaps below the belt
        for sgn in (1, -1):
            box(P, J.C(v(0, -0.18, sgn * 0.128)), Rc @ rot_x(-sgn * 0.12), (0.15, 0.17, 0.012), self.tabard)
            box(P, J.C(v(0, -0.18, sgn * 0.142)), Rc @ rot_x(-sgn * 0.12), (0.03, 0.15, 0.006), self.trim)
            out = J.Rp @ v(0, 0, sgn)
            strip(P, J.pelvis + v(0, 0.05, 0) + out * 0.155, J.Rp @ v(1, 0, 0), 0.22, 0.42,
                  nz(v(0, -1, 0) + out * 0.25), J.p["drag"] * 0.5, self.tabard, nseg=2, flutter=0.1,
                  phase=J.p["sway"] * 6.28 + sgn)
        sphere(P, J.pelvis + v(0, 0.06, 0), J.Rp, (0.165, 0.035, 0.13), self.leath)
        # Shoulders: mail, a small steel spaulder
        for sd, sh in (("r", J.sh_r), ("l", J.sh_l)):
            k = -1 if sd == "r" else 1
            sphere(P, sh + Rc @ v(k * 0.02, 0.02, 0), Rc @ rot_z(k * 0.4), (0.09, 0.07, 0.09), self.steel)
        for sd in "rl":
            arm(P, J, sd, self.mail, self.mail, self.leath, r=(0.053, 0.047, 0.046, 0.04), hand_r=0.043)
        # Head: face, beard, mail coif, kettle hat
        Rh = J.Rh
        seg(P, J.neck, J.head, 0.05, 0.048, self.mail)
        sphere(P, J.head, Rh, (0.097, 0.115, 0.107), self.skin)
        sphere(P, J.head + Rh @ v(0, -0.06, 0.06), Rh, (0.07, 0.06, 0.05), self.beard)
        sphere(P, J.head + Rh @ v(0, -0.03, -0.035), Rh, (0.115, 0.12, 0.11), self.mail)
        sphere(P, J.head + Rh @ v(0, 0.055, 0), Rh, (0.118, 0.1, 0.118), self.steel)
        sphere(P, J.head + Rh @ v(0, 0.052, 0), Rh @ rot_x(0.05), (0.175, 0.02, 0.175), self.steel)
        # Sword and shield
        sword(P, J, 0.7, 0.036, self.blade, self.steel, self.leath, self.steel, guard_w=0.1, grip_len=0.1)
        shield(P, J, self.face, self.steel, self.steel, radius=0.26)
        return P

# ------------------------------------------------------------------------------
# Human archer: hooded longbowman in leather
# ------------------------------------------------------------------------------

class HumanArcher(Figure):
    name   = "human-archer"
    scale  = 0.99
    rim    = srgb("#b09a70") * 0.16
    body   = Body()
    impact = 0.72

    hood   = Mat("#2c3a2a", noise=0.38, nfreq=8, spec=0.05)
    jerk   = Mat("#5a4330", noise=0.32, nfreq=10, spec=0.12)
    gamb   = Mat("#77694f", noise=0.35, nfreq=16, spec=0.05)
    skin   = Mat("#a27a60", noise=0.18, spec=0.1)
    leath  = Mat("#3a2a1c", noise=0.3, spec=0.15)
    pants  = Mat("#433d31", noise=0.3)
    boots  = Mat("#2a1f17", noise=0.3, spec=0.15)
    wood   = Mat("#6e4b2a", noise=0.3, nfreq=22, spec=0.2)
    horn   = Mat("#c9bea0", noise=0.2)
    string = Mat("#d6cfba", noise=0.0, rim=0.3)
    shaft  = Mat("#8a6a44", noise=0.2)
    head   = Mat("#6c737a", metal=0.8, spec=0.6, power=30)
    fletch = Mat("#c8c0aa", noise=0.2)
    shadow = Mat("#0e0c0a", noise=0.0, rim=0.0)

    bow_kw = {}

    def stance(self):
        return dict(hand_l=v(0.25, -0.46, 0.1), bow_up=v(0.1, 0.72, 0.7), bow_fwd=v(0.9, 0.2, -0.3),
                    pole_l=v(0.6, -0.5, -0.6), foot_r=v(-0.12, 0.085, -0.02), foot_l=v(0.12, 0.085, 0.05),
                    root=v(0, -0.02, 0))

    def walk(self, s):
        return pose(**walk_base(s, self.stance(), stride=0.31, lift=0.1, arm_r=v(-0.24, -0.5, 0.05), drag=0.45))

    def shoot(self, **kw):
        ank = 0.085
        d = dict(self.stance())
        d.update(twist=-1.05, pelvis_yaw=-0.45, head_yaw=0.95, head_pitch=0.05, lean=0.05,
                 foot_l=v(0.06, ank, 0.16), foot_r=v(-0.14, ank, -0.12), root=v(0, -0.04, 0),
                 hand_l=v(0.66, 0.04, 0.04), bow_up=v(0.0, 1.0, 0.12), bow_fwd=v(1, 0, 0),
                 pole_l=v(0.2, -1.0, -0.2), pole_r=v(-0.3, -0.2, -1.0))
        d.update(kw)
        return pose(**d)

    def attack_keys(self):
        low    = self.shoot(hand_l=v(0.45, -0.32, 0.22), bow_up=v(0.45, 0.85, 0.2), bow_fwd=v(0.85, -0.45, 0),
                            hand_r=v(0.3, -0.3, 0.2), draw=1.0, arrow=1.0)
        raised = self.shoot(hand_r=v(0.46, 0.04, 0.06), draw=1.0, arrow=1.0)
        full   = self.shoot(hand_r=v(0.03, 0.15, 0.1), draw=1.0, arrow=1.0, pole_r=v(-0.2, 0.0, -1.0))
        loose  = self.shoot(hand_r=v(-0.1, 0.17, 0.02), draw=0.0, arrow=0.0, pole_r=v(-0.2, 0.0, -1.0))
        reach  = self.shoot(hand_r=v(-0.1, 0.24, -0.2), draw=0.0, arrow=0.0,
                            hand_l=v(0.52, -0.2, 0.15), bow_up=v(0.3, 0.9, 0.15))
        nock   = self.shoot(hand_r=v(0.32, -0.24, 0.18), draw=1.0, arrow=1.0,
                            hand_l=v(0.45, -0.3, 0.22), bow_up=v(0.45, 0.85, 0.2), bow_fwd=v(0.85, -0.45, 0))
        return [(0.0, low), (0.22, raised), (0.62, full), (0.72, full), (0.74, loose), (0.85, reach),
                (0.95, nock)]

    def death(self, s):
        return keyed(death_keys(self.stance(), v(-0.35, 0.1, 0.15), v(0.4, 0.05, 0.2)), s, loop=False)

    def dress_bow(self, P, J):
        bow(P, J, self.wood, self.horn, self.string, self.shaft, self.head, self.fletch,
            half=0.66, bend=0.12, thick=0.021)

    def dress(self, J):
        P = []
        for sd in "rl":
            leg(P, J, sd, self.pants, self.boots, self.boots, r=(0.07, 0.056, 0.055, 0.046))
        Rc, Rh = J.Rc, J.Rh
        # Gambeson under a leather jerkin, belt, skirts
        sphere(P, J.C(v(0, -0.14, 0.01)), Rc, (0.18, 0.2, 0.135), self.jerk)
        sphere(P, J.C(v(0, -0.32, 0.015)), Rc, (0.155, 0.14, 0.115), self.jerk)
        seg(P, J.pelvis + v(0, 0.05, 0), J.pelvis + v(0, -0.2, 0), 0.145, 0.17, self.gamb, zhint=J.Rp @ v(0, 0, 1), fz=0.78)
        sphere(P, J.pelvis + v(0, 0.06, 0), J.Rp, (0.16, 0.035, 0.125), self.leath)
        box(P, J.C(v(0.03, -0.12, 0.0)), Rc @ rot_z(0.75), (0.02, 0.24, 0.142), self.leath)  # baldric
        for sd in "rl":
            arm(P, J, sd, self.gamb, self.gamb, self.leath, r=(0.052, 0.046, 0.045, 0.039), hand_r=0.042)
        seg(P, J.el_l * 0.6 + J.ha_l * 0.4, J.ha_l, 0.05, 0.045, self.leath)  # bracer
        # Quiver on the back
        quiver(P, J, self.leath, self.fletch)
        # Head: face in a hood with a short mantle
        seg(P, J.neck, J.head, 0.048, 0.046, self.skin)
        sphere(P, J.head, Rh, (0.095, 0.112, 0.108), self.skin)
        sphere(P, J.head + Rh @ v(0, 0.015, -0.04), Rh, (0.128, 0.14, 0.128), self.hood)
        box(P, J.head + Rh @ v(0, 0.05, 0.075), Rh @ rot_x(0.35), (0.09, 0.02, 0.03), self.hood)   # brow
        sphere(P, J.head + Rh @ v(0, 0.01, 0.06), Rh, (0.07, 0.025, 0.05), self.shadow)           # eye shade
        seg(P, J.head + Rh @ v(0, 0.08, -0.1), J.head + Rh @ v(0, -0.02, -0.24), 0.06, 0.012, self.hood)
        sphere(P, J.C(v(0, 0.02, -0.01)), Rc, (0.24, 0.1, 0.17), self.hood)
        cape(P, J, self.hood, n=5, width=0.4, lengths=[0.38, 0.46, 0.42, 0.48, 0.4], back=0.12, top_y=0.0)
        self.dress_bow(P, J)
        return P

# ------------------------------------------------------------------------------
# Zombie warrior: a gaunt dead soldier in rusted scraps
# ------------------------------------------------------------------------------

def rot_skin(q):
    n = fbm(q * 7.0 + 3.3, 3)
    pale = srgb("#78837a")
    dark = srgb("#2c3631")
    bruise = srgb("#3d3443")
    t = np.clip((n - 0.38) * 3.0, 0, 1)[:, None]
    col = pale[None] * (1 - t) + dark[None] * t
    b = np.clip((fbm(q * 3.1 + 9.1, 2) - 0.6) * 4, 0, 1)[:, None]
    return col * (1 - b) + bruise[None] * b

class ZombieWarrior(Figure):
    name   = "zombie-warrior"
    scale  = 1.02
    rim    = srgb("#5fc4c8") * 0.32
    body   = Body(sh_w=0.18, hip_w=0.09)
    impact = 0.62

    skin   = Mat("#6b7468", pattern=rot_skin, noise=0.3, nfreq=14, spec=0.25, power=12)
    rust   = Mat("#46302a", metal=0.35, spec=0.25, power=12, noise=0.65, nfreq=12)
    iron   = Mat("#3b3a38", metal=0.6, spec=0.35, power=18, noise=0.5, nfreq=10)
    rag    = Mat("#29271f", noise=0.55, nfreq=9)
    rag2   = Mat("#3a3226", noise=0.55, nfreq=9)
    bone   = Mat("#a8a28a", noise=0.3)
    hair   = Mat("#16140f", noise=0.3)
    mouth  = Mat("#0b0706", noise=0.0, rim=0.0)
    socket = Mat("#050607", noise=0.0, rim=0.0)
    eyes   = Mat("#000000", emissive=srgb("#a8fff0") * 3.2, noise=0.0, rim=0.0, shadow=False)
    blade  = Mat("#6d5a4a", metal=0.55, spec=0.35, power=16, noise=0.6, nfreq=16)

    def stance(self):
        return dict(lean=0.42, roll=0.06, head_pitch=-0.42, head_roll=0.22, head_yaw=-0.1,
                    hand_r=v(-0.25, -0.45, 0.2), wdir=v(-0.12, -0.48, 0.87), wflat=v(1, 0, 0),
                    hand_l=v(0.22, -0.3, 0.36), pole_l=v(0.5, -0.6, -0.5),
                    foot_r=v(-0.13, 0.085, -0.06), foot_l=v(0.12, 0.085, 0.08), root=v(0, -0.07, -0.02),
                    jaw=0.45)

    def idle(self, s):
        p = idle_base(s, self.stance(), breathe=0.004, sway=0.07)
        a = 2 * math.pi * s
        p["head_roll"] = 0.22 + 0.12 * math.sin(a + 0.7)
        p["jaw"] = 0.45 + 0.35 * max(0.0, math.sin(2 * a))
        p["hand_l"] = p["hand_l"] + v(0.02 * math.sin(a), 0.03 * math.sin(a + 1), 0)
        return pose(**p)

    def walk(self, s):
        p = walk_base(s, self.stance(), stride=0.26, lift=0.07, limp=1.0, drag=0.25)
        a = 2 * math.pi * s
        p["roll"] = 0.06 + 0.12 * math.sin(a)
        p["head_roll"] = 0.22 - 0.15 * math.sin(a)
        p["hand_l"] = p["hand_l"] + v(0, 0.04 * math.sin(a), 0.05 * math.cos(a))
        p["jaw"] = 0.5 + 0.3 * math.sin(a * 2)
        return pose(**p)

    def attack_keys(self):
        ank = 0.085
        st = self.stance()
        ready  = at(st)
        wind   = at(st, lean=0.12, twist=-0.35, head_pitch=-0.15, hand_r=v(-0.2, 0.3, 0.0),
                    wdir=v(-0.1, 0.6, -0.8), hand_l=v(0.26, -0.05, 0.38), jaw=0.9,
                    foot_l=v(0.12, ank, 0.12), root=v(0, -0.04, -0.04))
        hit    = at(st, lean=0.68, twist=0.25, head_pitch=-0.6, hand_r=v(-0.02, -0.28, 0.46),
                    wdir=v(0.2, -0.5, 0.85), hand_l=v(0.3, -0.35, 0.15), jaw=1.0,
                    foot_l=v(0.12, ank, 0.25), root=v(0, -0.12, 0.06), drag=v(0, 0, -0.4))
        follow = at(st, lean=0.6, twist=0.35, head_pitch=-0.55, hand_r=v(0.1, -0.5, 0.25),
                    wdir=v(0.3, -0.9, 0.2), hand_l=v(0.3, -0.4, 0.1), jaw=0.8,
                    foot_l=v(0.12, ank, 0.22), root=v(0, -0.12, 0.05))
        return [(0.0, ready), (0.42, wind), (0.62, hit), (0.78, follow)]

    def death(self, s):
        return keyed(death_keys(self.stance(), v(-0.3, 0.0, 0.2), v(0.35, 0.1, 0.2),
                                extra_end=dict(jaw=1.0, head_roll=0.5)), s, loop=False)

    def dress(self, J):
        P = []
        sk = self.skin
        Rc, Rh = J.Rc, J.Rh
        # Gaunt legs, one rag-wrapped shin, a rusted greave on the other
        leg(P, J, "r", sk, sk, sk, r=(0.058, 0.044, 0.044, 0.034), foot_size=(0.042, 0.03, 0.1), knee=self.bone, knee_r=0.042)
        leg(P, J, "l", sk, self.rust, sk, r=(0.058, 0.044, 0.05, 0.042), foot_size=(0.042, 0.03, 0.1), knee=self.bone, knee_r=0.042)
        seg(P, J.kn_r * 0.7 + J.an_r * 0.3, J.kn_r * 0.2 + J.an_r * 0.8, 0.05, 0.042, self.rag2)
        # Hips: rags hanging from a rotten belt
        sphere(P, J.pelvis + v(0, 0.0, 0), J.Rp, (0.14, 0.09, 0.105), self.rag)
        skirt(P, J, self.rag, n=7, radius=0.135, y=0.03, lengths=[0.38, 0.3, 0.42, 0.25, 0.36, 0.44, 0.28], flare=0.15)
        sphere(P, J.pelvis + v(0, 0.06, 0), J.Rp, (0.145, 0.03, 0.11), self.iron)
        # Torso: sunken flesh, ribs on the right, a broken breastplate on the left
        sphere(P, J.C(v(0, -0.14, 0.0)), Rc, (0.175, 0.19, 0.125), sk)
        sphere(P, J.C(v(0, -0.32, -0.005)), Rc, (0.12, 0.12, 0.09), sk)
        for k in range(4):
            y = -0.08 - k * 0.055
            seg(P, J.C(v(-0.03, y, 0.11)), J.C(v(-0.15, y - 0.03, 0.05)), 0.014, 0.012, self.bone)
        sphere(P, J.C(v(0.06, -0.13, 0.025)), Rc @ rot_z(-0.25) @ rot_y(0.3), (0.13, 0.19, 0.125), self.rust)
        box(P, J.C(v(0.0, -0.14, 0.11)), Rc @ rot_z(-0.6), (0.01, 0.12, 0.03), self.mouth)  # a split in it
        # One rusted pauldron
        sphere(P, J.sh_l + Rc @ v(0.03, 0.03, 0), Rc @ rot_z(0.5), (0.1, 0.065, 0.1), self.iron)
        sphere(P, J.sh_l + Rc @ v(0.05, -0.03, 0), Rc @ rot_z(0.5), (0.09, 0.04, 0.09), self.rust)
        # Arms: long and thin, claws
        arm(P, J, "r", sk, sk, sk, r=(0.044, 0.036, 0.036, 0.03), hand_r=0.04)
        arm(P, J, "l", sk, sk, sk, r=(0.044, 0.036, 0.036, 0.03), hand_r=0.042)
        d = nz(J.ha_l - J.el_l)
        for k in (-1, 0, 1):
            a = J.ha_l + d * 0.06 + (J.Rc @ v(0, 0, 1)) * 0.0 + nz(np.cross(d, J.fwd)) * k * 0.022
            seg(P, a, a + d * 0.07 + J.fwd * 0.02, 0.011, 0.003, self.bone)
        # A torn cloth sash over the shoulder
        strip(P, J.C(v(0.12, 0.0, -0.12)), Rc @ v(1, 0, 0), 0.12, 0.6, nz(v(0, -1, 0) + J.Rc @ v(0, 0, -0.3)),
              J.p["drag"] * 0.7, self.rag2, nseg=3, flutter=0.2, phase=J.p["sway"] * 6.28)
        # Head: hollow eyes that burn cold, slack jaw, a few strands of hair
        seg(P, J.neck, J.head, 0.04, 0.038, sk)
        sphere(P, J.head, Rh, (0.092, 0.112, 0.102), sk)
        for k in (-1, 1):
            ball(P, J.head + Rh @ v(k * 0.036, 0.012, 0.078), 0.026, self.socket)
            ball(P, J.head + Rh @ v(k * 0.036, 0.012, 0.094), 0.0105, self.eyes)
        jaw = J.p["jaw"]
        Rj = Rh @ rot_x(0.15 + 0.4 * jaw)
        sphere(P, J.head + Rh @ v(0, -0.05, 0.02) + Rj @ v(0, -0.035, 0.04), Rj, (0.068, 0.03, 0.06), sk)
        sphere(P, J.head + Rh @ v(0, -0.05, 0.06) + Rj @ v(0, 0.0, 0.0), Rj, (0.045, 0.012 + 0.02 * jaw, 0.03), self.mouth)
        for k in range(5):
            a = J.head + Rh @ v(-0.06 + k * 0.03, 0.09, -0.03 - abs(k - 2) * 0.015)
            b = a + Rh @ v((k - 2) * 0.015, -0.12, -0.05) + v(0, -0.06, 0)
            seg(P, a, b, 0.01, 0.004, self.hair)
        # A notched, rusted sword
        sword(P, J, 0.68, 0.04, self.blade, self.iron, self.rag, self.iron, guard_w=0.1, grip_len=0.1,
              notches=[(0.35, 1), (0.6, -1), (0.8, 1)])
        return P

# ------------------------------------------------------------------------------
# Skeleton archer: bones in a tattered hood with a black bow
# ------------------------------------------------------------------------------

class SkeletonArcher(HumanArcher):
    name   = "skeleton-archer"
    scale  = 1.0
    rim    = srgb("#6fb8e8") * 0.36
    body   = Body(sh_w=0.18, hip_w=0.085)
    impact = 0.72

    bone   = Mat("#b7b4a4", noise=0.4, nfreq=16, spec=0.3, power=22)
    bone_d = Mat("#8f8b7c", noise=0.45, nfreq=16, spec=0.2, power=18)
    rag    = Mat("#1b2226", noise=0.55, nfreq=9, spec=0.05)
    rag2   = Mat("#232a2e", noise=0.55, nfreq=9, spec=0.05)
    iron   = Mat("#34363a", metal=0.6, spec=0.35, power=18, noise=0.5, nfreq=10)
    socket = Mat("#040506", noise=0.0, rim=0.0)
    eyes   = Mat("#000000", emissive=srgb("#7fd8ff") * 3.4, noise=0.0, rim=0.0, shadow=False)
    wood   = Mat("#151517", noise=0.3, nfreq=22, spec=0.35, power=30)
    horn   = Mat("#b7b4a4", noise=0.3)
    string = Mat("#8c8f8a", noise=0.0, rim=0.3)
    shaft  = Mat("#2a2420", noise=0.2)
    head   = Mat("#3b3d40", metal=0.7, spec=0.5, power=25)
    fletch = Mat("#262a2c", noise=0.2)
    leath  = Mat("#1e1a17", noise=0.35, spec=0.1)

    def stance(self):
        d = super().stance()
        d.update(head_pitch=-0.05, jaw=0.15, lean=0.04)
        return d

    def idle(self, s):
        p = idle_base(s, self.stance(), breathe=0.003, sway=0.03)
        a = 2 * math.pi * s
        p["jaw"] = 0.15 + 0.25 * max(0.0, math.sin(3 * a))
        p["head_yaw"] = 0.12 * math.sin(a)
        return pose(**p)

    def walk(self, s):
        p = walk_base(s, self.stance(), stride=0.3, lift=0.1, arm_r=v(-0.22, -0.5, 0.05), drag=0.4)
        p["jaw"] = 0.2 + 0.15 * math.sin(4 * math.pi * s)
        return pose(**p)

    def death(self, s):
        return keyed(death_keys(self.stance(), v(-0.35, 0.1, 0.15), v(0.4, 0.05, 0.2),
                                extra_end=dict(jaw=1.0)), s, loop=False)

    def dress_bow(self, P, J):
        bow(P, J, self.wood, self.horn, self.string, self.shaft, self.head, self.fletch,
            half=0.6, bend=0.1, thick=0.022, recurve=True)

    def dress(self, J):
        P = []
        b, bd = self.bone, self.bone_d
        Rc, Rh, Rp = J.Rc, J.Rh, J.Rp
        r = 0.03
        # Legs
        for sd in "rl":
            leg(P, J, sd, b, b, b, r=(0.034, 0.026, 0.028, 0.022), foot_size=(0.036, 0.022, 0.095), knee=b, knee_r=0.04)
        # Pelvis, spine, ribcage, sternum, collarbones
        sphere(P, J.pelvis + v(0, 0.0, 0), Rp, (0.12, 0.06, 0.07), b)
        for k in (-1, 1):
            sphere(P, J.pelvis + Rp @ v(k * 0.08, 0.04, -0.01), Rp @ rot_z(k * 0.5), (0.06, 0.07, 0.03), bd)
        seg(P, J.pelvis + v(0, 0.02, 0), J.chest, 0.03, 0.03, bd, zhint=J.fwd)
        for k in range(5):
            y = -0.07 - k * 0.058
            w = 0.155 - k * 0.012 - (0.02 if k == 0 else 0)
            for sgn in (-1, 1):
                p0 = J.C(v(sgn * 0.02, y, -0.065))
                p1 = J.C(v(sgn * w, y - 0.02, -0.005))
                p2 = J.C(v(sgn * (w - 0.05), y - 0.045, 0.085))
                p3 = J.C(v(sgn * 0.025, y - 0.03, 0.105))
                seg(P, p0, p1, 0.016, 0.017, b)
                seg(P, p1, p2, 0.017, 0.015, b)
                if k < 4:
                    seg(P, p2, p3, 0.013, 0.012, bd)
        box(P, J.C(v(0, -0.15, 0.108)), Rc @ rot_x(-0.12), (0.022, 0.1, 0.012), b)
        for sh in (J.sh_r, J.sh_l):
            seg(P, J.C(v(0, -0.01, 0.07)), sh, 0.018, 0.022, b)
        # Arms
        for sd in "rl":
            arm(P, J, sd, b, b, b, r=(0.028, 0.024, 0.024, 0.019), hand_r=0.034, elbow=b, elbow_r=0.03)
            sh = J.sh_r if sd == "r" else J.sh_l
            ball(P, sh, 0.04, b)
        # A rusted pauldron and a strap
        sphere(P, J.sh_l + Rc @ v(0.03, 0.035, 0), Rc @ rot_z(0.5), (0.11, 0.07, 0.11), self.iron)
        box(P, J.C(v(0.0, -0.13, 0.0)), Rc @ rot_z(-0.75), (0.018, 0.26, 0.11), self.leath)
        quiver(P, J, self.leath, self.fletch)
        # Rags at the waist
        skirt(P, J, self.rag, n=6, radius=0.12, y=0.04, lengths=[0.4, 0.3, 0.46, 0.26, 0.38, 0.32], flare=0.2)
        # Skull: sockets, cold eyes, nose hole, teeth and jaw
        seg(P, J.neck, J.head, 0.024, 0.022, bd)
        sphere(P, J.head + Rh @ v(0, 0.015, -0.01), Rh, (0.092, 0.1, 0.105), b)
        sphere(P, J.head + Rh @ v(0, -0.03, 0.045), Rh, (0.07, 0.06, 0.06), b)
        for k in (-1, 1):
            ball(P, J.head + Rh @ v(k * 0.036, 0.0, 0.082), 0.026, self.socket)
            ball(P, J.head + Rh @ v(k * 0.036, 0.0, 0.096), 0.011, self.eyes)
        box(P, J.head + Rh @ v(0, -0.04, 0.102), Rh, (0.01, 0.014, 0.01), self.socket)
        jaw = J.p["jaw"]
        Rj = Rh @ rot_x(0.1 + 0.45 * jaw)
        sphere(P, J.head + Rh @ v(0, -0.07, 0.0) + Rj @ v(0, -0.02, 0.05), Rj, (0.058, 0.022, 0.05), b)
        box(P, J.head + Rh @ v(0, -0.066, 0.088), Rh, (0.034, 0.008, 0.006), self.socket)
        # Tattered hood and mantle, rags down the back
        sphere(P, J.head + Rh @ v(0, 0.035, -0.045), Rh, (0.125, 0.135, 0.125), self.rag)
        box(P, J.head + Rh @ v(0, 0.08, 0.06), Rh @ rot_x(0.45), (0.1, 0.018, 0.035), self.rag)
        seg(P, J.head + Rh @ v(0, 0.09, -0.12), J.head + Rh @ v(0, -0.03, -0.25), 0.055, 0.01, self.rag)
        sphere(P, J.C(v(0, 0.01, -0.03)), Rc, (0.22, 0.08, 0.16), self.rag)
        cape(P, J, self.rag2, n=5, width=0.38, lengths=[0.7, 0.45, 0.82, 0.52, 0.66], back=0.1, top_y=0.0, flutter=0.25)
        self.dress_bow(P, J)
        return P

FIGURES = [Hero(), HumanWarrior(), HumanArcher(), ZombieWarrior(), SkeletonArcher()]

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------

def render_cell(job):
    fi, anim, i = job
    fig = FIGURES[fi]
    frames = ANIMS[anim][0]
    P = fig.prims(anim, i, frames)
    rows = [render(P, d * math.pi / 4, fig.rim) for d in range(DIRECTIONS)]
    return fi, anim, i, rows

def to_u8(a):
    return (np.clip(a, 0, 1) * 255 + 0.5).astype(np.uint8)

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", default=os.path.join(os.path.dirname(__file__), "..", "data", "base", "graphics", "entity", "figure"))
    ap.add_argument("--preview", help="write a contact sheet here instead")
    ap.add_argument("--only", nargs="*", help="figure names")
    ap.add_argument("--anims", nargs="*", help="animation names")
    ap.add_argument("--frames", nargs="*", type=int, help="frame indices (preview)")
    ap.add_argument("--jobs", type=int, default=os.cpu_count())
    args = ap.parse_args()

    figs = [i for i, f in enumerate(FIGURES) if not args.only or f.name in args.only]
    anims = [a for a in ANIMS if not args.anims or a in args.anims]
    jobs = [(fi, a, i) for fi in figs for a in anims for i in range(ANIMS[a][0])
            if not args.frames or i in args.frames]

    cells = {}
    with Pool(args.jobs) as pool:
        for fi, a, i, rows in pool.imap_unordered(render_cell, jobs):
            cells[(fi, a, i)] = rows
            print(f"\r{len(cells)}/{len(jobs)}", end="", flush=True)
    print()

    if args.preview:
        # One block per (figure, animation): directions across, frames down
        blocks = []
        for fi in figs:
            for a in anims:
                idx = sorted(i for (f, aa, i) in cells if f == fi and aa == a)
                rows = [np.concatenate(cells[(fi, a, i)], axis=1) for i in idx]
                blocks.append(np.concatenate(rows, axis=0))
        wmax = max(b.shape[1] for b in blocks)
        blocks = [np.pad(b, ((0, 0), (0, wmax - b.shape[1]), (0, 0))) for b in blocks]
        Image.fromarray(to_u8(np.concatenate(blocks, axis=0))).save(args.preview)
        return

    for fi in figs:
        fig = FIGURES[fi]
        d = os.path.join(args.out, fig.name)
        os.makedirs(d, exist_ok=True)
        for a in anims:
            n = ANIMS[a][0]
            rows = [np.concatenate([cells[(fi, a, i)][dd] for i in range(n)], axis=1) for dd in range(DIRECTIONS)]
            Image.fromarray(to_u8(np.concatenate(rows, axis=0))).save(os.path.join(d, a + ".png"), optimize=True)
            print("wrote", os.path.join(d, a + ".png"))

if __name__ == "__main__":
    main()

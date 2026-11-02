# ==============================================================================
# tools/figlib/rig.py
#
# A humanoid rig for gen_figures.py: a pose (a dict of a few angles, hand
# targets in the chest's frame and ankle targets on the ground) becomes joint
# positions, with two-bone IK for the arms and legs. Poses are keyed and
# blended (keyed()), or built procedurally (walk and idle cycles).
#
# Model space: meters, y up, feet on y = 0, facing +z, right hand on -x.
# ==============================================================================

import math
import numpy as np
from .render import nz, rot_x, rot_y, rot_z

def v(*a):
    return np.array(a, dtype=np.float64)

class Body:
    """Proportions (m)"""
    def __init__(self, **kw):
        self.pelvis  = 0.96   # pelvis joint height, standing
        self.spine   = 0.42   # pelvis -> chest (top of the ribcage)
        self.neck    = 0.22   # chest -> head center
        self.sh_w    = 0.20   # chest -> shoulder, sideways
        self.hip_w   = 0.095
        self.upper   = 0.29
        self.fore    = 0.27
        self.thigh   = 0.44
        self.shin    = 0.43
        self.ankle   = 0.085
        for k, x in kw.items():
            setattr(self, k, x)

# Every pose key and its default. Vectors in the chest's frame unless noted.
BASE = dict(
    root       = v(0, 0, 0),          # pelvis offset (model space)
    pelvis_yaw = 0.0,
    lean       = 0.0,                 # spine pitch, + forward
    roll       = 0.0,                 # spine roll, + toward the left side
    twist      = 0.0,                 # chest yaw on the pelvis, + turns left
    head_yaw   = 0.0,
    head_pitch = 0.0,                 # + looks down
    head_roll  = 0.0,
    hand_r     = v(-0.24, -0.52, 0.05),
    hand_l     = v( 0.24, -0.52, 0.05),
    pole_r     = v(-0.5, -0.3, -1.0), # elbow hints
    pole_l     = v( 0.5, -0.3, -1.0),
    foot_r     = v(-0.11, 0.085, 0.0),  # ankle targets (model space)
    foot_l     = v( 0.11, 0.085, 0.0),
    toe_r      = 0.0,                 # foot pitch, + toes down
    toe_l      = 0.0,
    wdir       = v(0, 1, 0),          # weapon (right hand): blade direction
    wflat      = v(1, 0, 0),          #   and the blade's flat normal
    sdir       = v(0, 0, 1),          # shield (left hand) facing
    bow_up     = v(0, 1, 0),          # bow (left hand): limb axis
    bow_fwd    = v(1, 0, 0),          #   and where it shoots
    draw       = 0.0,                 # 1: the string is at the right hand
    arrow      = 0.0,                 # > 0.5: an arrow is nocked
    drag       = v(0, 0, 0),          # cloth trails this way (model space)
    sway       = 0.0,                 # cloth flutter phase
    jaw        = 0.0,                 # 0 shut .. 1 open
    blink      = 0.0,
)

def pose(**kw):
    p = {k: (x.copy() if isinstance(x, np.ndarray) else x) for k, x in BASE.items()}
    for k, x in kw.items():
        if k not in p:
            raise KeyError(k)
        p[k] = np.asarray(x, dtype=np.float64) if isinstance(p[k], np.ndarray) else float(x)
    return p

def blend(a, b, t):
    out = {}
    for k in a:
        out[k] = a[k] * (1 - t) + b[k] * t
    return out

def smooth(t):
    return t * t * (3 - 2 * t)

def keyed(keys, s, loop=True):
    """keys: [(s, pose)], s ascending in [0, 1]. Smoothstep between keys;
    looping wraps from the last key to the first."""
    if loop and keys[-1][0] < 1.0:
        keys = keys + [(1.0, keys[0][1])]
    s = s % 1.0 if loop else min(max(s, 0.0), 1.0)
    for (s0, p0), (s1, p1) in zip(keys, keys[1:]):
        if s0 <= s <= s1:
            t = 0.0 if s1 <= s0 else (s - s0) / (s1 - s0)
            return blend(p0, p1, smooth(t))
    return keys[-1][1]

def ik(root, target, l1, l2, pole):
    d = target - root
    dist = float(np.linalg.norm(d))
    dn = d / max(dist, 1e-6)
    dist = min(max(dist, abs(l1 - l2) + 1e-3), l1 + l2 - 1e-3)
    ca = (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)
    a = math.acos(min(max(ca, -1.0), 1.0))
    pp = pole - (pole @ dn) * dn
    pp = nz(pp) if np.linalg.norm(pp) > 1e-6 else nz(np.cross(dn, [1, 0, 0]))
    mid = root + l1 * (math.cos(a) * dn + math.sin(a) * pp)
    end = root + dn * dist
    return mid, end

class Joints:
    pass

def skeleton(b, p):
    J = Joints()
    J.p = p
    J.Rp = rot_y(p["pelvis_yaw"])
    J.pelvis = v(0, b.pelvis, 0) + p["root"]
    J.Rc = J.Rp @ rot_x(p["lean"]) @ rot_z(p["roll"]) @ rot_y(p["twist"])
    J.chest = J.pelvis + J.Rp @ rot_x(p["lean"]) @ rot_z(p["roll"]) @ v(0, b.spine, 0)
    J.Rh = J.Rc @ rot_y(p["head_yaw"]) @ rot_x(p["head_pitch"]) @ rot_z(p["head_roll"])
    J.neck = J.chest + J.Rc @ v(0, 0.06, -0.01)
    J.head = J.neck + J.Rh @ v(0, b.neck - 0.06, 0.025)

    C = lambda x: J.chest + J.Rc @ x
    J.C = C
    J.sh_r = C(v(-b.sh_w, -0.03, -0.01))
    J.sh_l = C(v( b.sh_w, -0.03, -0.01))
    J.el_r, J.ha_r = ik(J.sh_r, C(p["hand_r"]), b.upper, b.fore, J.Rc @ p["pole_r"])
    J.el_l, J.ha_l = ik(J.sh_l, C(p["hand_l"]), b.upper, b.fore, J.Rc @ p["pole_l"])

    J.hip_r = J.pelvis + J.Rp @ v(-b.hip_w, -0.05, 0)
    J.hip_l = J.pelvis + J.Rp @ v( b.hip_w, -0.05, 0)
    knee_pole = lambda s: J.Rp @ nz(v(s * 0.15, 0, 1))
    J.kn_r, J.an_r = ik(J.hip_r, p["foot_r"], b.thigh, b.shin, knee_pole(-1))
    J.kn_l, J.an_l = ik(J.hip_l, p["foot_l"], b.thigh, b.shin, knee_pole(1))

    # Feet point the way the pelvis does, pitched by toe_*
    J.Rf_r = J.Rp @ rot_x(p["toe_r"])
    J.Rf_l = J.Rp @ rot_x(p["toe_l"])

    # Weapon frames in model space
    J.wdir    = nz(J.Rc @ p["wdir"])
    J.wflat   = nz(J.Rc @ p["wflat"])
    J.sdir    = nz(J.Rc @ p["sdir"])
    J.bow_up  = nz(J.Rc @ p["bow_up"])
    bf = J.Rc @ p["bow_fwd"]
    bf = bf - (bf @ J.bow_up) * J.bow_up
    J.bow_fwd = nz(bf)
    J.fwd   = J.Rc @ v(0, 0, 1)
    J.right = J.Rc @ v(-1, 0, 0)
    J.up    = J.Rc @ v(0, 1, 0)
    return J

# ------------------------------------------------------------------------------
# Procedural cycles
# ------------------------------------------------------------------------------

def walk_feet(s, stride=0.27, lift=0.09, width=0.11, ankle=0.085, limp=0.0):
    """Ankle targets and pelvis bob for a walk cycle at phase s. `limp` drags
    the right foot (less lift and stride) and lurches."""
    a = 2 * math.pi * s
    zr = stride * (1 - 0.35 * limp) * math.cos(a)
    zl = -stride * math.cos(a)
    lr = lift * (1 - 0.8 * limp) * max(0.0, -math.sin(a))
    ll = lift * max(0.0, math.sin(a))
    bob = -0.022 * math.cos(2 * a) - 0.03 - 0.03 * limp * max(0.0, math.sin(a))
    return dict(
        foot_r = v(-width, ankle + lr, zr),
        foot_l = v( width, ankle + ll, zl),
        toe_r  = 0.5 * lr / max(lift, 1e-6) * 0.6,
        toe_l  = 0.5 * ll / max(lift, 1e-6) * 0.6,
        root   = v(0, bob, 0),
        pelvis_yaw = 0.10 * math.cos(a),
        twist  = -0.16 * math.cos(a),
    )

def swing_hand(s, side, base, amount=0.17):
    """A free hand at `base` (chest frame) swinging opposite its leg."""
    a = 2 * math.pi * s
    out = np.array(base, dtype=np.float64).copy()
    out[2] += (-amount if side == "r" else amount) * math.cos(a)
    out[1] += 0.03 * abs(math.cos(a))
    return out

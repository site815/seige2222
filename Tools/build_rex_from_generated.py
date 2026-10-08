"""Rex revision 6: a generated base mesh, photo-projected coat, the existing rig and card fur.

    blender -b -P Tools/build_rex_from_generated.py [-- --preview-only] [-- --mesh=<glb>]

Inputs (not tracked in Git):
  Art/CompanionDog/Source/generated/<mesh>.glb   image-to-3D shape generated from the
                                                  owner's photograph (Hunyuan3D, hosted)
  Saved/rex_photos/rex_main_matte.png             the same photograph, background removed
Outputs: the usual Exports/*.fbx, Textures/*.png, dog_manifest.json, Source/CompanionDog.blend
and Previews/*.png. Rig, bone names, clip names, asset paths and the walk stride stay as in
revision 4 so runtime code and save fingerprints are untouched.
"""
from pathlib import Path
import sys, math, json, hashlib
import bpy, bmesh
import numpy as np
from mathutils import Vector, Quaternion, Matrix
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
for d in ('Source', 'Exports', 'Textures', 'Previews'):
    (ART / d).mkdir(parents=True, exist_ok=True)
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
PREVIEW_ONLY = '--preview-only' in ARGS
MESH = next((a.split('=', 1)[1] for a in ARGS if a.startswith('--mesh=')), 'hy21shape_generation_0.glb')
TARGET_TRIS = int(next((a.split('=', 1)[1] for a in ARGS if a.startswith('--tris=')), '48000'))
# Feathering cards: revision 6 read as straw stuck to the coat in the engine, so
# revision 7 keeps them shorter, sparser and lying closer to the body.
CARD_LENGTH = float(next((a.split('=', 1)[1] for a in ARGS if a.startswith('--card-length=')), '0.72'))
CARD_DENSITY = float(next((a.split('=', 1)[1] for a in ARGS if a.startswith('--card-density=')), '0.6'))
PHOTO = ROOT / 'Saved/rex_photos/rex_main_matte.png'
RNG = np.random.default_rng(8152226)
BACK_HEIGHT_CM = 59.0

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01; scene.render.fps = 30
bpy.context.preferences.filepaths.save_version = 0


def log(*a):
    print(*a, flush=True)


def srgb_to_linear(c):
    return tuple((x / 12.92) if x <= .04045 else ((x + .055) / 1.055) ** 2.4 for x in c)


def hexcolor(h):
    return srgb_to_linear(tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)))


def ease(t):
    t = max(0, min(1, t)); return t * t * (3 - 2 * t)


GOLD_DARK = np.array(hexcolor('C4873B')); GOLD = np.array(hexcolor('D59A4E')); GOLD_LIGHT = np.array(hexcolor('DFB06B'))
CREAM = np.array(hexcolor('E9D2A8')); WHITE = np.array(hexcolor('F0E6D2'))
PALETTE = {'Coat': ((.55, .33, .14, 1), .62, 0), 'FurCard': ((.55, .33, .14, 1), .55, 0)}

# ---------------------------------------------------------------- procedural fur textures
N = 1024


def save_image(name, rgba, srgb=False):
    img = bpy.data.images.new(name, width=rgba.shape[1], height=rgba.shape[0], alpha=True)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    img.pixels.foreach_set(np.ascontiguousarray(rgba, dtype=np.float32).reshape(-1))
    path = str(ART / 'Textures' / (name + '.png'))
    img.filepath_raw = path; img.file_format = 'PNG'; img.save()
    img.source = 'FILE'; img.filepath = path; img.reload()
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    return img


def value_noise(shape, cells, rng, octaves=4, stretch=(1, 1)):
    out = np.zeros(shape, np.float32); amp = 1.0
    for o in range(octaves):
        cy = max(1, int(cells[0] * (2 ** o) / stretch[0])); cx = max(1, int(cells[1] * (2 ** o) / stretch[1]))
        grid = rng.random((cy, cx)).astype(np.float32)
        ys = np.linspace(0, cy, shape[0], endpoint=False); xs = np.linspace(0, cx, shape[1], endpoint=False)
        y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
        fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
        fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
        y1 = (y0 + 1) % cy; x1 = (x0 + 1) % cx
        g = (grid[y0][:, x0] * (1 - fy) * (1 - fx) + grid[y0][:, x1] * (1 - fy) * fx + grid[y1][:, x0] * fy * (1 - fx) + grid[y1][:, x1] * fy * fx)
        out += g * amp; amp *= .5
    out -= out.min(); out /= max(1e-6, out.max())
    return out


def smoothstep(e0, e1, x):
    e0 = np.asarray(e0, np.float32); e1 = np.asarray(e1, np.float32)
    span = np.where(np.abs(e1 - e0) < 1e-6, 1e-6, e1 - e0)
    t = np.clip((x - e0) / span, 0, 1)
    return t * t * (3 - 2 * t)


rng = np.random.default_rng(815)
streak = value_noise((N, N), (24, 24), rng, octaves=5, stretch=(1, 9))
fine = value_noise((N, N), (96, 96), rng, octaves=3, stretch=(1, 5))
height = np.clip(.55 * streak + .45 * fine, 0, 1)
detail = np.clip(.74 + (height - .5) * .55, .45, 1.0)
dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 2.2
dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 2.2
nrm = np.stack((-dx, -dy, np.ones_like(dx)), axis=-1); nrm /= np.linalg.norm(nrm, axis=-1)[..., None]
rgba = np.ones((N, N, 4), np.float32); rgba[..., :3] = nrm * .5 + .5
fur_normal = save_image('T_Dog_FurNormal', rgba)
rgba = np.ones((N, N, 4), np.float32); rgba[..., :3] = detail[..., None]
fur_detail = save_image('T_Dog_FurDetail', rgba)

CELLS = (4, 2)
atlas = np.zeros((N, N, 4), np.float32)
cw, ch = N // CELLS[0], N // CELLS[1]
for cy in range(CELLS[1]):
    for cx in range(CELLS[0]):
        v = np.linspace(0, 1, ch)[:, None]; u = np.linspace(0, 1, cw)[None, :]
        alpha = np.zeros((ch, cw), np.float32); shade = np.zeros((ch, cw), np.float32)
        locks = 7 + int(rng.integers(0, 4))
        for s_i in range(locks):
            root = .10 + .80 * (s_i + rng.random()) / locks
            spread = (root - .5) * (.30 + .45 * rng.random())
            wave = (rng.random() - .5) * .12
            length = .62 + .38 * rng.random()
            width = .045 + .035 * rng.random()
            center = root + spread * v ** 1.4 + wave * np.sin(v * 4.0 + s_i)
            taper = np.clip((length - v) / length, 0, 1)
            w = width * (.30 + .70 * taper ** .7)
            d = np.abs(u - center)
            a = smoothstep(w, w * .45, d) * smoothstep(0, .08, length - v)
            tone = .80 + .18 * rng.random()
            alpha = np.maximum(alpha, a)
            shade = np.where(a > .15, np.maximum(shade, tone * (.92 + .08 * np.clip(1 - d / np.maximum(w, 1e-4), 0, 1))), shade)
        lines = 1 - .08 * (np.sin(u * 400 + np.sin(v * 9) * 3) > .6)
        tip = .80 + .34 * np.clip(v, 0, 1)
        rgb = np.clip(np.where(shade > 0, shade, .85) * tip * lines, 0, 1)[..., None]
        block = atlas[cy * ch:(cy + 1) * ch, cx * cw:(cx + 1) * cw]
        block[..., :3] = np.repeat(rgb, 3, axis=-1); block[..., 3] = alpha
fur_atlas = save_image('T_Dog_FurAtlas', atlas)

# ---------------------------------------------------------------- base mesh: import, align, scale, decimate
bpy.ops.import_scene.gltf(filepath=str(ART / 'Source/generated' / MESH))
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects:
    if o.type != 'MESH':
        bpy.data.objects.remove(o, do_unlink=True)
skin = meshes[0]
for o in meshes[1:]:
    bpy.data.objects.remove(o, do_unlink=True)
skin.name = 'Skin'; skin.data.name = 'SkinMesh'
bpy.ops.object.select_all(action='DESELECT'); skin.select_set(True); bpy.context.view_layer.objects.active = skin
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
V = np.array([v.co for v in skin.data.vertices], np.float64)
# Nose points -Y in the generated frame: rotate so it points +X, then ground and scale.
R = np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]], float)
V = V @ R.T
V[:, 2] -= V[:, 2].min()
xs = V[:, 0]; L = xs.max() - xs.min()
mid = (xs > xs.min() + .35 * L) & (xs < xs.min() + .65 * L)
V *= BACK_HEIGHT_CM / V[mid, 2].max()
V[:, 1] -= np.median(V[mid, 1])       # centre the trunk on the x axis
for v, p in zip(skin.data.vertices, V):
    v.co = p
skin.data.update()
log('IMPORTED', MESH, 'tris', len(skin.data.polygons), 'length', round(float(V[:, 0].max() - V[:, 0].min()), 1), 'height', round(float(V[:, 2].max()), 1))
current = sum(len(p.vertices) - 2 for p in skin.data.polygons)
dec = skin.modifiers.new('Budget', 'DECIMATE'); dec.ratio = min(1.0, TARGET_TRIS / float(current))
bpy.ops.object.modifier_apply(modifier=dec.name)
for p in skin.data.polygons:
    p.use_smooth = True
skin.data.update()
V = np.array([v.co for v in skin.data.vertices], np.float64)
log('DECIMATED tris', sum(len(p.vertices) - 2 for p in skin.data.polygons))


# ---------------------------------------------------------------- measurements
def measure(V):
    m = {}
    zs = V[:, 2]; xs = V[:, 0]; ys = V[:, 1]
    sel = (zs > 10) & (zs < 14)
    P = V[sel][:, :2]; xm = np.median(P[:, 0])
    legs = {}
    for name, mk in (('fore', P[:, 0] > xm), ('hind', P[:, 0] <= xm)):
        for side, ms in (('L', P[:, 1] > 0), ('R', P[:, 1] <= 0)):
            q = P[mk & ms]
            legs[name + side] = q.mean(0) if len(q) else np.array([xm, 0.0])
    m['legs'] = legs
    m['fore_x'] = float((legs['foreL'][0] + legs['foreR'][0]) / 2)
    m['hind_x'] = float((legs['hindL'][0] + legs['hindR'][0]) / 2)
    m['fore_y'] = float((abs(legs['foreL'][1]) + abs(legs['foreR'][1])) / 2)
    m['hind_y'] = float((abs(legs['hindL'][1]) + abs(legs['hindR'][1])) / 2)
    nose_i = int(np.argmax(xs)); m['nose'] = V[nose_i].copy()
    head = V[xs > xs.max() - 26]
    m['head_c'] = head.mean(0)
    m['back'] = float(zs[(xs > m['hind_x'] + 4) & (xs < m['fore_x'] - 10)].max())
    trunk_top = []
    for x0 in np.arange(m['hind_x'] - 10, m['fore_x'] + 10, 5):
        s = (xs >= x0) & (xs < x0 + 5)
        if s.sum() > 20:
            trunk_top.append((x0 + 2.5, float(zs[s].max())))
    m['topline'] = trunk_top
    tail = V[(xs < m['hind_x'] - 8) & (zs > 8)]
    m['tail'] = tail
    return m


M = measure(V)
log('MEASURE fore_x %.1f hind_x %.1f fore_y %.1f hind_y %.1f back %.1f nose %s' % (M['fore_x'], M['hind_x'], M['fore_y'], M['hind_y'], M['back'], np.round(M['nose'], 1)))


# ---------------------------------------------------------------- photo projection (in the photographed pose)
photo = None
if PHOTO.exists():
    img = bpy.data.images.load(str(PHOTO)); img.colorspace_settings.name = 'sRGB'
    W_, H_ = img.size
    px = np.array(img.pixels[:], np.float32).reshape(H_, W_, 4)   # row 0 = bottom
    photo = px
    alpha = px[..., 3] > .5
    rows, cols = np.where(alpha)
    PB = (cols.min(), rows.min(), cols.max(), rows.max())
    log('PHOTO', W_, H_, 'bbox', PB)
skin.data.update()
VN0 = np.array([v.normal for v in skin.data.vertices], np.float64)
view = np.array((1.0, 0, 0))                 # camera on +X looking at the face
Y0, Y1 = V[:, 1].min(), V[:, 1].max(); Z0, Z1 = V[:, 2].min(), V[:, 2].max()


def photo_uv(p):
    if photo is None:
        return (0.0, 0.0)
    hb = (PB[3] - PB[1]) / H_; sc = hb / (Z1 - Z0)
    u = (PB[0] + PB[2]) / 2 / W_ + (p[1] - (Y0 + Y1) / 2) * sc * H_ / W_
    v = PB[1] / H_ + (p[2] - Z0) * sc
    return (float(u), float(v))


def photo_sample(p, n):
    """(linear colour or None, weight) from the photo; far side mirrored."""
    if photo is None:
        return None, 0.0
    facing = float(np.dot(n, view))
    q = p if facing >= 0 else np.array((p[0], -p[1], p[2]))
    u, v = photo_uv(q)
    x = int(np.clip(u * W_, 0, W_ - 1)); y = int(np.clip(v * H_, 0, H_ - 1))
    c = photo[y, x]
    if c[3] < .5:
        return None, 0.0
    return tuple(k * .88 for k in srgb_to_linear(tuple(float(k) for k in c[:3]))), ease((abs(facing) - .15) / .45)


PROJ_UV = []; PROJ_COL = []; PROJ_W = []
HEAD_X0 = M['fore_x'] + 8                      # dark photo pixels are genuine only on the face (eyes, nose, mouth)
TAIL_X1 = M['hind_x'] - 13                     # the tail hangs behind the body in the photograph: nothing to project
for i in range(len(V)):
    facing = float(np.dot(VN0[i], view))
    q = V[i] if facing >= 0 else np.array((V[i][0], -V[i][1], V[i][2]))
    PROJ_UV.append(photo_uv(q))
    c, w = photo_sample(V[i], VN0[i])
    if c is not None and V[i][0] < TAIL_X1:
        c, w = None, 0.0
    elif c is not None and V[i][0] < HEAD_X0 and (.2126 * c[0] + .7152 * c[1] + .0722 * c[2]) < .16:
        # Shadowed chest, inner legs and ground between the legs: the side
        # projection would smear them across the flank and belly as dark blotches.
        c, w = None, 0.0
    PROJ_COL.append(c); PROJ_W.append(w if c is not None else 0.0)

# ---------------------------------------------------------------- pose normalisation (warps)
# Legs: slide each leg so the stance is symmetric; head: turn the muzzle to face +X.
FORE_X, HIND_X = M['fore_x'], M['hind_x']
LEG_Y_F, LEG_Y_H = max(8.0, M['fore_y']), max(8.0, M['hind_y'])
BACK = M['back']
for name, (cx_, cy_) in M['legs'].items():
    tx = FORE_X if name.startswith('fore') else HIND_X
    ty = (LEG_Y_F if name.startswith('fore') else LEG_Y_H) * (1 if name.endswith('L') else -1)
    d = np.array((tx - cx_, ty - cy_))
    if np.linalg.norm(d) < .3:
        continue
    for i, p in enumerate(V):
        if p[2] > .62 * BACK:
            continue
        if (p[1] > 0) != name.endswith('L'):
            continue
        if abs(p[0] - cx_) > 16:
            continue
        if name.startswith('fore') and p[0] < (FORE_X + HIND_X) / 2:
            continue
        if name.startswith('hind') and p[0] > (FORE_X + HIND_X) / 2:
            continue
        w = ease((.62 * BACK - p[2]) / (.45 * BACK))
        V[i, 0] += d[0] * w; V[i, 1] += d[1] * w
# Head: measured turn around the neck base.
neck_base = np.array((FORE_X - 2, 0.0, BACK + 2))
nose_dir = M['nose'] - neck_base
yaw = math.atan2(nose_dir[1], nose_dir[0])
measured_pitch = math.atan2(nose_dir[2], math.hypot(nose_dir[0], nose_dir[1]))
pitch = measured_pitch - math.radians(8)         # rotate about +Y by (measured - desired): carry the nose 8 deg above level
log('HEAD turn yaw %.1f measured pitch %.1f correction %.1f deg' % (math.degrees(yaw), math.degrees(measured_pitch), math.degrees(pitch)))
Rz = Matrix.Rotation(-yaw, 3, 'Z'); Ry = Matrix.Rotation(pitch, 3, 'Y')
for i, p in enumerate(V):
    if p[0] < FORE_X - 12 or p[2] < BACK - 8:
        continue
    w = ease((p[0] - (FORE_X - 10)) / 14) if p[2] > BACK + 2 else ease((p[0] - (FORE_X - 10)) / 14) * ease((p[2] - (BACK - 8)) / 10)
    if w <= 0:
        continue
    rel = Vector(p - neck_base)
    turned = (Ry @ (Rz @ rel)) if w >= 1 else rel.lerp(Ry @ (Rz @ rel), w)
    V[i] = neck_base + np.array(turned)
# Tail carriage: the generated tail hangs about 30 degrees below level and
# curls to the photographed side, which puts the tip inside the grass sward.
# Carry it level with a slight droop and a lifted tip, centred on the spine.
TAIL_ROOT_X = HIND_X - 13
tail_sel = (V[:, 0] < TAIL_ROOT_X - 1) & (V[:, 2] > 12)
if tail_sel.sum() > 100:
    tail_pts = V[tail_sel]
    near = tail_pts[tail_pts[:, 0] > tail_pts[:, 0].max() - 4].mean(0)
    far = tail_pts[tail_pts[:, 0] < tail_pts[:, 0].min() + 6].mean(0)
    pivot = np.array((TAIL_ROOT_X, float(near[1]), float(near[2])))
    d0 = far - near; d0 /= np.linalg.norm(d0)
    d1 = np.array((-.99, 0.0, -.12)); d1 /= np.linalg.norm(d1)
    axis = np.cross(d0, d1); sin_a = np.linalg.norm(axis); cos_a = float(np.dot(d0, d1))
    if sin_a > 1e-4:
        axis /= sin_a
        K = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
        ROT = np.eye(3) + sin_a * K + (1 - cos_a) * K @ K
    else:
        ROT = np.eye(3)
    span = float(np.linalg.norm(far - near))
    for i in np.where(V[:, 0] < TAIL_ROOT_X + 4)[0]:
        if V[i, 2] <= 12:
            continue
        w = ease((TAIL_ROOT_X - V[i, 0]) / 5)
        if w <= 0:
            continue
        rel = V[i] - pivot
        turned = ROT @ rel
        t = max(0.0, min(1.0, float(np.linalg.norm(rel)) / max(span, 1)))
        turned[1] -= pivot[1] * t          # straighten the side curl toward the spine
        turned[2] += 6.0 * t * t            # lifted tip
        V[i] = pivot + rel * (1 - w) + turned * w
    log('TAIL carriage from %s to level (span %.1f cm, %d vertices)' % (np.round(d0, 2), span, int(tail_sel.sum())))
V[:, 2] -= V[:, 2].min()
for v, p in zip(skin.data.vertices, V):
    v.co = p
skin.data.update()
M = measure(V)
FORE_X, HIND_X, BACK = M['fore_x'], M['hind_x'], M['back']
NOSE = M['nose']
log('NORMALISED fore_x %.1f hind_x %.1f back %.1f nose %s length %.1f' % (FORE_X, HIND_X, BACK, np.round(NOSE, 1), V[:, 0].max() - V[:, 0].min()))

# ---------------------------------------------------------------- rig
HEAD_BASE = np.array((FORE_X + 6, 0, BACK + 14))
TAIL_PTS = []
tail = M['tail']
if len(tail) > 200:
    # Chain points sampled along the carried tail's centreline, by distance
    # rather than vertex count so the dense rump does not pull the chain.
    tail = tail[tail[:, 0] < TAIL_ROOT_X - 1] if (tail[:, 0] < TAIL_ROOT_X - 1).sum() > 100 else tail
    x_hi, x_lo = float(tail[:, 0].max()), float(tail[:, 0].min())
    for frac in (0, .25, .5, .75, 1.0):
        xt = x_hi - frac * (x_hi - x_lo); seg = tail[np.abs(tail[:, 0] - xt) < max(3.0, (x_hi - x_lo) * .08)]
        TAIL_PTS.append(seg.mean(0) if len(seg) else np.array((xt, 0.0, BACK - 12)))
    TAIL_PTS[0] = np.array((HIND_X - 8, 0, BACK - 6))
else:
    TAIL_PTS = [np.array(p) for p in ((HIND_X - 8, 0, BACK - 6), (HIND_X - 20, -1, BACK - 12), (HIND_X - 32, -2, BACK - 22), (HIND_X - 42, -2.5, BACK - 32), (HIND_X - 50, -2, BACK - 36))]
TAIL = [tuple(float(x) for x in p) for p in TAIL_PTS]
SPEC = [('root', (0, 0, 0), (0, 0, 8), None),
        ('pelvis', (HIND_X - 4, 0, .74 * BACK), (HIND_X + 14, 0, .76 * BACK), 'root'),
        ('spine', (HIND_X + 14, 0, .76 * BACK), (FORE_X - 2, 0, .82 * BACK), 'pelvis'),
        ('neck', (FORE_X - 2, 0, .82 * BACK), tuple(HEAD_BASE), 'spine'),
        ('head', tuple(HEAD_BASE), (NOSE[0] - 4, 0, NOSE[2] - 1), 'neck'),
        ('jaw', (HEAD_BASE[0] + 8, 0, HEAD_BASE[2] - 6), (NOSE[0] - 2, 0, NOSE[2] - 9), 'head')]
for side, label in ((-1, 'R'), (1, 'L')):
    yf, yh = side * LEG_Y_F, side * LEG_Y_H
    SPEC.extend([(f'fore_upper_{label}', (FORE_X + 3, yf, .80 * BACK), (FORE_X, yf, .50 * BACK), 'spine'),
                 (f'fore_lower_{label}', (FORE_X, yf, .50 * BACK), (FORE_X + 3, yf, 6), f'fore_upper_{label}'),
                 (f'fore_paw_{label}', (FORE_X + 3, yf, 6), (FORE_X + 11, yf, 3.8), f'fore_lower_{label}'),
                 (f'hind_upper_{label}', (HIND_X - 4, yh, .75 * BACK), (HIND_X + 5, yh, .50 * BACK), 'pelvis'),
                 (f'hind_lower_{label}', (HIND_X + 5, yh, .50 * BACK), (HIND_X - 8, yh, 14), f'hind_upper_{label}'),
                 (f'hind_paw_{label}', (HIND_X - 8, yh, 14), (HIND_X - 1, yh, 3.8), f'hind_lower_{label}'),
                 (f'ear_{label}', (HEAD_BASE[0] + 3, side * 9.5, HEAD_BASE[2] + 6), (HEAD_BASE[0] - 1, side * 13, HEAD_BASE[2] - 13), 'head')])
for i in range(4):
    SPEC.append((f'tail_{i + 1}', TAIL[i], TAIL[i + 1], 'pelvis' if i == 0 else f'tail_{i}'))
arm = bpy.data.armatures.new('CompanionDog_Skeleton'); rig = bpy.data.objects.new('CompanionDogRig', arm)
bpy.context.collection.objects.link(rig)
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for name, a, b, parent in SPEC:
    bone = arm.edit_bones.new(name); bone.head = a; bone.tail = b; bone.use_connect = False
    if parent:
        bone.parent = arm.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT'); rig.show_in_front = True
SEGMENTS = {name: (Vector(a), Vector(b)) for name, a, b, parent in SPEC}


def distance_segment(p, a, b):
    delta = b - a; t = max(0, min(1, (p - a).dot(delta) / delta.length_squared)); return (p - a - delta * t).length


def bone_weights(p):
    side = 'L' if p.y >= 0 else 'R'
    fore = ease((.84 * BACK - p.z) / (.38 * BACK)) * ease((abs(p.y) - 2.5) / 5) * math.exp(-((p.x - (FORE_X + 2)) / 17) ** 4)
    hind = ease((.88 * BACK - p.z) / (.40 * BACK)) * ease((abs(p.y) - 2.5) / 5) * math.exp(-((p.x - (HIND_X - 1)) / 19) ** 4)
    head = ease((p.x - (HEAD_BASE[0] - 4)) / 14) * ease((p.z - (BACK - 2)) / 16)
    neck = ease((p.x - (FORE_X - 6)) / 20) * ease((p.z - (.74 * BACK)) / 18) * (1 - head)
    tail = ease(((HIND_X - 6) - p.x) / 8) * ease((p.z - 12) / 10)
    base = max(0, 1 - fore - hind - head - neck - tail)
    values = []
    for amount, candidates in [(fore, [f'fore_upper_{side}', f'fore_lower_{side}', f'fore_paw_{side}']),
                               (hind, [f'hind_upper_{side}', f'hind_lower_{side}', f'hind_paw_{side}']),
                               (head, ['head']), (neck, ['neck']), (tail, ['tail_1', 'tail_2', 'tail_3', 'tail_4']), (base, ['pelvis', 'spine'])]:
        if amount < .0001:
            continue
        raw = [(1 / max(distance_segment(p, *SEGMENTS[n]), 2) ** 3, n) for n in candidates]
        den = sum(a for a, n in raw)
        values.extend((amount * a / den, n) for a, n in raw)
    values = sorted(values, reverse=True)[:4]
    total = sum(x[0] for x in values)
    return [(w / total, n) for w, n in values]


def weight_object(o):
    groups = {n: o.vertex_groups.get(n) or o.vertex_groups.new(name=n) for n in SEGMENTS}
    for v in o.data.vertices:
        for w, n in bone_weights(o.matrix_world @ v.co):
            groups[n].add([v.index], w, 'REPLACE')


weight_object(skin)

# ---------------------------------------------------------------- coat colour: baked photo + zoning
VN = np.array([v.normal for v in skin.data.vertices], np.float64)


def zoning_color(p, n):
    x, y, z = p; nz = n[2]; ax = abs(y)
    dorsal = ease((nz + .15) / .75)
    base = GOLD_LIGHT * (1 - dorsal) + GOLD * dorsal
    ventral = ease((-nz + .1) / .6) * ease((.9 * BACK - z) / 14)
    legs = ease((24 - z) / 12)
    chest = ease((x - (FORE_X - 12)) / 14) * ease((.9 * BACK - z) / 12) * ease((16 - ax) / 6)
    cream = max(ventral, legs * .9, chest)
    col = base * (1 - cream) + CREAM * cream
    muzzle = ease((x - (NOSE[0] - 16)) / 5) * ease((z - (NOSE[2] - 14)) / 5)
    brow = ease((x - (NOSE[0] - 22)) / 4) * ease((z - (NOSE[2] - 4)) / 4) * ease((9 - ax) / 4) * .9
    white = max(muzzle, brow) * .55   # the photo already carries the pale face; keep the zoning subtle
    col = col * (1 - white) + WHITE * white
    return tuple(float(max(0, min(1, c))) for c in col)


SKIN_COLOR = []
for i in range(len(V)):
    zc = zoning_color(V[i], VN[i]); pc = PROJ_COL[i]; w = PROJ_W[i]
    SKIN_COLOR.append(tuple(zc[k] * (1 - w) + (pc[k] if pc else zc[k]) * w for k in range(3)))


def coat_color(p, n, nearest=None):
    """Card colour: the skin colour beneath the root, slightly lighter (feathering is paler)."""
    base = SKIN_COLOR[nearest] if nearest is not None else zoning_color(np.asarray(p, float), np.asarray(n, float))
    return tuple(min(1.0, c * 1.03) for c in base) + (1.0,)


# Skin UVs: a smart unwrap for the baked albedo, plus projection/mirror UVs for the bake source.
bpy.ops.object.select_all(action='DESELECT'); skin.select_set(True); bpy.context.view_layer.objects.active = skin
me = skin.data
me.uv_layers.new(name='UVMap')
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=.003)
bpy.ops.object.mode_set(mode='OBJECT')
me.uv_layers.new(name='Proj'); me.uv_layers.new(name='RestZ')
me.color_attributes.new(name='CoatColor', type='FLOAT_COLOR', domain='CORNER')
me.color_attributes.new(name='ProjWeight', type='FLOAT_COLOR', domain='CORNER')
attr = me.color_attributes['CoatColor']; wattr = me.color_attributes['ProjWeight']; me.color_attributes.active_color = attr
proj = me.uv_layers['Proj']; rz = me.uv_layers['RestZ']
vert_zone = [zoning_color(V[i], VN[i]) for i in range(len(V))]
for loop in me.loops:
    i = loop.vertex_index
    attr.data[loop.index].color = vert_zone[i] + (1.0,)
    w = PROJ_W[i]
    wattr.data[loop.index].color = (w, w, w, 1.0)
    proj.data[loop.index].uv = PROJ_UV[i]
    rz.data[loop.index].uv = (V[i][2] / 120, 0)
me.uv_layers.active = me.uv_layers['UVMap']

# Bake source material: photo (projection UVs) mixed with zoning by ProjWeight -> emission.
bake_mat = bpy.data.materials.new('BakeSource'); bake_mat.use_nodes = True
nt = bake_mat.node_tree; nodes, links = nt.nodes, nt.links
for n_ in list(nodes):
    nodes.remove(n_)
out = nodes.new('ShaderNodeOutputMaterial'); emit = nodes.new('ShaderNodeEmission'); links.new(emit.outputs[0], out.inputs['Surface'])
zone = nodes.new('ShaderNodeVertexColor'); zone.layer_name = 'CoatColor'
wnode = nodes.new('ShaderNodeVertexColor'); wnode.layer_name = 'ProjWeight'
mix = nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MIX'
links.new(wnode.outputs['Color'], mix.inputs['Fac']); links.new(zone.outputs['Color'], mix.inputs['Color1'])
if photo is not None:
    tex = nodes.new('ShaderNodeTexImage'); tex.image = img; tex.extension = 'CLIP'
    uvn = nodes.new('ShaderNodeUVMap'); uvn.uv_map = 'Proj'; links.new(uvn.outputs['UV'], tex.inputs['Vector'])
    dim = nodes.new('ShaderNodeMixRGB'); dim.blend_type = 'MULTIPLY'; dim.inputs[0].default_value = 1; dim.inputs[2].default_value = (.88, .88, .88, 1)
    links.new(tex.outputs['Color'], dim.inputs[1]); links.new(dim.outputs[0], mix.inputs['Color2'])
else:
    links.new(zone.outputs['Color'], mix.inputs['Color2'])
links.new(mix.outputs['Color'], emit.inputs['Color'])
bake_img = bpy.data.images.new('T_Dog_Albedo', width=2048, height=2048, alpha=False); bake_img.colorspace_settings.name = 'sRGB'
bake_node = nodes.new('ShaderNodeTexImage'); bake_node.image = bake_img; nodes.active = bake_node
me.materials.clear(); me.materials.append(bake_mat)
scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = 1
scene.render.bake.use_selected_to_active = False; scene.render.bake.margin = 8
bpy.ops.object.bake(type='EMIT')
albedo_path = str(ART / 'Textures/T_Dog_Albedo.png')
bake_img.filepath_raw = albedo_path; bake_img.file_format = 'PNG'; bake_img.save()
bake_img.source = 'FILE'; bake_img.filepath = albedo_path; bake_img.reload(); bake_img.colorspace_settings.name = 'sRGB'
log('BAKED albedo', albedo_path)

# ---------------------------------------------------------------- final materials
MATS = {}
coat = bpy.data.materials.new('DM_Coat'); coat.use_nodes = True
nodes, links = coat.node_tree.nodes, coat.node_tree.links
p = nodes.get('Principled BSDF'); p.inputs['Roughness'].default_value = .62
alb = nodes.new('ShaderNodeTexImage'); alb.image = bake_img
uvn = nodes.new('ShaderNodeUVMap'); uvn.uv_map = 'UVMap'; links.new(uvn.outputs['UV'], alb.inputs['Vector'])
det = nodes.new('ShaderNodeTexImage'); det.image = fur_detail; det.projection = 'BOX'; det.projection_blend = .35
coords = nodes.new('ShaderNodeTexCoord'); mapping = nodes.new('ShaderNodeMapping'); mapping.inputs['Scale'].default_value = (1 / 42, 1 / 42, 1 / 42)
links.new(coords.outputs['Object'], mapping.inputs['Vector']); links.new(mapping.outputs['Vector'], det.inputs['Vector'])
mixd = nodes.new('ShaderNodeMixRGB'); mixd.blend_type = 'MULTIPLY'; mixd.inputs[0].default_value = .6
links.new(alb.outputs['Color'], mixd.inputs[1]); links.new(det.outputs['Color'], mixd.inputs[2]); links.new(mixd.outputs[0], p.inputs['Base Color'])
nt_ = nodes.new('ShaderNodeTexImage'); nt_.image = fur_normal; nt_.projection = 'BOX'; nt_.projection_blend = .35
links.new(mapping.outputs['Vector'], nt_.inputs['Vector'])
nm = nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = .4
links.new(nt_.outputs['Color'], nm.inputs['Color']); links.new(nm.outputs['Normal'], p.inputs['Normal'])
p.inputs['Subsurface Weight'].default_value = .15; p.inputs['Subsurface Radius'].default_value = (1.2, .6, .3)
p.inputs['Sheen Weight'].default_value = .12; p.inputs['Sheen Tint'].default_value = (1, .85, .6, 1)
MATS['Coat'] = coat
card = bpy.data.materials.new('DM_FurCard'); card.use_nodes = True
nodes, links = card.node_tree.nodes, card.node_tree.links
p = nodes.get('Principled BSDF'); p.inputs['Roughness'].default_value = .55
vc = nodes.new('ShaderNodeVertexColor'); vc.layer_name = 'CoatColor'
tex = nodes.new('ShaderNodeTexImage'); tex.image = fur_atlas; tex.interpolation = 'Cubic'
uvn = nodes.new('ShaderNodeUVMap'); uvn.uv_map = 'UVMap'; links.new(uvn.outputs['UV'], tex.inputs['Vector'])
mixc = nodes.new('ShaderNodeMixRGB'); mixc.blend_type = 'MULTIPLY'; mixc.inputs[0].default_value = 1
links.new(vc.outputs['Color'], mixc.inputs[1]); links.new(tex.outputs['Color'], mixc.inputs[2]); links.new(mixc.outputs[0], p.inputs['Base Color'])
links.new(tex.outputs['Alpha'], p.inputs['Alpha'])
p.inputs['Sheen Weight'].default_value = .15; p.inputs['Sheen Tint'].default_value = (1, .85, .6, 1)
geo = nodes.new('ShaderNodeNewGeometry'); neg = nodes.new('ShaderNodeVectorMath'); neg.operation = 'SCALE'; neg.inputs[3].default_value = -1
links.new(geo.outputs['Normal'], neg.inputs[0])
pick = nodes.new('ShaderNodeMix'); pick.data_type = 'VECTOR'
links.new(geo.outputs['Backfacing'], pick.inputs['Factor']); links.new(geo.outputs['Normal'], pick.inputs[4]); links.new(neg.outputs[0], pick.inputs[5])
links.new(pick.outputs[1], p.inputs['Normal'])
trans = nodes.new('ShaderNodeBsdfTranslucent'); links.new(mixc.outputs[0], trans.inputs['Color']); links.new(pick.outputs[1], trans.inputs['Normal'])
tmix = nodes.new('ShaderNodeMixShader'); tmix.inputs[0].default_value = .22
links.new(p.outputs['BSDF'], tmix.inputs[1]); links.new(trans.outputs[0], tmix.inputs[2])
rays = nodes.new('ShaderNodeLightPath'); transparent = nodes.new('ShaderNodeBsdfTransparent'); sm = nodes.new('ShaderNodeMixShader')
links.new(rays.outputs['Is Shadow Ray'], sm.inputs[0]); links.new(tmix.outputs[0], sm.inputs[1]); links.new(transparent.outputs[0], sm.inputs[2])
links.new(sm.outputs[0], nodes.get('Material Output').inputs['Surface'])
card.blend_method = 'HASHED'; card.use_backface_culling = False
MATS['FurCard'] = card
me.materials.clear(); me.materials.append(MATS['Coat'])
me.color_attributes.remove(me.color_attributes['ProjWeight'])
me.uv_layers.remove(me.uv_layers['Proj'])

# ---------------------------------------------------------------- fur cards
GRAVITY = np.array((0, 0, -1.0))


def fur_region(p, n):
    """Feathering only: (length_cm, density_cards_per_cm2, droop, standing_probability)."""
    x, y, z = p; ax = abs(y); nz = n[2]; nx = n[0]
    if x > NOSE[0] - 17 and z > NOSE[2] - 16:        # muzzle and eyes: no cards
        return 0, 0, 0, 0
    if x > HEAD_BASE[0] - 2 and z > BACK + 6 and ax < 8:   # skull: none (baked albedo)
        return 0, 0, 0, 0
    if x > HEAD_BASE[0] - 6 and ax > 8.5 and z > BACK - 8:  # ears and cheeks: soft locks down
        return 3.2, .30, .95, .05
    if x < HIND_X - 10 and z > 10:                   # tail plume
        return 9.5 if nz > -.3 else 7.5, .40, .72, .12
    if x > FORE_X - 6 and .70 * BACK < z < BACK + 4 and nz < .35:   # ruff below the neck
        return 5.0, .18, .70, .08
    legs_z = z < .5 * BACK and (abs(x - FORE_X - 2) < 9 or abs(x - HIND_X) < 9)
    if legs_z:
        if nx < -.25 and z > 8:                      # feathering behind the legs
            return 5.5, .20, .95, .08
        return 0, 0, 0, 0
    if z < 9:
        return 0, 0, 0, 0
    if nz < -.15 and z < .8 * BACK:                  # chest, brisket, belly skirt
        return 7.5 if x > 0 else 6.0, .20, .92, .08
    if x < HIND_X + 6 and z < .78 * BACK and ax > 7:  # pants
        return 5.5, .20, .85, .08
    if x > FORE_X - 8 and z < .82 * BACK and ax > 7:  # upper foreleg feathering
        return 4.5, .18, .85, .08
    return 0, 0, 0, 0                                # back and sides: baked albedo only


def sample_surface(o, count):
    me_ = o.data; me_.calc_loop_triangles()
    tris = np.array([[v for v in t.vertices] for t in me_.loop_triangles], np.int32)
    Vv = np.array([v.co for v in me_.vertices], np.float32); VNn = np.array([v.normal for v in me_.vertices], np.float32)
    a, b, c = Vv[tris[:, 0]], Vv[tris[:, 1]], Vv[tris[:, 2]]
    area = .5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    pick = RNG.choice(len(tris), size=count, p=area / area.sum())
    r1, r2 = RNG.random(count), RNG.random(count)
    s = np.sqrt(r1); w0, w1, w2 = 1 - s, s * (1 - r2), s * r2
    P = (a[pick] * w0[:, None] + b[pick] * w1[:, None] + c[pick] * w2[:, None])
    Nn = (VNn[tris[pick, 0]] * w0[:, None] + VNn[tris[pick, 1]] * w1[:, None] + VNn[tris[pick, 2]] * w2[:, None])
    Nn /= np.linalg.norm(Nn, axis=1)[:, None]
    return P, Nn, count / float(area.sum())


def build_fur_cards():
    P, Nn, rho = sample_surface(skin, 24000)
    log('SKIN_AREA_CM2', round(float(24000 / rho)))
    verts, faces, uvs, colors, custom_normals, weights = [], [], [], [], [], []
    kd = KDTree(len(skin.data.vertices))
    for v in skin.data.vertices:
        kd.insert(v.co, v.index)
    kd.balance()
    skin_weights = {v.index: [(g.weight, skin.vertex_groups[g.group].name) for g in v.groups] for v in skin.data.vertices}
    cards = 0; rows = 4
    for p, n in zip(P, Nn):
        Lc, density, droop, standing = fur_region(p, n)
        if Lc <= 0 or RNG.random() > density * CARD_DENSITY / rho:
            continue
        Lc *= CARD_LENGTH * (.75 + .5 * RNG.random()); W = Lc * (.80 + .30 * RNG.random())
        back = np.array((-1.0, 0, 0)); back -= n * np.dot(back, n)
        if np.linalg.norm(back) < 1e-3:
            back = np.array((0, 1.0, 0)) - n * n[1]
        back /= np.linalg.norm(back)
        d = back * (1 - droop) + GRAVITY * droop + n * .16; d /= np.linalg.norm(d)
        t = np.cross(n, d); t /= max(1e-6, np.linalg.norm(t))
        roll = (RNG.random() - .5) * math.radians(110)
        if RNG.random() < standing:
            roll += math.radians(90) * (1 if RNG.random() < .5 else -1)
        w = t * math.cos(roll) + np.cross(d, t) * math.sin(roll)
        bend = GRAVITY * .18 * droop + n * .10
        base = len(verts)
        for k in range(rows):
            u = k / (rows - 1)
            pos = p + n * .3 + d * (u * Lc) + bend * (u * u * Lc); pos[2] = max(pos[2], .4)
            half = W * .5 * (1 - .4 * u)
            verts.append(pos - w * half); verts.append(pos + w * half)
        for k in range(rows - 1):
            a = base + 2 * k; faces.append((a, a + 1, a + 3, a + 2))
        cell = int(RNG.integers(0, CELLS[0] * CELLS[1])); cu, cv = cell % CELLS[0], cell // CELLS[0]
        u0, u1 = cu / CELLS[0], (cu + 1) / CELLS[0]; v0, v1 = cv / CELLS[1], (cv + 1) / CELLS[1]
        for k in range(rows):
            u = k / (rows - 1); uvs.append((u0, v0 + (v1 - v0) * u)); uvs.append((u1, v0 + (v1 - v0) * u))
        _, index, _ = kd.find(Vector(p)); weights.extend([skin_weights[index]] * (rows * 2))
        col = coat_color(p, n, index)
        colors.extend([col] * (rows * 2)); custom_normals.extend([tuple(n)] * (rows * 2))
        cards += 1
    me_ = bpy.data.meshes.new('FurCards'); me_.from_pydata(verts, [], faces); me_.update()
    o = bpy.data.objects.new('FurCards', me_); bpy.context.collection.objects.link(o)
    me_.materials.append(MATS['FurCard'])
    for poly in me_.polygons:
        poly.use_smooth = True
    me_.uv_layers.new(name='UVMap'); me_.uv_layers.new(name='RestZ')
    me_.color_attributes.new(name='CoatColor', type='FLOAT_COLOR', domain='CORNER')
    attr_ = me_.color_attributes['CoatColor']; me_.color_attributes.active_color = attr_
    uv = me_.uv_layers['UVMap']; uvz = me_.uv_layers['RestZ']
    for loop in me_.loops:
        attr_.data[loop.index].color = colors[loop.vertex_index]; uv.data[loop.index].uv = uvs[loop.vertex_index]; uvz.data[loop.index].uv = (0, 1)
    me_.normals_split_custom_set_from_vertices([Vector(c) for c in custom_normals])
    groups = {n_: o.vertex_groups.new(name=n_) for n_ in SEGMENTS}
    for i, wts in enumerate(weights):
        for wgt, name in wts:
            groups[name].add([i], wgt, 'REPLACE')
    log('FUR_CARDS', cards, 'triangles', cards * (rows - 1) * 2)
    return o


# Cards are opt-in since revision 7: in the engine they read as dark straw at
# every zoom the player can reach, so the baked coat ships alone for now.
fur = build_fur_cards() if '--cards' in ARGS else None

# ---------------------------------------------------------------- assemble, skin, animate
bpy.ops.object.select_all(action='DESELECT'); skin.select_set(True)
if fur:
    fur.select_set(True)
bpy.context.view_layer.objects.active = skin
if fur:
    bpy.ops.object.join()
dog = bpy.context.object; dog.name = 'SK_CompanionDog'; dog.data.name = 'CompanionDog_Geometry'
dog.parent = rig
mod = dog.modifiers.new('Skinning', 'ARMATURE'); mod.object = rig; mod.use_deform_preserve_volume = True
bpy.ops.object.vertex_group_limit_total(limit=4); bpy.ops.object.vertex_group_normalize_all(lock_active=False)
dog.data.calc_loop_triangles(); dog['rex_likeness_revision'] = 6

import importlib.util
spec = importlib.util.spec_from_file_location('rex_likeness', ART / 'Source/likeness.py')
likeness = importlib.util.module_from_spec(spec); spec.loader.exec_module(likeness)


def local_rotation(name, world_axis, radians):
    bone = rig.pose.bones[name]; axis = rig.data.bones[name].matrix_local.to_3x3().inverted() @ Vector(world_axis)
    bone.rotation_mode = 'QUATERNION'; bone.rotation_quaternion = Quaternion(axis, radians)


def make_idle(frames=91):
    for b in rig.pose.bones:
        b.rotation_mode = 'QUATERNION'; b.rotation_quaternion = Quaternion(); b.location = (0, 0, 0)
    action = bpy.data.actions.new('A_DogIdle'); rig.animation_data_create(); rig.animation_data.action = action
    for frame in range(1, frames + 1):
        phase = (frame - 1) / (frames - 1) * 2 * math.pi
        for b in rig.pose.bones:
            b.rotation_quaternion = Quaternion(); b.location = (0, 0, 0)
        local_rotation('spine', (0, 1, 0), math.sin(phase) * .006)
        local_rotation('neck', (0, 1, 0), math.sin(phase + .3) * .018)
        local_rotation('head', (0, 0, 1), math.sin(phase * .999) * .05)
        local_rotation('jaw', (0, 1, 0), .01 + math.sin(phase * 2) * .01)
        for side, label in ((-1, 'R'), (1, 'L')):
            local_rotation(f'ear_{label}', (1, 0, 0), math.sin(phase * 2 + side * .4) * .035)
        for i in range(1, 5):
            local_rotation(f'tail_{i}', (0, 0, 1), math.sin(phase * 2 - i * .48) * .12)
        for b in rig.pose.bones:
            b.keyframe_insert('rotation_quaternion', frame=frame); b.keyframe_insert('location', frame=frame)
    action.use_fake_user = True
    return action


clips = {'A_DogIdle': make_idle(), 'A_DogWalk': likeness.author_walk(rig)}
rig.animation_data.action = None
for b in rig.pose.bones:
    b.rotation_quaternion = Quaternion(); b.location = (0, 0, 0)
scene.frame_set(1)


def export(path, animation=False):
    bpy.ops.object.select_all(action='DESELECT'); dog.select_set(True); rig.select_set(True); bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH', 'ARMATURE'} if not animation else {'ARMATURE'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z', add_leaf_bones=False,
                             armature_nodetype='NULL', use_armature_deform_only=True, mesh_smooth_type='FACE', colors_type='SRGB',
                             bake_anim=animation, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0,
                             bake_anim_force_startend_keying=True, path_mode='STRIP')


if not PREVIEW_ONLY:
    export(ART / 'Exports/SK_CompanionDog.fbx')
    for name, action in clips.items():
        rig.animation_data.action = action; scene.frame_start = 1; scene.frame_end = 91 if name.endswith('Idle') else likeness.WALK_FRAMES
        export(ART / 'Exports' / (name + '.fbx'), True)
rig.animation_data.action = clips['A_DogIdle']; scene.frame_start = 1; scene.frame_end = 91; scene.frame_set(1)

lo = Vector(tuple(min(v.co[i] for v in dog.data.vertices) for i in range(3)))
hi = Vector(tuple(max(v.co[i] for v in dog.data.vertices) for i in range(3)))
report = {
    'asset': 'SK_CompanionDog', 'revision': 6,
    'source': 'Base mesh generated from the owner\'s photograph with a hosted image-to-3D model (Tencent Hunyuan3D shape generation), aligned, pose-normalised and decimated here; coat albedo baked from the same photograph projected onto the mesh and blended with authored zoning; rig, clips and fur cards authored procedurally',
    'generated_mesh': MESH, 'forward_axis': '+X', 'up_axis': '+Z', 'units': 'centimeters', 'root_motion': False,
    'reference_pose_bounds_cm': {'min': list(lo), 'max': list(hi)},
    'triangles': len(dog.data.loop_triangles), 'vertices': len(dog.data.vertices), 'bones': [n for n, a, b, p in SPEC],
    'materials': {m.name: {'color': list(PALETTE[m.name[3:]][0]), 'roughness': PALETTE[m.name[3:]][1], 'metallic': PALETTE[m.name[3:]][2]} for m in dog.data.materials},
    'used_materials': sorted({dog.data.materials[p.material_index].name for p in dog.data.polygons}),
    'textures': {'T_Dog_Albedo': 'baked 2048 sRGB albedo (photo projection + zoning) on UV0', 'T_Dog_FurAtlas': 'procedural lock atlas, 4x2 cells', 'T_Dog_FurDetail': 'procedural streak brightness', 'T_Dog_FurNormal': 'derived from the streak height'},
    'uv_channels': {'0': 'baked albedo islands on skin; fur atlas on cards', '1': 'rest-pose Z/120 on skin; (0,1) marks cards'},
    'skeleton_fit_cm': {'fore_x': FORE_X, 'hind_x': HIND_X, 'fore_y': LEG_Y_F, 'hind_y': LEG_Y_H, 'back_height': BACK, 'nose': [float(x) for x in NOSE]},
    'animations': {'A_DogIdle': {'duration_seconds': 3, 'fps': 30, 'loop': True}, 'A_DogWalk': likeness.walk_metadata(clips['A_DogWalk'])},
    'coat_vertex_attribute': 'CoatColor', 'max_bone_influences': 4, 'dog_name': 'Rex', 'name_on_tag': False,
    'fur_cards': {'enabled': fur is not None, 'length_scale': CARD_LENGTH, 'density_scale': CARD_DENSITY},
    'exports': {p_.name: hashlib.sha256(p_.read_bytes()).hexdigest() for p_ in (ART / 'Exports').glob('*.fbx')} if not PREVIEW_ONLY else {},
    'unreal_import_pending': True, 'runtime_review_pending': True,
}
if not PREVIEW_ONLY:
    (ART / 'dog_manifest.json').write_text(json.dumps(report, indent=2) + '\n')

# ---------------------------------------------------------------- review scene
world = bpy.data.worlds.new('Rex review'); scene.world = world; world.use_nodes = True
bg = world.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (.55, .68, .85, 1); bg.inputs['Strength'].default_value = .9
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 4.0; sun.angle = math.radians(2.5); sun.color = (1, .96, .9)
sun_o = bpy.data.objects.new('Sun', sun); bpy.context.collection.objects.link(sun_o); sun_o.rotation_euler = (math.radians(52), math.radians(8), math.radians(-35))
floor_mat = bpy.data.materials.new('PreviewOnly_Grass'); floor_mat.use_nodes = True
floor_mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.10, .16, .05, 1)
bpy.ops.mesh.primitive_plane_add(size=2000); floor = bpy.context.object; floor.name = 'PreviewOnly_Floor'; floor.data.materials.append(floor_mat)
cam_data = bpy.data.cameras.new('ReviewCamera'); cam = bpy.data.objects.new('ReviewCamera', cam_data); bpy.context.collection.objects.link(cam); scene.camera = cam
scene.render.engine = 'CYCLES'; scene.cycles.samples = 48; scene.cycles.use_denoising = True
scene.render.resolution_x = 1400; scene.render.resolution_y = 1050; scene.view_settings.exposure = .0
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
log('REX_V6_READY', json.dumps({'triangles': report['triangles'], 'vertices': report['vertices'], 'bounds': report['reference_pose_bounds_cm'], 'fit': report['skeleton_fit_cm']}))

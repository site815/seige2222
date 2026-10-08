"""Rex companion dog, revision 5: lofted golden-retriever anatomy with a card coat.

Run with the bundled Blender in background mode (or the bpy module):
    blender -b -P Tools/create_companion_dog_v2.py [-- --preview-only]

Everything is authored procedurally here; the owner's reference photographs are
used only as a visual guide for proportions and coat zoning and are never read,
copied or sampled. Centimeters; +X forward, +Z up; ground-level root; in-place
30 fps clips. The rig, bone names, clip names, asset paths and the walk stride
are identical to revision 4 so the runtime and save fingerprints are untouched.
"""
from pathlib import Path
import sys, math, json, hashlib, random
import bpy, bmesh
import numpy as np
from mathutils import Vector, Quaternion, Matrix
from mathutils.kdtree import KDTree

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
for d in ('Source', 'Exports', 'Textures', 'Previews'):
    (ART / d).mkdir(parents=True, exist_ok=True)
PREVIEW_ONLY = '--preview-only' in sys.argv
RNG = np.random.default_rng(8152225)

# ---------------------------------------------------------------- scene setup
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
scene.render.fps = 30
bpy.context.preferences.filepaths.save_version = 0


def srgb_to_linear(c):
    return tuple((x / 12.92) if x <= .04045 else ((x + .055) / 1.055) ** 2.4 for x in c)


def hexcolor(h):
    return srgb_to_linear(tuple(int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)))


# Coat zoning colours (linear). Taken by eye from the owner's photographs of an
# older golden retriever: rich gold on the back and ears, cream underneath and
# on the legs, a white muzzle and brow.
GOLD_DARK = np.array(hexcolor('CF9244'))
GOLD = np.array(hexcolor('E2AC5E'))
GOLD_LIGHT = np.array(hexcolor('ECC582'))
CREAM = np.array(hexcolor('F1E2C0'))
WHITE = np.array(hexcolor('F5EFE3'))

PALETTE = {  # name: (linear colour, roughness, metallic)
    'Coat': ((.55, .33, .14, 1), .62, 0),
    'FurCard': ((.55, .33, .14, 1), .55, 0),
    'Shell': ((.55, .33, .14, 1), .55, 0),
    'Nose': ((.012, .010, .009, 1), .38, 0),
    'Eye': ((.030, .014, .006, 1), .18, 0),
    'Iris': ((.090, .038, .012, 1), .25, 0),
    'Mouth': ((.030, .011, .012, 1), .55, 0),
    'Tongue': ((.46, .13, .15, 1), .42, 0),
    'Teeth': ((.78, .74, .62, 1), .30, 0),
    'Collar': ((.03, .30, .32, 1), .72, 0),
    'CollarEdge': ((.012, .022, .028, 1), .67, 0),
    'Metal': ((.56, .60, .58, 1), .26, .85),
}

# ---------------------------------------------------------------- textures
N = 1024


def save_image(name, rgba, srgb=False):
    img = bpy.data.images.new(name, width=rgba.shape[1], height=rgba.shape[0], alpha=True)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    img.pixels.foreach_set(np.ascontiguousarray(rgba, dtype=np.float32).reshape(-1))
    path = str(ART / 'Textures' / (name + '.png'))
    img.filepath_raw = path
    img.file_format = 'PNG'
    img.save()
    img.source = 'FILE'; img.filepath = path; img.reload()
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    return img


def value_noise(shape, cells, rng, octaves=4, stretch=(1, 1)):
    """Tileable fractal value noise; stretch elongates features along an axis."""
    out = np.zeros(shape, np.float32)
    amp = 1.0
    for o in range(octaves):
        cy = max(1, int(cells[0] * (2 ** o) / stretch[0]))
        cx = max(1, int(cells[1] * (2 ** o) / stretch[1]))
        grid = rng.random((cy, cx)).astype(np.float32)
        ys = np.linspace(0, cy, shape[0], endpoint=False)
        xs = np.linspace(0, cx, shape[1], endpoint=False)
        y0 = np.floor(ys).astype(int); x0 = np.floor(xs).astype(int)
        fy = (ys - y0)[:, None]; fx = (xs - x0)[None, :]
        fy = fy * fy * (3 - 2 * fy); fx = fx * fx * (3 - 2 * fx)
        y1 = (y0 + 1) % cy; x1 = (x0 + 1) % cx
        g = (grid[y0][:, x0] * (1 - fy) * (1 - fx) + grid[y0][:, x1] * (1 - fy) * fx +
             grid[y1][:, x0] * fy * (1 - fx) + grid[y1][:, x1] * fy * fx)
        out += g * amp
        amp *= .5
    out -= out.min(); out /= max(1e-6, out.max())
    return out


# Fur relief: streaks elongated along U (the head-to-tail direction on the body).
rng = np.random.default_rng(815)
streak = value_noise((N, N), (24, 24), rng, octaves=5, stretch=(1, 9))
fine = value_noise((N, N), (96, 96), rng, octaves=3, stretch=(1, 5))
height = np.clip(.55 * streak + .45 * fine, 0, 1)
detail = np.clip(.72 + (height - .5) * .62, .40, 1.0)
dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 2.2
dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 2.2
nrm = np.stack((-dx, -dy, np.ones_like(dx)), axis=-1)
nrm /= np.linalg.norm(nrm, axis=-1)[..., None]
rgba = np.ones((N, N, 4), np.float32); rgba[..., :3] = nrm * .5 + .5
fur_normal = save_image('T_Dog_FurNormal', rgba)
rgba = np.ones((N, N, 4), np.float32); rgba[..., :3] = detail[..., None]
fur_detail = save_image('T_Dog_FurDetail', rgba)

# Fur card atlas: 4 x 2 cells, each a soft lock of fur rooted at the bottom edge
# (wide, soft-edged strands that overlap into a mostly opaque clump).
CELLS = (4, 2)
atlas = np.zeros((N, N, 4), np.float32)
cw, ch = N // CELLS[0], N // CELLS[1]


def smoothstep(e0, e1, x):
    """Array-safe smoothstep; e0 > e1 reverses the ramp."""
    e0 = np.asarray(e0, np.float32); e1 = np.asarray(e1, np.float32)
    span = np.where(np.abs(e1 - e0) < 1e-6, 1e-6, e1 - e0)
    t = np.clip((x - e0) / span, 0, 1)
    return t * t * (3 - 2 * t)


for cy in range(CELLS[1]):
    for cx in range(CELLS[0]):
        v = np.linspace(0, 1, ch)[:, None]      # 0 at the root row (bottom)
        u = np.linspace(0, 1, cw)[None, :]
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
            tone = .78 + .18 * rng.random()
            alpha = np.maximum(alpha, a)
            shade = np.where(a > .15, np.maximum(shade, tone * (.92 + .08 * np.clip(1 - d / np.maximum(w, 1e-4), 0, 1))), shade)
        # fine strand lines inside the lock, low contrast
        lines = 1 - .08 * (np.sin(u * 400 + np.sin(v * 9) * 3) > .6)
        tip = .78 + .34 * np.clip(v, 0, 1)
        rgb = np.clip(np.where(shade > 0, shade, .85) * tip * lines, 0, 1)[..., None]
        block = atlas[cy * ch:(cy + 1) * ch, cx * cw:(cx + 1) * cw]
        block[..., :3] = np.repeat(rgb, 3, axis=-1)
        block[..., 3] = alpha
fur_atlas = save_image('T_Dog_FurAtlas', atlas)

# Shell mask: tiling strand cross-sections with random heights; a shell layer at
# height h keeps texels whose value exceeds h, so strands thin out towards the tips.
shell = np.zeros((N, N), np.float32)
yy, xx = np.mgrid[0:N, 0:N].astype(np.float32)
dots = 13000
dx_ = rng.random(dots) * N; dy_ = rng.random(dots) * N
dr_ = 2.0 + 3.0 * rng.random(dots); dh_ = .25 + .75 * rng.random(dots) ** .7
for i in range(dots):
    r = int(math.ceil(dr_[i])) + 1
    x0, y0 = int(dx_[i]), int(dy_[i])
    ys = (np.arange(y0 - r, y0 + r + 1) % N); xs = (np.arange(x0 - r, x0 + r + 1) % N)
    sub = shell[np.ix_(ys, xs)]
    gy, gx = np.mgrid[y0 - r:y0 + r + 1, x0 - r:x0 + r + 1]
    d = np.sqrt((gx - dx_[i]) ** 2 + (gy - dy_[i]) ** 2) / dr_[i]
    val = dh_[i] * np.clip(1 - d, 0, 1) ** .6
    shell[np.ix_(ys, xs)] = np.maximum(sub, val)
rgba = np.ones((N, N, 4), np.float32); rgba[..., :3] = shell[..., None]
shell_mask = save_image('T_Dog_ShellMask', rgba)

# ---------------------------------------------------------------- materials
MATS = {}
for name, (color, rough, metal) in PALETTE.items():
    m = bpy.data.materials.new('DM_' + name)
    m.use_nodes = True
    m.diffuse_color = color
    nodes, links = m.node_tree.nodes, m.node_tree.links
    p = nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = color
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    if name in ('Coat', 'FurCard', 'Shell'):
        vc = nodes.new('ShaderNodeVertexColor'); vc.layer_name = 'CoatColor'
        if name == 'Shell':
            # Base position from the authored UV channels; layer height in RestZ.y.
            uvn = nodes.new('ShaderNodeUVMap'); uvn.uv_map = 'UVMap'; uvz = nodes.new('ShaderNodeUVMap'); uvz.uv_map = 'RestZ'
            sep = nodes.new('ShaderNodeSeparateXYZ'); links.new(uvn.outputs['UV'], sep.inputs[0])
            sepz = nodes.new('ShaderNodeSeparateXYZ'); links.new(uvz.outputs['UV'], sepz.inputs[0])
            mx = nodes.new('ShaderNodeMath'); mx.operation = 'MULTIPLY_ADD'; mx.inputs[1].default_value = 260; mx.inputs[2].default_value = -120; links.new(sep.outputs[0], mx.inputs[0])
            my = nodes.new('ShaderNodeMath'); my.operation = 'MULTIPLY_ADD'; my.inputs[1].default_value = 120; my.inputs[2].default_value = -60; links.new(sep.outputs[1], my.inputs[0])
            mz = nodes.new('ShaderNodeMath'); mz.operation = 'MULTIPLY'; mz.inputs[1].default_value = 120; links.new(sepz.outputs[0], mz.inputs[0])
            comb = nodes.new('ShaderNodeCombineXYZ'); links.new(mx.outputs[0], comb.inputs[0]); links.new(my.outputs[0], comb.inputs[1]); links.new(mz.outputs[0], comb.inputs[2])
            mapping = nodes.new('ShaderNodeMapping'); mapping.inputs['Scale'].default_value = (1 / 28.0, 1 / 28.0, 1 / 28.0); links.new(comb.outputs[0], mapping.inputs['Vector'])
            mask = nodes.new('ShaderNodeTexImage'); mask.image = shell_mask; mask.projection = 'BOX'; mask.projection_blend = .3; mask.interpolation = 'Linear'
            links.new(mapping.outputs['Vector'], mask.inputs['Vector'])
            cut = nodes.new('ShaderNodeMath'); cut.operation = 'GREATER_THAN'; links.new(mask.outputs['Color'], cut.inputs[0]); links.new(sepz.outputs[1], cut.inputs[1])
            links.new(cut.outputs[0], p.inputs['Alpha'])
            lighten = nodes.new('ShaderNodeMath'); lighten.operation = 'MULTIPLY_ADD'; lighten.inputs[1].default_value = .16; lighten.inputs[2].default_value = .94; links.new(sepz.outputs[1], lighten.inputs[0])
            mixc = nodes.new('ShaderNodeMixRGB'); mixc.blend_type = 'MULTIPLY'; mixc.inputs[0].default_value = 1
            links.new(vc.outputs['Color'], mixc.inputs[1]); links.new(lighten.outputs[0], mixc.inputs[2]); links.new(mixc.outputs[0], p.inputs['Base Color'])
            p.inputs['Sheen Weight'].default_value = .15; p.inputs['Sheen Tint'].default_value = (1, .85, .6, 1)
            m.blend_method = 'HASHED'
            rays = nodes.new('ShaderNodeLightPath'); transparent = nodes.new('ShaderNodeBsdfTransparent'); sm = nodes.new('ShaderNodeMixShader')
            links.new(rays.outputs['Is Shadow Ray'], sm.inputs[0]); links.new(p.outputs['BSDF'], sm.inputs[1]); links.new(transparent.outputs[0], sm.inputs[2])
            links.new(sm.outputs[0], nodes.get('Material Output').inputs['Surface'])
        elif name == 'Coat':
            coords = nodes.new('ShaderNodeTexCoord')
            mapping = nodes.new('ShaderNodeMapping'); mapping.inputs['Scale'].default_value = (1 / 42, 1 / 42, 1 / 42)
            links.new(coords.outputs['Object'], mapping.inputs['Vector'])
            det = nodes.new('ShaderNodeTexImage'); det.image = fur_detail; det.projection = 'BOX'; det.projection_blend = .35
            links.new(mapping.outputs['Vector'], det.inputs['Vector'])
            mix = nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs[0].default_value = .75
            links.new(vc.outputs['Color'], mix.inputs[1]); links.new(det.outputs['Color'], mix.inputs[2])
            links.new(mix.outputs[0], p.inputs['Base Color'])
            nt = nodes.new('ShaderNodeTexImage'); nt.image = fur_normal; nt.projection = 'BOX'; nt.projection_blend = .35
            links.new(mapping.outputs['Vector'], nt.inputs['Vector'])
            nm = nodes.new('ShaderNodeNormalMap'); nm.inputs['Strength'].default_value = .5
            links.new(nt.outputs['Color'], nm.inputs['Color']); links.new(nm.outputs['Normal'], p.inputs['Normal'])
            p.inputs['Subsurface Weight'].default_value = .18
            p.inputs['Subsurface Radius'].default_value = (1.2, .6, .3)
            p.inputs['Sheen Weight'].default_value = .12; p.inputs['Sheen Tint'].default_value = (1, .85, .6, 1)
        else:
            tex = nodes.new('ShaderNodeTexImage'); tex.image = fur_atlas; tex.interpolation = 'Cubic'
            uvn = nodes.new('ShaderNodeUVMap'); uvn.uv_map = 'UVMap'; links.new(uvn.outputs['UV'], tex.inputs['Vector'])
            mix = nodes.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs[0].default_value = 1
            links.new(vc.outputs['Color'], mix.inputs[1]); links.new(tex.outputs['Color'], mix.inputs[2])
            links.new(mix.outputs[0], p.inputs['Base Color'])
            links.new(tex.outputs['Alpha'], p.inputs['Alpha'])
            p.inputs['Sheen Weight'].default_value = .15; p.inputs['Sheen Tint'].default_value = (1, .85, .6, 1)
            geo = nodes.new('ShaderNodeNewGeometry'); neg = nodes.new('ShaderNodeVectorMath'); neg.operation = 'SCALE'; neg.inputs[3].default_value = -1
            links.new(geo.outputs['Normal'], neg.inputs[0])
            pick = nodes.new('ShaderNodeMix'); pick.data_type = 'VECTOR'
            links.new(geo.outputs['Backfacing'], pick.inputs['Factor']); links.new(geo.outputs['Normal'], pick.inputs[4]); links.new(neg.outputs[0], pick.inputs[5])
            links.new(pick.outputs[1], p.inputs['Normal'])
            trans = nodes.new('ShaderNodeBsdfTranslucent'); links.new(mix.outputs[0], trans.inputs['Color']); links.new(pick.outputs[1], trans.inputs['Normal'])
            tmix = nodes.new('ShaderNodeMixShader'); tmix.inputs[0].default_value = .22
            links.new(p.outputs['BSDF'], tmix.inputs[1]); links.new(trans.outputs[0], tmix.inputs[2])
            m.blend_method = 'HASHED'
            m.use_backface_culling = False
            # Cards do not cast their own shadows; the body silhouette does.
            rays = nodes.new('ShaderNodeLightPath'); transparent = nodes.new('ShaderNodeBsdfTransparent')
            sm = nodes.new('ShaderNodeMixShader')
            links.new(rays.outputs['Is Shadow Ray'], sm.inputs[0]); links.new(tmix.outputs[0], sm.inputs[1])
            links.new(transparent.outputs[0], sm.inputs[2]); links.new(sm.outputs[0], nodes.get('Material Output').inputs['Surface'])
    if name == 'Eye':
        p.inputs['Coat Weight'].default_value = .6; p.inputs['Coat Roughness'].default_value = .05
    if name == 'Nose':
        p.inputs['Coat Weight'].default_value = .25
        det = nodes.new('ShaderNodeTexNoise'); det.inputs['Scale'].default_value = 60
        bump = nodes.new('ShaderNodeBump'); bump.inputs['Strength'].default_value = .25
        links.new(det.outputs['Fac'], bump.inputs['Height']); links.new(bump.outputs['Normal'], p.inputs['Normal'])
    MATS[name] = m

# ---------------------------------------------------------------- mesh helpers
BODY, PARTS = [], []


def finish(o, name, mat='Coat', bone=None, body=False):
    o.name = name
    o.data.materials.append(MATS[mat])
    for poly in o.data.polygons:
        poly.use_smooth = True
    if bone:
        g = o.vertex_groups.new(name=bone); g.add(list(range(len(o.data.vertices))), 1, 'REPLACE')
    (BODY if body else PARTS).append(o)
    return o


def mesh_object(name, verts, faces):
    me = bpy.data.meshes.new(name); me.from_pydata([tuple(v) for v in verts], [], faces); me.update()
    o = bpy.data.objects.new(name, me); bpy.context.collection.objects.link(o)
    return o


def active(o):
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o


def superellipse(a, theta, hw, hup, hdn, nup, ndn, lean=0.0):
    """Point on an asymmetric superellipse in the (side, up) plane."""
    c, s = math.cos(theta), math.sin(theta)
    n = nup if s >= 0 else ndn
    h = hup if s >= 0 else hdn
    r = (abs(c) ** n + abs(s) ** n) ** (-1 / n)
    return r * c * hw, r * s * h + lean * (r * c * hw)


def profile_loft(name, stations, sides=32, mat='Coat', body=True, bone=None, axis='x'):
    """Loft along +X. Station: (x, zc, hw, hup, hdn, nup, ndn[, yc])."""
    verts, faces = [], []
    for st in stations:
        x, zc, hw, hup, hdn, nup, ndn = st[:7]
        yc = st[7] if len(st) > 7 else 0.0
        for j in range(sides):
            t = 2 * math.pi * j / sides
            y, z = superellipse(0, t, hw, hup, hdn, nup, ndn)
            verts.append((x, y + yc, z + zc))
    for i in range(1, len(stations)):
        for j in range(sides):
            a = (i - 1) * sides + j; b = (i - 1) * sides + (j + 1) % sides
            c = i * sides + (j + 1) % sides; d = i * sides + j
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(sides))))
    faces.append(tuple((len(stations) - 1) * sides + j for j in range(sides)))
    return finish(mesh_object(name, verts, faces), name, mat, bone, body)


def tube(name, points, radii, sides=20, mat='Coat', body=True, bone=None):
    """Loft along an arbitrary polyline with (ry, rz) radii; frames follow the tangent."""
    pts = [Vector(p) for p in points]; verts, faces = [], []
    for i, p in enumerate(pts):
        tangent = (pts[min(i + 1, len(pts) - 1)] - pts[max(0, i - 1)]).normalized()
        ref = Vector((0, 1, 0)) if abs(tangent.y) < .95 else Vector((1, 0, 0))
        side = (ref - tangent * ref.dot(tangent)).normalized(); up = tangent.cross(side).normalized()
        ry, rz = radii[i] if isinstance(radii[i], tuple) else (radii[i], radii[i])
        for j in range(sides):
            a = 2 * math.pi * j / sides
            verts.append(p + side * math.cos(a) * ry + up * math.sin(a) * rz)
    for i in range(1, len(pts)):
        for j in range(sides):
            a = (i - 1) * sides + j; b = (i - 1) * sides + (j + 1) % sides
            c = i * sides + (j + 1) % sides; d = i * sides + j
            faces.append((a, b, c, d))
    faces.append(tuple(reversed(range(sides)))); faces.append(tuple((len(pts) - 1) * sides + j for j in range(sides)))
    return finish(mesh_object(name, verts, faces), name, mat, bone, body)


def ellipsoid(name, center, scale, mat='Coat', body=True, bone=None, segments=24, rings=16, rotation=None):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1, location=center)
    o = bpy.context.object; o.scale = scale
    if rotation is not None:
        o.rotation_euler = rotation
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return finish(o, name, mat, bone, body)


# ---------------------------------------------------------------- anatomy (cm)
# Proportions of an adult male golden retriever: ~58 cm at the withers, ~70 cm
# from point of shoulder to point of buttock, head ~27 cm occiput to nose.
FORE_X, HIND_X, LEG_Y = 23.0, -29.0, 12.7
OCCIPUT_X, NOSE_X = 44.0, 72.5


def hx(x):
    """Map the authored head stations (39..81.2) onto the shorter real skull."""
    return OCCIPUT_X + (x - 39.0) * (NOSE_X - OCCIPUT_X) / 42.2


profile_loft('Trunk', [
    (-44, 46.5, 4.8, 7.0, 6.0, 2.2, 2.0),
    (-41, 46.5, 9.6, 10.0, 8.6, 2.3, 2.0),
    (-35, 46.0, 13.0, 11.4, 10.2, 2.4, 2.0),
    (-27, 46.0, 12.8, 12.2, 9.4, 2.4, 2.0),
    (-18, 45.5, 13.4, 13.0, 11.4, 2.4, 2.1),
    (-9, 45.0, 14.4, 13.8, 13.4, 2.4, 2.2),
    (0, 45.0, 14.9, 14.2, 14.3, 2.4, 2.3),
    (9, 45.5, 14.4, 14.6, 14.4, 2.5, 2.4),
    (16, 46.5, 13.4, 15.0, 13.4, 2.5, 2.3),
    (23, 48.0, 11.8, 13.2, 11.2, 2.4, 2.2),
    (29, 50.0, 9.0, 10.2, 8.4, 2.2, 2.0),
    (33, 52.5, 5.2, 6.2, 5.2, 2.0, 2.0),
], sides=40)
ellipsoid('Prosternum', (29, 0, 45), (7.5, 9.8, 9.6))
# Neck: thick, carried at ~45 degrees into the occiput.
tube('Neck', [(22, 0, 54), (28, 0, 59.5), (34, 0, 66), (39.5, 0, 72), (44, 0, 76.5)],
     [(12.6, 13.0), (11.8, 12.4), (10.8, 11.4), (9.6, 10.0), (8.2, 8.6)], sides=30)
# Head: a domed skull volume and a separate deep muzzle volume meet at the stop;
# brows, cheeks and flews are added as volumes so the union keeps real creases.
SKULL = [
    (44.0, 76.5, 5.8, 5.6, 6.0, 2.0, 2.0),
    (46.5, 77.6, 8.8, 8.4, 8.4, 2.2, 2.0),
    (49.5, 78.0, 10.2, 8.8, 9.2, 2.3, 2.1),
    (52.5, 78.0, 10.6, 8.8, 9.6, 2.3, 2.2),
    (55.5, 77.6, 10.2, 8.6, 9.8, 2.3, 2.3),
    (57.5, 77.0, 9.2, 8.0, 9.6, 2.3, 2.4),
    (59.0, 76.4, 7.6, 6.6, 8.6, 2.3, 2.4),
    (60.2, 76.0, 5.2, 4.6, 6.0, 2.2, 2.2),
    (60.8, 76.0, 2.4, 2.2, 2.6, 2.0, 2.0),
]
def mx(x):
    return x if x <= 57 else 57 + (x - 57) * .89


MUZZLE = [
    (52.0, 73.0, 7.4, 4.4, 6.0, 2.3, 2.4),
    (57.0, 73.4, 7.2, 4.8, 7.0, 2.5, 2.6),
    (mx(61.0), 73.8, 6.8, 4.9, 7.6, 2.6, 2.8),
    (mx(65.0), 73.8, 6.4, 4.8, 7.4, 2.6, 2.8),
    (mx(68.5), 73.7, 6.0, 4.6, 6.8, 2.6, 2.7),
    (mx(71.0), 73.6, 5.6, 4.4, 6.0, 2.5, 2.6),
    (mx(72.8), 73.7, 5.0, 4.0, 5.0, 2.6, 2.6),
    (mx(73.8), 73.9, 3.8, 3.2, 3.9, 2.4, 2.4),
    (mx(74.4), 74.1, 2.2, 1.9, 2.1, 2.0, 2.0),
]
HEAD = MUZZLE
profile_loft('Skull', SKULL, sides=36)
profile_loft('Muzzle', MUZZLE, sides=32)
for side in (-1, 1):
    ellipsoid('Brow', (58.2, side * 4.3, 83.0), (2.8, 2.6, 1.7))
    ellipsoid('Cheek', (52.5, side * 9.8, 72.5), (5.5, 3.4, 5.0))
    ellipsoid('Flew', (63.0, side * 6.2, 68.6), (6.0, 1.9, 3.4))
for side in (-1, 1):
    y = side * LEG_Y; fx = FORE_X; hxx = HIND_X
    # Shoulder blade and upper arm merge into the ribcage.
    ellipsoid('Shoulder', (fx - 4, side * 11.2, 50), (8.8, 5.8, 12.5), rotation=(0, math.radians(-12), 0))
    tube('Upper arm', [(fx + 4, y, 48), (fx + 2, y, 40), (fx, y, 29)], [(7.2, 8.0), (6.0, 6.6), (5.2, 5.4)])
    tube('Forearm', [(fx, y, 29.5), (fx + 1.5, y, 20), (fx + 2.5, y, 12), (fx + 3.5, y, 8), (fx + 5.5, y, 4.5)],
         [(5.1, 5.4), (4.5, 4.8), (4.1, 4.3), (3.9, 4.0), (4.0, 3.4)])
    ellipsoid('Forepaw', (fx + 7.5, y, 3.6), (6.3, 5.2, 3.5))
    for toe in (-2.9, -1.0, 1.0, 2.9):
        ellipsoid('Fore toe', (fx + 11.6, y + toe, 2.9), (2.9, 1.45, 2.5), segments=14, rings=10)
    ellipsoid('Thigh', (hxx - 2, side * 11.6, 41.0), (11.5, 7.0, 14.0), rotation=(0, math.radians(18), 0))
    tube('Upper hind leg', [(hxx - 4, y, 44), (hxx + 1, y, 36), (hxx + 5, y, 30)], [(8.0, 9.0), (6.6, 7.2), (5.4, 5.8)])
    tube('Lower hind leg', [(hxx + 5, y, 30), (hxx - 1, y, 22), (hxx - 8, y, 14.5), (hxx - 6.5, y, 9), (hxx - 4, y, 4.8)],
         [(5.4, 5.8), (4.5, 4.8), (4.0, 4.2), (3.7, 3.9), (3.8, 3.4)])
    ellipsoid('Hind paw', (hxx - 1.5, y, 3.5), (5.9, 4.9, 3.4))
    for toe in (-2.6, -.9, .9, 2.6):
        ellipsoid('Hind toe', (hxx + 2.4, y + toe, 2.8), (2.7, 1.35, 2.4), segments=14, rings=10)
TAIL = [(-40, 0, 49), (-53, -1, 47), (-68, -2, 40), (-84, -2.5, 30), (-98, -2, 25)]
tube('Tail', TAIL, [(3.4, 3.2), (3.1, 3.0), (2.5, 2.4), (1.7, 1.6), (.5, .5)], sides=18)

# Union into one skin, relax, then shape features before decimation.
bpy.ops.object.select_all(action='DESELECT')
for o in BODY:
    o.select_set(True)
bpy.context.view_layer.objects.active = BODY[0]
bpy.ops.object.join()
skin = bpy.context.object; skin.name = 'Skin'
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
remesh = skin.modifiers.new('Union', 'REMESH'); remesh.mode = 'VOXEL'; remesh.voxel_size = .70; remesh.use_smooth_shade = True
bpy.ops.object.modifier_apply(modifier=remesh.name)
relax = skin.modifiers.new('Relax', 'SMOOTH'); relax.factor = .5; relax.iterations = 1
bpy.ops.object.modifier_apply(modifier=relax.name)

EYES = [(58.8, side * 4.0, 80.6) for side in (-1, 1)]
EYE_DIR = [Vector((.72, side * .46, .52)).normalized() for side in (-1, 1)]
STOP_X, BRIDGE_TOP = 59.6, MUZZLE[3][1] + MUZZLE[3][3]


def gauss(d, r):
    return math.exp(-(d / r) ** 2)


def feature_offset(p, n):
    """Signed displacement along the normal, in cm: sockets, brows, stop, cheeks."""
    x, y, z = p
    off = 0.0
    for c in EYES:
        d = (Vector(p) - Vector(c)).length
        off -= 1.25 * gauss(d, 2.3)                                                       # orbit
    off -= .60 * gauss((Vector(p) - Vector((STOP_X, 0, 80.8))).length, 2.0)             # stop crease
    if 61 < x < 73 and z > 75:
        off += .30 * gauss(y, 1.6) * gauss(z - BRIDGE_TOP, 2.5)                            # nasal bridge
    if 57 < x < 73 and 64 < z < 70 and abs(y) > 2:
        off -= .35 * gauss(z - 67.0, 1.1)                                                  # upper lip line
    if x < 28 and 36 < z < 52 and abs(y) > 8:
        off -= .35 * gauss(x + 16, 10) * gauss(z - 42, 7)                                  # flank tuck
    return off


offs = np.array([feature_offset(v.co, v.normal) for v in skin.data.vertices], np.float32)
for v, o in zip(skin.data.vertices, offs):
    v.co += v.normal * float(o)
dec = skin.modifiers.new('Budget', 'DECIMATE'); dec.ratio = .26
bpy.ops.object.modifier_apply(modifier=dec.name)
relax = skin.modifiers.new('Relax2', 'SMOOTH'); relax.factor = .25; relax.iterations = 1
bpy.ops.object.modifier_apply(modifier=relax.name)
for poly in skin.data.polygons:
    poly.use_smooth = True
PARTS.append(skin)
print('SKIN_TRIANGLES', sum(len(p.vertices) - 2 for p in skin.data.polygons), flush=True)

# ---------------------------------------------------------------- rig (same bones and clips as revision 4)
SPEC = [('root', (0, 0, 0), (0, 0, 8), None), ('pelvis', (HIND_X - 4, 0, 43), (-12, 0, 44), 'root'), ('spine', (-12, 0, 44), (22, 0, 48), 'pelvis'),
        ('neck', (22, 0, 48), (42, 0, 74), 'spine'), ('head', (42, 0, 74), (64, 0, 75), 'neck'), ('jaw', (52, 0, 68), (70, 0, 64), 'head')]
for side, label in ((-1, 'R'), (1, 'L')):
    y = side * LEG_Y; fx = FORE_X; hxx = HIND_X
    SPEC.extend([(f'fore_upper_{label}', (fx + 3, y, 47), (fx, y, 29), 'spine'), (f'fore_lower_{label}', (fx, y, 29), (fx + 3, y, 6), f'fore_upper_{label}'),
                 (f'fore_paw_{label}', (fx + 3, y, 6), (fx + 11, y, 3.8), f'fore_lower_{label}'), (f'hind_upper_{label}', (hxx - 4, y, 44), (hxx + 5, y, 30), 'pelvis'),
                 (f'hind_lower_{label}', (hxx + 5, y, 30), (hxx - 8, y, 14), f'hind_upper_{label}'), (f'hind_paw_{label}', (hxx - 8, y, 14), (hxx - 1, y, 3.8), f'hind_lower_{label}'),
                 (f'ear_{label}', (47, side * 10.5, 82), (44, side * 14, 63), 'head')])
for i in range(4):
    SPEC.append((f'tail_{i + 1}', TAIL[i], TAIL[i + 1], 'pelvis' if i == 0 else f'tail_{i}'))
arm = bpy.data.armatures.new('CompanionDog_Skeleton'); rig = bpy.data.objects.new('CompanionDogRig', arm)
bpy.context.collection.objects.link(rig); active(rig); bpy.ops.object.mode_set(mode='EDIT')
for name, a, b, parent in SPEC:
    bone = arm.edit_bones.new(name); bone.head = a; bone.tail = b; bone.use_connect = False
    if parent:
        bone.parent = arm.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT'); rig.show_in_front = True
SEGMENTS = {name: (Vector(a), Vector(b)) for name, a, b, parent in SPEC}


def distance_segment(p, a, b):
    delta = b - a; t = max(0, min(1, (p - a).dot(delta) / delta.length_squared)); return (p - a - delta * t).length


def ease(t):
    t = max(0, min(1, t)); return t * t * (3 - 2 * t)


def bone_weights(p):
    """Smooth anatomical weights for a rest-pose point; returns [(weight, bone)]."""
    side = 'L' if p.y >= 0 else 'R'
    fore = ease((50 - p.z) / 23) * ease((abs(p.y) - 4) / 7) * math.exp(-((p.x - (FORE_X + 2)) / 19) ** 4)
    hind = ease((53 - p.z) / 24) * ease((abs(p.y) - 4) / 7) * math.exp(-((p.x - (HIND_X - 2)) / 20) ** 4)
    head = ease((p.x - 40) / 18) * ease((p.z - 57) / 18)
    neck = ease((p.x - 18) / 23) * ease((p.z - 44) / 19) * (1 - head)
    tail = ease((-37 - p.x) / 8) * ease((p.z - 15) / 10)
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

# ---------------------------------------------------------------- ears, face, collar
for side, label in ((-1, 'R'), (1, 'L')):
    s = side
    # Pendant ear: thick leaf hanging from an arc on the side of the skull.
    ear = tube('Ear ' + label, [(50.5, s * 9.0, 82.5), (49.0, s * 12.2, 78.5), (47.0, s * 13.4, 72.5), (45.5, s * 13.2, 66.5), (44.5, s * 12.2, 61.5)],
               [(1.3, 4.0), (1.6, 8.4), (1.4, 9.2), (1.1, 7.2), (.3, 1.8)], sides=24, body=False, bone=f'ear_{label}')
    for v in ear.data.vertices:   # slightly cupped leaf that follows the cheek
        v.co.y += s * .8 * (1 - abs(v.co.z - 72.5) / 11) * (abs(v.co.x - 46.5) / 9)
    # Eye: dark almond eyeball with a glossy cornea, set into its socket.
    c = Vector(EYES[0 if side < 0 else 1]); n = EYE_DIR[0 if side < 0 else 1]
    eye = ellipsoid('Eye ' + label, c, (1, 1, 1), 'Eye', body=False, bone='head', segments=28, rings=18)
    t = Vector((-n.y, n.x, 0)).normalized(); up = n.cross(t).normalized()
    for v in eye.data.vertices:
        q = v.co.copy(); v.co = t * q.x * 1.55 + up * q.z * 1.2 + n * q.y * 1.15
    pupil = ellipsoid('Pupil ' + label, c + n * 1.05, (1, 1, 1), 'Nose', body=False, bone='head', segments=20, rings=12)
    for v in pupil.data.vertices:
        q = v.co.copy(); v.co = t * q.x * .55 + up * q.z * .55 + n * q.y * .12
    iris = ellipsoid('Iris ' + label, c + n * .98, (1, 1, 1), 'Iris', body=False, bone='head', segments=24, rings=14)
    for v in iris.data.vertices:
        q = v.co.copy(); v.co = t * q.x * .95 + up * q.z * .85 + n * q.y * .14
    rim = [c + n * .45 + t * (math.cos(a) * 1.75) + up * (math.sin(a) * 1.35) for a in [j * 2 * math.pi / 24 for j in range(25)]]
    curve = bpy.data.curves.new('Eyelid ' + label, 'CURVE'); curve.dimensions = '3D'; curve.bevel_depth = .22; curve.bevel_resolution = 2
    sp = curve.splines.new('BEZIER'); sp.bezier_points.add(len(rim) - 1)
    for bp, co in zip(sp.bezier_points, rim):
        bp.co = co; bp.handle_left_type = 'AUTO'; bp.handle_right_type = 'AUTO'
    o = bpy.data.objects.new('Eyelid ' + label, curve); bpy.context.collection.objects.link(o); active(o); bpy.ops.object.convert(target='MESH')
    finish(bpy.context.object, 'Eyelid ' + label, 'Nose', 'head')
# Nose leather with nostrils, open panting mouth with lower jaw, tongue and canines.
NX = NOSE_X + .2
nose = ellipsoid('Nose', (NX, 0, 75.2), (1.4, 2.4, 2.0), 'Nose', body=False, bone='head', segments=32, rings=20)
for v in nose.data.vertices:
    v.co.y *= .78 + .22 * ((v.co.z - 75.2) / 2.0 + 1) * .5
    v.co.x = NX + (v.co.x - NX) * (.5 if v.co.x > NX else 1.0)
for side in (-1, 1):
    ellipsoid('Nostril', (NX + .7, side * 1.25, 74.9), (.45, .65, .6), 'Mouth', body=False, bone='head', segments=16, rings=10)
JAW = [(53, 71.4, 7.8, 1.2, 2.8, 2.2, 2.4), (58, 70.0, 7.4, 1.4, 3.2, 2.3, 2.6), (62.5, 68.6, 6.8, 1.4, 3.2, 2.4, 2.7),
       (66.5, 67.6, 6.0, 1.3, 2.8, 2.4, 2.6), (70.0, 67.3, 4.7, 1.2, 2.0, 2.2, 2.3), (72.2, 67.5, 2.6, .8, 1.1, 2, 2)]
profile_loft('Lower jaw', JAW, sides=24, body=False, bone='jaw')
profile_loft('Mouth cavity', [(55, 69.6, 6.0, 1.2, 2.2, 2, 2), (61, 68.4, 5.8, 1.2, 2.6, 2, 2), (67, 67.4, 5.0, 1.0, 2.4, 2, 2), (72, 67.6, 2.8, .6, 1.2, 2, 2)],
             sides=20, mat='Mouth', body=False, bone='head')
tongue = tube('Tongue', [(59, 0, 69.0), (63, .6, 68.8), (67, 1.2, 68.4), (70.5, 1.6, 67.7), (73.2, 1.8, 66.9)],
              [(3.2, .7), (4.0, .7), (4.1, .65), (3.5, .6), (.5, .3)], sides=20, mat='Tongue', body=False, bone='jaw')
for side in (-1, 1):
    tube('Canine', [(62.5, side * 5.7, 68.2), (62.9, side * 5.5, 66.9), (63.3, side * 5.2, 66.0)], [.8, .5, .08], sides=10, mat='Teeth', body=False, bone='head')
    tube('Lower canine', [(64.6, side * 5.3, 68.3), (64.8, side * 5.2, 69.4)], [.55, .1], sides=10, mat='Teeth', body=False, bone='jaw')
# Collar: flat webbing band close to the neck, with a round tag.
axis = Vector((.52, 0, .85)).normalized(); center = Vector((31.5, 0, 61.5)); sidev = Vector((0, 1, 0)); cross = axis.cross(sidev)


def band_ring(name, radius, half_width, thickness, mat, segments=72):
    verts, faces = [], []
    for i in range(segments):
        a = 2 * math.pi * i / segments
        radial = (sidev * math.cos(a) + cross * math.sin(a)).normalized()
        c = center + radial * radius + cross * (.9 * math.sin(a))
        for dw in (-half_width, half_width):
            for dt in (0, thickness):
                verts.append(c + axis * dw + radial * dt)
    for i in range(segments):
        j = (i + 1) % segments
        b0, b1 = 4 * i, 4 * j
        faces.append((b0 + 1, b1 + 1, b1 + 3, b0 + 3))
        faces.append((b0 + 2, b1 + 2, b1 + 0, b0 + 0))
        faces.append((b0 + 0, b1 + 0, b1 + 1, b0 + 1))
        faces.append((b0 + 3, b1 + 3, b1 + 2, b0 + 2))
    return finish(mesh_object(name, verts, faces), name, mat, 'neck')


band_ring('Collar', 12.4, 1.35, .45, 'Collar')
band_ring('Collar edge', 12.35, 1.5, .25, 'CollarEdge')
tagpos = center - cross * 13.4 + Vector((1.2, 0, -3.0))
tag = ellipsoid('Tag', tagpos, (.35, 1.9, 1.9), 'Metal', body=False, bone='neck', segments=24, rings=12)
letters = bpy.data.curves.new('Tag text', 'FONT'); letters.body = 'REX'; letters.size = 1.3; letters.align_x = 'CENTER'; letters.extrude = .05
text = bpy.data.objects.new('Tag text', letters); bpy.context.collection.objects.link(text)
text.location = tagpos + Vector((.42, 0, -.45)); text.rotation_euler = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0))).to_euler()
active(text); bpy.ops.object.convert(target='MESH'); finish(bpy.context.object, 'Tag text', 'Nose', 'neck')

# ---------------------------------------------------------------- coat zoning


def coat_color(p, n):
    x, y, z = p; nz = n.z; ax = abs(y)
    dorsal = ease((nz + .15) / .75)                       # 1 on the back, 0 underneath
    base = GOLD_LIGHT * (1 - dorsal) + GOLD * dorsal
    if x < -37 and z > 20:                                # tail: gold on top, cream below
        base = CREAM * (1 - dorsal) + GOLD * dorsal
    # Cream chest, belly and lower legs; white blaze/muzzle/brow of an old dog.
    ventral = ease((-nz + .1) / .6) * ease((52 - z) / 14)
    legs = ease((24 - z) / 12)
    chest = ease((x - 10) / 14) * ease((52 - z) / 12) * ease((16 - ax) / 6)
    cream = max(ventral, legs * .9, chest)
    col = base * (1 - cream) + CREAM * cream
    muzzle = ease((x - 56) / 5) * ease((z - 60) / 5) * ease((86 - z) / 6)
    brow = ease((x - 52) / 4) * ease((z - 76) / 4) * ease((9 - ax) / 4) * .9
    cheek = ease((x - 49) / 5) * ease((z - 60) / 8) * ease((78 - z) / 6) * ease((ax - 4) / 5) * .7
    white = max(muzzle, brow, cheek)
    col = col * (1 - white) + WHITE * white
    ear = ease((ax - 9.5) / 2.5) * ease((x - 41) / 3) * ease((52 - x) / 3) * ease((z - 63) / 4) * ease((87 - z) / 3)
    col = col * (1 - ear) + GOLD_DARK * ear
    v = 1 + .045 * math.sin(x * .9 + y * .6 + z * 1.3) + .03 * math.sin(x * .27 - z * .41 + y * .2)
    return tuple(float(max(0, min(1, c * v))) for c in col) + (1.0,)


def apply_coat_attributes(o, color_fn=None):
    """Vertex colours plus UV0/UV1 holding the rest-pose position for triplanar detail."""
    active(o); bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    me = o.data
    if len(me.loops) == 0:
        print('EMPTY_PART', o.name, flush=True)
        return
    me.uv_layers.new(name='UVMap'); me.uv_layers.new(name='RestZ')
    me.color_attributes.new(name='CoatColor', type='FLOAT_COLOR', domain='CORNER')
    # Re-fetch by name: adding layers reallocates the mesh custom data.
    attr = me.color_attributes['CoatColor']; me.color_attributes.active_color = attr
    uv0 = me.uv_layers['UVMap']; uv1 = me.uv_layers['RestZ']
    is_coat = me.materials[0] in (MATS['Coat'], MATS['FurCard'])
    vn = {v.index: v.normal.copy() for v in me.vertices}
    for loop in me.loops:
        p = me.vertices[loop.vertex_index].co
        attr.data[loop.index].color = (color_fn or coat_color)(p, vn[loop.vertex_index]) if is_coat else (1, 1, 1, 1)
        uv0.data[loop.index].uv = ((p.x + 120) / 260, (p.y + 60) / 120)
        uv1.data[loop.index].uv = (p.z / 120, 0)


for o in PARTS:
    apply_coat_attributes(o)


def build_shells(layers=6):
    """Offset copies of a decimated skin; layer height in RestZ.y drives the shell mask."""
    active(skin); bpy.ops.object.duplicate()
    base = bpy.context.object; base.name = 'ShellBase'
    dec = base.modifiers.new('ShellBudget', 'DECIMATE'); dec.ratio = .30
    bpy.ops.object.modifier_apply(modifier=dec.name)
    me = base.data
    V = np.array([v.co for v in me.vertices], np.float32); Nn = np.array([v.normal for v in me.vertices], np.float32)
    L = np.array([shell_length(tuple(v), tuple(n)) for v, n in zip(V, Nn)], np.float32)
    faces = [tuple(pl.vertices) for pl in me.polygons]
    cols = [coat_color(Vector(v), Vector(n)) for v, n in zip(V, Nn)]
    made = []
    for k in range(1, layers + 1):
        h = k / layers
        verts = V + Nn * (L * h)[:, None]
        o = mesh_object(f'Shell{k}', verts, faces)
        o.data.materials.append(MATS['Shell'])
        for poly in o.data.polygons:
            poly.use_smooth = True
        md = o.data
        md.uv_layers.new(name='UVMap'); md.uv_layers.new(name='RestZ')
        md.color_attributes.new(name='CoatColor', type='FLOAT_COLOR', domain='CORNER')
        attr = md.color_attributes['CoatColor']; md.color_attributes.active_color = attr
        uv0 = md.uv_layers['UVMap']; uv1 = md.uv_layers['RestZ']
        for loop in md.loops:
            b = V[loop.vertex_index]
            attr.data[loop.index].color = cols[loop.vertex_index]
            uv0.data[loop.index].uv = ((b[0] + 120) / 260, (b[1] + 60) / 120)
            uv1.data[loop.index].uv = (b[2] / 120, h - .5 / layers)
        md.normals_split_custom_set_from_vertices([Vector(n) for n in Nn])
        groups = {n: o.vertex_groups.new(name=n) for n in SEGMENTS}
        for i, b in enumerate(V):
            for w, n in bone_weights(Vector(b)):
                groups[n].add([i], w, 'REPLACE')
        made.append(o); PARTS.append(o)
    bpy.data.objects.remove(base, do_unlink=True)
    print('SHELLS', layers, 'triangles', layers * len(faces) * 2, flush=True)
    return made


# ---------------------------------------------------------------- fur cards
GRAVITY = np.array((0, 0, -1.0))


def fur_region(p, n):
    """Feathering cards only: (length_cm, density_cards_per_cm2, droop, standing_probability)."""
    x, y, z = p; ax = abs(y); nz = n[2]; nx = n[0]
    if x > 59 and z > 60:                       # muzzle: short smooth hair, shells only
        return 0, 0, 0, 0
    if 38 < x < 53 and ax > 9.0 and 60 < z < 88:  # ears: soft locks hanging down
        return 3.6, .40, .95, .05
    if 47 < x < 58 and ax > 6.5 and 62 < z < 79:  # cheek fluff, swept back
        return 2.6, .30, .55, .05
    if x < -37 and z > 15:                      # tail plume
        return 11.0 if nz > -.3 else 9.0, .40, .72, .15
    if x > 20 and 48 < z < 70 and nz < .45:     # ruff below the neck
        return 6.0, .30, .70, .1
    legs_z = z < 30 and (abs(x - FORE_X - 2) < 9 or abs(x - HIND_X) < 9)
    if legs_z:
        if nx < -.25 and z > 8:                 # feathering on the back of the legs
            return 6.0, .22, .95, .1
        return 0, 0, 0, 0
    if z < 10:
        return 0, 0, 0, 0
    if nz < -.15 and z < 48:                    # chest, brisket and belly skirt
        return 8.5 if x > 0 else 7.0, .22, .92, .1
    if x < -16 and z < 46 and ax > 8:           # hind "pants"
        return 6.5, .22, .85, .1
    if x > 16 and z < 50 and ax > 8:            # upper foreleg feathering
        return 5.5, .22, .85, .1
    return 0, 0, 0, 0                           # back and sides: shells only


def shell_length(p, n):
    """Shell fur depth in cm for a skin point; blended so layers stay continuous."""
    x, y, z = p; ax = abs(y)
    face = ease((x - 54) / 5) * ease((z - 58) / 6)
    legs = ease((30 - z) / 8) * max(ease((9 - abs(x - FORE_X - 2)) / 4), ease((9 - abs(x - HIND_X)) / 4))
    feet = ease((10 - z) / 4)
    ruff = ease((x - 16) / 8) * ease((z - 46) / 8) * (1 - face)
    tail = ease((-35 - x) / 5)
    body = 2.2 * (1 - face) * (1 - legs) * (1 - tail) + 2.6 * ruff + 2.0 * tail
    body = min(body, 2.8)
    value = body * (1 - legs) * (1 - face) + 1.0 * legs * (1 - feet) + .5 * feet
    skull = ease((z - 70) / 6) * (1 - ease((x - 59) / 3))
    return value + .7 * face * skull           # muzzle 0, skull/cheeks up to 0.7


def sample_surface(o, count):
    """Area-weighted random surface points; returns positions, normals and samples per cm2."""
    me = o.data; me.calc_loop_triangles()
    tris = np.array([[v for v in t.vertices] for t in me.loop_triangles], np.int32)
    V = np.array([v.co for v in me.vertices], np.float32)
    VN = np.array([v.normal for v in me.vertices], np.float32)
    a, b, c = V[tris[:, 0]], V[tris[:, 1]], V[tris[:, 2]]
    area = .5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    pick = RNG.choice(len(tris), size=count, p=area / area.sum())
    r1, r2 = RNG.random(count), RNG.random(count)
    s = np.sqrt(r1); w0, w1, w2 = 1 - s, s * (1 - r2), s * r2
    P = (a[pick] * w0[:, None] + b[pick] * w1[:, None] + c[pick] * w2[:, None])
    Nn = (VN[tris[pick, 0]] * w0[:, None] + VN[tris[pick, 1]] * w1[:, None] + VN[tris[pick, 2]] * w2[:, None])
    Nn /= np.linalg.norm(Nn, axis=1)[:, None]
    return P, Nn, count / float(area.sum())


def build_fur_cards():
    roots, normals, densities = [], [], []
    ears = [o for o in PARTS if o.name.startswith('Ear ')]
    for o, count in [(skin, 22000)] + [(e, 600) for e in ears]:
        P, Nn, rho = sample_surface(o, count); roots.append(P); normals.append(Nn); densities.append(np.full(count, rho, np.float32))
    P = np.concatenate(roots); Nn = np.concatenate(normals); RHO = np.concatenate(densities)
    print('SKIN_AREA_CM2', round(float(22000 / densities[0][0])), flush=True)
    verts, faces, uvs, colors, custom_normals, weights = [], [], [], [], [], []
    kd = KDTree(len(skin.data.vertices))
    for v in skin.data.vertices:
        kd.insert(v.co, v.index)
    kd.balance()
    skin_weights = {}
    for v in skin.data.vertices:
        skin_weights[v.index] = [(g.weight, skin.vertex_groups[g.group].name) for g in v.groups]
    ear_names = {o.name: f"ear_{o.name[-1]}" for o in ears}
    ear_bounds = {o.name: (np.array(o.bound_box).min(0), np.array(o.bound_box).max(0)) for o in ears}
    cards = 0
    for p, n, rho in zip(P, Nn, RHO):
        L, density, droop, standing = fur_region(p, n)
        if L <= 0 or RNG.random() > density / rho:
            continue
        L *= .75 + .5 * RNG.random()
        W = L * (.70 + .30 * RNG.random())
        # Hair lies toward the tail and droops with gravity.
        back = np.array((-1.0, 0, 0)); back -= n * np.dot(back, n)
        if np.linalg.norm(back) < 1e-3:
            back = np.array((0, 1.0, 0)) - n * n[1]
        back /= np.linalg.norm(back)
        d = back * (1 - droop) + GRAVITY * droop + n * .30
        d /= np.linalg.norm(d)
        t = np.cross(n, d); t /= max(1e-6, np.linalg.norm(t))
        roll = (RNG.random() - .5) * math.radians(110)
        if RNG.random() < standing:
            roll += math.radians(90) * (1 if RNG.random() < .5 else -1)
        w = t * math.cos(roll) + np.cross(d, t) * math.sin(roll)
        bend = GRAVITY * .18 * droop + n * .10
        base = len(verts)
        rows = 4
        for k in range(rows):
            u = k / (rows - 1)
            pos = p + n * .35 + d * (u * L) + bend * (u * u * L)
            pos[2] = max(pos[2], .4)
            half = W * .5 * (1 - .4 * u)
            verts.append(pos - w * half); verts.append(pos + w * half)
        for k in range(rows - 1):
            a = base + 2 * k
            faces.append((a, a + 1, a + 3, a + 2))
        cell = int(RNG.integers(0, CELLS[0] * CELLS[1]))
        cu, cv = cell % CELLS[0], cell // CELLS[0]
        u0, u1 = cu / CELLS[0], (cu + 1) / CELLS[0]; v0, v1 = cv / CELLS[1], (cv + 1) / CELLS[1]
        for k in range(rows):
            u = k / (rows - 1)
            uvs.append((u0, v0 + (v1 - v0) * u)); uvs.append((u1, v0 + (v1 - v0) * u))
        col = coat_color(Vector(p), Vector(n))
        colors.extend([col] * (rows * 2)); custom_normals.extend([tuple(n)] * (rows * 2))
        # Skinning follows the nearest skin vertex (ear cards follow their ear bone).
        bone = None
        for name, (lo, hi) in ear_bounds.items():
            if np.all(p >= lo - .5) and np.all(p <= hi + .5):
                bone = ear_names[name]
        if bone:
            wts = [(1.0, bone)]
        else:
            _, index, _ = kd.find(Vector(p)); wts = skin_weights[index]
        weights.extend([wts] * (rows * 2))
        cards += 1
    me = bpy.data.meshes.new('FurCards'); me.from_pydata(verts, [], faces); me.update()
    o = bpy.data.objects.new('FurCards', me); bpy.context.collection.objects.link(o)
    o.data.materials.append(MATS['FurCard'])
    for poly in me.polygons:
        poly.use_smooth = True
    me.uv_layers.new(name='UVMap'); me.uv_layers.new(name='RestZ')
    me.color_attributes.new(name='CoatColor', type='FLOAT_COLOR', domain='CORNER')
    attr = me.color_attributes['CoatColor']; me.color_attributes.active_color = attr
    uv = me.uv_layers['UVMap']; uvz = me.uv_layers['RestZ']
    for loop in me.loops:
        attr.data[loop.index].color = colors[loop.vertex_index]
        uv.data[loop.index].uv = uvs[loop.vertex_index]
        uvz.data[loop.index].uv = (0, 1)
    me.normals_split_custom_set_from_vertices([Vector(c) for c in custom_normals])
    groups = {n: o.vertex_groups.new(name=n) for n in SEGMENTS}
    for i, wts in enumerate(weights):
        for wgt, name in wts:
            groups[name].add([i], wgt, 'REPLACE')
    PARTS.append(o)
    print('FUR_CARDS', cards, 'triangles', cards * (rows - 1) * 2, flush=True)
    return o


shells = build_shells()
fur = build_fur_cards()
for o in PARTS:
    if o is fur or o is skin:
        continue
    weight_object(o) if not o.vertex_groups else None

# ---------------------------------------------------------------- assemble, skin, animate
bpy.ops.object.select_all(action='DESELECT')
for o in PARTS:
    o.select_set(True)
bpy.context.view_layer.objects.active = skin
bpy.ops.object.join()
dog = bpy.context.object; dog.name = 'SK_CompanionDog'; dog.data.name = 'CompanionDog_Geometry'
dog.parent = rig
mod = dog.modifiers.new('Skinning', 'ARMATURE'); mod.object = rig; mod.use_deform_preserve_volume = True
active(dog); bpy.ops.object.vertex_group_limit_total(limit=4); bpy.ops.object.vertex_group_normalize_all(lock_active=False)
dog.data.calc_loop_triangles()
dog['rex_likeness_revision'] = 5

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
        local_rotation('jaw', (0, 1, 0), .02 + math.sin(phase * 2) * .015)
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

# ---------------------------------------------------------------- export


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
    'asset': 'SK_CompanionDog', 'revision': 5,
    'source': 'Original procedural geometry, rig, animation and textures; the owner\'s reference photographs guided proportions and coat zoning by eye and were not read, copied or sampled',
    'forward_axis': '+X', 'up_axis': '+Z', 'units': 'centimeters', 'root_motion': False,
    'reference_pose_bounds_cm': {'min': list(lo), 'max': list(hi)},
    'triangles': len(dog.data.loop_triangles), 'vertices': len(dog.data.vertices), 'bones': [n for n, a, b, p in SPEC],
    'materials': {m.name: {'color': list(PALETTE[m.name[3:]][0]), 'roughness': PALETTE[m.name[3:]][1], 'metallic': PALETTE[m.name[3:]][2]} for m in dog.data.materials},
    'used_materials': sorted({dog.data.materials[p.material_index].name for p in dog.data.polygons}),
    'textures': {'T_Dog_FurAtlas': 'procedural strand atlas, 4x2 cells, alpha coverage', 'T_Dog_FurDetail': 'procedural streak brightness', 'T_Dog_FurNormal': 'derived from the streak height'},
    'uv_channels': {'0': 'rest-pose X/Y encoded as (x+120)/260,(y+60)/120 on skin; fur atlas on cards', '1': 'rest-pose Z/120 on skin; (0,1) marks cards'},
    'animations': {'A_DogIdle': {'duration_seconds': 3, 'fps': 30, 'loop': True}, 'A_DogWalk': likeness.walk_metadata(clips['A_DogWalk'])},
    'coat_vertex_attribute': 'CoatColor', 'max_bone_influences': 4, 'dog_name': 'Rex', 'name_on_tag': True,
    'cosmetic_revision': 'Rex revision 5: lofted retriever anatomy (broad skull, defined stop, deep muzzle, pendant ears, deep chest), photo-guided coat zoning with the white muzzle and brow of an older dog, and a procedural fur-card coat with feathering on the ruff, chest, belly, legs, pants and tail plume.',
    'exports': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (ART / 'Exports').glob('*.fbx')} if not PREVIEW_ONLY else {},
    'unreal_import_pending': True, 'runtime_review_pending': True,
}
if not PREVIEW_ONLY:
    (ART / 'dog_manifest.json').write_text(json.dumps(report, indent=2) + '\n')

# ---------------------------------------------------------------- review scene (saved with the source)
world = bpy.data.worlds.new('Rex review'); scene.world = world; world.use_nodes = True
bg = world.node_tree.nodes['Background']; bg.inputs['Color'].default_value = (.55, .68, .85, 1); bg.inputs['Strength'].default_value = .9
sun = bpy.data.lights.new('Sun', 'SUN'); sun.energy = 3.0; sun.angle = math.radians(2.5); sun.color = (1, .96, .9)
sun_o = bpy.data.objects.new('Sun', sun); bpy.context.collection.objects.link(sun_o)
sun_o.rotation_euler = (math.radians(52), math.radians(8), math.radians(-35))
floor_mat = bpy.data.materials.new('PreviewOnly_Grass'); floor_mat.diffuse_color = (.10, .16, .05, 1); floor_mat.use_nodes = True
floor_mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.10, .16, .05, 1)
bpy.ops.mesh.primitive_plane_add(size=2000); floor = bpy.context.object; floor.name = 'PreviewOnly_Floor'; floor.data.materials.append(floor_mat)
cam_data = bpy.data.cameras.new('ReviewCamera'); cam = bpy.data.objects.new('ReviewCamera', cam_data); bpy.context.collection.objects.link(cam); scene.camera = cam
scene.render.engine = 'CYCLES'; scene.cycles.samples = 48; scene.cycles.use_denoising = True
scene.cycles.transparent_max_bounces = 64; scene.cycles.max_bounces = 8
scene.render.resolution_x = 1400; scene.render.resolution_y = 1050
scene.view_settings.view_transform = 'Standard'; scene.view_settings.exposure = .1
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
print('REX_V5_READY', json.dumps({'triangles': report['triangles'], 'vertices': report['vertices'], 'bounds': report['reference_pose_bounds_cm']}), flush=True)

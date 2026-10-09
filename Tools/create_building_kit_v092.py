"""Original building kit v0.9.2: distinct silhouettes for the families that shared one hall.

Run with Blender --background --python (or the bpy module). Centimetres, Z up, authored
with the front (doors, docks, berths) toward -Y; export() turns every mesh +90 degrees
about Z so the front reaches Unreal +X, the side every building's access port and road
use (Rules access_port [1,0]) and the side the default camera (yaw 135) looks at. The
FBX/Unreal import keeps X and mirrors Y, so authored (x, y) lands at Unreal (-y, -x).
Pivot at the horizontal bounding-box centre on the ground (the industry convention).
Reuses the industry palette (IM_* slots -> MI_Industry_* in Unreal) so the
construction reveal material keeps working. Writes:
  Art/BuildingKitV092/Exports/SM_<Kind>.fbx      (ignored by git; regenerated)
  Art/BuildingKitV092/kit_manifest.json          (dimensions, triangles, hashes, mapping)
  Art/BuildingKitV092/Source/BuildingKitV092.blend
Options: -- --only=solarArray,hangar   builds a subset;  -- --no-blend  skips the .blend.
"""
from pathlib import Path
import bpy, math, json, hashlib, sys
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art'; KIT = ART / 'BuildingKitV092'
OUT = KIT / 'Exports'; SOURCE = KIT / 'Source'
for p in (OUT, SOURCE): p.mkdir(parents=True, exist_ok=True)
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
ONLY = next((a.split('=', 1)[1].split(',') for a in ARGS if a.startswith('--only=')), None)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene; scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = .01

industry = json.loads((ART / 'IndustryExports/industry_manifest.json').read_text(encoding='utf-8'))
MATS = {}; PARTS = []; ASSETS = {}; records = {}
EXPORT_YAW_DEGREES = 90  # authored -Y front -> Unreal +X (access-port side); see the docstring
TEX = ART / 'Textures/Industry'
for name, s in industry['palette'].items():
    m = bpy.data.materials.new(name); m.use_nodes = True; m.diffuse_color = s['color']
    ns = m.node_tree.nodes; ls = m.node_tree.links; p = ns.get('Principled BSDF')
    p.inputs['Base Color'].default_value = s['color']; p.inputs['Metallic'].default_value = s['metallic']; p.inputs['Roughness'].default_value = s['roughness']
    p.inputs['Emission Color'].default_value = s['color']; p.inputs['Emission Strength'].default_value = s['emission']
    surface = s.get('surface')
    if surface and (TEX / f'T_Industry_{surface}_Color.png').exists():
        ct = ns.new('ShaderNodeTexImage'); ct.image = bpy.data.images.load(str(TEX / f'T_Industry_{surface}_Color.png'), check_existing=True)
        mix = ns.new('ShaderNodeMixRGB'); mix.blend_type = 'MULTIPLY'; mix.inputs[0].default_value = 1; mix.inputs[2].default_value = s['color']
        ls.new(ct.outputs['Color'], mix.inputs[1]); ls.new(mix.outputs[0], p.inputs['Base Color'])
    MATS[name.removeprefix('IM_')] = m


# ------------------------------------------------------------------ primitives (industry conventions)
def finish(o, name, mat):
    o.name = name; o.data.materials.append(MATS[mat]); PARTS.append(o); return o


def bevel(o, width=3, segments=2):
    if width:
        m = o.modifiers.new('Manufactured edge radii', 'BEVEL'); m.width = width; m.segments = segments
        bpy.context.view_layer.objects.active = o; bpy.ops.object.modifier_apply(modifier=m.name)
    m = o.modifiers.new('Weighted face normals', 'WEIGHTED_NORMAL'); m.keep_sharp = True; bpy.ops.object.modifier_apply(modifier=m.name)


def box(name, p, d, mat='Ceramic', edge=3, rotation=0, tilt=0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=p); o = bpy.context.object; o.dimensions = d
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True); bevel(o, edge)
    o.rotation_euler = (tilt, 0, rotation); return finish(o, name, mat)


def beam(name, a, b, width, depth=None, mat='Steel'):
    a, b = Vector(a), Vector(b); o = box(name, (a + b) / 2, (width, depth or width, (b - a).length), mat, min(width * .15, 2))
    o.rotation_euler = (b - a).to_track_quat('Z', 'Y').to_euler(); return o


def tube(name, a, b, r, mat='Steel', r2=None, sides=20):
    a, b = Vector(a), Vector(b)
    bpy.ops.mesh.primitive_cone_add(vertices=sides, radius1=r, radius2=r if r2 is None else r2, depth=(b - a).length, location=(a + b) / 2)
    o = bpy.context.object; o.rotation_euler = (b - a).to_track_quat('Z', 'Y').to_euler()
    if sides >= 16:
        for face in o.data.polygons: face.use_smooth = len(face.vertices) <= 4
    return finish(o, name, mat)


def cyl(name, p, r, h, mat='Steel', sides=32, r2=None):
    return tube(name, (p[0], p[1], p[2] - h / 2), (p[0], p[1], p[2] + h / 2), r, mat, r2, sides)


def torus(name, p, r, t, mat='Steel', rotation=None):
    bpy.ops.mesh.primitive_torus_add(major_segments=48, minor_segments=8, major_radius=r, minor_radius=t, location=p, rotation=rotation or (0, 0, 0))
    for face in bpy.context.object.data.polygons: face.use_smooth = True
    return finish(bpy.context.object, name, mat)


def dome(name, p, r, mat='Ceramic', segments=48, rings=12, cut=0.0):
    """Upper part of a sphere (cut = fraction of the radius below the equator to keep)."""
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings * 2, radius=r, location=p)
    o = bpy.context.object
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='DESELECT'); bpy.ops.object.mode_set(mode='OBJECT')
    for v in o.data.vertices: v.select = v.co.z < -r * cut - 1e-3
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.delete(type='VERT'); bpy.ops.object.mode_set(mode='OBJECT')
    for face in o.data.polygons: face.use_smooth = True
    return finish(o, name, mat)


def pipe(name, points, r=9, mat='Steel'):
    cu = bpy.data.curves.new(name, 'CURVE'); cu.dimensions = '3D'; cu.resolution_u = 2; cu.bevel_depth = r; cu.bevel_resolution = 3
    sp = cu.splines.new('POLY'); sp.points.add(len(points) - 1)
    for q, p in zip(sp.points, points): q.co = (*p, 1)
    ob = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(ob); bpy.context.view_layer.objects.active = ob
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.ops.object.convert(target='MESH'); return finish(bpy.context.object, name, mat)


def text(name, word, p, size=70, rotation=(math.pi / 2, 0, 0), mat='Ceramic'):
    cu = bpy.data.curves.new(name, 'FONT'); cu.body = word; cu.size = size; cu.extrude = .7; cu.align_x = 'CENTER'; cu.align_y = 'CENTER'
    o = bpy.data.objects.new(name, cu); bpy.context.collection.objects.link(o); o.location = p; o.rotation_euler = rotation
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.convert(target='MESH'); return finish(bpy.context.object, name, mat)


def window_y(x, y, z, w, h):
    box('Window thermally broken frame', (x, y, z), (w + 15, 14, h + 15), 'Carbon', 3)
    box('Blue low-reflection glazing', (x, y - 9, z), (w, 5, h), 'Glass', 1)
    for a in range(1, max(1, round(w / 110))): box('Glazing mullion', (x - w / 2 + a * w / max(1, round(w / 110)), y - 13, z), (5, 10, h), 'Steel', .5)


def vent_y(x, y, z, w, h):
    box('Ventilation recess', (x, y, z), (w, 12, h), 'Carbon', 2)
    for k in range(max(3, int(h / 14))): box('Angled ventilation louver', (x, y - 9, z - h / 2 + 8 + k * 14), (w - 10, 10, 4), 'Steel', .4)


def door_y(x, y, floor, w=115, h=215):
    box('Personnel airlock frame', (x, y, floor + h / 2), (w + 26, 26, h + 24), 'Slate', 5)
    box('Personnel airlock leaf', (x, y - 22, floor + h / 2), (w, 8, h), 'Ceramic', 3)
    window_y(x, y - 29, floor + h * .70, w * .60, h * .16)
    box('Airlock downlight', (x, y - 28, floor + h + 20), (w - 12, 8, 6), 'Light', 1)


def shutter_y(x, y, floor, w, h, mat='Carbon'):
    """Vehicle-scale roller door on the -Y face with hazard striping and a canopy."""
    box('Vehicle bay frame', (x, y, floor + h / 2), (w + 40, 24, h + 30), 'Slate', 5)
    box('Vehicle bay recess', (x, y - 10, floor + h / 2), (w, 14, h), mat, 3)
    for z in range(int(floor + 20), int(floor + h - 10), 34): box('Roller shutter slat', (x, y - 20, z), (w - 16, 10, 26), 'Steel', 1)
    for sx in (-1, 1): box('Hazard stripe', (x + sx * (w / 2 + 12), y - 14, floor + h / 2), (14, 6, h - 10), 'Yellow', .5)
    box('Bay canopy', (x, y - 110, floor + h + 42), (w + 120, 240, 26), 'Slate', 4)
    box('Bay canopy lamp', (x, y - 215, floor + h + 26), (w - 40, 10, 5), 'Light', 1)


def foundation(w, d, h=48):
    box('Reinforced concrete grade slab', (0, 0, h / 2), (w, d, h), 'Concrete', 10)
    for x in range(int(-w / 2 + 250), int(w / 2), 450): box('Concrete expansion joint', (x, 0, h), (2, d - 10, 1), 'Carbon', 0)


def fan(x, y, z, r=60):
    cyl('Cooling fan shroud', (x, y, z), r, 20, 'Carbon'); cyl('Fan rim', (x, y, z + 11), r - 2, 4, 'Steel', 24)
    cyl('Fan motor', (x, y, z + 13), r * .22, 12, 'Steel', 16)
    for a in range(0, 360, 60):
        t = math.radians(a); box('Fan blade', (x + math.cos(t) * r * .49, y + math.sin(t) * r * .49, z + 10), (r * .61, r * .15, 3), 'Steel', .5, rotation=t + .2)


def hvac(x, y, z, w=200, d=290, h=150):
    box('Air handling plant', (x, y, z + h / 2), (w, d, h), 'Ceramic', 6); vent_y(x, y - d / 2 - 5, z + h * .47, w - 32, h - 25)
    for offset in (-d * .24, d * .24): fan(x, y + offset, z + h + 8, min(w * .34, d * .20))


def tank(x, y, z, r=110, h=330, mat='Steel'):
    cyl('Pressurized process vessel', (x, y, z + h / 2), r, h, mat); cyl('Vessel dished top', (x, y, z + h + 20), r, 40, mat, 32, r * .3)
    for zz in (z + 25, z + h - 25): torus('Vessel reinforcement ring', (x, y, zz), r + 2, 6, 'Slate')
    for a in range(0, 360, 120):
        t = math.radians(a); beam('Vessel support leg', (x + math.cos(t) * r * .73, y + math.sin(t) * r * .73, z - 70), (x + math.cos(t) * r * .73, y + math.sin(t) * r * .73, z + 35), 16, mat='Slate')


def roof_barrel(x, y, z, w, d, rise=90, mat='Ceramic'):
    verts = []; faces = []; steps = 16
    for j in (0, 1):
        for i in range(steps + 1):
            u = i / steps; verts.append((x + (u - .5) * w, y + (j - .5) * d, z + math.sin(u * math.pi) * rise))
    for i in range(steps): faces.append((i, i + 1, i + steps + 2, i + steps + 1))
    me = bpy.data.meshes.new('Curved standing seam canopy'); me.from_pydata(verts, [], faces); me.update()
    ob = bpy.data.objects.new(me.name, me); bpy.context.collection.objects.link(ob); finish(ob, ob.name, mat)
    for i in range(0, steps + 1, 2):
        u = i / steps; tube('Barrel roof standing seam', (x + (u - .5) * w, y - d / 2, z + math.sin(u * math.pi) * rise + 2), (x + (u - .5) * w, y + d / 2, z + math.sin(u * math.pi) * rise + 2), 2, 'Steel', sides=6)


def gable_glass(x, y, z, w, d, h, mat_frame='Steel'):
    """Greenhouse bay: glazed gable roof + glazed walls on a slim steel frame."""
    box('Greenhouse side glazing', (x - w / 2, y, z + h * .5), (4, d, h), 'Glass', 0); box('Greenhouse side glazing', (x + w / 2, y, z + h * .5), (4, d, h), 'Glass', 0)
    box('Greenhouse end glazing', (x, y - d / 2, z + h * .5), (w, 4, h), 'Glass', 0); box('Greenhouse end glazing', (x, y + d / 2, z + h * .5), (w, 4, h), 'Glass', 0)
    rise = w * .36
    for sx in (-1, 1):
        o = box('Greenhouse roof glazing', (x + sx * w / 4, y, z + h + rise / 2), (math.hypot(w / 2, rise) + 6, d, 4), 'Glass', 0)
        o.rotation_euler = (0, -sx * math.atan2(rise, w / 2), 0)
    for yy in [y - d / 2 + i * d / max(1, round(d / 220)) for i in range(round(d / 220) + 1)]:
        for sx in (-1, 1): beam('Greenhouse frame post', (x + sx * w / 2, yy, z), (x + sx * w / 2, yy, z + h), 7, mat=mat_frame)
        beam('Greenhouse rafter', (x - w / 2, yy, z + h), (x, yy, z + h + rise), 6, mat=mat_frame); beam('Greenhouse rafter', (x, yy, z + h + rise), (x + w / 2, yy, z + h), 6, mat=mat_frame)
    tube('Greenhouse ridge', (x, y - d / 2, z + h + rise + 3), (x, y + d / 2, z + h + rise + 3), 5, mat_frame, sides=8)
    tube('Greenhouse gutter', (x - w / 2 - 4, y - d / 2, z + h - 4), (x - w / 2 - 4, y + d / 2, z + h - 4), 5, 'Slate', sides=8)


def solar_panel(x, y, z, w, h, tilt=math.radians(30), rotation=0, post=True):
    """Tilted PV module (glass face up-sun = -Y) on a post or A-frame."""
    o = box('Photovoltaic module', (x, y, z), (w, h, 5), 'PV', 1); o.rotation_euler = (tilt, 0, rotation)
    f = box('Module frame', (x, y, z - 4), (w + 6, h + 6, 4), 'Steel', 1); f.rotation_euler = (tilt, 0, rotation)
    for k in range(1, 4):
        g = box('Module string divider', (x - w / 2 + k * w / 4, y, z + 1), (2, h - 8, 5.5), 'Carbon', 0); g.rotation_euler = (tilt, 0, rotation)
    if post:
        beam('Module support post', (x, y, 40), (x, y, z - 6), 12, mat='Steel')


# ------------------------------------------------------------------ v3 dressing: parapets, downpipes, ladders, lamps, yard props
def dress_hall(cx, cy, z0, w, d, h, ladder='+x', lamps='-y', pipes=True, parapet=True):
    """Roof-edge parapet, corner downpipes with hoppers, a caged roof ladder,
    wall lamps on the working face and a dark kick plate: the small vertical
    breaks a real industrial shed has at the player's zoom."""
    top = z0 + h
    if parapet:
        for sy in (-1, 1): box('Parapet coping', (cx, cy + sy * (d / 2 + 6), top + 10), (w + 24, 16, 24), 'Steel', 0)
        for sx in (-1, 1): box('Parapet coping', (cx + sx * (w / 2 + 6), cy, top + 10), (16, d + 24, 24), 'Steel', 0)
    box('Wall kick plate', (cx, cy - d / 2 - 2, z0 + 22), (w + 4, 6, 44), 'Slate', 0)
    box('Wall kick plate', (cx, cy + d / 2 + 2, z0 + 22), (w + 4, 6, 44), 'Slate', 0)
    if pipes:
        for sx in (-1, 1):
            for sy in (-1, 1):
                x, y = cx + sx * (w / 2 + 10), cy + sy * (d / 2 + 10)
                tube('Rainwater downpipe', (x, y, z0 + 8), (x, y, top - 10), 6, 'Steel', sides=8)
                box('Downpipe hopper head', (x, y, top - 4), (24, 24, 26), 'Steel', 0)
                box('Downpipe shoe', (x, y, z0 + 10), (20, 20, 16), 'Slate', 0)
    if ladder:
        sign = 1 if ladder[0] == '+' else -1
        if ladder[1] == 'x':
            lx, ly0, ly1 = cx + sign * (w / 2 + 22), cy + d * .18 - 24, cy + d * .18 + 24
            for ly in (ly0, ly1): beam('Roof ladder stile', (lx, ly, z0), (lx, ly, top + 100), 6, mat='Yellow')
            for z in range(int(z0 + 30), int(top + 80), 30): box('Ladder rung', (lx, (ly0 + ly1) / 2, z), (4, 46, 4), 'Steel', 0)
            for z in range(int(z0 + 240), int(top + 90), 90): box('Ladder cage hoop', (lx + sign * 30, (ly0 + ly1) / 2, z), (4, 70, 4), 'Yellow', 0)
            box('Roof hatch', (cx + sign * (w / 2 - 80), cy + d * .18, top + 18), (90, 90, 20), 'Slate', 0)
        else:
            ly, lx0, lx1 = cy + sign * (d / 2 + 22), cx + w * .18 - 24, cx + w * .18 + 24
            for lx in (lx0, lx1): beam('Roof ladder stile', (lx, ly, z0), (lx, ly, top + 100), 6, mat='Yellow')
            for z in range(int(z0 + 30), int(top + 80), 30): box('Ladder rung', ((lx0 + lx1) / 2, ly, z), (46, 4, 4), 'Steel', 0)
            for z in range(int(z0 + 240), int(top + 90), 90): box('Ladder cage hoop', ((lx0 + lx1) / 2, ly + sign * 30, z), (70, 4, 4), 'Yellow', 0)
            box('Roof hatch', (cx + w * .18, cy + sign * (d / 2 - 80), top + 18), (90, 90, 20), 'Slate', 0)
    if lamps:
        sign = 1 if lamps[0] == '+' else -1
        n = max(2, int(w // 520))
        for k in range(n):
            x = cx - w / 2 + (k + .5) * w / n
            if lamps[1] == 'y':
                y = cy + sign * (d / 2 + 14)
                box('Wall lamp bracket', (x, y, top - 70), (10, 24, 10), 'Slate', 0); box('Wall floodlamp', (x, y + sign * 12, top - 80), (36, 14, 12), 'Light', 0)


import random as _random
_PROP_SKIP = ('Reinforced concrete grade slab', 'Concrete expansion joint', 'apron', 'Apron', 'marking', 'guide line', 'lane line', 'stripe')


def _ground_footprints(z_top, rise=40):
    """XY rectangles of every part that touches the slab (min z near its top)."""
    bpy.context.view_layer.update()
    rects = []
    for o in PARTS:
        if any(t in o.name for t in _PROP_SKIP):
            continue
        pts = [o.matrix_world @ Vector(c) for c in o.bound_box]
        if min(p.z for p in pts) > z_top + rise:
            continue
        r = [min(p.x for p in pts), min(p.y for p in pts), max(p.x for p in pts), max(p.y for p in pts)]
        # Doors, roller shutters and charging berths all face -Y: keep their approach clear.
        if 'Vehicle bay frame' in o.name: r[1] -= 450
        elif 'Personnel airlock frame' in o.name: r[1] -= 220
        elif 'Isolated charging plinth' in o.name: r[1] -= 300
        rects.append(tuple(r))
    return rects


def _free(rects, x, y, rx, ry, margin=40):
    return all(x + rx + margin < a or x - rx - margin > c or y + ry + margin < b or y - ry - margin > dd for a, b, c, dd in rects)


def _pallet(x, y, z, rng, yaw=0):
    box('Pallet', (x, y, z + 7), (120, 100, 14), 'Carbon', 0, rotation=yaw)
    layers = rng.choice((1, 1, 2))
    for k in range(layers):
        box('Palletised crate', (x, y, z + 14 + 42 + k * 84), (108, 90, 82), rng.choice(('Ceramic', 'Blue', 'Yellow', 'Slate')), 2, rotation=yaw)


def _drums(x, y, z, rng):
    mat = rng.choice(('Blue', 'Yellow', 'Steel', 'Slate'))
    for i in range(rng.choice((3, 4, 6))):
        cyl('Process drum', (x + (i % 3) * 64 - 64, y + (i // 3) * 64, z + 44), 29, 88, mat, 12)


def _forklift(x, y, z, yaw):
    c, s = math.cos(yaw), math.sin(yaw)
    def at(dx, dy): return (x + dx * c - dy * s, y + dx * s + dy * c)
    box('Forklift body', (*at(0, 0), z + 55), (170, 100, 90), 'Yellow', 4, rotation=yaw)
    box('Forklift counterweight', (*at(-80, 0), z + 60), (40, 96, 100), 'Slate', 4, rotation=yaw)
    for dx in (-50, 50):
        for dy in (-48, 48): cyl('Forklift wheel', (*at(dx, dy), z + 22), 22, 18, 'Carbon', 10).rotation_euler = (math.pi / 2, 0, yaw)
    for dy in (-38, 38): beam('Forklift overhead guard post', (*at(-40 if dy < 0 else -40, dy), z + 100), (*at(-40, dy), z + 210), 6, mat='Slate'); beam('Forklift overhead guard post', (*at(30, dy), z + 100), (*at(30, dy), z + 210), 6, mat='Slate')
    box('Forklift overhead guard', (*at(-5, 0), z + 212), (90, 86, 6), 'Slate', 0, rotation=yaw)
    for dy in (-30, 30): beam('Forklift mast', (*at(95, dy), z + 20), (*at(95, dy), z + 230), 9, mat='Steel')
    for dy in (-28, 28): box('Forklift tine', (*at(150, dy), z + 12), (110, 12, 5), 'Steel', 0, rotation=yaw)


def _gas_rack(x, y, z):
    box('Cylinder rack frame', (x, y - 30, z + 70), (200, 8, 140), 'Steel', 0)
    for i in range(5): cyl('Compressed gas cylinder', (x - 80 + i * 40, y, z + 70), 13, 140, ('Blue', 'Steel', 'Yellow')[i % 3], 8)


def _barrier(x, y, z, yaw):
    box('Concrete jersey barrier', (x, y, z + 40), (300, 60, 80), 'Concrete', 14, rotation=yaw)


def scatter_props(z_top, half_w, half_d, seed, count=10, edge=70):
    """Deterministic yard clutter in the free slab area (pallets, drums, gas
    racks, barriers, the odd forklift): visual density without new gameplay."""
    rng = _random.Random(seed)
    rects = _ground_footprints(z_top)
    placed = 0; tries = 0; forklift = False
    while placed < count and tries < count * 60:
        tries += 1
        kind = rng.choice(('pallet', 'pallet', 'pallet', 'drums', 'drums', 'gas', 'barrier', 'forklift'))
        if kind == 'forklift' and forklift:
            kind = 'pallet'
        rx, ry = {'pallet': (70, 60), 'drums': (110, 80), 'gas': (110, 45), 'barrier': (155, 35), 'forklift': (150, 110)}[kind]
        x = rng.uniform(-half_w + edge + rx, half_w - edge - rx); y = rng.uniform(-half_d + edge + ry, half_d - edge - ry)
        if not _free(rects, x, y, rx, ry):
            continue
        yaw = rng.choice((0, math.pi / 2, math.pi, -math.pi / 2)) + rng.uniform(-.15, .15)
        if kind == 'pallet':
            _pallet(x, y, z_top, rng, yaw)
            if rng.random() < .5 and _free(rects, x + 130, y, rx, ry): _pallet(x + 130, y, z_top, rng, yaw); rects.append((x + 60, y - ry, x + 200, y + ry))
        elif kind == 'drums': _drums(x, y, z_top, rng)
        elif kind == 'gas': _gas_rack(x, y, z_top)
        elif kind == 'barrier': _barrier(x, y, z_top, rng.choice((0, math.pi / 2)))
        else: _forklift(x, y, z_top, yaw); forklift = True
        rects.append((x - rx, y - ry, x + rx, y + ry)); placed += 1
    return placed


BERTHS = []


def berth(x, y, z, facing_degrees):
    """One docking point for a stored worker body, in the authored frame
    (facing measured from +X towards +Y). export() converts it to Unreal."""
    BERTHS.append((x, y, z, facing_degrees))


def export(name, description, families):
    bpy.ops.object.select_all(action='DESELECT')
    for o in PARTS: o.select_set(True)
    bpy.context.view_layer.objects.active = PARTS[0]; bpy.ops.object.join(); o = bpy.context.object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Authored front (-Y) -> +X here -> Unreal +X after the import's Y mirror.
    # transform_apply also turns the custom (weighted) split normals.
    o.rotation_euler = (0, 0, math.radians(EXPORT_YAW_DEGREES)); bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)
    lo = [min(v.co[i] for v in o.data.vertices) for i in range(3)]; hi = [max(v.co[i] for v in o.data.vertices) for i in range(3)]
    shift = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))
    for v in o.data.vertices: v.co -= shift
    turn = math.radians(EXPORT_YAW_DEGREES); berths = []
    for bx, by, bz, facing in BERTHS:
        # Blender (x, y) after the turn and recentring; Unreal mirrors Y.
        x = bx * math.cos(turn) - by * math.sin(turn) - shift.x; y = bx * math.sin(turn) + by * math.cos(turn) - shift.y
        yaw = -(facing + EXPORT_YAW_DEGREES); yaw = (yaw + 180) % 360 - 180
        berths.append([round(x, 1), round(-y, 1), round(bz - shift.z, 1), round(yaw, 1)])
    BERTHS.clear()
    uv = o.data.uv_layers.active or o.data.uv_layers.new(name='UVMap')
    for face in o.data.polygons:
        axis = max(range(3), key=lambda k: abs(face.normal[k])); axes = ([1, 2], [0, 2], [0, 1])[axis]
        for i in face.loop_indices:
            p = o.data.vertices[o.data.loops[i].vertex_index].co; uv.data[i].uv = (p[axes[0]] / 160, p[axes[1]] / 160)
    o.name = name; o.data.name = name + '_Mesh'; o.data.calc_loop_triangles(); dims = [hi[i] - lo[i] for i in range(3)]
    path = OUT / (name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'}, apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False, add_leaf_bones=False, mesh_smooth_type='FACE', path_mode='STRIP')
    records[name] = {'fbx': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'dimensions': dims, 'triangles': len(o.data.loop_triangles),
                     'materials': [m.name for m in o.data.materials], 'description': description, 'unreal_path': '/Game/Art/' + name, 'building_families': families}
    if berths:
        records[name]['berths_unreal'] = berths  # [x, y, z, yaw] cm/degrees in the Unreal mesh frame
    ASSETS[name] = o; PARTS.clear(); print('KIT_MESH ' + name + ' ' + json.dumps({'dims': [round(d) for d in dims], 'tris': records[name]['triangles']}), flush=True)


def wanted(kind):
    return ONLY is None or kind in ONLY


# ------------------------------------------------------------------ SOLAR ARRAYS (plot 2460, logistics 25.2 m class)
def solar_array(level):
    foundation(2460, 2460, 30)
    rows = {1: 4, 2: 5, 3: 5}[level]; cols = {1: 5, 2: 5, 3: 4}[level]
    pw = {1: 330, 2: 380, 3: 480}[level]; ph = {1: 200, 2: 230, 3: 300}[level]
    pitch_y = 2300 / rows; pitch_x = 2300 / cols
    for r in range(rows):
        y = -1150 + pitch_y * (r + .5)
        if level == 2:
            tube('Single-axis tracker torque tube', (-1150 + 60, y, 165), (1150 - 60, y, 165), 10, 'Steel', sides=10)
        for c in range(cols):
            x = -1150 + pitch_x * (c + .5)
            if level == 1:
                for sx in (-1, 1): beam('A-frame rail', (x + sx * pw * .42, y - 70, 30), (x + sx * pw * .42, y + 60, 150), 8, mat='Steel')
                solar_panel(x, y, 150, pw, ph, math.radians(28), post=False)
            elif level == 2:
                beam('Tracker pier', (x, y, 30), (x, y, 160), 14, mat='Steel'); solar_panel(x, y, 175, pw, ph, math.radians(22), post=False)
            else:
                beam('Dual-axis tracker mast', (x, y, 30), (x, y, 230), 20, mat='Steel'); cyl('Slew drive', (x, y, 236), 26, 22, 'Slate', 16)
                solar_panel(x, y, 262, pw, ph, math.radians(32), post=False)
        beam('Row cable tray', (-1150 + 40, y + 95, 36), (1150 - 40, y + 95, 36), 10, 6, 'Slate')
    kx = 1020 - 60 * level
    box('Inverter and switchgear kiosk', (kx, 1110, 110), (420 + 80 * level, 190, 160), 'Ceramic', 5); vent_y(kx, 1012, 110, 300, 90)
    for i in range(level): box('Inverter cooling unit', (kx - 120 + i * 120, 1110, 205), (90, 110, 50), 'Steel', 3)
    tube('Array service mast', (-1080, 1100, 30), (-1080, 1100, 520), 7, 'Steel'); box('Mast beacon', (-1080, 1100, 525), (16, 16, 16), 'Amber', 2)
    text('Array identity', 'PV / %02d' % level, (kx, 1010, 220), 36, mat='Ceramic')
    export('SM_SolarArray' + ('' if level == 1 else str(level)), ['Fixed-tilt photovoltaic field on A-frames with an inverter kiosk', 'Single-axis tracker photovoltaic field on torque tubes', 'Dual-axis tracker photovoltaic field on slewing masts'][level - 1], ['solar_array' if level == 1 else 'solar_array_%d' % level])


# ------------------------------------------------------------------ BATTERY BANK (plot 2460)
def battery_bank():
    foundation(2460, 2460, 40)
    for row, y in enumerate((-640, 0, 640)):
        for col, x in enumerate((-820, -275, 275, 820)):
            box('Battery container module', (x, y, 40 + 150), (500, 250, 300), 'Slate', 6)
            box('Container roof cooling plant', (x, y, 40 + 300 + 40), (330, 160, 80), 'Ceramic', 4); fan(x - 70, y, 40 + 300 + 84, 44); fan(x + 70, y, 40 + 300 + 84, 44)
            box('Fire-suppression cabinet', (x - 200, y - 130, 40 + 110), (60, 10, 180), 'Yellow', 2)
            box('Module status lamp', (x + 200, y - 130, 40 + 270), (30, 6, 10), 'Light', 1)
            box('Container corrugation seam', (x, y - 127, 40 + 150), (480, 2, 2), 'Carbon', 0)
        beam('Busbar gantry', (-1100, y + 150, 40 + 420), (1100, y + 150, 40 + 420), 24, 24, 'Steel')
        for x in (-1100, 1100): beam('Gantry column', (x, y + 150, 40), (x, y + 150, 40 + 420), 24, mat='Steel')
        for x in (-820, -275, 275, 820): pipe('Busbar riser', [(x, y + 150, 40 + 395), (x, y + 150, 40 + 320), (x, y + 60, 40 + 320)], 8, 'Copper')
    box('Power conversion hall', (0, -1050, 40 + 160), (1500, 300, 320), 'Ceramic', 8); vent_y(0, -1205, 200, 900, 160)
    box('Conversion hall roof', (0, -1050, 40 + 324), (1520, 320, 10), 'Slate', 2)
    for x in (-500, 0, 500): hvac(x, -1050, 40 + 320, 180, 240, 110)
    box('Grid interface transformer', (1050, -1050, 40 + 130), (300, 260, 260), 'Slate', 6)
    for k in range(6): box('Transformer cooling fin', (1050 - 125 + k * 50, -1050, 40 + 130), (8, 300, 220), 'Steel', .5)
    for k in range(3): cyl('Transformer bushing', (980 + k * 70, -1050, 40 + 300), 14, 80, 'Ceramic', 12)
    text('Bank identity', 'BESS', (0, -1200, 40 + 290), 48, mat='Ceramic')
    dress_hall(0, -1050, 40, 1500, 300, 320, ladder='+x', lamps='-y')
    scatter_props(40, 1230, 1230, seed=11, count=6)
    export('SM_BatteryBank', 'Twelve battery container modules under busbar gantries, with a power-conversion hall and grid transformer', ['battery_bank'])


# ------------------------------------------------------------------ TRADING PORT (plot 2460)
def trading_port(level):
    foundation(2460, 2460, 40)
    pr = 830 + 60 * (level - 1)
    cyl('Landing pad', (160, 60, 40 + 10), pr, 20, 'Concrete', 48); torus('Pad marking ring', (160, 60, 40 + 21), pr - 40, 4, 'Yellow'); torus('Pad inner marking', (160, 60, 40 + 21), pr * .42, 4, 'Yellow')
    for a in range(0, 360, 30):
        t = math.radians(a); box('Pad edge light', (160 + math.cos(t) * (pr - 12), 60 + math.sin(t) * (pr - 12), 40 + 24), (18, 18, 8), 'Light', 1)
    text('Pad identity', 'PORT %d' % level, (160, 60, 40 + 22), 200, rotation=(0, 0, 0), mat='Yellow')
    tx, ty = -980, -950
    cyl('Control tower shaft', (tx, ty, 40 + 330), 110, 660, 'Ceramic', 16); cyl('Control tower cab', (tx, ty, 40 + 760), 190, 200, 'Slate', 12)
    for i in range(12):
        a = (i + .5) * math.tau / 12; box('Tower cab glazing', (tx + math.cos(a) * 182, ty + math.sin(a) * 182, 40 + 775), (92, 10, 120), 'Glass', 2, rotation=a - math.pi / 2)
    cyl('Tower cab roof', (tx, ty, 40 + 872), 205, 24, 'Ceramic', 12); tube('Approach radar mast', (tx, ty, 40 + 884), (tx, ty, 40 + 1140), 9, 'Steel')
    box('Radar array', (tx, ty, 40 + 1150), (160, 30, 70), 'Steel', 3); box('Mast beacon', (tx, ty, 40 + 1200), (20, 20, 20), 'Amber', 2)
    gy = -520 - 80 * level
    for x in (-1130, 1130): beam('Cargo gantry column', (x, gy, 40), (x, gy, 40 + 560), 40, mat='Yellow')
    beam('Cargo gantry bridge', (-1150, gy, 40 + 560), (1150, gy, 40 + 560), 44, 60, 'Yellow'); beam('Gantry rail', (-1150, gy, 40 + 598), (1150, gy, 40 + 598), 12, 70, 'Steel')
    box('Gantry trolley', (-100 + 300 * level, gy, 40 + 640), (200, 160, 80), 'Slate', 4)
    for k in range(2): tube('Hoist cable', (-150 + 300 * level + k * 100, gy, 40 + 600), (-150 + 300 * level + k * 100, gy, 40 + 320), 3, 'Steel', sides=6)
    box('Spreader beam', (-100 + 300 * level, gy, 40 + 310), (360, 60, 24), 'Yellow', 3)
    stacks = {1: [(900, 900)], 2: [(900, 900), (900, 620)], 3: [(900, 900), (900, 620), (620, 900)]}[level]
    for i, (x, y) in enumerate(stacks):
        for k in range(2 if i == 0 else 1): box('Cargo container', (x, y, 40 + 122 + k * 245), (600, 245, 245), ['Blue', 'Yellow', 'Ceramic'][(i + k) % 3], 5)
    box('Customs and dispatch office', (-900, 950, 40 + 150), (700, 400, 300), 'Ceramic', 8); window_y(-900, 748, 40 + 190, 460, 110); door_y(-650, 748, 40, 96, 206)
    roof_barrel(-900, 950, 40 + 300, 720, 420, 60, 'Slate')
    for x in (-500, 400, 900):
        tube('Apron floodlight mast', (x, -1150, 40), (x, -1150, 40 + 700), 8, 'Steel'); box('Floodlight head', (x, -1150, 40 + 710), (90, 30, 30), 'Light', 2)
    dress_hall(-900, 950, 40, 700, 400, 300, ladder='-x', lamps='-y', parapet=False)
    scatter_props(40, 1230, 1230, seed=20 + level, count=4 + 2 * level)
    export('SM_TradingPort' + ('' if level == 1 else str(level)), 'Shuttle landing pad with edge lighting, control tower, cargo gantry crane, container stacks and a dispatch office', ['trading_port' if level == 1 else 'trading_port_%d' % level])


# ------------------------------------------------------------------ ALLOY REFINERY (processor plot 2070)
def refinery():
    foundation(2060, 2060)
    box('Smelting hall', (-380, 120, 48 + 260), (1250, 1400, 520), 'Slate', 8)
    box('Smelting hall roof', (-380, 120, 48 + 524), (1270, 1420, 12), 'Ceramic', 2)
    box('Smelting hall apron', (-380, -820, 48 + 6), (700, 420, 12), 'Carbon', 1)
    for x in range(-900, 200, 220): window_y(x, -587, 48 + 400, 160, 90)
    shutter_y(-380, -590, 48, 520, 380)
    for i, x in enumerate((420, 780)):
        cyl('Blast furnace shell', (x, 300, 48 + 420), 230 - 20 * i, 840, 'Slate', 32); cyl('Furnace throat cone', (x, 300, 48 + 880), 230 - 20 * i, 120, 'Steel', 32, 110)
        cyl('Furnace hearth jacket', (x, 300, 48 + 110), 250 - 20 * i, 220, 'Steel', 32)
        for zz in (48 + 230, 48 + 480, 48 + 730): torus('Furnace reinforcement band', (x, 300, zz), 232 - 20 * i, 8, 'Steel')
        tube('Furnace exhaust stack', (x, 300, 48 + 940), (x, 300, 48 + 1550 - 150 * i), 60 - 8 * i, 'Steel'); torus('Stack cap', (x, 300, 48 + 1550 - 150 * i), 60 - 8 * i, 10, 'Slate')
        box('Tapping floor', (x, 300, 48 + 240), (560, 560, 20), 'Steel', 2)
        for a in range(0, 360, 90):
            t = math.radians(a); beam('Furnace access stair stringer', (x + math.cos(t) * 300, 300 + math.sin(t) * 300, 48 + 20), (x + math.cos(t) * 300, 300 + math.sin(t) * 300, 48 + 240), 14, mat='Steel')
        box('Hot blast duct', (x, 300, 48 + 600), (40, 40, 40), 'Copper', 2)
        pipe('Hot blast main', [(x, 70, 48 + 600), (x, -300, 48 + 600), (x, -300, 48 + 300), (x, -700, 48 + 300)], 24, 'Copper')
    pipe('Downcomer', [(420, 540, 48 + 900), (420, 760, 48 + 900), (420, 760, 48 + 200)], 32, 'Steel'); pipe('Downcomer', [(780, 540, 48 + 800), (780, 760, 48 + 800), (780, 760, 48 + 200)], 28, 'Steel')
    box('Dust cyclone house', (620, 780, 48 + 150), (420, 300, 300), 'Ceramic', 6); cyl('Dust cyclone', (620, 780, 48 + 420), 110, 240, 'Steel', 24); cyl('Cyclone cone', (620, 780, 48 + 240), 110, 120, 'Steel', 24, 30)
    for x in (-800, -500): beam('Ore conveyor stringer', (x, -760, 48 + 60), (x, 640, 48 + 560), 22, 30, 'Steel')
    beam('Ore conveyor belt', (-650, -750, 48 + 80), (-650, 630, 48 + 580), 300, 14, 'Carbon')
    for y in range(-700, 620, 110): beam('Conveyor transverse cleat', (-790, y, 48 + 90 + (y + 750) * .36), (-510, y, 48 + 90 + (y + 750) * .36), 10, 12, 'Yellow')
    box('Ore receiving hopper', (-650, -850, 48 + 120), (560, 300, 240), 'Ceramic', 8); box('Hopper grizzly', (-650, -850, 48 + 245), (500, 240, 10), 'Steel', 1)
    box('Slag granulation pit', (-650, 850, 48 + 25), (500, 300, 50), 'Concrete', 4); box('Granulation water', (-650, 850, 48 + 52), (470, 270, 4), 'Glass', 0)
    text('Refinery identity', 'ALLOY', (-380, -590, 48 + 500), 60, mat='Yellow')
    dress_hall(-380, 120, 48, 1250, 1400, 520, ladder='-x', lamps='-y')
    scatter_props(48, 1030, 1030, seed=31, count=8)
    export('SM_Refinery', 'Smelting hall with twin blast furnaces, hot-blast mains, tall stacks, inclined ore conveyor, receiving hopper, dust cyclone and slag pit', ['alloy_refinery'])


# ------------------------------------------------------------------ FUEL / BIOFUEL REFINERY (processor plot 2070)
def fuel_refinery():
    foundation(2060, 2060)
    box('Tank farm bund wall', (-430, 420, 48 + 45), (1180, 1180, 90), 'Concrete', 6); box('Bund interior', (-430, 420, 48 + 92), (1120, 1120, 4), 'Carbon', 0)
    for i, (x, y) in enumerate(((-720, 140), (-150, 140), (-430, 700))):
        r = 250 if i < 2 else 300
        cyl('Fuel storage tank', (x, y, 48 + 92 + 220), r, 440, 'Ceramic', 48); cyl('Tank roof cone', (x, y, 48 + 92 + 468), r, 56, 'Steel', 48, r * .12)
        for zz in (48 + 92 + 80, 48 + 92 + 220, 48 + 92 + 360): torus('Tank wind girder', (x, y, zz), r + 4, 6, 'Slate')
        for a in range(0, 360, 90):
            t = math.radians(a); beam('Tank spiral stair', (x + math.cos(t) * (r + 20), y + math.sin(t) * (r + 20), 48 + 92 + 60 + a * 1.1), (x + math.cos(t + .5) * (r + 20), y + math.sin(t + .5) * (r + 20), 48 + 92 + 60 + (a + 90) * 1.1), 12, 36, 'Steel')
        box('Tank manway', (x, y - r - 2, 48 + 92 + 80), (60, 10, 60), 'Slate', 3)
    cx, cy = 560, -120
    cyl('Distillation column', (cx, cy, 48 + 700), 120, 1400, 'Steel', 32); cyl('Column dished head', (cx, cy, 48 + 1420), 120, 60, 'Steel', 32, 50)
    for zz in range(48 + 300, 48 + 1400, 280):
        cyl('Column platform', (cx, cy, zz), 200, 14, 'Slate', 24); torus('Platform handrail', (cx, cy, zz + 60), 196, 4, 'Yellow'); torus('Platform handrail', (cx, cy, zz + 110), 196, 4, 'Yellow')
    cyl('Column skirt', (cx, cy, 48 + 60), 140, 120, 'Concrete', 24)
    pipe('Overhead vapour line', [(cx, cy, 48 + 1380), (cx + 300, cy, 48 + 1380), (cx + 300, cy, 48 + 400), (cx + 300, cy + 300, 48 + 400)], 22, 'Steel')
    box('Overhead condenser', (cx + 300, cy + 380, 48 + 400), (220, 160, 220), 'Ceramic', 6); fan(cx + 300, cy + 380, 48 + 518, 70)
    cyl('Reflux drum', (cx + 300, cy + 620, 48 + 220), 90, 320, 'Steel', 24)
    fx, fy = 860, 760
    tube('Flare stack', (fx, fy, 48), (fx, fy, 48 + 1700), 22, 'Steel'); cyl('Flare tip', (fx, fy, 48 + 1720), 32, 60, 'Slate', 12); cyl('Flare flame', (fx, fy, 48 + 1790), 26, 90, 'Amber', 12, 6)
    for a in range(0, 360, 120):
        t = math.radians(a); beam('Flare guy wire', (fx + math.cos(t) * 260, fy + math.sin(t) * 260, 48), (fx, fy, 48 + 1300), 3, mat='Steel')
    box('Pump house', (300, 760, 48 + 150), (520, 420, 300), 'Slate', 8); vent_y(300, 548, 48 + 170, 360, 140); door_y(520, 548, 48, 96, 206)
    box('Pump house roof', (300, 760, 48 + 304), (540, 440, 10), 'Ceramic', 2)
    for x in (-800, -200, 400): pipe('Pipe rack run', [(x, -900, 48 + 230), (x, 900, 48 + 230)], 11, 'Steel'); pipe('Pipe rack run', [(x + 40, -900, 48 + 230), (x + 40, 900, 48 + 230)], 8, 'Copper')
    for y in range(-900, 901, 450):
        beam('Pipe rack portal', (-1000, y, 48), (-1000, y, 48 + 250), 16, mat='Steel'); beam('Pipe rack portal', (1000, y, 48), (1000, y, 48 + 250), 16, mat='Steel'); beam('Pipe rack cross', (-1000, y, 48 + 250), (1000, y, 48 + 250), 14, 20, 'Steel')
    shutter_y(-430, -980, 48, 360, 280, 'Carbon')
    text('Refinery identity', 'FUEL', (-430, -985, 48 + 420), 60, mat='Yellow')
    dress_hall(300, 760, 48, 520, 420, 300, ladder='+x', lamps='-y')
    scatter_props(48, 1030, 1030, seed=37, count=6)
    export('SM_FuelRefinery', 'Bunded tank farm, distillation column with platforms, overhead condenser, reflux drum, flare stack, pump house and pipe racks', ['fuel_refinery', 'biofuel_refinery'])


# ------------------------------------------------------------------ FOOD PRODUCER (processor plot 2070)
def greenhouse():
    foundation(2060, 2060, 30)
    for i, x in enumerate((-690, -230, 230, 690)):
        gable_glass(x, 160, 30, 420, 1560, 300)
        for y in range(-560, 900, 300): box('Grow bench', (x, y, 30 + 60), (340, 240, 100), 'Carbon', 2); box('Crop canopy', (x, y, 30 + 130), (320, 220, 40), 'Blue', 6)
    box('Processing and packing block', (0, -860, 30 + 180), (1900, 420, 360), 'Ceramic', 8)
    box('Packing block roof', (0, -860, 30 + 364), (1920, 440, 10), 'Slate', 2)
    box('Dispatch apron', (-620, -1250, 30 + 6), (520, 330, 12), 'Carbon', 1)
    for x in (-700, -350, 0, 350, 700): window_y(x, -1072, 30 + 230, 220, 110)
    shutter_y(-620, -1075, 30, 360, 260); door_y(620, -1075, 30, 96, 206)
    for x in (-600, 0, 600): hvac(x, -860, 30 + 360, 180, 240, 110)
    for x in (-900, 900): cyl('Irrigation water tank', (x, 980, 30 + 170), 90, 340, 'Ceramic', 24); cyl('Water tank cap', (x, 980, 30 + 355), 90, 30, 'Steel', 24, 30)
    pipe('Irrigation main', [(-900, 980, 30 + 240), (-900, 900, 30 + 240), (900, 900, 30 + 240), (900, 980, 30 + 240)], 8, 'Blue')
    text('Producer identity', 'FOOD', (0, -1075, 30 + 330), 60, mat='Ceramic')
    dress_hall(0, -860, 30, 1900, 420, 360, ladder='+x', lamps='-y')
    scatter_props(30, 1030, 1030, seed=41, count=6)
    export('SM_Greenhouse', 'Four glazed gable greenhouse bays with grow benches, a processing block, irrigation tanks and mains', ['food_producer'])


# ------------------------------------------------------------------ WORKS: clean production hall (conductor, substrate, circuit, component, battery)
def works():
    foundation(2060, 2060)
    box('Production hall', (-120, 100, 48 + 290), (1560, 1500, 580), 'Ceramic', 8)
    box('Dark roof membrane', (-120, 100, 48 + 584), (1580, 1520, 10), 'Slate', 2)
    box('Delivery apron', (-520, -860, 48 + 6), (640, 380, 12), 'Carbon', 1)
    for x in range(-760, 561, 220):
        box('Facade cassette seam', (x - 110, -652, 48 + 290), (3, 6, 560), 'Carbon', 0); window_y(x, -652, 48 + 440, 160, 100)
    box('Window band', (-120, -654, 48 + 180), (1500, 8, 120), 'Glass', 1)
    shutter_y(-520, -655, 48, 420, 340); door_y(300, -655, 48, 96, 206)
    for y in (-450, -150, 150, 450):
        for x in (-700, -300, 100, 500): box('Roof skylight', (x, y, 48 + 585), (260, 180, 24), 'Glass', 2)
    for x in (-750, -250, 250): hvac(x, 650, 48 + 580, 220, 300, 160)
    box('Chilled-water plant', (640, 600, 48 + 120), (420, 420, 240), 'Slate', 6)
    for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)): fan(640 + sx * 100, 600 + sy * 100, 48 + 248, 80)
    for k in range(4): cyl('Process gas cylinder', (820, -200 + k * 110, 48 + 120), 34, 240, ['Steel', 'Blue', 'Steel', 'Yellow'][k], 16)
    box('Gas cabinet canopy', (820, -35, 48 + 270), (140, 520, 16), 'Steel', 2)
    pipe('Process gas main', [(820, 160, 48 + 240), (820, 300, 48 + 240), (660, 300, 48 + 240), (660, 300, 48 + 450), (660, 650, 48 + 450)], 9, 'Steel')
    box('Shipping dock', (-520, -860, 48 + 40), (560, 300, 80), 'Concrete', 4); box('Dock leveller', (-520, -780, 48 + 82), (300, 100, 6), 'Yellow', 1)
    text('Works identity', 'WORKS', (-120, -660, 48 + 540), 60, mat='Carbon')
    dress_hall(-120, 100, 48, 1560, 1500, 580, ladder='-x', lamps='-y')
    scatter_props(48, 1030, 1030, seed=43, count=9)
    export('SM_Works', 'White clean production hall with a window band, roof skylights, rooftop air handling, chilled-water plant and process-gas cabinet', ['conductor_works', 'substrate_works', 'circuit_works', 'component_works', 'battery_works'])


# ------------------------------------------------------------------ AI CHIP WORKS: two-storey cleanroom block
def chip_works():
    foundation(2060, 2060)
    box('Cleanroom block', (-150, 100, 48 + 400), (1400, 1300, 800), 'Ceramic', 8)
    for z in (48 + 200, 48 + 600):
        for x in range(-750, 451, 200): box('Cleanroom panel joint', (x, -552, z), (3, 6, 360), 'Carbon', 0)
        box('Cleanroom window strip', (-150, -554, z + 60), (1300, 8, 80), 'Glass', 1)
    box('Interstitial service floor band', (-150, 100, 48 + 400), (1416, 1316, 40), 'Slate', 2)
    box('Fab roof air plenum', (-150, 100, 48 + 870), (1200, 1100, 140), 'Slate', 8)
    for x in (-600, -150, 300): hvac(x, 100, 48 + 940, 240, 340, 170)
    for y in (-350, 100, 550): pipe('External exhaust duct', [(560, y, 48 + 100), (560, y, 48 + 1120), (-150, y, 48 + 1120)], 30, 'Steel')
    tube('Scrubber exhaust stack', (640, -350, 48 + 1000), (640, -350, 48 + 1500), 28, 'Steel'); torus('Stack cap', (640, -350, 48 + 1500), 28, 6, 'Slate')
    cyl('Bulk nitrogen tank', (820, 650, 48 + 300), 110, 600, 'Ceramic', 32); cyl('Nitrogen tank head', (820, 650, 48 + 620), 110, 50, 'Ceramic', 32, 20)
    cyl('Nitrogen vaporizer', (820, 350, 48 + 160), 60, 320, 'Steel', 12)
    for k in range(6): box('Vaporizer fin', (820, 350, 48 + 160), (170, 10, 300), 'Steel', .5, rotation=k * math.pi / 6)
    box('Gowning and office wing', (-150, -850, 48 + 160), (1000, 320, 320), 'Ceramic', 8); window_y(-350, -1012, 48 + 200, 300, 110); door_y(100, -1012, 48, 96, 206)
    box('Office wing roof', (-150, -850, 48 + 324), (1020, 340, 10), 'Slate', 2)
    box('Airlock apron', (-1100, 200, 48 + 6), (260, 460, 12), 'Carbon', 1)
    box('Loading airlock', (-900, 200, 48 + 170), (260, 400, 340), 'Slate', 6)
    box('Airlock door', (-1031, 200, 48 + 150), (6, 220, 280), 'Carbon', 2)
    text('Works identity', 'CHIP FAB', (-150, -1015, 48 + 300), 50, mat='Blue')
    dress_hall(-150, 100, 48, 1400, 1300, 800, ladder='-x', lamps=None)
    dress_hall(-150, -850, 48, 1000, 320, 320, ladder=None, lamps='-y', pipes=False)
    scatter_props(48, 1030, 1030, seed=47, count=6)
    export('SM_ChipWorks', 'Two-storey cleanroom block with interstitial floor, roof plenum, external exhaust ducts, scrubber stack, nitrogen tank and gowning wing', ['ai_chip_works'])


# ------------------------------------------------------------------ FUSION REACTOR WORKS
def fusion_works():
    foundation(2060, 2060)
    cx, cy = -200, 150
    cyl('Containment drum', (cx, cy, 48 + 150), 640, 300, 'Concrete', 48)
    dome('Containment dome', (cx, cy, 48 + 300), 620, 'Ceramic', 48, 10, 0)
    torus('Dome base flange', (cx, cy, 48 + 302), 632, 10, 'Steel'); torus('Dome ring light', (cx, cy, 48 + 300), 644, 5, 'Light')
    for a in range(0, 360, 30):
        t = math.radians(a); beam('Dome buttress', (cx + math.cos(t) * 660, cy + math.sin(t) * 660, 48), (cx + math.cos(t) * 420, cy + math.sin(t) * 420, 48 + 760), 26, 40, 'Slate')
    cyl('Dome apex vent', (cx, cy, 48 + 940), 70, 60, 'Steel', 16); box('Apex beacon', (cx, cy, 48 + 985), (20, 20, 20), 'Amber', 2)
    box('Turbine hall', (720, 150, 48 + 220), (560, 1300, 440), 'Slate', 8); roof_barrel(720, 150, 48 + 440, 580, 1320, 70)
    for y in (-350, 0, 350): box('Turbine hall louvre', (438, y, 48 + 300), (6, 160, 200), 'Carbon', 1)
    for i, y in enumerate((-650, 650)):
        cyl('Cooling tower', (-200, y + (-200 if i == 0 else 200), 48 + 380), 250, 760, 'Concrete', 32, 160); cyl('Cooling tower throat', (-200, y + (-200 if i == 0 else 200), 48 + 790), 160, 60, 'Concrete', 32, 185)
        box('Cooling tower plume', (-200, y + (-200 if i == 0 else 200), 48 + 860), (120, 120, 60), 'Ceramic', 20)
    for y in (-100, 100): pipe('Primary coolant loop', [(cx + 600, y, 48 + 330), (420, y, 48 + 330), (420, y, 48 + 180)], 26, 'Copper')
    pipe('Steam main', [(440, 500, 48 + 500), (300, 500, 48 + 500), (300, 850, 48 + 500), (-200, 850, 48 + 500)], 20, 'Steel')
    box('Control annex', (-950, -850, 48 + 150), (360, 320, 300), 'Ceramic', 8); window_y(-950, -1012, 48 + 200, 240, 100); door_y(-850, -1012, 48, 96, 206)
    for x in (300, 700): box('Switchyard transformer', (x, -850, 48 + 110), (220, 200, 220), 'Slate', 5)
    for x in (300, 700):
        for k in range(3): cyl('Transformer bushing', (x - 60 + k * 60, -850, 48 + 270), 12, 90, 'Ceramic', 12)
    text('Works identity', 'FUSION', (cx, cy - 650, 48 + 230), 60, mat='Ceramic')
    dress_hall(720, 150, 48, 560, 1300, 440, ladder='+x', lamps=None, parapet=False)
    dress_hall(-950, -850, 48, 360, 320, 300, ladder=None, lamps='-y')
    scatter_props(48, 1030, 1030, seed=53, count=5)
    export('SM_FusionWorks', 'Containment dome on a concrete drum with buttresses, barrel-roofed turbine hall, twin cooling towers, coolant and steam mains, control annex and switchyard', ['fusion_reactor_works'])


# ------------------------------------------------------------------ AMMUNITION WORKS: bunkered magazines
def ammunition_works():
    foundation(2060, 2060)
    for i, x in enumerate((-640, 0, 640)):
        box('Magazine cell', (x, 200, 48 + 170), (500, 1100, 340), 'Concrete', 10)
        box('Magazine earth berm', (x, 200, 48 + 90), (640, 1240, 180), 'Slate', 40)
        box('Blast door frame', (x, -352, 48 + 150), (260, 20, 300), 'Slate', 4); box('Blast door leaf', (x, -364, 48 + 140), (220, 14, 260), 'Carbon', 3)
        for sx in (-1, 1): box('Door hazard stripe', (x + sx * 100, -372, 48 + 140), (16, 2, 250), 'Yellow', .5)
        text('Magazine letter', 'ABC'[i], (x, -375, 48 + 320), 70, mat='Yellow')
        tube('Lightning mast', (x + 300, 800, 48), (x + 300, 800, 48 + 900), 7, 'Steel'); box('Mast finial', (x + 300, 800, 48 + 905), (10, 10, 10), 'Steel', 1)
    for x in (-320, 320): box('Blast wall', (x, 200, 48 + 210), (60, 1200, 420), 'Concrete', 6)
    box('Filling and assembly hall', (0, -820, 48 + 160), (1600, 380, 320), 'Ceramic', 8)
    box('Filling hall roof', (0, -820, 48 + 324), (1620, 400, 10), 'Slate', 2)
    box('Dispatch apron', (-500, -1250, 48 + 6), (520, 300, 12), 'Carbon', 1)
    for x in (-600, -200, 200, 600): vent_y(x, -1012, 48 + 190, 180, 120)
    shutter_y(-500, -1012, 48, 300, 240); door_y(500, -1012, 48, 96, 206)
    for x in (-400, 400): hvac(x, -820, 48 + 320, 180, 240, 100)
    box('Explosives transfer road', (0, -500, 48 + 4), (1800, 200, 8), 'Carbon', 1)
    for x in range(-800, 801, 400): box('Road marking', (x, -500, 48 + 9), (120, 14, 1), 'Yellow', 0)
    text('Works identity', 'MUNITIONS', (0, -1012, 48 + 300), 46, mat='Yellow')
    dress_hall(0, -820, 48, 1600, 380, 320, ladder='+x', lamps='-y')
    scatter_props(48, 1030, 1030, seed=59, count=6)
    export('SM_AmmunitionWorks', 'Three bermed magazine cells behind blast walls and doors, lightning masts, a filling hall and a transfer road', ['ammunition_works'])


# ------------------------------------------------------------------ FUEL GENERATOR: gen-set containers
def fuel_generator():
    foundation(2060, 2060, 40)
    for i, y in enumerate((-430, 230)):
        box('Generator container', (-150, y, 40 + 140), (1220, 300, 280), 'Slate', 6)
        for k in range(5): vent_y(-150 - 480 + k * 240, y - 155, 40 + 150, 180, 160)
        box('Container roof radiator', (-150 + 400, y, 40 + 300), (380, 240, 40), 'Ceramic', 3); fan(-150 + 320, y, 40 + 324, 70); fan(-150 + 490, y, 40 + 324, 70)
        for k in range(2):
            x = -150 - 420 + k * 300
            tube('Exhaust riser', (x, y, 40 + 280), (x, y, 40 + 560), 18, 'Steel'); cyl('Exhaust silencer', (x, y, 40 + 620), 46, 180, 'Steel', 20); tube('Exhaust stack', (x, y, 40 + 710), (x, y, 40 + 980), 16, 'Steel')
        box('Container control panel', (-150 + 600, y - 160, 40 + 160), (120, 10, 200), 'Ceramic', 2); box('Panel display', (-150 + 600, y - 166, 40 + 220), (60, 2, 40), 'Light', .5)
    tx = 760
    cyl('Day tank', (tx, -100, 40 + 190), 150, 900, 'Ceramic', 32); cyl('Day tank head', (tx, -100 - 480, 40 + 190), 150, 60, 'Ceramic', 32, 80)
    for yy in (-420, 220): box('Tank saddle', (tx, yy, 40 + 60), (340, 80, 120), 'Concrete', 4)
    pipe('Fuel supply line', [(tx, 200, 40 + 190), (tx, 500, 40 + 190), (-150, 500, 40 + 190), (-150, 380, 40 + 190)], 10, 'Copper')
    box('Fuel bund', (tx, -100, 40 + 20), (420, 1060, 40), 'Concrete', 4)
    box('Switchgear room', (-150, 800, 40 + 150), (1000, 300, 300), 'Ceramic', 8); vent_y(-150, 645, 40 + 160, 600, 140); door_y(250, 645, 40, 96, 206)
    box('Switchgear roof', (-150, 800, 40 + 304), (1020, 320, 10), 'Slate', 2)
    for x in (-500, -100, 300): box('Outdoor breaker', (x, 1000, 40 + 120), (120, 90, 240), 'Slate', 4)
    text('Generator identity', 'GEN', (-150, -590, 40 + 300), 60, mat='Yellow')
    dress_hall(-150, 800, 40, 1000, 300, 300, ladder='-x', lamps='-y')
    scatter_props(40, 1030, 1030, seed=61, count=8)
    export('SM_FuelGenerator', 'Twin containerised generator sets with radiators and silenced exhaust stacks, a bunded day tank and a switchgear room', ['fuel_generator'])


# ------------------------------------------------------------------ HANGARS: vehicle, tank and mech factories (plot 3120)
def hangar(kind):
    foundation(3100, 3100)
    w, d, h = {'wheeled': (2300, 2200, 760), 'tracked': (2500, 2300, 820), 'mech': (2100, 2100, 1150)}[kind]
    hx, hy = -150, 250
    box('Assembly hangar', (hx, hy, 48 + h / 2), (w, d, h), 'Ceramic', 10)
    roof_barrel(hx, hy, 48 + h, w + 20, d + 20, {'wheeled': 160, 'tracked': 150, 'mech': 110}[kind], 'Slate')
    box('Hangar base band', (hx, hy, 48 + 60), (w + 8, d + 8, 120), 'Slate', 4)
    for x in range(int(hx - w / 2 + 150), int(hx + w / 2 - 100), 300): box('Hangar rib', (x, hy, 48 + h / 2), (24, d + 30, h), 'Steel', 2)
    dw, dh = {'wheeled': (1300, 560), 'tracked': (1500, 600), 'mech': (900, 980)}[kind]
    shutter_y(hx, hy - d / 2, 48, dw, dh)
    for sx in (-1, 1): window_y(hx + sx * (dw / 2 + 300), hy - d / 2 - 2, 48 + h * .55, 220, 110)
    box('Assembly apron', (hx, hy - d / 2 - 480, 48 + 6), (dw + 600, 900, 12), 'Carbon', 1)
    for x in range(int(hx - dw / 2), int(hx + dw / 2) + 1, 200): box('Apron guide line', (x, hy - d / 2 - 480, 48 + 13), (10, 860, 1), 'Yellow', 0)
    if kind == 'tracked':
        box('Heavy loading ramp', (hx, hy - d / 2 - 980, 48 + 30), (dw, 300, 60), 'Concrete', 6)
        for x in (hx - 700, hx + 700): cyl('Ramp bollard', (x, hy - d / 2 - 1000, 48 + 90), 22, 120, 'Yellow', 12)
    if kind == 'mech':
        gx = hx + w / 2 + 230
        for y in (hy - 600, hy + 600): beam('Erection gantry column', (gx, y, 48), (gx, y, 48 + 1500), 60, mat='Yellow')
        beam('Erection gantry bridge', (gx, hy - 640, 48 + 1500), (gx, hy + 640, 48 + 1500), 60, 80, 'Yellow'); box('Erection trolley', (gx, hy, 48 + 1560), (120, 300, 100), 'Slate', 4)
        box('Mech erection pad', (gx, hy, 48 + 6), (400, 1400, 12), 'Carbon', 1)
    for x in (hx - w * .3, hx, hx + w * .3): hvac(x, hy + d / 2 - 250, 48 + h + 20, 220, 300, 140)
    box('Parts store', (hx + w / 2 + 180 if kind != 'mech' else hx - w / 2 - 240, hy + 400, 48 + 170), (300, 1000, 340), 'Ceramic', 8)
    for k in range(4): box('Parts container', (hx - w / 2 - 240 + (k % 2) * 260, hy - 500 + (k // 2) * 300, 48 + 122), (240, 245, 245), ['Blue', 'Yellow'][k % 2], 5)
    text('Hangar identity', {'wheeled': 'WHEELED', 'tracked': 'TRACKED', 'mech': 'MECH'}[kind], (hx, hy - d / 2 - 4, 48 + h - 60), 70, mat='Yellow')
    dress_hall(hx, hy, 48, w, d, h, ladder='+x' if kind != 'mech' else '-x', lamps='-y', parapet=False)
    scatter_props(48, 1550, 1550, seed={'wheeled': 67, 'tracked': 71, 'mech': 73}[kind], count=10)
    export({'wheeled': 'SM_Hangar', 'tracked': 'SM_HangarTracked', 'mech': 'SM_HangarMech'}[kind],
           {'wheeled': 'Barrel-roofed assembly hangar with a wide roller door, marked apron, parts store and container yard',
            'tracked': 'Heavier barrel-roofed hangar with a reinforced ramp, wide roller door, parts store and container yard',
            'mech': 'Tall assembly hangar with a high door and an external erection gantry for walking chassis'}[kind],
           {'wheeled': ['vehicle_factory', 'vehicle_factory_2', 'vehicle_factory_3'], 'tracked': ['tank_factory', 'tank_factory_2', 'tank_factory_3'], 'mech': ['mech_factory', 'mech_factory_2', 'mech_factory_3']}[kind])



# ------------------------------------------------------------------ TOWER BASES (footprint 90 -> 1080 cm; mounts sit at z=250)
def tower_extras(kind, level, deck_z):
    """Visible level growth around an unchanged 250 cm weapon deck. Level 2:
    deck guard rail, radar mast, second equipment locker, barrier line. Level 3
    adds a wider armoured deck skirt, floodlight masts, a generator pack and a
    taller surveillance mast. Every part stays clear of the level-1 features."""
    if level < 2:
        return
    square = kind in ('kinetic', 'missile')
    if square:
        for sx in (-1, 1):
            box('Deck guard rail', (sx * 372, 0, deck_z + 90), (6, 760, 6), 'Yellow', 0); box('Deck guard rail', (0, sx * 372, deck_z + 90), (760, 6, 6), 'Yellow', 0)
            box('Deck mid rail', (sx * 372, 0, deck_z + 50), (5, 760, 5), 'Steel', 0); box('Deck mid rail', (0, sx * 372, deck_z + 50), (760, 5, 5), 'Steel', 0)
        for sx in (-1, 1):
            for k in range(-3, 4): beam('Deck rail post', (sx * 372, k * 125, deck_z), (sx * 372, k * 125, deck_z + 92), 5, mat='Steel'); beam('Deck rail post', (k * 125, sx * 372, deck_z), (k * 125, sx * 372, deck_z + 92), 5, mat='Steel')
    else:
        torus('Deck guard rail', (0, 0, deck_z + 90), 372, 3, 'Yellow'); torus('Deck mid rail', (0, 0, deck_z + 50), 372, 2.5, 'Steel')
        for a in range(0, 360, 30):
            t = math.radians(a); beam('Deck rail post', (math.cos(t) * 372, math.sin(t) * 372, deck_z), (math.cos(t) * 372, math.sin(t) * 372, deck_z + 92), 5, mat='Steel')
    mx, my = -440, 440
    tube('Radar mast', (mx, my, 24), (mx, my, deck_z + 260), 9, 'Steel', sides=10)
    for z in (24 + 150, deck_z + 60): box('Mast stay collar', (mx, my, z), (30, 30, 10), 'Slate', 0)
    cyl('Radar dish', (mx, my, deck_z + 280), 55, 14, 'Ceramic', 16, 20).rotation_euler = (math.radians(70), 0, math.radians(-45))
    box('Mast beacon', (mx, my, deck_z + 268), (14, 14, 14), 'Amber', 0)
    lx, ly = (-440, -300) if kind == 'missile' else (440, 440)
    box('Equipment locker', (lx, ly, 24 + 85), (190, 150, 170), 'Slate', 4); vent_y(lx, ly - 77, 24 + 95, 120, 90)
    box('Locker status lamp', (lx + 50, ly - 77, 24 + 160), (24, 6, 8), 'Light', 0)
    for x in (-300, 0, 300): box('Concrete jersey barrier', (x, -505, 24 + 40), (280, 56, 80), 'Concrete', 14)
    if level < 3:
        return
    if square:
        box('Armoured deck skirt', (0, 0, deck_z - 32), (900, 900, 24), 'Slate', 6)
        for sx in (-1, 1): box('Skirt hazard band', (sx * 452, 0, deck_z - 30), (4, 880, 14), 'Yellow', 0); box('Skirt hazard band', (0, sx * 452, deck_z - 30), (880, 4, 14), 'Yellow', 0)
    else:
        cyl('Armoured deck skirt', (0, 0, deck_z - 32), 455, 24, 'Slate', 32); torus('Skirt hazard band', (0, 0, deck_z - 32), 456, 6, 'Yellow')
    for sx, sy in ((1, -1), (-1, -1)):
        x, y = sx * 470, sy * 470
        tube('Floodlight mast', (x, y, 24), (x, y, deck_z + 330), 7, 'Steel', sides=8); box('Floodlight bank', (x, y, deck_z + 338), (70, 26, 26), 'Light', 0)
    gx, gy = (-420, 200) if kind == 'missile' else (-420, -200)
    box('Generator pack', (gx, gy, 24 + 75), (170, 300, 150), 'Ceramic', 5)
    for k in range(5): box('Generator radiator fin', (gx - 86, gy - 120 + k * 60, 24 + 80), (4, 40, 120), 'Steel', 0)
    tube('Generator exhaust', (gx, gy + 80, 24 + 150), (gx, gy + 80, 24 + 330), 9, 'Steel', sides=10)
    tube('Surveillance mast', (480, -150, 24), (480, -150, deck_z + 520), 10, 'Steel', sides=10)
    box('Surveillance pod', (480, -150, deck_z + 530), (60, 60, 46), 'Slate', 4); box('Surveillance lens', (480, -182, deck_z + 530), (30, 4, 20), 'Glass', 0)


def tower_base(kind, level=1):
    """Four weapon-family bases sharing one 760 cm deck at 250 cm, where the
    procedural weapon mounts of every level are placed by SeigeCombatVisuals."""
    foundation(1080, 1080, 24)
    deck_z = 250
    suffix = '' if level == 1 else str(level)
    tower_extras(kind, level, deck_z)
    if kind == 'laser':
        cyl('Octagonal mast', (0, 0, 24 + 108), 230, 216, 'Slate', 8)
        for a in range(0, 360, 45):
            t = math.radians(a); box('Heat-sink fin', (math.cos(t) * 262, math.sin(t) * 262, 24 + 120), (18, 70, 200), 'Steel', 1, rotation=t)
        torus('Capacitor ring', (0, 0, 24 + 60), 250, 14, 'Copper'); torus('Capacitor ring', (0, 0, 24 + 170), 250, 14, 'Copper')
        cyl('Mast deck', (0, 0, deck_z - 10), 380, 20, 'Steel', 8); torus('Deck lip', (0, 0, deck_z - 2), 378, 5, 'Yellow')
        box('Power conditioning cabinet', (420, -300, 24 + 95), (220, 180, 190), 'Ceramic', 5); vent_y(420, -392, 24 + 110, 150, 110)
        pipe('Mast feed conduit', [(420, -210, 24 + 150), (260, -150, 24 + 150), (260, -150, 24 + 215)], 10, 'Copper')
        box('Status beacon', (0, 0, deck_z + 6), (24, 24, 8), 'Light', 1)
        beam('Access ladder rail', (-300, 0, 24), (-300, 0, deck_z), 6, mat='Yellow'); beam('Access ladder rail', (-300, 40, 24), (-300, 40, deck_z), 6, mat='Yellow')
        for z in range(50, deck_z - 10, 30): beam('Ladder rung', (-300, 0, 24 + z - 24), (-300, 40, 24 + z - 24), 4, mat='Steel')
        export('SM_TowerLaser' + suffix, 'Octagonal laser mast with heat-sink fins and capacitor rings under a round weapon deck', ['turret' if level == 1 else 'turret' + '_%d' % level])
    elif kind == 'kinetic':
        box('Armoured bunker', (0, 0, 24 + 108), (620, 620, 216), 'Concrete', 40)
        for a in range(0, 360, 90):
            t = math.radians(a); box('Sloped armour plate', (math.cos(t) * 330, math.sin(t) * 330, 24 + 90), (24, 560, 180), 'Slate', 6, rotation=t, tilt=0)
        box('Bunker deck', (0, 0, deck_z - 10), (760, 760, 20), 'Steel', 4)
        for sx in (-1, 1):
            for sy in (-1, 1): box('Deck corner block', (sx * 350, sy * 350, deck_z - 20), (60, 60, 40), 'Slate', 4)
        box('Shell hoist house', (0, 420, 24 + 110), (160, 140, 220), 'Slate', 6); beam('Hoist arm', (0, 420, 24 + 230), (0, 300, deck_z + 30), 12, mat='Yellow')
        box('Ammunition door', (0, -312, 24 + 90), (140, 10, 180), 'Carbon', 3)
        for sx in (-1, 1): box('Door stripe', (sx * 60, -318, 24 + 90), (10, 2, 170), 'Yellow', .5)
        for i, a in enumerate(range(0, 360, 60)):
            t = math.radians(a); cyl('Berm bollard', (math.cos(t) * 480, math.sin(t) * 480, 24 + 40), 14, 80, 'Yellow', 10)
        export('SM_TowerKinetic' + suffix, 'Squat armoured bunker with sloped plates, a shell hoist and a square weapon deck', ['kinetic_tower' if level == 1 else 'kinetic_tower' + '_%d' % level])
    elif kind == 'missile':
        for sx in (-1, 1):
            for sy in (-1, 1): beam('Platform leg', (sx * 300, sy * 300, 24), (sx * 300, sy * 300, deck_z - 20), 40, mat='Steel')
        for sx in (-1, 1): beam('Platform brace', (sx * 300, -300, 24 + 40), (sx * 300, 300, deck_z - 60), 14, mat='Steel'); beam('Platform brace', (-300, sx * 300, 24 + 40), (300, sx * 300, deck_z - 60), 14, mat='Steel')
        box('Launcher platform', (0, 0, deck_z - 10), (760, 760, 20), 'Steel', 4)
        for sx in (-1, 1): box('Platform edge stripe', (sx * 372, 0, deck_z + 1), (16, 760, 2), 'Yellow', 0); box('Platform edge stripe', (0, sx * 372, deck_z + 1), (760, 16, 2), 'Yellow', 0)
        box('Reload magazine container', (0, 0, 24 + 110), (520, 240, 220), 'Slate', 6)
        for k in range(3): box('Magazine door', (-170 + k * 170, -122, 24 + 100), (120, 6, 160), 'Carbon', 2)
        box('Magazine hazard band', (0, -124, 24 + 200), (500, 4, 14), 'Yellow', .5)
        beam('Reload crane mast', (-450, 0, 24), (-450, 0, deck_z + 160), 20, mat='Yellow'); beam('Reload crane jib', (-450, 0, deck_z + 150), (-40, 0, deck_z + 120), 14, mat='Yellow')
        tube('Crane hook cable', (-100, 0, deck_z + 118), (-100, 0, deck_z + 20), 3, 'Steel', sides=6)
        box('Fire-control cabin', (430, 330, 24 + 90), (220, 180, 180), 'Ceramic', 5); window_y(430, 239, 24 + 110, 150, 70)
        export('SM_TowerMissile' + suffix, 'Raised steel launcher platform over a reload magazine, with a reload crane and fire-control cabin', ['missile_tower' if level == 1 else 'missile_tower' + '_%d' % level])
    else:
        cyl('Containment column', (0, 0, 24 + 108), 150, 216, 'Slate', 24)
        for i, z in enumerate((24 + 60, 24 + 125, 24 + 190)): torus('Plasma induction coil', (0, 0, z), 280 - i * 25, 26, 'Copper')
        torus('Coil energiser ring', (0, 0, 24 + 220), 220, 8, 'Light')
        for a in range(0, 360, 60):
            t = math.radians(a); beam('Coil support strut', (math.cos(t) * 330, math.sin(t) * 330, 24), (math.cos(t) * 190, math.sin(t) * 190, deck_z - 20), 18, mat='Steel')
        cyl('Plasma deck', (0, 0, deck_z - 10), 380, 20, 'Steel', 24); torus('Deck rim', (0, 0, deck_z), 378, 5, 'Yellow')
        for a in range(0, 360, 90):
            t = math.radians(a); box('Cooling vane', (math.cos(t) * 460, math.sin(t) * 460, 24 + 100), (40, 140, 200), 'Steel', 2, rotation=t)
        box('Plasma reactor cabinet', (0, 470, 24 + 95), (260, 160, 190), 'Ceramic', 5); vent_y(0, 388, 24 + 110, 180, 110)
        pipe('Coolant loop', [(0, 390, 24 + 60), (0, 300, 24 + 60), (0, 300, 24 + 200), (0, 220, 24 + 200)], 10, 'Copper')
        export('SM_TowerPlasma' + suffix, 'Stacked copper induction coils on a containment column with cooling vanes under a round weapon deck', ['plasma_tower' if level == 1 else 'plasma_tower' + '_%d' % level])


# ------------------------------------------------------------------ DEPOT (plot 2460)
def depot():
    foundation(2460, 2460, 40)
    box('Warehouse hall', (-300, 150, 40 + 290), (1600, 1700, 580), 'Ceramic', 8)
    roof_barrel(-300, 150, 40 + 580, 1620, 1720, 120, 'Slate')
    for x in range(-1000, 401, 200): box('Wall pilaster', (x, 150, 40 + 290), (20, 1730, 560), 'Slate', 2)
    for i, x in enumerate((-800, -300, 200)): shutter_y(x, 150 - 850, 40, 360, 320)
    box('Loading apron', (-300, 150 - 850 - 420, 40 + 6), (1500, 700, 12), 'Carbon', 1)
    for x in (-800, -300, 200):
        for sx in (-1, 1): box('Dock lane line', (x + sx * 190, 150 - 850 - 420, 40 + 13), (8, 640, 1), 'Yellow', 0)
    box('Dispatch office', (660, -600, 40 + 150), (420, 320, 300), 'Ceramic', 6); window_y(660, -762, 40 + 190, 300, 100); door_y(820, -762, 40, 96, 206)
    for i in range(3):
        for k in range(2 if i < 2 else 1): box('Stacked container', (780 + (i % 2) * 0, 100 + i * 270, 40 + 122 + k * 245), (620, 245, 245), ['Blue', 'Yellow', 'Ceramic'][(i + k) % 3], 5)
    beam('Yard gantry column', (560, -60, 40), (560, -60, 40 + 640), 36, mat='Yellow'); beam('Yard gantry column', (1120, -60, 40), (1120, -60, 40 + 640), 36, mat='Yellow')
    beam('Yard gantry column', (560, 860, 40), (560, 860, 40 + 640), 36, mat='Yellow'); beam('Yard gantry column', (1120, 860, 40), (1120, 860, 40 + 640), 36, mat='Yellow')
    beam('Yard gantry rail', (560, -80, 40 + 640), (560, 880, 40 + 640), 30, 40, 'Yellow'); beam('Yard gantry rail', (1120, -80, 40 + 640), (1120, 880, 40 + 640), 30, 40, 'Yellow')
    beam('Yard gantry bridge', (540, 400, 40 + 660), (1140, 400, 40 + 660), 40, 50, 'Yellow'); box('Yard trolley', (840, 400, 40 + 700), (160, 140, 60), 'Slate', 4)
    for y in range(-500, 1001, 300): box('Roof ridge vent', (-300, y, 40 + 580 + 124), (220, 120, 30), 'Steel', 3)
    text('Depot identity', 'DEPOT', (-300, -703, 40 + 520), 70, mat='Carbon')
    dress_hall(-300, 150, 40, 1600, 1700, 580, ladder='-x', lamps='-y', parapet=False)
    dress_hall(660, -600, 40, 420, 320, 300, ladder=None, lamps='-y')
    scatter_props(40, 1230, 1230, seed=79, count=14)
    export('SM_DepotYard', 'Barrel-roofed warehouse with three loading docks and a marked apron, dispatch office and a gantry-served container yard', ['depot'])


# ------------------------------------------------------------------ WORKER FACTORY (processor plot 2060)
def worker_factory():
    foundation(2060, 2060)
    box('Assembly hall', (-120, 200, 48 + 300), (1500, 1300, 600), 'Slate', 8)
    box('Hall roof', (-120, 200, 48 + 606), (1540, 1340, 16), 'Ceramic', 3)
    for y in (-250, 150, 550): box('Roof clerestory', (-120, y, 48 + 660), (1300, 160, 90), 'Glass', 2); box('Clerestory cap', (-120, y, 48 + 712), (1320, 180, 14), 'Steel', 2)
    box('Glazed assembly front', (-120, -454, 48 + 330), (1300, 10, 420), 'Glass', 1)
    for x in range(-700, 461, 130): box('Front mullion', (x, -456, 48 + 330), (10, 14, 430), 'Steel', .5)
    box('Front lintel', (-120, -456, 48 + 560), (1400, 30, 60), 'Ceramic', 3)
    shutter_y(-620, -456, 48, 300, 260); door_y(480, -456, 48, 96, 206)
    cyl('Test track', (-120, -870, 48 + 4), 560, 8, 'Carbon', 48); cyl('Test track infield', (-120, -870, 48 + 5), 440, 8, 'Concrete', 48)
    torus('Track edge line', (-120, -870, 48 + 9), 500, 3, 'Yellow')
    for a in range(0, 360, 45):
        t = math.radians(a)
        if abs(math.sin(t)) > .3: beam('Charging post', (-120 + math.cos(t) * 700, -870 + math.sin(t) * 300, 48), (-120 + math.cos(t) * 700, -870 + math.sin(t) * 300, 48 + 140), 14, mat='Steel')
    for x in (-700, -300, 100, 500): hvac(x, 750, 48 + 600, 180, 240, 110)
    box('Parts receiving dock', (820, 300, 48 + 150), (300, 800, 300), 'Ceramic', 6); shutter_y(820, -100, 48, 220, 220)
    text('Factory identity', 'WORKER ASSEMBLY', (-120, -462, 48 + 590), 44, mat='Carbon')
    dress_hall(-120, 200, 48, 1500, 1300, 600, ladder='-x', lamps=None)
    scatter_props(48, 1030, 1030, seed=83, count=7)
    export('SM_WorkerFactory', 'Dark assembly hall with a glazed front, roof clerestories, a receiving dock and a test track with charging posts', ['worker_factory'])

# ------------------------------------------------------------------ LATTICE HELPERS
def lattice_tower(cx, cy, z0, base, top, height, levels, leg=14, brace=7, mat='Steel'):
    """Four-leg tapered lattice: legs, a horizontal ring per level and X-bracing
    on every face. Returns the top z and the half-width at the top."""
    def half(z): return base / 2 + (top - base) / 2 * (z - z0) / height
    corners = ((1, 1), (-1, 1), (-1, -1), (1, -1))
    for sx, sy in corners:
        beam('Lattice leg', (cx + sx * base / 2, cy + sy * base / 2, z0), (cx + sx * top / 2, cy + sy * top / 2, z0 + height), leg, mat=mat)
    for k in range(levels + 1):
        z = z0 + height * k / levels; h = half(z)
        for i in range(4):
            (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
            beam('Lattice ring', (cx + ax * h, cy + ay * h, z), (cx + bx * h, cy + by * h, z), brace, mat=mat)
        if k < levels:
            z2 = z0 + height * (k + 1) / levels; h2 = half(z2)
            for i in range(4):
                (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 4]
                beam('Lattice brace', (cx + ax * h, cy + ay * h, z), (cx + bx * h2, cy + by * h2, z2), brace * .8, mat=mat)
                beam('Lattice brace', (cx + bx * h, cy + by * h, z), (cx + ax * h2, cy + ay * h2, z2), brace * .8, mat=mat)
    return z0 + height, top / 2


def fence(half_w, half_d, z0, gate_w=320, h=180):
    """Perimeter fence: posts, two rails and a gate gap on the -Y side."""
    def run(a, b):
        n = max(1, int((b - a).length // 250))
        for i in range(n + 1):
            p = a + (b - a) * i / n; beam('Fence post', (p.x, p.y, z0), (p.x, p.y, z0 + h), 6, mat='Slate')
        for z in (z0 + h * .45, z0 + h - 4): beam('Fence rail', (a.x, a.y, z), (b.x, b.y, z), 4, mat='Steel')
    run(Vector((-half_w, half_d, 0)), Vector((half_w, half_d, 0)))
    run(Vector((half_w, half_d, 0)), Vector((half_w, -half_d, 0))); run(Vector((-half_w, -half_d, 0)), Vector((-half_w, half_d, 0)))
    run(Vector((-half_w, -half_d, 0)), Vector((-gate_w / 2, -half_d, 0))); run(Vector((gate_w / 2, -half_d, 0)), Vector((half_w, -half_d, 0)))


# ------------------------------------------------------------------ SENSOR MAST (plot 780)
def sensor_mast():
    """Tapered lattice mast with a top platform, four phased-array faces, a
    rotating search radar and a beacon whip; equipment shelter, UPS cabinet,
    cable tray, floodlight and a perimeter fence on a compact pad."""
    foundation(780, 780, 24)
    z0 = 24; mx, my = 40, 60
    for sx in (-1, 1):
        for sy in (-1, 1): box('Mast footing', (mx + sx * 125, my + sy * 125, z0 + 14), (70, 70, 28), 'Concrete', 6)
    top, half = lattice_tower(mx, my, z0 + 28, 250, 110, 1060, 7)
    box('Mast platform grating', (mx, my, top + 4), (230, 230, 8), 'Steel', 1)
    for sx in (-1, 1):
        box('Platform guard rail', (mx + sx * 113, my, top + 92), (5, 230, 5), 'Yellow', 0); box('Platform guard rail', (mx, my + sx * 113, top + 92), (230, 5, 5), 'Yellow', 0)
        for sy in (-1, 1): beam('Platform rail post', (mx + sx * 113, my + sy * 113, top + 8), (mx + sx * 113, my + sy * 113, top + 94), 5, mat='Steel')
    for i, (dx, dy, rot) in enumerate(((0, -1, 0), (1, 0, math.pi / 2), (0, 1, math.pi), (-1, 0, -math.pi / 2))):
        box('Phased array face', (mx + dx * 82, my + dy * 82, top + 110), (150, 16, 150), 'Slate', 3, rotation=rot)
        box('Array radiating tiles', (mx + dx * 92, my + dy * 92, top + 110), (128, 4, 128), 'Glass', 0, rotation=rot)
    cyl('Radar turntable', (mx, my, top + 205), 40, 22, 'Steel', 24)
    box('Search radar antenna', (mx, my, top + 250), (300, 26, 64), 'Ceramic', 4)
    box('Radar feed horn', (mx, my - 34, top + 236), (40, 30, 24), 'Carbon', 2)
    tube('Beacon whip', (mx + 70, my + 70, top + 8), (mx + 70, my + 70, top + 380), 3, 'Steel', sides=6); box('Obstruction beacon', (mx + 70, my + 70, top + 386), (14, 14, 14), 'Amber', 0)
    for sx in (-1, 1): tube('Whip antenna', (mx + sx * 95, my - 95, top + 8), (mx + sx * 95, my - 95, top + 220), 2, 'Steel', sides=6)
    lx, ly = mx + 125 + 22, my - 20
    for y in (ly - 22, ly + 22): beam('Mast ladder stile', (lx, y, z0 + 28), (lx - 70, y, top), 5, mat='Yellow')
    box('Equipment shelter', (-230, -210, z0 + 125), (260, 210, 250), 'Ceramic', 6)
    box('Shelter roof', (-230, -210, z0 + 254), (276, 226, 10), 'Slate', 2)
    door_y(-290, -210 - 105 - 13, z0, 96, 200); box('Shelter AC unit', (-150, -317, z0 + 165), (90, 24, 70), 'Steel', 3); vent_y(-150, -330, z0 + 165, 70, 50)
    box('UPS battery cabinet', (250, -250, z0 + 85), (120, 90, 170), 'Slate', 4); box('Cabinet status lamp', (250, -296, z0 + 150), (30, 4, 8), 'Light', 0)
    beam('Cable tray', (-100, -210, z0 + 210), (mx - 120, my - 120, z0 + 210), 14, 6, 'Steel')
    tube('Floodlight pole', (300, 280, z0), (300, 280, z0 + 520), 7, 'Steel', sides=8); box('Floodlight head', (300, 270, z0 + 528), (50, 30, 20), 'Light', 0)
    fence(375, 375, z0)
    export('SM_SensorMast', 'Tapered lattice sensor mast with phased-array faces, rotating search radar and beacon whip; equipment shelter, UPS cabinet, cable tray and fenced pad', ['sensor'])


# ------------------------------------------------------------------ EXTRACTION RIG (plot 1380)
def extraction_rig():
    """Drilling derrick on a substructure over the wellhead (the bound
    deposit), top drive and drill string, pipe rack, process tanks, inclined
    conveyor to an ore hopper over a loading bay, operator cabin and power
    skid: reads as extraction for any deposit type."""
    foundation(1380, 1380, 36)
    z0 = 36; wx, wy = 0, 150
    box('Rig substructure', (wx, wy, z0 + 110), (440, 440, 220), 'Slate', 6)
    for sx in (-1, 1): box('Substructure open bay', (wx + sx * 140, wy - 221, z0 + 90), (120, 6, 170), 'Carbon', 1)
    box('Drill floor', (wx, wy, z0 + 226), (460, 460, 12), 'Steel', 2)
    for sx in (-1, 1):
        box('Drill floor rail', (wx + sx * 228, wy, z0 + 320), (5, 460, 5), 'Yellow', 0); box('Drill floor rail', (wx, wy + sx * 228, z0 + 320), (460, 5, 5), 'Yellow', 0)
        for k in (-1, 0, 1):
            beam('Drill floor rail post', (wx + sx * 228, wy + k * 225, z0 + 232), (wx + sx * 228, wy + k * 225, z0 + 322), 5, mat='Steel')
            beam('Drill floor rail post', (wx + k * 225, wy + sx * 228, z0 + 232), (wx + k * 225, wy + sx * 228, z0 + 322), 5, mat='Steel')
    top, half = lattice_tower(wx, wy, z0 + 232, 300, 130, 820, 6, leg=16)
    box('Crown block', (wx, wy, top + 30), (150, 150, 60), 'Steel', 4); cyl('Crown sheave', (wx, wy - 40, top + 40), 28, 14, 'Slate', 20).rotation_euler = (math.pi / 2, 0, 0)
    box('Top drive', (wx, wy, z0 + 640), (70, 90, 140), 'Yellow', 4); tube('Drill line', (wx, wy, z0 + 710), (wx, wy, top), 3, 'Steel', sides=6)
    tube('Drill string', (wx, wy, z0 + 232), (wx, wy, z0 + 570), 9, 'Steel', sides=12)
    cyl('Wellhead', (wx, wy, z0 + 260), 34, 50, 'Copper', 20)
    box('Pipe rack', (-460, 380, z0 + 60), (320, 260, 20), 'Slate', 2)
    for k in range(7): tube('Racked drill pipe', (-610, 270 + k * 36, z0 + 80), (-310, 270 + k * 36, z0 + 80), 8, 'Steel', sides=10)
    for y in (300, 580):
        cyl('Process tank', (470, y, z0 + 140), 110, 280, 'Ceramic', 32); cyl('Tank head', (470, y, z0 + 290), 110, 24, 'Steel', 32, 40)
        torus('Tank ring', (470, y, z0 + 60), 112, 5, 'Slate')
    pipe('Process line', [(wx + 200, wy + 40, z0 + 180), (360, wy + 40, z0 + 180), (360, 300, z0 + 180), (370, 300, z0 + 180)], 7, 'Copper')
    hx, hy = 330, -360
    for sx in (-1, 1):
        for sy in (-1, 1): beam('Hopper leg', (hx + sx * 90, hy + sy * 70, z0), (hx + sx * 90, hy + sy * 70, z0 + 320), 14, mat='Steel')
    box('Ore hopper', (hx, hy, z0 + 400), (220, 180, 160), 'Slate', 4); box('Hopper chute', (hx, hy, z0 + 290), (90, 70, 60), 'Steel', 2)
    box('Loading bay marking', (hx, hy - 60, z0 + 2), (260, 300, 2), 'Carbon', 0)
    for sx in (-1, 1): box('Loading bay stripe', (hx + sx * 125, hy - 60, z0 + 3), (8, 300, 1), 'Yellow', 0)
    beam('Inclined conveyor', (wx + 150, wy - 150, z0 + 250), (hx - 20, hy + 60, z0 + 480), 50, 16, 'Steel')
    beam('Conveyor belt', (wx + 150, wy - 150, z0 + 262), (hx - 20, hy + 60, z0 + 492), 40, 4, 'Carbon')
    for t in (.3, .65):
        x = wx + 150 + (hx - 20 - wx - 150) * t; y = wy - 150 + (hy + 60 - wy + 150) * t; z = z0 + 250 + 230 * t
        beam('Conveyor trestle', (x, y, z0), (x, y, z - 8), 12, mat='Slate')
    box('Operator cabin', (-420, -330, z0 + 130), (300, 220, 260), 'Ceramic', 6); box('Cabin roof', (-420, -330, z0 + 264), (316, 236, 10), 'Slate', 2)
    window_y(-460, -330 - 110 - 7, z0 + 170, 150, 70); door_y(-330, -330 - 110 - 13, z0, 90, 200)
    box('Power skid', (-470, 40, z0 + 80), (240, 150, 160), 'Slate', 4); vent_y(-470, 40 - 80, z0 + 90, 180, 100); fan(-470, 40, z0 + 168, 45)
    box('Skid status lamp', (-400, 40 - 77, z0 + 140), (30, 4, 8), 'Light', 0)
    for x in (-600, 600): tube('Rig floodlight pole', (x, -600, z0), (x, -600, z0 + 560), 7, 'Steel', sides=8); box('Rig floodlight', (x, -590, z0 + 566), (50, 30, 20), 'Light', 0)
    scatter_props(z0, 690, 690, seed=97, count=5)
    export('SM_ExtractionRig', 'Drilling derrick on a substructure over the wellhead with top drive, pipe rack, process tanks, inclined conveyor to an ore hopper over a loading bay, operator cabin and power skid', ['extraction_mine'])


# ------------------------------------------------------------------ ROBOT SERVICE BAY (plot 2460)
def service_bay():
    """Three open charging berths in front of a plant hall (charge electronics,
    control gallery, roof chillers), with a coolant plant and a parts cage on
    the sides. The cantilever canopy covers only the pedestals so the contact
    plates, and the stored worker bodies docked on them at runtime, stay
    visible from the strategy camera; berth positions go to the manifest."""
    foundation(2460, 2460, 40)
    z0 = 40; lanes = (-740, 0, 740)
    hall_y, hall_w, hall_d, hall_h = 700, 2000, 640, 560
    front = hall_y - hall_d / 2
    box('Service plant hall', (0, hall_y, z0 + hall_h / 2), (hall_w, hall_d, hall_h), 'Ceramic', 8)
    box('Plant hall roof membrane', (0, hall_y, z0 + hall_h + 6), (hall_w + 20, hall_d + 20, 12), 'Slate', 2)
    for x in range(-900, 901, 300): box('Hall wall pilaster', (x, hall_y, z0 + hall_h / 2), (18, hall_d + 14, hall_h - 10), 'Slate', 2)
    for x in lanes: window_y(x, front - 7, z0 + 330, 380, 90)
    for x in (-370, 370): door_y(x, front - 13, z0, 110, 215)
    for x in (-600, 0, 600): hvac(x, hall_y + 40, z0 + hall_h + 12, 240, 320, 110)
    dress_hall(0, hall_y, z0, hall_w, hall_d, hall_h, ladder='-x', lamps=None)
    # Canopy over the pedestals only, meeting the hall face below its roofline.
    canopy_front, canopy_z = -260, 520
    depth = front - canopy_front; mid = (front + canopy_front) / 2
    box('Cantilever charging canopy', (0, mid, canopy_z), (2300, depth, 30), 'Slate', 6)
    for x in range(-1100, 1101, 220): box('Canopy standing seam', (x, mid, canopy_z + 17), (6, depth - 20, 6), 'Steel', 0)
    for x in (-1100, -370, 370, 1100):
        beam('Canopy column', (x, canopy_front + 40, z0), (x, canopy_front + 40, canopy_z - 15), 26, mat='Slate')
        box('Column protective sleeve', (x, canopy_front + 40, z0 + 70), (44, 44, 140), 'Yellow', 3)
        beam('Canopy portal girder', (x, canopy_front + 20, canopy_z - 30), (x, front, canopy_z - 30), 22, 34, 'Steel')
    box('Service identity fascia', (0, canopy_front - 13, canopy_z - 20), (2320, 26, 90), 'Slate', 4)
    for x, word in ((-370, 'SERVICE'), (370, 'CHARGE')): text('Service identity', word, (x, canopy_front - 28, canopy_z - 20), 46, mat='Ceramic')
    for i, x in enumerate(lanes):
        box('Bay number panel', (x, canopy_front - 28, canopy_z - 20), (120, 4, 56), 'Carbon', 1)
        text('Bay number', str(i + 1), (x, canopy_front - 31, canopy_z - 20), 42, mat='Yellow')
        for dx in (-82, 82): box('Bay status lamp', (x + dx, canopy_front - 29, canopy_z - 20), (18, 4, 26), 'Light' if dx < 0 else 'Amber', 0)
        box('Berth floodlight', (x, canopy_front + 30, canopy_z - 22), (240, 40, 10), 'Light', 1)
        # One berth: isolated plinth, contact plate, lane marks, pedestal, lead, diagnostic arm.
        box('Isolated charging plinth', (x, -530, z0 + 12), (440, 740, 24), 'Carbon', 6)
        box('Berth charging contact plate', (x, -470, z0 + 30), (180, 220, 12), 'Steel', 3)
        for side in (-1, 1): box('Berth lane stripe', (x + side * 232, -530, z0 + 25), (10, 720, 2), 'Yellow', 0)
        text('Berth floor number', str(i + 1), (x, -760, z0 + 25), 90, rotation=(0, 0, 0), mat='Yellow')
        for side in (-1, 1): cyl('Lane bollard', (x + side * 250, -960, z0 + 50), 14, 100, 'Yellow', 12)
        box('Charging pedestal', (x, -100, z0 + 150), (150, 110, 300), 'Slate', 8)
        box('Charge connector terminal', (x, -158, z0 + 200), (110, 8, 150), 'Carbon', 3)
        box('Terminal illuminated display', (x, -164, z0 + 255), (80, 3, 44), 'Light', 1)
        pipe('Flexible robotic charge lead', [(x + 60, -162, z0 + 190), (x + 110, -230, z0 + 130), (x + 70, -330, z0 + 62), (x + 30, -372, z0 + 56)], 8, 'Carbon')
        beam('Service arm vertical mount', (x - 175, -110, z0), (x - 175, -110, z0 + 300), 28, mat='Steel')
        tube('Manipulator shoulder joint', (x - 195, -110, z0 + 300), (x - 155, -110, z0 + 300), 40, 'Yellow', sides=20)
        beam('Manipulator upper link', (x - 175, -110, z0 + 300), (x - 140, -300, z0 + 380), 30, mat='Yellow')
        tube('Manipulator elbow joint', (x - 160, -300, z0 + 380), (x - 120, -300, z0 + 380), 32, 'Slate', sides=20)
        beam('Manipulator tool link', (x - 140, -300, z0 + 380), (x - 70, -440, z0 + 262), 22, mat='Ceramic')
        box('Robot diagnostic tool head', (x - 66, -450, z0 + 240), (60, 66, 52), 'Steel', 5)
        box('Tool head inspection light', (x - 66, -484, z0 + 240), (36, 3, 12), 'Light', 1)
        berth(x, -470, z0 + 36, 90)  # docked bodies face the pedestal (+Y)
    for sx in (-1, 1): box('Battery buffer cabinet', (sx * 1100, 150, z0 + 110), (160, 300, 220), 'Slate', 5); box('Cabinet status lamp', (sx * 1100, -2, z0 + 190), (40, 4, 10), 'Light', 0)
    # Coolant plant on the +X side (the side the default camera sees), parts cage on -X.
    for y in (560, 900):
        cyl('Coolant buffer tank', (1105, y, z0 + 160), 90, 320, 'Steel', 32); cyl('Coolant tank head', (1105, y, z0 + 335), 90, 30, 'Steel', 32, 30)
        torus('Tank reinforcement ring', (1105, y, z0 + 60), 92, 5, 'Slate'); torus('Tank reinforcement ring', (1105, y, z0 + 260), 92, 5, 'Slate')
    pipe('Coolant supply main', [(1105, 650, z0 + 120), (1030, 650, z0 + 120), (1030, 650, z0 + 400), (1000, 650, z0 + 400)], 9, 'Copper')
    pipe('Coolant return main', [(1105, 810, z0 + 200), (1040, 810, z0 + 200), (1040, 810, z0 + 430), (1000, 810, z0 + 430)], 9, 'Copper')
    box('Chiller skid', (1090, 1140, z0 + 70), (200, 160, 140), 'Ceramic', 5); fan(1090, 1140, z0 + 146, 60)
    box('Parts cage floor', (-1110, 720, z0 + 4), (200, 600, 8), 'Steel', 0)
    for y in (440, 720, 1000):
        for x in (-1205, -1015): beam('Parts cage post', (x, y, z0), (x, y, z0 + 260), 6, mat='Steel')
    for k, y in enumerate(range(470, 921, 150)):
        box('Spare parts crate', (-1110, y, z0 + 50), (150, 110, 90), ('Blue', 'Yellow', 'Ceramic')[k % 3], 3)
        box('Spare parts crate', (-1110, y, z0 + 145), (130, 100, 90), ('Ceramic', 'Blue', 'Yellow')[k % 3], 3)
    scatter_props(z0, 1230, 1230, seed=89, count=6)
    export('SM_ServiceBay', 'Open three-berth worker charging and service bay: exposed contact plates, pedestals with charge leads and diagnostic arms under a cantilever canopy, plant hall with control gallery, coolant plant and parts cage', ['robot_service_bay'])


BUILDERS = {
    'solarArray': lambda: solar_array(1), 'solarArray2': lambda: solar_array(2), 'solarArray3': lambda: solar_array(3),
    'batteryBank': battery_bank, 'tradingPort': lambda: trading_port(1), 'tradingPort2': lambda: trading_port(2), 'tradingPort3': lambda: trading_port(3),
    'refinery': refinery, 'fuelRefinery': fuel_refinery, 'greenhouse': greenhouse, 'works': works, 'chipWorks': chip_works,
    'fusionWorks': fusion_works, 'ammunitionWorks': ammunition_works, 'fuelGenerator': fuel_generator,
    'hangar': lambda: hangar('wheeled'), 'hangarTracked': lambda: hangar('tracked'), 'hangarMech': lambda: hangar('mech'),
    'towerLaser': lambda: tower_base('laser'), 'towerKinetic': lambda: tower_base('kinetic'), 'towerMissile': lambda: tower_base('missile'), 'towerPlasma': lambda: tower_base('plasma'),
    'towerLaser2': lambda: tower_base('laser', 2), 'towerKinetic2': lambda: tower_base('kinetic', 2), 'towerMissile2': lambda: tower_base('missile', 2), 'towerPlasma2': lambda: tower_base('plasma', 2),
    'towerLaser3': lambda: tower_base('laser', 3), 'towerKinetic3': lambda: tower_base('kinetic', 3), 'towerMissile3': lambda: tower_base('missile', 3), 'towerPlasma3': lambda: tower_base('plasma', 3),
    'depotYard': depot, 'workerFactory': worker_factory, 'serviceBay': service_bay,
    'sensorMast': sensor_mast, 'extractionRig': extraction_rig,
}
for kind, build in BUILDERS.items():
    if wanted(kind):
        build()

# Layout the retained review scene on a grid and save it.
for i, (name, o) in enumerate(ASSETS.items()):
    o.location = ((i % 5) * 4200, (i // 5) * 4200, 0)
manifest = {'version': 1, 'units': 'centimeters', 'authored_front': '-Y', 'export_yaw_degrees': EXPORT_YAW_DEGREES, 'unreal_front': '+X',
            'generator': 'Tools/create_building_kit_v092.py',
            'materials': 'IM_* slots from Art/IndustryExports/industry_manifest.json (MI_Industry_* in Unreal)',
            'meshes': records}
if ONLY and (KIT / 'kit_manifest.json').exists():
    previous = json.loads((KIT / 'kit_manifest.json').read_text(encoding='utf-8')); previous['meshes'].update(records); manifest['meshes'] = previous['meshes']
(KIT / 'kit_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
if '--no-blend' not in ARGS:
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE / 'BuildingKitV092.blend'))
print('KIT_READY ' + json.dumps({k: v['triangles'] for k, v in records.items()}), flush=True)

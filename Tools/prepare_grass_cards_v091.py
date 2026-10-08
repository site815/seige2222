"""Render CC0 Grass Medium 02 tufts into an impostor card atlas and build cheap card clumps.

Run with the bundled Blender:
  .tools\\blender-4.5.14-windows-x64\\blender.exe --background --python Tools\\prepare_grass_cards_v091.py

Why: the v0.8/v0.9 meadow sward is a 33,632-triangle Nanite assembly whose masked
(alpha-tested) blades force Nanite's programmable rasterizer. At 3840x1600 that one
subsystem costs more GPU time than the rest of the frame. Manor Lords and most
open-world games instead render grass as a handful of textured cards per clump,
rendered through the ordinary (non-Nanite) path with a dithered distance fade.

This script keeps the project's licensing model: the only inputs are the already
retained CC0 Poly Haven grass_medium_02 geometry and its photographic maps. The
tufts are rendered orthographically (unlit emission, straight alpha) into a 4x2
atlas, the colour is padded into transparent texels so mips do not darken, and two
clump meshes made of bent cards are exported as FBX with vertex colours
(R = normalised height for wind and base shading, G = per-card random phase).
"""
from pathlib import Path
import bpy, json, math, random, hashlib, struct, zlib, sys, time
import numpy as np
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV091'
SOURCES = json.loads((ROOT / 'Art/EnvironmentV04/sources.json').read_text())
for folder in ('Renders', 'Exports', 'Source'):
    (ART / folder).mkdir(parents=True, exist_ok=True)
CELL = 1024
LETTERS = 'bcde'
ANGLES = (0.0, 90.0)
started = time.time()


def log(message):
    print('GRASS_CARDS ' + message, flush=True)


# ---------------------------------------------------------------- source tufts
source = SOURCES['assets']['grass_medium_02']
bpy.ops.wm.open_mainfile(filepath=str(ROOT / source['original_source']['local_path']))
tuft_meshes = {}
for letter in LETTERS:
    ob = bpy.data.objects.get('grass_medium_02_' + letter)
    if not ob or ob.type != 'MESH':
        raise RuntimeError('Missing source tuft grass_medium_02_' + letter)
    mesh = ob.data.copy()
    mesh.transform(ob.matrix_world)
    tuft_meshes[letter] = mesh
for ob in list(bpy.data.objects):
    bpy.data.objects.remove(ob, do_unlink=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

def bake_material(name, color_path, alpha_path):
    """Unlit emission of the photographed colour with the photographed alpha."""
    color_image = bpy.data.images.load(str(color_path), check_existing=True)
    alpha_image = bpy.data.images.load(str(alpha_path), check_existing=True)
    alpha_image.colorspace_settings.name = 'Non-Color'
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    for n in list(nodes):
        nodes.remove(n)
    out = nodes.new('ShaderNodeOutputMaterial')
    emission = nodes.new('ShaderNodeEmission')
    emission.inputs['Strength'].default_value = 1.0
    transparent = nodes.new('ShaderNodeBsdfTransparent')
    mix = nodes.new('ShaderNodeMixShader')
    tex_color = nodes.new('ShaderNodeTexImage'); tex_color.image = color_image
    tex_alpha = nodes.new('ShaderNodeTexImage'); tex_alpha.image = alpha_image
    links.new(tex_color.outputs['Color'], emission.inputs['Color'])
    links.new(tex_alpha.outputs['Color'], mix.inputs['Fac'])
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(emission.outputs[0], mix.inputs[2])
    links.new(mix.outputs[0], out.inputs['Surface'])
    return material


def texture_paths(asset):
    record = SOURCES['assets'][asset]
    padded = ROOT / ('Art/EnvironmentV04/Textures/%s/%s_diff_padded_2k.png' % (asset, asset))
    color = padded if padded.exists() else ROOT / record['maps']['surface']['color']['local_path']
    return color, ROOT / record['maps']['surface']['alpha']['local_path']


material = bake_material('GrassCardBakeMedium', *texture_paths('grass_medium_02'))
turf_material = bake_material('GrassCardBakeBermuda', *texture_paths('grass_bermuda_01'))
# Retained dense Bermuda turf (230 photographed tufts) supplies the full base of
# each card; it was prepared in centimetres for the earlier sward assembly.
with bpy.data.libraries.load(str(ROOT / 'Art/EnvironmentV04/Source/SM_MeadowGrassA.blend'), link=False) as (available, loaded):
    loaded.objects = ['SM_MeadowGrassA']
turf_template = loaded.objects[0]
if not turf_template:
    raise RuntimeError('Prepare the Bermuda turf (Art/EnvironmentV04) first')
turf_mesh = turf_template.data.copy()
turf_mesh.transform(Matrix.Scale(.01, 4))
turf_coords = np.array([v.co[:] for v in turf_mesh.vertices])
tlo, thi = turf_coords.min(axis=0), turf_coords.max(axis=0)
turf_mesh.transform(Matrix.Translation(Vector((-(tlo[0] + thi[0]) / 2, -(tlo[1] + thi[1]) / 2, -tlo[2]))))
turf_mesh.materials.clear(); turf_mesh.materials.append(turf_material)
for poly in turf_mesh.polygons:
    poly.material_index = 0
bpy.data.objects.remove(turf_template, do_unlink=True)

# ---------------------------------------------------------------- render setup
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 24
scene.cycles.use_denoising = False
scene.cycles.transparent_max_bounces = 96
scene.cycles.max_bounces = 0
scene.cycles.diffuse_bounces = 0
scene.cycles.glossy_bounces = 0
scene.cycles.transmission_bounces = 0
scene.cycles.volume_bounces = 0
scene.cycles.filter_width = 1.0
scene.render.film_transparent = True
scene.render.resolution_x = CELL
scene.render.resolution_y = CELL
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '8'
scene.render.image_settings.compression = 15
scene.view_settings.view_transform = 'Standard'
scene.view_settings.look = 'None'
scene.view_settings.exposure = 0
scene.view_settings.gamma = 1
scene.display_settings.display_device = 'sRGB'
world = scene.world or bpy.data.worlds.new('CardWorld')
scene.world = world
world.use_nodes = True
background = world.node_tree.nodes.get('Background')
if background:
    background.inputs['Strength'].default_value = 0
cam_data = bpy.data.cameras.new('CardCamera')
cam_data.type = 'ORTHO'
cam_data.clip_start = 0.01
cam_data.clip_end = 100
camera = bpy.data.objects.new('CardCamera', cam_data)
scene.collection.objects.link(camera)
scene.camera = camera

cells = []
centred_meshes = {}
for letter in LETTERS:
    base = tuft_meshes[letter]
    coords = np.array([v.co[:] for v in base.vertices])
    lo, hi = coords.min(axis=0), coords.max(axis=0)
    # Centre horizontally, root on the ground plane.
    centred = base.copy()
    centred.transform(Matrix.Translation(Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))))
    centred.materials.clear(); centred.materials.append(material)
    for poly in centred.polygons:
        poly.material_index = 0
    centred_meshes[letter] = centred
# Each atlas cell is a small group of overlapping photographed tufts, so one
# card already reads as dense cover; sparse single tufts need far more cards.
CELL_LAYOUTS = 8
TUFTS_PER_CELL = 5
cell_rng = random.Random(9101)
for index in range(CELL_LAYOUTS):
    group = []
    turf = bpy.data.objects.new('turf_%d' % index, turf_mesh.copy())
    scene.collection.objects.link(turf)
    turf.location = (cell_rng.uniform(-.03, .03), cell_rng.uniform(-.03, .03), 0)
    turf.rotation_euler = (0, 0, cell_rng.uniform(0, math.tau))
    turf_size = cell_rng.uniform(.85, 1.0)
    turf.scale = (turf_size, turf_size, turf_size * cell_rng.uniform(1.0, 1.3))
    group.append(turf)
    letters = [cell_rng.choice(LETTERS) for _ in range(TUFTS_PER_CELL)]
    for t, letter in enumerate(letters):
        ob = bpy.data.objects.new('tuft_%d_%d' % (index, t), centred_meshes[letter].copy())
        scene.collection.objects.link(ob)
        spread = .11
        ob.location = ((t - (TUFTS_PER_CELL - 1) / 2) * spread + cell_rng.uniform(-.05, .05), cell_rng.uniform(-.08, .08), 0)
        ob.rotation_euler = (0, 0, cell_rng.uniform(0, math.tau))
        size = cell_rng.uniform(1.1, 1.5)
        ob.scale = (size, size, size * cell_rng.uniform(1.0, 1.3))
        group.append(ob)
    bpy.context.view_layer.update()
    world_coords = np.concatenate([np.array([(ob.matrix_world @ v.co)[:] for v in ob.data.vertices]) for ob in group])
    wlo, whi = world_coords.min(axis=0), world_coords.max(axis=0)
    width = float(whi[0] - wlo[0]); height = float(whi[2] - wlo[2])
    size = max(width, height) * 1.03
    camera.location = ((wlo[0] + whi[0]) / 2, -10, size / 2)
    camera.rotation_euler = (math.radians(90), 0, 0)
    cam_data.ortho_scale = size
    path = ART / 'Renders' / ('cell_%02d.png' % index)
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    cells.append({'index': index, 'tufts': letters, 'turf': 'grass_bermuda_01', 'file': path.name, 'size_m': size,
                  'group_width_m': width, 'group_height_m': height})
    log('rendered cell %d (%s): %.2f m wide, %.2f m tall, cell %.2f m (%.1fs)' % (index, ''.join(letters), width, height, size, time.time() - started))
    for ob in group:
        bpy.data.objects.remove(ob, do_unlink=True)

# ---------------------------------------------------------------- atlas assembly
COLS, ROWS = 4, 2
atlas = np.zeros((ROWS * CELL, COLS * CELL, 4), dtype=np.float32)   # bottom-up rows like Blender
for index, cell in enumerate(cells):
    image = bpy.data.images.load(str(ART / 'Renders' / cell['file']), check_existing=False)
    image.colorspace_settings.name = 'Non-Color'
    pixels = np.empty(CELL * CELL * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(CELL, CELL, 4)
    col, row = index % COLS, index // COLS
    atlas[row * CELL:(row + 1) * CELL, col * CELL:(col + 1) * CELL] = pixels
    cell['atlas_column'] = col; cell['atlas_row'] = row
    # UV rectangle in Blender convention (v = 0 at the bottom).
    cell['uv_min'] = [col / COLS, row / ROWS]
    cell['uv_max'] = [(col + 1) / COLS, (row + 1) / ROWS]
    cell['coverage'] = float((pixels[..., 3] > .5).mean())
    bpy.data.images.remove(image)

# Pad colour into transparent texels (several texels) so bilinear/mip filtering
# never blends toward black at blade edges.
alpha = atlas[..., 3]
rgb = atlas[..., :3]
solid = alpha > .02
rgb[~solid] = 0
for _ in range(10):
    acc = np.zeros_like(rgb); cnt = np.zeros(alpha.shape, dtype=np.float32)
    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)):
        shifted_solid = np.roll(solid, (dy, dx), axis=(0, 1))
        shifted_rgb = np.roll(rgb, (dy, dx), axis=(0, 1))
        acc += shifted_rgb * shifted_solid[..., None]
        cnt += shifted_solid
    fill = (~solid) & (cnt > 0)
    rgb[fill] = acc[fill] / cnt[fill][:, None]
    solid = solid | fill
# Remaining untouched texels get the mean blade colour.
mean_rgb = rgb[alpha > .5].mean(axis=0)
rgb[~solid] = mean_rgb
atlas[..., :3] = rgb


def write_png(path, image):
    """Minimal PNG writer: image is (H, W, 4) float rows top-down."""
    h, w = image.shape[:2]
    data = (np.clip(image, 0, 1) * 255 + .5).astype(np.uint8)
    raw = b''.join(b'\x00' + data[y].tobytes() for y in range(h))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    path.write_bytes(png)


atlas_path = ART / 'Exports' / 'T_GrassCardAtlasV091.png'
write_png(atlas_path, atlas[::-1])
log('atlas written %s (%d x %d, mean blade colour %s)' % (atlas_path.name, COLS * CELL, ROWS * CELL, [round(float(c), 4) for c in mean_rgb]))

# ---------------------------------------------------------------- card clumps
material_cards = bpy.data.materials.new('PH_grass_card_v091')
material_cards.diffuse_color = (.12, .20, .035, 1)


def build_clump(name, seed, target_width_cm, target_height_cm):
    rng = random.Random(seed)
    vertices, faces, uvs, colors = [], [], [], []

    def card(center, yaw, scale, cell, lean):
        size = cell['size_m'] * scale
        direction = Vector((math.cos(yaw), math.sin(yaw), 0))     # card plane direction
        normal = Vector((-direction.y, direction.x, 0))           # bend direction
        phase = rng.random()
        rows = ((0.0, 0.0), (0.5, .35), (1.0, 1.0))                # height fraction, bend fraction
        base = len(vertices)
        u0, v0 = cell['uv_min']; u1, v1 = cell['uv_max']
        for height, bend in rows:
            for side in (-1, 1):
                p = Vector(center) + direction * (side * size * .5) + normal * (lean * size * bend) + Vector((0, 0, size * height))
                vertices.append((p.x, p.y, p.z))
                uvs.append((u0 + (u1 - u0) * (side + 1) / 2, v0 + (v1 - v0) * height))
                colors.append((height, phase, 1.0, 1.0))
        for r in range(2):
            a = base + r * 2
            faces.append((a, a + 1, a + 3, a + 2))

    # Three large cards crossing at the centre, then two rings of satellite
    # cards. Nine textured cards cover roughly 1.2 m so neighbouring clumps
    # overlap at the authored placement density instead of leaving bare ground.
    center_cells = rng.sample(cells, 3)
    for i, cell in enumerate(center_cells):
        card((rng.uniform(-.03, .03), rng.uniform(-.03, .03), -.035), math.radians(i * 60 + rng.uniform(-8, 8)),
             rng.uniform(1.1, 1.3), cell, rng.uniform(.06, .12) * rng.choice((-1, 1)))
    for ring, (count, radius_scale, size_lo, size_hi) in enumerate(((3, .5, .9, 1.05), (3, .85, .75, .9))):
        for i in range(count):
            angle = math.radians(i * 120 + ring * 60 + rng.uniform(-22, 22))
            cell = rng.choice(cells)
            radius = cell['size_m'] * radius_scale
            card((math.cos(angle) * radius, math.sin(angle) * radius, -.035), rng.uniform(0, math.tau),
                 rng.uniform(size_lo, size_hi), cell, rng.uniform(.05, .12) * rng.choice((-1, 1)))
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    uv_layer = mesh.uv_layers.new(name='UV0')
    color_layer = mesh.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='CORNER')
    mesh.color_attributes.active_color_index = 0
    mesh.color_attributes.render_color_index = 0
    for poly in mesh.polygons:
        poly.use_smooth = False
        for loop_index in poly.loop_indices:
            vertex_index = mesh.loops[loop_index].vertex_index
            uv_layer.data[loop_index].uv = uvs[vertex_index]
            color_layer.data[loop_index].color = colors[vertex_index]
    # Scale the authored assembly to the retained sward footprint so placement
    # density, clearances and proxies keep their meaning; convert to centimetres.
    coords = np.array(vertices)
    lo, hi = coords.min(axis=0), coords.max(axis=0)
    # Uniform scale keeps every card's photographic aspect; the width follows.
    s = target_height_cm / (hi[2] - lo[2])
    mesh.transform(Matrix.Diagonal(Vector((s, s, s, 1))) @ Matrix.Translation(Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))))
    mesh.materials.append(material_cards)
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    scene.unit_settings.scale_length = .01
    path = ART / 'Exports' / (name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE',
                             bake_anim=False, add_leaf_bones=False, path_mode='STRIP', use_mesh_modifiers=False)
    scene.unit_settings.scale_length = 1.0
    mesh.calc_loop_triangles()
    coords = np.array([v.co[:] for v in mesh.vertices])
    lo, hi = coords.min(axis=0), coords.max(axis=0)
    record = {'fbx': path.name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'triangles': len(mesh.loop_triangles), 'vertices': len(mesh.vertices), 'cards': len(faces) // 2,
              'dimensions_cm': [float(hi[0] - lo[0]), float(hi[1] - lo[1]), float(hi[2] - lo[2])],
              'materials': [material_cards.name], 'ground_z': float(lo[2])}
    log('clump %s: %d triangles, %s cm' % (name, record['triangles'], [round(d, 1) for d in record['dimensions_cm']]))
    return obj, record


manifest = {'units': 'centimeters', 'license': 'CC0-1.0', 'source_manifest': 'Art/EnvironmentV04/sources.json',
            'description': 'Impostor card clumps rendered from retained CC0 grass_medium_02 tufts; non-Nanite distance-faded meadow cover.',
            'atlas': {'file': atlas_path.name, 'sha256': hashlib.sha256(atlas_path.read_bytes()).hexdigest(),
                      'width': COLS * CELL, 'height': ROWS * CELL, 'columns': COLS, 'rows': ROWS, 'cells': cells,
                      'mean_blade_colour_srgb': [float(c) for c in mean_rgb], 'uv_origin': 'bottom-left (Blender)'},
            'meshes': {}}
kept = []
for name, seed, width, height in (('SM_GrassCardV091A', 91001, 0, 64.0), ('SM_GrassCardV091B', 91002, 0, 58.0)):
    obj, record = build_clump(name, seed, width, height)
    manifest['meshes'][name] = record
    kept.append(obj)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/Grass_Cards.blend'), compress=True)
(ART / 'Exports/grass_cards_manifest.json').write_text(json.dumps(manifest, indent=2))
log('complete in %.1fs' % (time.time() - started))
print('GRASS_CARDS_V091_PREPARED ' + json.dumps({k: v for k, v in manifest.items() if k != 'atlas'}), flush=True)

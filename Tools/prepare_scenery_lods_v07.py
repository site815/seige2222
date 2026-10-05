"""Create original opaque grass and source-derived canopy LODs; preserve near foliage.

Run with Blender --background --python Tools/prepare_scenery_lods_v07.py.
The grass uses curved blades; crowns derive from the licensed source leaf distribution.
"""
from pathlib import Path
import bpy, json, math, random, hashlib
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV07'
for folder in ('Source', 'Exports'):
    (ART / folder).mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
manifest = json.loads((ART / 'proxy_manifest.json').read_text())
manifest['runtime_review_pending'] = True
color_measurements = json.loads((ART / 'source_color_measurements.json').read_text())
def measured(name):
    return color_measurements[name]['linear_albedo_mean']

def save_mesh(name, vertices, faces, colors, target_dimensions, material_name):
    mesh = bpy.data.meshes.new(name + '_Mesh')
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    lo = Vector(tuple(min(v.co[i] for v in mesh.vertices) for i in range(3)))
    hi = Vector(tuple(max(v.co[i] for v in mesh.vertices) for i in range(3)))
    for vertex in mesh.vertices:
        vertex.co = Vector(((vertex.co.x - (lo.x + hi.x) / 2) * target_dimensions[0] / (hi.x - lo.x),
                            (vertex.co.y - (lo.y + hi.y) / 2) * target_dimensions[1] / (hi.y - lo.y),
                            (vertex.co.z - lo.z) * target_dimensions[2] / (hi.z - lo.z)))
    color = mesh.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='CORNER')
    mesh.color_attributes.active_color_index = 0
    mesh.color_attributes.render_color_index = 0
    for polygon in mesh.polygons:
        polygon.use_smooth = True
        for loop in polygon.loop_indices:
            color.data[loop].color = colors[mesh.loops[loop].vertex_index]
    # Source assets use UVs; proxies need only vertex color but provide a valid
    # channel for import diagnostics and future externally authored materials.
    uv = mesh.uv_layers.new(name='UV0')
    for loop in mesh.loops:
        p = mesh.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (p.x / target_dimensions[0] + .5, p.z / target_dimensions[2])
    material = bpy.data.materials.get(material_name) or bpy.data.materials.new(material_name)
    material.diffuse_color = (.12, .20, .035, 1)
    mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = ART / 'Exports' / (name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
                            apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                            axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE',
                            bake_anim=False, add_leaf_bones=False, path_mode='STRIP')
    mesh.calc_loop_triangles()
    manifest['meshes'][name] = {'fbx': path.name, 'triangles': len(mesh.loop_triangles),
        'vertices': len(mesh.vertices), 'dimensions_cm': target_dimensions,
        'material': material_name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'opaque': True, 'ground_z': 0, 'unreal_path': '/Game/Art/NatureV07/' + name,
        'linear_vertex_color_min': [min(c[i] for c in colors) for i in range(3)],
        'linear_vertex_color_max': [max(c[i] for c in colors) for i in range(3)]}
    print('SCENERY_LOD_CREATED ' + json.dumps(manifest['meshes'][name]), flush=True)

# A 1.4m sward made from 768 actual leaves: short dense cover and bent tall
# silhouettes. No filled ground rectangle, opacity mask, dither, or wind WPO.
vertices, faces, colors = [], [], []
rng = random.Random(702222)
def blade(center, height, width, angle, bend, segments, tone):
    base = len(vertices)
    direction = Vector((math.cos(angle), math.sin(angle), 0))
    side = Vector((-direction.y, direction.x, 0))
    albedo = measured('grass_medium_02' if segments > 1 else 'grass_bermuda_01')
    for j in range(segments):
        t = j / segments
        middle = Vector(center) + direction * bend * t * t + Vector((0, 0, height * t))
        radius = width * .5 * (1 - t) ** .65
        for sign in (-1, 1):
            vertices.append(tuple(middle + side * radius * sign))
            shade = tone * (.75 + .35 * t)
            colors.append(tuple(c * shade for c in albedo) + (1,))
    vertices.append(tuple(Vector(center) + direction * bend + Vector((0, 0, height))))
    colors.append(tuple(c * tone * 1.1 for c in albedo) + (1,))
    for j in range(segments - 1):
        a = base + j * 2
        faces.extend(((a, a + 1, a + 2), (a + 1, a + 3, a + 2)))
    a = base + (segments - 1) * 2
    faces.append((a, a + 1, len(vertices) - 1))

for y in range(8):
    for x in range(8):
        center = ((x - 3.5) * 15 + rng.uniform(-5, 5), (y - 3.5) * 15 + rng.uniform(-5, 5), 0)
        for i in range(10):
            blade(center, rng.uniform(9, 18), rng.uniform(1.1, 2.1), rng.random() * math.tau,
                  rng.uniform(5, 14), 1, rng.uniform(.65, 1))
for y in range(4):
    for x in range(4):
        center = ((x - 1.5) * 31 + rng.uniform(-8, 8), (y - 1.5) * 31 + rng.uniform(-8, 8), 1)
        for i in range(8):
            blade(center, rng.uniform(24, 54), rng.uniform(1.4, 2.5), rng.random() * math.tau,
                  rng.uniform(8, 22), 3, rng.uniform(.65, 1))
save_mesh('SM_GrassProxy', vertices, faces, colors, [140, 140, 55], 'M_GrassProxy')

bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/Scenery_LODs.blend'))
(ART / 'proxy_manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
canopy_script = ROOT / 'Tools/prepare_source_canopies_v07.py'
exec(compile(canopy_script.read_text(), str(canopy_script), 'exec'), {'__file__': str(canopy_script)})
print('SCENERY_LODS_COMPLETE', flush=True)

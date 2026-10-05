"""Blender-only, deterministic CC0 distant canopy derivatives; v0.7 untouched.

Replace closed voxel crowns with open, folded foliage sprays distributed at
actual source leaf positions. Source wood/UVs and exact runtime bounds survive.
These are distance representations, not botanical individual leaf models.
"""
from pathlib import Path
import bpy, numpy as np, json, math, random, hashlib
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV08/Canopies'
for folder in ('Source', 'Exports', 'Previews'):
    (ART / folder).mkdir(parents=True, exist_ok=True)
OLD = json.loads((ROOT / 'Art/EnvironmentV07/proxy_manifest.json').read_text(encoding='utf-8'))
PALETTE = json.loads((ROOT / 'Art/EnvironmentV07/proxy_palette.json').read_text(encoding='utf-8'))
REPORT = {'license': 'CC0-1.0 source derivatives; retained original creator attribution',
          'units': 'centimeters', 'runtime_review_pending': True, 'meshes': {}}


def material(name, color):
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    nt = m.node_tree
    shader = nt.nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value = .88
    shader.inputs['Specular IOR Level'].default_value = .12
    attr = nt.nodes.new('ShaderNodeVertexColor')
    attr.layer_name = 'Color'
    nt.links.new(attr.outputs['Color'], shader.inputs['Base Color'])
    return m


def make(name, old_name, source_file, source_name, leaf_match, count, cell, seed, conifer):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT / source_file))
    source = bpy.data.objects[source_name]
    mesh = source.data
    coords = np.empty(len(mesh.vertices)*3, np.float32)
    mesh.vertices.foreach_get('co', coords)
    coords = coords.reshape(-1, 3)
    indices = np.empty(len(mesh.loops), np.int32)
    mesh.loops.foreach_get('vertex_index', indices)
    counts = np.empty(len(mesh.polygons), np.int32)
    slots = np.empty(len(mesh.polygons), np.int32)
    mesh.polygons.foreach_get('loop_total', counts)
    mesh.polygons.foreach_get('material_index', slots)
    leaves = [i for i, m in enumerate(mesh.materials) if leaf_match in m.name]
    used = np.unique(indices[np.repeat(np.isin(slots, leaves), counts)])
    points = coords[used]
    dims = np.array(OLD['meshes'][old_name]['dimensions_cm'])
    # Preserve a uniform 3-D sample of source foliage, not a filled envelope.
    # Each occupied cell contributes at most one location, avoiding polygon-
    # density bias. Cell selection uses a fixed permutation, never camera state.
    cells = np.floor(points / cell).astype(np.int32)
    _, first = np.unique(cells, axis=0, return_index=True)
    rng_np = np.random.default_rng(seed)
    selected = rng_np.permutation(first)[:count]
    centers = points[selected].copy()
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    # Existing low-poly source wood already has the correct dimensions/pivot.
    path = ROOT / 'Art/EnvironmentV07/Source' / (old_name + '.blend')
    with bpy.data.libraries.load(str(path), link=False) as (available, loaded):
        loaded.objects = [old_name]
    previous = loaded.objects[0]
    bpy.context.scene.collection.objects.link(previous)
    previous.hide_set(False)
    previous.hide_render = False
    pm = previous.data
    wood_vertices, wood_faces, wood_uvs, wood_mats = [], [], [], []
    old_to_new = {}
    for face in pm.polygons:
        if pm.materials[face.material_index].name.startswith('M_CanopyProxy'):
            continue
        verts, uvs = [], []
        for li in face.loop_indices:
            idx = pm.loops[li].vertex_index
            if idx not in old_to_new:
                old_to_new[idx] = len(wood_vertices)
                wood_vertices.append(tuple(pm.vertices[idx].co))
            verts.append(old_to_new[idx])
            uvs.append(tuple(pm.uv_layers.active.data[li].uv))
        wood_faces.append(verts)
        wood_uvs.append(uvs)
        wood_mats.append(face.material_index)
    source_mats = list(pm.materials)
    bpy.data.objects.remove(previous, do_unlink=True)
    verts, faces, colors, uvs = [], [], [], []
    randomizer = random.Random(seed)
    leaf_color = PALETTE['conifer' if conifer else 'broadleaf']
    # Three independent folded pointed blades make each porous spray. Open
    # geometry gives real contour gaps and normal variation without masking.
    for center in centers:
        angle = randomizer.uniform(0, math.tau)
        length = randomizer.uniform(38, 65) if conifer else randomizer.uniform(60, 102)
        lift = randomizer.uniform(-.12, .40)
        for blade in range(3):
            az = angle + (blade-1) * (.72 if conifer else 1.35)
            direction = Vector((math.cos(az), math.sin(az), lift + randomizer.uniform(-.2, .2))).normalized()
            side = direction.cross(Vector((0, 0, 1))).normalized()
            normal = side.cross(direction).normalized()
            p = Vector(center) + direction * randomizer.uniform(-length*.24, length*.1)
            width = length * (randomizer.uniform(.12, .22) if conifer else randomizer.uniform(.23, .34))
            # A crease gives separate gently pitched lighting on each side.
            tip = p + direction * length
            mid = p + direction * length*.47
            points_leaf = [p, mid+side*width, tip, mid-side*width,
                           mid+normal*length*randomizer.uniform(.09, .17)]
            offset = len(verts)
            tone = randomizer.uniform(.70, 1.28)
            hue = randomizer.uniform(-.06, .06)
            for j, point in enumerate(points_leaf):
                verts.append(tuple(point))
                edge = .91 if j in (0, 2) else 1
                colors.append((leaf_color[0]*tone*(1+hue)*edge,
                               leaf_color[1]*tone*edge,
                               leaf_color[2]*tone*(1-hue)*edge, 1))
            leaf_uv = [(0,.5),(.47,0),(1,.5),(.47,1),(.47,.5)]
            for a,b,c in ((0,1,4),(1,2,4),(2,3,4),(3,0,4)):
                faces.append((offset+a,offset+b,offset+c))
                uvs.append([leaf_uv[a],leaf_uv[b],leaf_uv[c]])
    foliage_faces = len(faces)
    wood_start = len(verts)
    verts.extend(wood_vertices)
    colors.extend([(.10,.065,.03,0)]*len(wood_vertices))
    faces.extend([[wood_start+i for i in f] for f in wood_faces])
    uvs.extend(wood_uvs)
    result = bpy.data.meshes.new(name+'_Mesh')
    result.from_pydata(verts, [], faces)
    result.update()
    leaf_mat = material('M_ConiferSpraysV08' if conifer else 'M_BroadleafSpraysV08', leaf_color)
    result.materials.append(leaf_mat)
    wood_slot_map = {}
    for old_slot in sorted(set(wood_mats)):
        wood_slot_map[old_slot] = len(result.materials)
        result.materials.append(source_mats[old_slot])
    uv = result.uv_layers.new(name='UV0')
    attr = result.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='CORNER')
    result.color_attributes.active_color_index = result.color_attributes.render_color_index = 0
    # Adding a loop-domain color layer reallocates Blender CustomData. Reacquire
    # the UV handle before writing, otherwise stale RNA points into color memory.
    uv = result.uv_layers['UV0']
    for i, face in enumerate(result.polygons):
        face.material_index = 0 if i < foliage_faces else wood_slot_map[wood_mats[i-foliage_faces]]
        face.use_smooth = i >= foliage_faces  # folded sprays retain their crease
        for j, li in enumerate(face.loop_indices):
            uv.data[li].uv = uvs[i][j]
            attr.data[li].color = colors[result.loops[li].vertex_index]
    actual_colors = np.empty(len(result.loops)*4, np.float32)
    result.color_attributes['Color'].data.foreach_get('color', actual_colors)
    expected_colors = np.array([colors[loop.vertex_index] for loop in result.loops])
    if not np.allclose(actual_colors.reshape(-1,4), expected_colors, atol=1.e-7):
        raise RuntimeError('Foliage color data differs from authored colors: '+name)
    xyz = np.array(verts)
    low, high = xyz.min(axis=0), xyz.max(axis=0)
    xyz = (xyz-low)*dims/(high-low)
    xyz[:,:2] -= dims[:2]/2
    result.vertices.foreach_set('co', xyz.astype(np.float32).ravel())
    result.update()
    obj = bpy.data.objects.new(name, result)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = .01
    export = ART/'Exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(export), use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
        mesh_smooth_type='FACE', bake_anim=False, add_leaf_bones=False, path_mode='STRIP')
    result.calc_loop_triangles()
    record = {'fbx': export.name, 'triangles': len(result.loop_triangles),
              'vertices': len(result.vertices), 'dimensions_cm': dims.tolist(), 'ground_z': 0,
              'material': leaf_mat.name, 'material_slots': [m.name for m in result.materials],
              'sha256': hashlib.sha256(export.read_bytes()).hexdigest(),
              'source_file': source_file, 'source_object': source_name,
              'source_asset': 'fir_tree_01' if conifer else 'jacaranda_tree',
              'license': 'CC0-1.0 derivative', 'sprays': len(centers), 'source_cells': len(first),
              'method': 'source foliage cell sampling; open folded pointed sprays; retained source wood',
              'opaque': True, 'two_sided_foliage': True, 'runtime_review_pending': True,
              'unreal_path': '/Game/Art/NatureV08/'+name,
              'previous_triangles': OLD['meshes'][old_name]['triangles']}
    record['verified_loop_colors'] = len(result.loops)
    if record['triangles'] > record['previous_triangles']*1.03:
        raise RuntimeError('Distant mesh triangle budget exceeded: '+name)
    REPORT['meshes'][name] = record
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    world = bpy.data.worlds.new('CanopyReviewWorld')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (.24,.28,.32,1)
    world.node_tree.nodes['Background'].inputs[1].default_value = .7
    scene.world = world
    bpy.ops.object.light_add(type='SUN', location=(0,0,10000))
    sun = bpy.context.object
    sun.rotation_euler = (.6,-.4,-.55)
    sun.data.energy = 2.7
    sun.data.angle = .12
    bpy.ops.object.camera_add(location=(dims[0]*1.3,-dims[0]*1.8,dims[2]*1.25))
    cam = bpy.context.object
    cam.rotation_euler = (Vector((0,0,dims[2]*.5))-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = max(dims[0],dims[2])*1.27
    cam.data.clip_end = 100000
    scene.camera = cam
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 1000
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.film_transparent = False
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Medium High Contrast'
    scene.render.filepath = str(ART/'Previews'/(name+'.png'))
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source'/(name+'.blend')), compress=True)
    bpy.ops.render.render(write_still=True)
    print('CANOPY_V08_READY '+json.dumps(record), flush=True)


make('SM_BroadleafSpraysV08', 'SM_BroadleafProxy',
     'Art/EnvironmentV07/Source/SM_JacarandaNearV07.blend', 'SM_JacarandaNearV07', '_leaves', 1450, 35, 822221, False)
make('SM_ConiferSpraysV08', 'SM_ConiferProxy',
     'Art/Nature/Source/fir_tree_01.blend', 'SM_FirA', '_twig', 1160, 22, 822222, True)
(ART/'canopy_manifest.json').write_text(json.dumps(REPORT,indent=2)+'\n', encoding='utf-8')
(ART/'canopy_palette.json').write_text(json.dumps({'broadleaf': PALETTE['broadleaf'], 'conifer': PALETTE['conifer']},indent=2)+'\n', encoding='utf-8')

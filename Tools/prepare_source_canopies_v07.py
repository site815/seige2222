"""Derive opaque distant crowns from the actual CC0 source leaf distribution.

Blender-only, deterministic, offline. Keeps the real trunk/branch geometry at a
reduced resolution and existing photographic bark UVs. Does not alter near trees.
"""
from pathlib import Path
import bpy, numpy as np, json, hashlib, math
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT/'Art/EnvironmentV07'
manifest = json.loads((ART/'proxy_manifest.json').read_text())

def make_proxy(name, source_file, source_name, leaf_match, cell_cm, target_canopy, target_wood, photo_family):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/source_file))
    source = bpy.data.objects[source_name]
    mesh = source.data
    coords = np.empty(len(mesh.vertices)*3, np.float32)
    mesh.vertices.foreach_get('co', coords); coords = coords.reshape(-1, 3)
    indices = np.empty(len(mesh.loops), np.int32)
    mesh.loops.foreach_get('vertex_index', indices)
    starts, counts, materials = (np.empty(len(mesh.polygons), np.int32) for _ in range(3))
    for key, values in [('loop_start', starts), ('loop_total', counts), ('material_index', materials)]: mesh.polygons.foreach_get(key, values)
    leaf_slots = [i for i, material in enumerate(mesh.materials) if leaf_match in material.name]
    leaf_polys = np.isin(materials, leaf_slots)
    leaf_vertices = np.unique(indices[np.repeat(leaf_polys, counts)])
    points = coords[leaf_vertices]
    origin = points.min(axis=0)-cell_cm*3
    cells = np.floor((points-origin)/cell_cm).astype(np.int32)
    shape = cells.max(axis=0)+4
    occupied = np.zeros(tuple(shape), dtype=bool)
    occupied[tuple(cells.T)] = True
    # One local dilation joins individual leaf fragments, while retaining the
    # authored crown's large gaps and concave branch distribution.
    expanded = occupied.copy()
    for axis in range(3):
        expanded |= np.roll(occupied, 1, axis)
        expanded |= np.roll(occupied, -1, axis)
    occupied = expanded
    quads = []
    for axis in range(3):
        a, b = (axis+1)%3, (axis+2)%3
        for sign in (-1, 1):
            surface = occupied & ~np.roll(occupied, -sign, axis)
            base = np.argwhere(surface)
            if sign > 0: base[:, axis] += 1
            corners = np.zeros((4, 3), np.int32)
            corners[1, a] = 1; corners[2, a] = 1; corners[2, b] = 1; corners[3, b] = 1
            if sign < 0: corners = corners[::-1]
            quads.append(base[:, None, :]+corners[None, :, :])
    quads = np.concatenate(quads)
    vertices, mapping = np.unique(quads.reshape(-1, 3), axis=0, return_inverse=True)
    canopy_mesh = bpy.data.meshes.new(name+'_CanopyMesh')
    canopy_mesh.from_pydata((vertices*cell_cm+origin).tolist(), [], mapping.reshape(-1, 4).tolist())
    canopy_mesh.update()
    canopy = bpy.data.objects.new(name+'_Canopy', canopy_mesh)
    bpy.context.scene.collection.objects.link(canopy)
    bpy.ops.object.select_all(action='DESELECT'); canopy.select_set(True); bpy.context.view_layer.objects.active = canopy
    smooth = canopy.modifiers.new('SmallScaleLeafSurface', 'SMOOTH'); smooth.factor = .7; smooth.iterations = 2
    bpy.ops.object.modifier_apply(modifier=smooth.name)
    canopy_mesh.calc_loop_triangles()
    reduce = canopy.modifiers.new('DistantLeafSurfaceBudget', 'DECIMATE')
    reduce.ratio = min(1, target_canopy/len(canopy_mesh.loop_triangles))
    bpy.ops.object.modifier_apply(modifier=reduce.name)
    leaf_material = bpy.data.materials.new('M_CanopyProxy')
    leaf_material.diffuse_color = (.075, .135, .030, 1)
    canopy.data.materials.append(leaf_material)
    uv = canopy.data.uv_layers.new(name='UV0')
    dims = np.asarray(manifest['meshes'][name]['dimensions_cm'])
    for loop in canopy.data.loops:
        p = canopy.data.vertices[loop.vertex_index].co
        uv.data[loop.index].uv = (p.x/dims[0]+.5, p.z/dims[2])

    # Keep the actual connected woody structure. Decimation preserves its UVs;
    # the importer reuses the original bark/trunk material instances.
    areas = np.empty(len(mesh.polygons), np.float32)
    mesh.polygons.foreach_get('area', areas)
    trunk_slots = [i for i, material in enumerate(mesh.materials) if '_trunk' in material.name]
    # Sub-centimeter twig surfaces inside the opaque leaf volume only add many
    # disconnected components that a decimator cannot simplify. Retain the
    # full main trunk and the substantial branches; distant twigs are covered
    # by the source-derived canopy surface.
    wood_polys = np.flatnonzero((~leaf_polys) & (np.isin(materials, trunk_slots) | (areas >= 25)))
    selected_loops = np.concatenate([np.arange(starts[i], starts[i]+counts[i]) for i in wood_polys])
    used, local = np.unique(indices[selected_loops], return_inverse=True)
    wood_faces, offset = [], 0
    for i in wood_polys:
        wood_faces.append(local[offset:offset+counts[i]].tolist()); offset += counts[i]
    wood_mesh = bpy.data.meshes.new(name+'_WoodMesh')
    wood_mesh.from_pydata(coords[used].tolist(), [], wood_faces)
    wood_mesh.update()
    for material in mesh.materials: wood_mesh.materials.append(material)
    wood_mesh.polygons.foreach_set('material_index', materials[wood_polys])
    source_uv = np.empty(len(mesh.loops)*2, np.float32)
    mesh.uv_layers.active.data.foreach_get('uv', source_uv)
    wood_uv = wood_mesh.uv_layers.new(name='UV0')
    wood_uv.data.foreach_set('uv', source_uv.reshape(-1, 2)[selected_loops].ravel())
    wood = bpy.data.objects.new(name+'_Wood', wood_mesh)
    bpy.context.scene.collection.objects.link(wood)
    bpy.ops.object.select_all(action='DESELECT'); wood.select_set(True); bpy.context.view_layer.objects.active = wood
    wood_mesh.calc_loop_triangles()
    reduce = wood.modifiers.new('DistantBranchBudget', 'DECIMATE'); reduce.ratio = min(1, target_wood/len(wood_mesh.loop_triangles))
    bpy.ops.object.modifier_apply(modifier=reduce.name)
    canopy.select_set(True); bpy.context.view_layer.objects.active = canopy
    bpy.ops.object.join()
    proxy = canopy; proxy.name = name; result = proxy.data
    material_ids = np.empty(len(result.polygons), np.int32)
    result.polygons.foreach_get('material_index', material_ids)
    used_materials = np.unique(material_ids)
    kept_materials = [result.materials[int(i)] for i in used_materials]
    result.materials.clear()
    for material in kept_materials: result.materials.append(material)
    result.polygons.foreach_set('material_index', np.searchsorted(used_materials, material_ids).astype(np.int32))
    xyz = np.empty(len(result.vertices)*3, np.float32); result.vertices.foreach_get('co', xyz); xyz = xyz.reshape(-1, 3)
    low, high = xyz.min(axis=0), xyz.max(axis=0)
    xyz = (xyz-low)*dims/(high-low); xyz[:, :2] -= dims[:2]/2
    result.vertices.foreach_set('co', xyz.astype(np.float32).ravel()); result.update()
    color = result.color_attributes.new(name='Color', type='FLOAT_COLOR', domain='CORNER')
    result.color_attributes.active_color_index = result.color_attributes.render_color_index = 0
    for polygon in result.polygons:
        polygon.use_smooth = True
        is_leaf = result.materials[polygon.material_index].name.startswith('M_CanopyProxy')
        for li in polygon.loop_indices:
            p = result.vertices[result.loops[li].vertex_index].co
            variation = .9+.1*math.sin(p.x*.017+p.y*.013+p.z*.023)
            rgb = (.075, .135, .030) if is_leaf else (.12, .075, .035)
            color.data[li].color = tuple(c*variation for c in rgb)+(1 if is_leaf else 0,)
    for ob in list(bpy.data.objects):
        if ob != proxy: bpy.data.objects.remove(ob, do_unlink=True)
    for collection in list(proxy.users_collection): collection.objects.unlink(proxy)
    bpy.context.scene.collection.objects.link(proxy)
    proxy.hide_set(False); proxy.hide_render = False; proxy.hide_viewport = False
    bpy.ops.object.select_all(action='DESELECT'); proxy.select_set(True); bpy.context.view_layer.objects.active = proxy
    bpy.context.scene.unit_settings.system = 'METRIC'; bpy.context.scene.unit_settings.scale_length = .01
    export = ART/'Exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(export), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', bake_anim=False, add_leaf_bones=False, path_mode='STRIP')
    result.calc_loop_triangles()
    record = manifest['meshes'][name]
    record.update({'triangles': len(result.loop_triangles), 'vertices': len(result.vertices),
        'sha256': hashlib.sha256(export.read_bytes()).hexdigest(), 'source_asset': photo_family,
        'source_object': source_name, 'source_file': source_file, 'license': 'CC0-1.0 derivative',
        'method': 'leaf-vertex occupancy surface plus reduced original woody structure',
        'leaf_cell_cm': cell_cm, 'occupied_cells': int(occupied.sum()), 'material_slots': [m.name for m in result.materials],
        'source_leaf_vertices': int(len(leaf_vertices)), 'runtime_review_pending': True,
        'small_branch_face_area_cutoff_cm2': 25,
        'linear_vertex_color_min': [.06, .06, .024], 'linear_vertex_color_max': [.12, .135, .035]})
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source'/(name+'.blend')), compress=True)
    # Shape review uses an orthographic Blender Workbench preview, not a game
    # performance/lighting claim. It exposes crown gaps before Unreal import.
    scene = bpy.context.scene; scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 1200; scene.render.resolution_y = 1000; scene.render.resolution_percentage = 100
    scene.display.shading.light = 'STUDIO'; scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_shadows = True; scene.display.shading.show_cavity = True
    scene.display.shading.background_type = 'WORLD'
    if not scene.world: scene.world = bpy.data.worlds.new('ProxyPreviewWorld')
    scene.world.color = (.13, .16, .18)
    camera = bpy.data.objects.new('ProxyShapeReview', bpy.data.cameras.new('ProxyShapeReview'))
    scene.collection.objects.link(camera); camera.location = (dims[0]*1.3, -dims[0]*1.8, dims[2]*1.15)
    camera.rotation_euler = (Vector((0, 0, dims[2]*.52))-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera.data.type = 'ORTHO'; camera.data.ortho_scale = max(dims[0], dims[2])*1.25; camera.data.clip_end = 100000
    scene.camera = camera; scene.render.filepath = str(ART/(name+'_shape.png'))
    bpy.ops.render.render(write_still=True)
    print('SOURCE_CANOPY_READY '+json.dumps(record), flush=True)

make_proxy('SM_BroadleafProxy', 'Art/EnvironmentV07/Source/SM_JacarandaNearV07.blend', 'SM_JacarandaNearV07', '_leaves', 30, 18000, 4000, 'jacaranda_tree')
make_proxy('SM_ConiferProxy', 'Art/Nature/Source/fir_tree_01.blend', 'SM_FirA', '_twig', 24, 14500, 3500, 'fir_tree_01')
manifest['license'] = 'Original grass geometry; source-derived tree geometry and retained photographic materials CC0-1.0'
(ART/'proxy_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')

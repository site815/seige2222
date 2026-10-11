"""Soften lower-neck contours while preserving the restored detailed Rex anatomy.

Run after Tools/build_rex_from_generated.py if regenerating the base source:
    blender -b -P Tools/clean_rex_mesh.py
This edits local neck contours only; mesh topology is preserved. Existing UVs,
photographic coat, facial vertices, skin weights, rig and animations are retained.
The Hunyuan-derived asset license exception remains unresolved; see provenance.
"""
from pathlib import Path
import bpy, bmesh, json, hashlib, math
from mathutils import Quaternion
ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
rig = bpy.data.objects['CompanionDogRig']
dog = bpy.data.objects['SK_CompanionDog']
if not dog.get('rex_bridge_cleanup_v1', False):
    bm = bmesh.new(); bm.from_mesh(dog.data)
    # Keep all closed limb topology. A rejected candidate removed cross-leg
    # polygons but exposed malformed inner paws; that candidate is not shipped.
    removed = 0
    neck = [v for v in bm.verts if 20 < v.co.x < 40 and 43 < v.co.z < 61 and abs(v.co.y) > 8]
    for _ in range(12):
        bmesh.ops.smooth_vert(bm, verts=neck, factor=.45, use_axis_x=True, use_axis_y=True, use_axis_z=True)
    smoothed = len(neck)
    bm.normal_update(); bm.to_mesh(dog.data); bm.free(); dog.data.update()
    dog['rex_bridge_cleanup_v1'] = True
    dog['removed_bridge_triangles'] = removed
    dog['smoothed_neck_vertices'] = smoothed
else:
    removed = int(dog['removed_bridge_triangles'])
    smoothed = int(dog['smoothed_neck_vertices'])
for v in dog.data.vertices:
    assert all(math.isfinite(x) for x in v.co)
    assert len(v.groups) <= 4
    assert abs(sum(g.weight for g in v.groups) - 1) < .001
assert len(rig.data.bones) == 24
assert len(dog.data.uv_layers) >= 2
rig.animation_data.action = None
for bone in rig.pose.bones:
    bone.rotation_quaternion = Quaternion(); bone.location = (0, 0, 0)
scene.frame_set(1)
bpy.ops.object.select_all(action='DESELECT'); dog.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
output = ART / 'Exports/SK_CompanionDog.fbx'
bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, object_types={'MESH','ARMATURE'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
    add_leaf_bones=False, armature_nodetype='NULL', use_armature_deform_only=True, mesh_smooth_type='FACE',
    colors_type='SRGB', bake_anim=False, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True, path_mode='STRIP')
rig.animation_data.action = bpy.data.actions['A_DogIdle']; scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
dog.data.calc_loop_triangles()
manifest_path = ART / 'dog_manifest.json'
m = json.loads(manifest_path.read_text())
m['triangles'] = len(dog.data.loop_triangles); m['vertices'] = len(dog.data.vertices)
m['exports']['SK_CompanionDog.fbx'] = hashlib.sha256(output.read_bytes()).hexdigest()
m['cosmetic_revision'] = 'Detailed revision 7 restored; lower-neck contours softened; original closed leg topology, photo coat, facial anatomy, UVs, rig and clips retained.'
m['cleanup'] = {'generator':'Tools/clean_rex_mesh.py', 'removed_bridge_triangles': removed, 'smoothed_neck_vertices': smoothed, 'facial_vertices_unchanged':True, 'photo_texture_unchanged':True}
manifest_path.write_text(json.dumps(m, indent=2)+'\n')
print('REX_TARGETED_CLEANUP', json.dumps(m['cleanup']), 'triangles', m['triangles'])

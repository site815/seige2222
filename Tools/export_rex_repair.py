"""Promote the reviewed offline Rex repair, export its compatible skeleton, and record hashes.

Run after rebuild_rex_anatomy.py, bake_rex_photos.py, reweight_rex.py and
validate_rex_deformation.py. This does not change gameplay or call a service.
"""
from pathlib import Path
import bpy, json, hashlib, shutil
from mathutils import Quaternion

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
STAGE = ROOT / 'Saved/rex-repair-v011'
bpy.ops.wm.open_mainfile(filepath=str(STAGE / 'Textured-repair.blend'))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
dog = bpy.data.objects['Rex_repaired']
rig = bpy.data.objects['CompanionDogRig']
donor = bpy.data.objects.get('SK_CompanionDog')
if donor:
    bpy.data.objects.remove(donor, do_unlink=True)
dog.name = 'SK_CompanionDog'
for name in ('A_DogIdle', 'A_DogWalk'):
    bpy.data.actions[name].use_fake_user = True
for material in dog.data.materials:
    if material.name == 'DM_Coat_Repaired':
        old = bpy.data.materials.get('DM_Coat')
        if old:
            old.name = 'Legacy donor coat'
        material.name = 'DM_Coat'
for texture in ('T_Dog_Albedo', 'T_Dog_Nose'):
    source = STAGE / (texture + '.png')
    if texture == 'T_Dog_Nose':
        source = ART / 'Source/PhotoPatches/NosePhoto.png'
    destination = ART / 'Textures' / (texture + '.png')
    shutil.copy2(source, destination)
    for material in dog.data.materials:
        for node in material.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image:
                if (texture == 'T_Dog_Albedo' and material.name == 'DM_Coat') or (texture == 'T_Dog_Nose' and material.name == 'DM_Nose'):
                    node.image.filepath = str(destination)
                    node.image.pack()
rest = dog.data.uv_layers['RestHeight']
for i, loop in enumerate(dog.data.loops):
    rest.data[i].uv = (dog.data.vertices[loop.vertex_index].co.z / 120, 0)
dog.data.uv_layers.active = dog.data.uv_layers['UV0']
dog.data.uv_layers['UV0'].active_render = True
if not dog.data.color_attributes.get('CoatColor'):
    color = dog.data.color_attributes.new(name='CoatColor', type='BYTE_COLOR', domain='CORNER')
    for value in color.data:
        value.color = (1, 1, 1, 1)
dog.data.color_attributes.active_color = dog.data.color_attributes['CoatColor']
rig.animation_data.action = None
rig.data.pose_position = 'POSE'
for bone in rig.pose.bones:
    bone.rotation_quaternion = Quaternion()
    bone.location = (0, 0, 0)
    bone.scale = (1, 1, 1)
scene.frame_set(1)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
dog.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
(ART / 'Exports').mkdir(parents=True, exist_ok=True)
output = ART / 'Exports/SK_CompanionDog.fbx'
bpy.ops.export_scene.fbx(filepath=str(output), use_selection=True, object_types={'MESH', 'ARMATURE'},
    apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
    add_leaf_bones=False, armature_nodetype='NULL', use_armature_deform_only=True, mesh_smooth_type='FACE',
    colors_type='SRGB', bake_anim=False, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
    bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True, path_mode='STRIP')
rig.animation_data.action = bpy.data.actions['A_DogIdle']
scene.frame_set(1)
bpy.data.orphans_purge(do_recursive=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
dog.data.calc_loop_triangles()
path = ART / 'dog_manifest.json'
data = json.loads(path.read_text())
data.update(revision=8, vertices=len(dog.data.vertices), triangles=len(dog.data.loop_triangles),
    source='Offline repair of the retained owner-photo Hunyuan-derived base: closed rebuilt limbs/paws/tail, bilateral face reconstruction, multi-photo coat and front-face reprojection, continuous local skin weights. No new hosted generation.',
    cosmetic_revision='Revision 8: repaired anatomy, front likeness, photographic coat and continuous limb/tail attachments.',
    used_materials=['DM_Coat', 'DM_Nose'], unreal_import_pending=True, runtime_review_pending=True)
data['materials'] = {'DM_Coat': {'color': [.55, .33, .14, 1], 'roughness': .8, 'metallic': 0},
                     'DM_Nose': {'color': [.02, .019, .018, 1], 'roughness': .46, 'metallic': 0}}
data['textures']['T_Dog_Albedo'] = '4096 sRGB UV0 bake: ray-transferred face, frontal reference projection, normalized photo fur patches'
data['textures']['T_Dog_Nose'] = '512 sRGB cropped and lighting-normalized owner photograph'
data['uv_channels'] = {'0': 'Photographic coat atlas; local nose photo projection', '1': 'Rest-pose Z/120'}
data['reference_pose_bounds_cm'] = {key: [fn(v.co[i] for v in dog.data.vertices) for i in range(3)] for key, fn in [('min', min), ('max', max)]}
data['exports']['SK_CompanionDog.fbx'] = hashlib.sha256(output.read_bytes()).hexdigest()
data['cleanup'] = {'pipeline': ['Tools/rebuild_rex_anatomy.py', 'Tools/bake_rex_photos.py', 'Tools/reweight_rex.py', 'Tools/export_rex_repair.py'],
    'closed_rebuilt_limbs_and_paws': True, 'closed_tail': True, 'bilateral_face_reconstruction': True,
    'photo_texture_rebaked': True, 'continuous_limb_and_tail_weights': True,
    'limitations': 'Smooth surface fur and simplified joint/feathering geometry remain; this is a prototype likeness correction.'}
data.pop('runtime_review', None)
path.write_text(json.dumps(data, indent=2) + '\n')
shutil.copy2(STAGE / 'deformation_validation.json', ART / 'deformation_validation.json')
for source, name in [('front', 'front'), ('left', 'profile'), ('right', 'right'), ('rear', 'rear'),
                     ('three_quarter', 'three_quarter'), ('face', 'face'), ('walk-4', 'walk_4'), ('walk-7', 'walk_7'), ('walk-10', 'walk_10')]:
    shutil.copy2(STAGE / ('textured-' + source + '.png'), ART / 'Previews' / ('dog_' + name + '.png'))
print('REX_REPAIR_EXPORTED', data['triangles'], 'triangles', data['vertices'], 'vertices', flush=True)

"""Regenerate the retained Rex animation FBX files from the saved source.

Run with Blender. Missing exports are created by default; --replace explicitly
replaces existing exports. The source actions and runtime assets are not edited.
"""
from pathlib import Path
import bpy, json, hashlib, sys

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
exports = ART / 'Exports'
exports.mkdir(parents=True, exist_ok=True)
manifest_path = ART / 'dog_manifest.json'
manifest = json.loads(manifest_path.read_text())
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
rig = bpy.data.objects['CompanionDogRig']
scene = bpy.context.scene
for name, definition in manifest['animations'].items():
    path = exports / (name + '.fbx')
    if path.exists() and '--replace' not in sys.argv:
        continue
    rig.animation_data.action = bpy.data.actions[name]
    scene.render.fps = definition['fps']
    scene.frame_start = 1
    scene.frame_end = 1 + round(definition['duration_seconds'] * definition['fps'])
    scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
        add_leaf_bones=False, armature_nodetype='NULL', use_armature_deform_only=True,
        mesh_smooth_type='FACE', colors_type='SRGB', bake_anim=True,
        bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True, path_mode='STRIP')
    manifest['exports'][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
print('REX_CLIPS_READY', flush=True)

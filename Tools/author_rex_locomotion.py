"""Author walk/run clips on Rex's retained rig without editing the dog model.

Run with the bundled Blender in background. The source mesh, skin weights,
materials and photo textures are preserved. Each in-place gait has a physical
stride for distance-driven playback; planted paws travel backwards at exactly
the matching world speed. License/provenance notes remain in CompanionDog.
"""
from pathlib import Path
import bpy
import hashlib
import json
import math
from mathutils import Quaternion, Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
FPS = 60
GAITS = {
    'A_DogWalk': dict(stride=.70, intervals=30, stance=.58, lift=4.5,
                      crouch=6.8, bounce=.35,
                      phases={'fore_L': 0, 'hind_L': .25, 'fore_R': .5, 'hind_R': .75}),
    'A_DogRun': dict(stride=1.30, intervals=20, stance=.34, lift=9.0,
                     crouch=10.6, bounce=1.0,
                     phases={'fore_L': 0, 'hind_R': 0, 'fore_R': .5, 'hind_L': .5}),
}


def model_digest(dog):
    """Guard the retained likeness, skinning, UVs and material slot identities."""
    digest = hashlib.sha256()
    for vertex in dog.data.vertices:
        digest.update(repr((tuple(vertex.co), [(g.group, g.weight) for g in vertex.groups])).encode())
    for polygon in dog.data.polygons:
        digest.update(repr((tuple(polygon.vertices), polygon.material_index)).encode())
    for uv in dog.data.uv_layers:
        for item in uv.data:
            digest.update(repr(tuple(item.uv)).encode())
    digest.update(repr([material.name for material in dog.data.materials]).encode())
    return digest.hexdigest()


bpy.ops.wm.open_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
bpy.context.preferences.filepaths.save_version = 0
scene = bpy.context.scene
scene.render.fps = FPS
rig = bpy.data.objects['CompanionDogRig']
dog = bpy.data.objects['SK_CompanionDog']
before = model_digest(dog)


def set_pose(name, head, tail=None, pitch=0):
    rest = rig.data.bones[name]
    rotation = rest.matrix_local.to_quaternion()
    if tail is not None:
        rotation = (rest.tail_local-rest.head_local).normalized().rotation_difference((tail-head).normalized()) @ rotation
    if pitch:
        rotation = Quaternion(Vector((0, 1, 0)), pitch) @ rotation
    matrix = rotation.to_matrix().to_4x4()
    matrix.translation = head
    rig.pose.bones[name].matrix = matrix
    bpy.context.view_layer.update()


def world_rotation(name, axis, angle):
    local_axis = rig.data.bones[name].matrix_local.to_3x3().inverted() @ Vector(axis)
    rig.pose.bones[name].rotation_quaternion = Quaternion(local_axis, angle)


def author(name, spec):
    old = bpy.data.actions.get(name)
    if old:
        if rig.animation_data and rig.animation_data.action == old:
            rig.animation_data.action = None
        bpy.data.actions.remove(old)
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data_create()
    rig.animation_data.action = action
    stride = spec['stride'] * 100
    stance = spec['stance']
    step = stride * stance
    checks = []
    first_pose = None
    for frame in range(1, spec['intervals'] + 2):
        scene.frame_set(frame)
        phase = (frame - 1) / spec['intervals']
        for bone in rig.pose.bones:
            bone.rotation_mode = 'QUATERNION'
            bone.rotation_quaternion = Quaternion()
            bone.location = (0, 0, 0)
            bone.scale = (1, 1, 1)
        # Weight remains low through stance, with restrained two-beat motion.
        dz = -spec['crouch'] + spec['bounce'] * math.cos(math.tau * 2 * (phase - .42))
        rig.pose.bones['pelvis'].location = rig.data.bones['pelvis'].matrix_local.to_3x3().inverted() @ Vector((0, 0, dz))
        world_rotation('spine', (0, 1, 0), math.sin(math.tau*2*phase) * .008)
        world_rotation('neck', (0, 1, 0), -math.sin(math.tau*2*phase) * .010)
        for i in range(1, 5):
            world_rotation(f'tail_{i}', (0, 0, 1), math.sin(phase*math.tau-i*.35)*.035)
        for side in ('L', 'R'):
            world_rotation('ear_'+side, (0, 1, 0), math.sin(phase*math.tau*2-.7)*(.015 if name.endswith('Walk') else .03))
        bpy.context.view_layer.update()
        for limb, offset in spec['phases'].items():
            kind, side = limb.split('_')
            u = (phase + offset) % 1
            pitch = 0
            if u < stance:
                x = step * .5 - stride*u
                lift = 0
            else:
                t = (u-stance)/(1-stance)
                # Hermite endpoints match the planted-paw velocity, removing
                # the abrupt stop/reversal at toe-off and touchdown.
                h00, h10 = 2*t**3-3*t*t+1, t**3-2*t*t+t
                h01, h11 = -2*t**3+3*t*t, t**3-t*t
                tangent = -stride*(1-stance)
                x = h00*(-step*.5) + h10*tangent + h01*(step*.5) + h11*tangent
                lift = spec['lift'] * math.sin(math.pi*t)**2
                pitch = math.radians(-8) * math.sin(math.pi*t)**2
            upper, lower, paw = [f'{kind}_{part}_{side}' for part in ('upper', 'lower', 'paw')]
            target = rig.data.bones[paw].head_local + Vector((x, 0, lift))
            hip = rig.pose.bones[upper].head.copy()
            delta = target-hip
            distance = delta.length
            l1, l2 = rig.data.bones[upper].length, rig.data.bones[lower].length
            if not abs(l1-l2) < distance < l1+l2:
                raise RuntimeError(f'{name} target beyond reach: {limb}, frame {frame}: {distance}/{l1+l2}')
            direction = delta/distance
            a = (l1*l1-l2*l2+distance*distance)/(2*distance)
            height = math.sqrt(max(0, l1*l1-a*a))
            perpendicular = Vector((direction.z, 0, -direction.x)).normalized()
            if kind == 'hind':
                perpendicular = -perpendicular
            knee = hip + direction*a + perpendicular*height
            set_pose(upper, hip, knee)
            set_pose(lower, knee, target)
            set_pose(paw, target, pitch=pitch)
            checks.append(dict(frame=frame, limb=limb, stance=u<stance,
                               target_error_cm=(rig.pose.bones[lower].tail-target).length,
                               foot_lift_cm=lift))
        pose = [tuple(bone.matrix[row][col] for row in range(4) for col in range(4)) for bone in rig.pose.bones]
        if first_pose is None:
            first_pose = pose
        for bone in rig.pose.bones:
            bone.keyframe_insert('rotation_quaternion', frame=frame)
            bone.keyframe_insert('location', frame=frame)
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:
                        key.interpolation = 'LINEAR'
    error = max(item['target_error_cm'] for item in checks)
    seam = max(abs(a-b) for one, two in zip(first_pose, pose) for a,b in zip(one,two))
    if error > .005 or seam > .005:
        raise RuntimeError(f'{name}: target/seam error {error}/{seam}')
    duration = spec['intervals']/FPS
    definition = dict(duration_seconds=duration, fps=FPS, loop=True,
                      stride_meters=spec['stride'], reference_speed_m_s=spec['stride']/duration,
                      reference_speed_cm_s=stride/duration, stance_fraction=stance,
                      phase_source='distance_walked_meters / stride_meters',
                      gait='four-beat walk' if name.endswith('Walk') else 'running diagonal trot',
                      max_ik_target_error_cm=error, loop_matrix_error=seam,
                      planted_paw_speed_cm_s=stride/duration,
                      stance_foot_lift_cm=0)
    for key in ('stride_meters', 'duration_seconds', 'reference_speed_m_s', 'max_ik_target_error_cm'):
        action[key] = definition[key]
    scene.frame_start = 1
    scene.frame_end = spec['intervals'] + 1
    scene.frame_set(1)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path = ART / 'Exports' / (name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
        add_leaf_bones=False, armature_nodetype='NULL', use_armature_deform_only=True,
        bake_anim=True, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
        bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True, path_mode='STRIP')
    return definition, hashlib.sha256(path.read_bytes()).hexdigest()


manifest_path = ART / 'dog_manifest.json'
manifest = json.loads(manifest_path.read_text())
for name, spec in GAITS.items():
    definition, digest = author(name, spec)
    manifest['animations'][name] = definition
    manifest['exports'][name+'.fbx'] = digest
assert before == model_digest(dog), 'Animation authoring changed retained model data'
rig.animation_data.action = bpy.data.actions['A_DogIdle']
scene.render.fps = 30
scene.frame_start = 1
scene.frame_end = 91
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
manifest['locomotion_revision'] = dict(revision=1, source='Tools/author_rex_locomotion.py',
    model_data_sha256=before, mesh_materials_uvs_weights_unchanged=True,
    clips=['A_DogWalk', 'A_DogRun'], import_pending=True)
manifest_path.write_text(json.dumps(manifest, indent=2)+'\n')
print('REX_LOCOMOTION_EXPORTED', json.dumps(manifest['animations']), flush=True)

"""Render the saved revision-5 Rex source for review (does not regenerate or export).

    blender -b -P Art/CompanionDog/Source/render_v5.py [-- --quick] [-- --views face,side]
"""
from pathlib import Path
import bpy, sys, math
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root / 'Source/CompanionDog.blend'))
scene = bpy.context.scene; cam = scene.camera; rig = bpy.data.objects['CompanionDogRig']
quick = '--quick' in sys.argv
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 24 if quick else 64
scene.cycles.use_denoising = True
scene.cycles.transparent_max_bounces = 64
scene.render.resolution_x = 900 if quick else 1400
scene.render.resolution_y = 675 if quick else 1050
scene.render.resolution_percentage = 100
views = {
    'dog_three_quarter': ((200, -235, 118), (5, 0, 46), 50),
    'dog_face': ((175, -118, 100), (58, 0, 74), 70),
    'dog_profile': ((6, -285, 80), (-8, 0, 44), 48),
    'dog_front': ((300, -40, 95), (30, 0, 50), 60),
}
wanted = None
for a in sys.argv:
    if a.startswith('--views='):
        wanted = a.split('=', 1)[1].split(',')


def render(name, position, target, lens):
    cam.location = position
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = lens
    scene.render.filepath = str(root / 'Previews' / (name + '.png'))
    bpy.ops.render.render(write_still=True)


rig.animation_data.action = bpy.data.actions['A_DogIdle']; scene.frame_set(1)
for name, (pos, tgt, lens) in views.items():
    if wanted and name.replace('dog_', '') not in wanted:
        continue
    render(name, pos, tgt, lens)
if not wanted or 'walk' in wanted:
    rig.animation_data.action = bpy.data.actions['A_DogWalk']; scene.frame_set(8)
    render('dog_walk_pose', (140, -240, 92), (0, 0, 40), 55)
print('REX_V5_PREVIEWS_READY', flush=True)

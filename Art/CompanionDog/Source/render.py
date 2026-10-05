"""Render actual saved Rex source for geometry review, without re-exporting it."""
from pathlib import Path
import bpy, sys
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root/'Source/CompanionDog.blend'))
scene=bpy.context.scene;cam=scene.camera;rig=bpy.data.objects['CompanionDogRig']
scene.render.resolution_x=1400;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
scene.cycles.samples=32
def render(name,position,target,lens=55):
    cam.location=position;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.lens=lens;scene.render.filepath=str(root/'Previews'/(name+'.png'))
    bpy.ops.render.render(write_still=True)
rig.animation_data.action=bpy.data.actions['A_DogIdle'];scene.frame_set(1)
if '--profile-only' in sys.argv:
    render('dog_profile',(8,-280,83),(-10,0,43),48)
else:
    render('dog_three_quarter',(210,-245,125),(0,0,43))
    render('dog_face',(180,-115,106),(55,0,72),70)
    render('dog_profile',(8,-280,83),(-10,0,43),48)
    rig.animation_data.action=bpy.data.actions['A_DogWalk'];scene.frame_set(8)
    render('dog_walk_pose',(140,-240,92),(0,0,40))
print('REX_SAVED_SOURCE_PREVIEWS_READY',flush=True)

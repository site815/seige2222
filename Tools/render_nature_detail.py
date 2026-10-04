"""Render a closer review view of the prepared local assets, without altering sources."""
from pathlib import Path
import bpy,math,random
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature'
bpy.ops.wm.open_mainfile(filepath=str(ART/'Source/Nature_Review.blend'))
scene=bpy.context.scene
scene.render.filepath=str(ART/'Previews/licensed_woodland.png')
bpy.ops.render.render(write_still=True)
source=next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('SM_FirA'))
rng=random.Random(142)
for i in range(14):
    ob=source.copy();scene.collection.objects.link(ob)
    ob.location=(rng.uniform(-1900,2300),rng.uniform(1200,4000),0)
    ob.rotation_euler.z=rng.random()*math.tau;ob.scale*=rng.uniform(.7,1.08)
scene.camera.location=(820,-900,420)
scene.camera.rotation_euler=(Vector((-170,200,400))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.lens=27
scene.render.resolution_x=1500;scene.render.resolution_y=1000
scene.render.filepath=str(ART/'Previews/woodland_detail.png');scene.cycles.samples=32
bpy.ops.render.render(write_still=True)
scene.camera.location=(330,-650,170)
scene.camera.rotation_euler=(Vector((110,-320,25))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera.data.lens=40
scene.render.filepath=str(ART/'Previews/fern_detail.png')
bpy.ops.render.render(write_still=True)

"""CPU review render of original v0.5 construction assets."""
from pathlib import Path
import bpy,math
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Construction'
bpy.ops.wm.open_mainfile(filepath=str(ART/'Source/Seige_Construction_Assets.blend'))
bpy.data.objects['SM_Shuttle'].location=(-1000,-100,0)
bpy.data.objects['SM_RobotService'].location=(720,200,0)
bpy.ops.mesh.primitive_plane_add(size=200000,location=(0,0,-2));ground=bpy.context.object
mat=bpy.data.materials.new('Review neutral floor');mat.diffuse_color=(.1,.13,.13,1);ground.data.materials.append(mat)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.world.use_nodes=True;scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.65,.78,.85,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.4
bpy.ops.object.light_add(type='AREA',location=(-1800,-2500,3600));light=bpy.context.object;light.data.energy=220000000;light.data.shape='DISK';light.data.size=2000;light.rotation_euler=(Vector((0,0,300))-light.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.object.camera_add(location=(2600,-4100,2400));camera=bpy.context.object;camera.rotation_euler=(Vector((100,0,220))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=3400;camera.data.clip_end=100000;scene.camera=camera
scene.render.resolution_x=1400;scene.render.resolution_y=900;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast';scene.view_settings.exposure=-.7
out=ART/'Previews';out.mkdir(exist_ok=True);scene.render.filepath=str(out/'construction_assets.png');bpy.ops.render.render(write_still=True)

"""Render the actual exported v0.4 canopy and meadow meshes for visual review."""
from pathlib import Path
import bpy,math,random,json
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
DATA=json.loads((ART/'sources.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
objects={}
for name in ('SM_JacarandaA','SM_MeadowGrassA'):
    with bpy.data.libraries.load(str(ART/'Source'/(name+'.blend')),link=True) as (src,dst):dst.objects=[name]
    o=dst.objects[0].copy();scene.collection.objects.link(o);objects[name]=o
tree=objects['SM_JacarandaA'];tree.location=(-1100,1000,0)
rng=random.Random(430)
for i,(x,y,s) in enumerate(((800,1700,.9),(2450,2800,1),(-2850,2450,.88),(-850,3400,.95),(1050,4050,1.05),(-2900,5000,1.1),(3250,5100,.93))):
    o=tree.copy();scene.collection.objects.link(o);o.location=(x,y,0);o.scale=(s,s,s);o.rotation_euler.z=rng.random()*math.tau
grass=objects['SM_MeadowGrassA'];grass.location=(0,-800,0)
for i in range(750):
    x=rng.uniform(-4200,4200);y=rng.uniform(-1800,4900)
    # Meadow close to camera, sparse under closed canopy.
    if y>400 and rng.random()<.72:continue
    o=grass.copy();scene.collection.objects.link(o);o.location=(x,y,0);s=rng.uniform(.8,1.2);o.scale=(s,s,s);o.rotation_euler.z=rng.random()*math.tau

bpy.ops.mesh.primitive_plane_add(size=30000,location=(0,0,-1));ground=bpy.context.object
m=bpy.data.materials.new('Actual rich meadow PBR');m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
coord=n.new('ShaderNodeTexCoord');mapping=n.new('ShaderNodeVectorMath');mapping.operation='SCALE';mapping.inputs[3].default_value=1/140
l.new(coord.outputs['Object'],mapping.inputs[0])
for role in ('color','normal','roughness'):
    image=bpy.data.images.load(str(ROOT/DATA['assets']['Grass004']['maps']['surface'][role]['local_path']))
    if role!='color':image.colorspace_settings.name='Non-Color'
    t=n.new('ShaderNodeTexImage');t.image=image;l.new(mapping.outputs[0],t.inputs[0])
    if role=='color':
        tint=n.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1;tint.inputs[2].default_value=(.55,.66,.52,1)
        l.new(t.outputs['Color'],tint.inputs[1]);l.new(tint.outputs[0],p.inputs['Base Color'])
    elif role=='roughness':l.new(t.outputs['Color'],p.inputs['Roughness'])
    else:
        normal=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs['Normal'])
ground.data.materials.append(m)
world=bpy.data.worlds.new('Soft daylight');world.use_nodes=True;scene.world=world
sky=world.node_tree.nodes.new('ShaderNodeTexSky');sky.sky_type='NISHITA';sky.sun_elevation=.6;sky.sun_rotation=1.9
world.node_tree.links.new(sky.outputs[0],world.node_tree.nodes['Background'].inputs[0]);world.node_tree.nodes['Background'].inputs[1].default_value=.22
bpy.ops.object.light_add(type='SUN');sun=bpy.context.object;sun.rotation_euler=(.45,-.5,-.9);sun.data.energy=2;sun.data.angle=.12
bpy.ops.object.camera_add(location=(4600,-5200,4800));camera=bpy.context.object
camera.rotation_euler=(Vector((0,1700,750))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.lens=45;camera.data.clip_end=100000;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.cycles.transparent_max_bounces=16
try:
    prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='OPTIX';prefs.get_devices()
    for d in prefs.devices:d.use=d.type!='CPU'
    scene.cycles.device='GPU'
except Exception:pass
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-.9
scene.render.resolution_x=1600;scene.render.resolution_y=1150;scene.render.resolution_percentage=100
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source/Environment_Review.blend'),compress=True)
bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source/Environment_Review.blend'),compress=True)
scene.render.filepath=str(ART/'Previews/full_crown_woodland.png');bpy.ops.render.render(write_still=True)
camera.location=(1650,-1050,1900);camera.rotation_euler=(Vector((-500,650,1450))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.lens=48
scene.render.filepath=str(ART/'Previews/crown_detail.png');bpy.ops.render.render(write_still=True)
camera.location=(80,-950,55);camera.rotation_euler=(Vector((0,-670,15))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.lens=38
scene.render.filepath=str(ART/'Previews/meadow_detail.png');bpy.ops.render.render(write_still=True)
print('ENVIRONMENT_V04_RENDER_COMPLETE',flush=True)

"""Inspect new swards on a bumpy physical ground mesh, with sparse wildflowers."""
from pathlib import Path
import bpy,json,math,random
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';DATA=json.loads((ART/'sources.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
objects={}
for file,names in (('Meadow_Swards.blend',['SM_MeadowSwardA','SM_MeadowSwardB']),('Meadow_Wildflowers.blend',['SM_WildflowerPatch'])):
    with bpy.data.libraries.load(str(ART/'Source'/file),link=True) as (src,dst):dst.objects=names
    for ob in dst.objects:objects[ob.name]=ob
def height(x,y):return 26*math.sin(x/380)+18*math.cos(y/410)+7*math.sin(x/65)*math.cos(y/80)+2*math.sin(x/13+y/17)
rng=random.Random(446)
for row in range(30):
    for col in range(28):
        x=(col-13.5)*64+rng.uniform(-20,20);y=(row-14.5)*64+rng.uniform(-20,20)
        # Low grass patches alternate with irregular mixed-height swards.
        scale=rng.uniform(.70,1.22);obj=objects['SM_MeadowSwardA' if rng.random()<.5 else 'SM_MeadowSwardB'].copy();scene.collection.objects.link(obj)
        obj.location=(x,y,height(x,y)-2);obj.rotation_euler.z=rng.random()*math.tau;obj.scale=(scale,scale,scale*rng.uniform(.8,1.1))
        if rng.random()<.035:
            flower=objects['SM_WildflowerPatch'].copy();scene.collection.objects.link(flower);flower.location=(x,y,height(x,y));flower.rotation_euler.z=rng.random()*math.tau
            flower.scale=(.8,.8,rng.uniform(.75,1.15))
vertices=[];faces=[];cells=180;step=20
for j in range(cells+1):
    for i in range(cells+1):
        x=(i-cells/2)*step;y=(j-cells/2)*step;vertices.append((x,y,height(x,y)-3))
        if i<cells and j<cells:
            a=j*(cells+1)+i;faces.append((a,a+1,a+cells+2,a+cells+1))
mesh=bpy.data.meshes.new('Uneven meadow review ground');mesh.from_pydata(vertices,[],faces);mesh.update()
ground=bpy.data.objects.new('Actual bumpy ground, 20 cm spacing',mesh);scene.collection.objects.link(ground)
for p in mesh.polygons:p.use_smooth=True
m=bpy.data.materials.new('Muted ground PBR');m.use_nodes=True;n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
coord=n.new('ShaderNodeTexCoord');mapping=n.new('ShaderNodeVectorMath');mapping.operation='SCALE';mapping.inputs[3].default_value=1/140;l.new(coord.outputs['Object'],mapping.inputs[0])
for role in ('color','normal','roughness'):
    t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(ROOT/DATA['assets']['Grass004']['maps']['surface'][role]['local_path']))
    if role!='color':t.image.colorspace_settings.name='Non-Color'
    l.new(mapping.outputs[0],t.inputs[0])
    if role=='color':
        tint=n.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1;tint.inputs[2].default_value=(.55,.66,.52,1);l.new(t.outputs[0],tint.inputs[1]);l.new(tint.outputs[0],p.inputs['Base Color'])
    elif role=='roughness':l.new(t.outputs[0],p.inputs['Roughness'])
    else:
        normal=n.new('ShaderNodeNormalMap');l.new(t.outputs[0],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs['Normal'])
mesh.materials.append(m)
world=bpy.data.worlds.new('Soft neutral daylight');world.use_nodes=True;scene.world=world
sky=world.node_tree.nodes.new('ShaderNodeTexSky');sky.sky_type='NISHITA';sky.sun_elevation=.9;sky.sun_rotation=1.9
world.node_tree.links.new(sky.outputs[0],world.node_tree.nodes['Background'].inputs[0]);world.node_tree.nodes['Background'].inputs[1].default_value=.22
bpy.ops.object.light_add(type='SUN');sun=bpy.context.object;sun.rotation_euler=(.5,-.65,-.7);sun.data.energy=2;sun.data.angle=.17
bpy.ops.object.camera_add(location=(260,-780,210));cam=bpy.context.object;cam.rotation_euler=(Vector((0,180,45))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=42;cam.data.clip_end=20000;scene.camera=cam
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.cycles.transparent_max_bounces=16
try:
    prefs=bpy.context.preferences.addons['cycles'].preferences;prefs.compute_device_type='OPTIX';prefs.get_devices()
    for d in prefs.devices:d.use=d.type!='CPU'
    scene.cycles.device='GPU'
except Exception:pass
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-.75
scene.render.resolution_x=1600;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
bpy.context.preferences.filepaths.save_version=0;path=ART/'Source/Meadow_Sward_Review.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True);bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
scene.render.filepath=str(ART/'Previews/meadow_swards_detail.png');bpy.ops.render.render(write_still=True)
cam.location=(620,-1050,650);cam.rotation_euler=(Vector((0,50,15))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=40
scene.render.filepath=str(ART/'Previews/meadow_swards_elevated.png');bpy.ops.render.render(write_still=True)
print('MEADOW_SWARDS_RENDERED',flush=True)

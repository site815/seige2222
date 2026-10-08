"""Bake HUD building portraits from the same original meshes used in the world."""
from pathlib import Path
import bpy,math,json
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art';OUT=ART/'OrbitalV09/UI';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
assets={};source_paths={}
sources=[(ART/'Source/Seige_Industry_Architecture.blend',['SM_Factory','SM_Depot','SM_Extractor','SM_Sensor','SM_Turret']),
         (ART/'Construction/Source/Seige_Construction_Assets.blend',['SM_RobotService']),
         (ART/'OrbitalV09/Source/Seige_Orbital_Workforce.blend',['SM_Shuttle','SM_Core','SM_Robot'])]
for path,names in sources:
    for name in names:source_paths[name]=path
    with bpy.data.libraries.load(str(path),link=False) as (src,dst):dst.objects=list(names)
    for o in dst.objects:
        if o is None:raise RuntimeError('Missing source model '+str(path))
        bpy.context.collection.objects.link(o);o.location=(0,0,0);o.hide_render=True;assets[o.name]=o
# The wall is runtime procedural geometry. Reproduce its actual rule dimensions
# and the same cap/posts/inside strip, rather than substituting a flat symbol.
walls=json.loads((ROOT/'Rules/walls.json').read_text())['walls'];parts=[]
def wall_box(p,d,color):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    m=bpy.data.materials.new('Wall');m.diffuse_color=(*color,1);o.data.materials.append(m);parts.append(o)
length=walls['segment_length_meters']*100;width=walls['width_meters']*100;height=walls['height_meters'][0]*100
wall_box((0,0,height*.5),(length,width,height),(.54,.62,.63));wall_box((0,0,height-15),(length+12,width+20,30),(.19,.24,.25))
for x in (-.47,.47):wall_box((length*x,0,height*.5),(30,width+30,height+20),(.32,.37,.38))
wall_box((0,-(width*.5+2),height*.7),(length*.86,4,7),(.25,.6,.55))
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();obj=bpy.context.object;obj.name='SM_Wall';obj.hide_render=True;assets[obj.name]=obj;source_paths[obj.name]=ROOT/'Source/Seige/SeigeWallVisuals.cpp'
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=20;scene.cycles.use_denoising=True
scene.render.film_transparent=True;scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGBA'
scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Orbital portrait neutral studio');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.48,.58,.7,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=.2
bpy.ops.object.light_add(type='SUN',location=(0,0,4000));key=bpy.context.object;key.rotation_euler=(.35,-.4,-.65);key.data.energy=3;key.data.angle=.12
bpy.ops.object.light_add(type='SUN',location=(0,0,3000));fill=bpy.context.object;fill.rotation_euler=(-.5,.65,2.4);fill.data.energy=.8;fill.data.angle=.35
bpy.ops.object.camera_add();camera=bpy.context.object;camera.data.type='ORTHO';camera.data.clip_end=100000;scene.camera=camera
report={}
for name,obj in assets.items():
    obj.hide_render=False
    corners=[obj.matrix_world@Vector(v) for v in obj.bound_box];low=Vector(tuple(min(v[i] for v in corners) for i in range(3)));high=Vector(tuple(max(v[i] for v in corners) for i in range(3)))
    center=(low+high)/2;extent=(high-low).length
    camera.location=center+Vector((1.1,-1.5,1.05))*extent
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.ortho_scale=extent*.91
    name_key=name.removeprefix('SM_');scene.render.filepath=str(OUT/('T_Building_'+name_key+'.png'));bpy.ops.render.render(write_still=True)
    report[name_key]={'source_mesh':name,'source_blend':str(source_paths[name].relative_to(ROOT)).replace('\\','/'),'texture':'/Game/Art/Interface/T_Building_'+name_key,'size':384}
    if name=='SM_Shuttle':
        scene.render.resolution_x=1024;scene.render.resolution_y=1024;scene.cycles.samples=48
        scene.render.filepath=str(OUT/'T_OrbitalHero.png');bpy.ops.render.render(write_still=True)
        scene.render.resolution_x=384;scene.render.resolution_y=384;scene.cycles.samples=20
    obj.hide_render=True
(OUT/'portraits.json').write_text(json.dumps(report,indent=2)+'\n')
print('SEIGE_BUILDING_PORTRAITS_COMPLETE',flush=True)

"""Original shuttle and robot service bay, reusing the project's industrial palette.

Blender batch; centimetres, centered XY, ground Z=0. No external models or images.
"""
from pathlib import Path
import bpy,math,json,hashlib
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art';OUT=ART/'Construction/Exports';SOURCE=ART/'Construction/Source'
OUT.mkdir(parents=True,exist_ok=True);SOURCE.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
industry=json.loads((ART/'IndustryExports/industry_manifest.json').read_text());MATS={};PARTS=[];ASSETS={};records={}
for name,settings in industry['palette'].items():
    material=bpy.data.materials.new(name);material.use_nodes=True;material.diffuse_color=settings['color']
    p=material.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=settings['color'];p.inputs['Metallic'].default_value=settings['metallic'];p.inputs['Roughness'].default_value=settings['roughness']
    p.inputs['Emission Color'].default_value=settings['color'];p.inputs['Emission Strength'].default_value=settings['emission']
    MATS[name.removeprefix('IM_')]=material
def finish(o,name,mat):
    o.name=name;o.data.materials.append(MATS[mat]);PARTS.append(o);return o
def box(name,p,d,mat='Ceramic',edge=3,rotation=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if edge:
        bevel=o.modifiers.new('Manufactured radiused edges','BEVEL');bevel.width=edge;bevel.segments=2;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=bevel.name)
    normal=o.modifiers.new('Weighted panel normals','WEIGHTED_NORMAL');normal.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=normal.name)
    o.rotation_euler.z=rotation;return finish(o,name,mat)
def tube(name,a,b,r,mat='Steel',r2=None,sides=16):
    a,b=Vector(a),Vector(b);bpy.ops.mesh.primitive_cone_add(vertices=sides,radius1=r,radius2=r if r2 is None else r2,depth=(b-a).length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    for face in o.data.polygons:face.use_smooth=len(face.vertices)<=4
    return finish(o,name,mat)
def beam(name,a,b,w,mat='Steel'):
    a,b=Vector(a),Vector(b);o=box(name,(a+b)/2,(w,w,(b-a).length),mat,min(w*.15,3));o.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler();return o
def pipe(name,points,r=5,mat='Carbon'):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=2;curve.bevel_depth=r;curve.bevel_resolution=2
    spline=curve.splines.new('BEZIER');spline.bezier_points.add(len(points)-1)
    for p,co in zip(spline.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(o);bpy.context.view_layer.objects.active=o;o.select_set(True);bpy.ops.object.convert(target='MESH');return finish(bpy.context.object,name,mat)
def vent(x,y,z,w,h):
    box('Recessed ventilation cassette',(x,y,z),(w,12,h),'Carbon',2)
    for i in range(max(3,int(h/13))):box('Louver blade',(x,y-8,z-h/2+8+i*13),(w-12,10,4),'Steel',.5)
def export(name,description):
    bpy.ops.object.select_all(action='DESELECT')
    for o in PARTS:o.select_set(True)
    bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();o=bpy.context.object
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    lo=Vector(tuple(min(v.co[i] for v in o.data.vertices) for i in range(3)));hi=Vector(tuple(max(v.co[i] for v in o.data.vertices) for i in range(3)))
    shift=Vector((-(lo.x+hi.x)/2,-(lo.y+hi.y)/2,-lo.z))
    for v in o.data.vertices:v.co+=shift
    o.name=name;o.data.name=name+'_Mesh';o.data.calc_loop_triangles()
    bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(70),island_margin=.01);bpy.ops.object.mode_set(mode='OBJECT')
    path=OUT/(name+'.fbx');bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False,add_leaf_bones=False,path_mode='STRIP')
    records[name]={'fbx':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'dimensions_cm':list(hi-lo),'triangles':len(o.data.loop_triangles),'materials':[m.name for m in o.data.materials],'description':description,'unreal_path':'/Game/Art/'+name}
    ASSETS[name]=o;PARTS.clear();print('CONSTRUCTION_ASSET '+name+' '+json.dumps(records[name]),flush=True)

# Compact vertical-lift shuttle: landing legs fit the core's raised docking ring.
box('Pressurized cargo keel',(0,20,215),(326,610,205),'Slate',35)
box('Upper ceramic cargo shell',(0,35,295),(312,490,97),'Ceramic',25)
box('Cockpit pressure frame',(0,-260,228),(280,170,155),'Carbon',22)
box('Angled forward glass',(0,-341,238),(231,12,100),'Glass',10)
for side in (-1,1):
    box('Cockpit side glass',(side*142,-272,255),(8,111,72),'Glass',8)
    for y in (-150,0,150):
        box('Cargo shell recessed seam',(side*166,y,228),(6,5,130),'Carbon',.5)
        box('Armored side cassette',(side*169,y+58,228),(9,100,120),'Ceramic',4)
        for z in (181,275):tube('Captive shell fixing',(side*178,y+27,z),(side*183,y+27,z),3,'Steel',sides=8)
    beam('VTOL side mounting spar',(side*138,70,176),(side*245,70,147),30,'Slate')
    for y in (-204,228):
        tube('Ducted lift rotor housing',(side*228,y,105),(side*228,y,205),75,'Slate',sides=32)
        tube('Lift duct lip',(side*228,y,199),(side*228,y,215),79,'Steel',sides=32)
        tube('Lift rotor recess',(side*228,y,208),(side*228,y,217),65,'Carbon',sides=32)
        for angle in range(0,360,60):
            a=math.radians(angle);beam('Static rotor vane',(side*228,y,219),(side*228+math.cos(a)*58,y+math.sin(a)*58,219),6,'Steel')
        beam('Shock absorbing landing strut',(side*184,y,154),(side*220,y,18),14,'Steel')
        box('Docking landing shoe',(side*220,y,9),(71,100,18),'Carbon',6)
        box('Landing shoe traction pad',(side*220,y,1.5),(65,91,3),'Slate',2)
    box('Position light',(side*186,-295,235),(15,23,12),'Light',3)
    vent(side*178,341,215,90,72)
box('Rear boarding ramp',(0,348,165),(235,20,173),'Steel',6)
for z in range(99,234,23):box('Ramp nonslip rung',(0,362,z),(210,7,4),'Carbon',.3)
box('Docking status strip',(0,365,269),(174,5,10),'Light',2)
beam('Upper longitudinal service rail',(-135,-100,353),(-135,244,353),8,'Steel');beam('Upper longitudinal service rail',(135,-100,353),(135,244,353),8,'Steel')
for x in (-90,90):
    tube('Rear maneuvering nozzle',(x,322,222),(x,403,222),37,'Carbon',r2=48,sides=24)
    tube('Nozzle rim',(x,397,222),(x,412,222),49,'Steel',sides=24)
box('Aft stabilizer',(0,246,389),(12,199,118),'Slate',6)
export('SM_Shuttle','Original compact cargo/emergency shuttle with ducted VTOL rotors, pressure cabin, paneled hull, maneuvering nozzles, landing shoes and rear boarding ramp. Parked at the core docking collar.')

# Three physical charge/service berths, visible through an open industrial facade.
box('Reinforced service foundation',(0,0,24),(1600,1250,48),'Concrete',10)
for x in (-530,0,530):box('Foundation expansion joint',(x,0,48),(3,1220,1),'Carbon',0)
box('Rear equipment room',(0,385,277),(1480,420,458),'Slate',9)
for x in (-520,0,520):
    box('Rear ceramic equipment cassette',(x,164,291),(465,14,325),'Ceramic',4);vent(x,154,267,340,185)
    box('Cooling unit',(x,371,538),(344,262,66),'Steel',5)
    tube('Roof cooling fan rim',(x,371,570),(x,371,594),99,'Slate',sides=32)
    tube('Roof cooling fan recess',(x,371,590),(x,371,596),82,'Carbon',sides=32)
    for angle in range(0,360,45):
        a=math.radians(angle);beam('Cooling fan guard',(x,371,602),(x+math.cos(a)*85,371+math.sin(a)*85,602),4,'Steel')
for x in (-710,-235,235,710):
    beam('Front service canopy column',(x,-486,49),(x,-486,500),24,'Slate')
    beam('Roof portal truss',(x,-486,486),(x,170,535),19,'Steel')
    box('Column protective yellow sleeve',(x,-486,128),(38,38,140),'Yellow',3)
box('Cantilever service canopy',(0,-157,542),(1510,770,36),'Ceramic',8)
for x in range(-680,681,170):box('Canopy standing seam',(x,-157,563),(5,748,6),'Steel',1)
box('Service identity fascia',(0,-552,507),(1525,36,75),'Slate',5)
for x in (-468,0,468):
    box('Recessed bay number panel',(x,-574,510),(122,3,33),'Carbon',1)
    for dx in (-22,0,22):box('Bay status charge lamps',(x+dx,-578,511),(11,3,19),'Light',1)
    box('Isolated charging plinth',(x,-238,62),(342,426,28),'Carbon',6)
    box('Berth charging contact plate',(x,-225,81),(156,186,12),'Steel',4)
    for side in (-1,1):box('Berth lane stripe',(x+side*147,-242,80),(7,398,3),'Yellow',.5)
    box('Charging pedestal',(x,14,219),(107,78,323),'Slate',8)
    box('Charge connector terminal',(x,-30,282),(84,7,146),'Carbon',3)
    box('Terminal illuminated display',(x,-36,316),(62,3,40),'Light',1)
    pipe('Flexible robotic charge lead',[(x+48,-29,222),(x+101,-94,174),(x+62,-195,105),(x+28,-208,110)],7,'Carbon')
    # Physical articulated service arms. Runtime aggregate robots sit in these bays.
    beam('Service arm vertical mount',(x-122,80,64),(x-122,80,308),28,'Steel')
    tube('Manipulator shoulder joint',(x-142,80,308),(x-102,80,308),40,'Yellow',sides=20)
    beam('Manipulator upper link',(x-122,80,308),(x-93,-76,388),31,'Yellow')
    tube('Manipulator elbow joint',(x-113,-76,388),(x-73,-76,388),33,'Slate',sides=20)
    beam('Manipulator tool link',(x-93,-76,388),(x-29,-178,267),23,'Ceramic')
    box('Robot diagnostic tool head',(x-25,-185,246),(66,71,55),'Steel',5)
    box('Toolhead inspection light',(x-25,-222,246),(38,3,13),'Light',1)
for side in (-1,1):
    pipe('Exposed coolant return',[(side*690,110,117),(side*690,135,450),(side*550,135,492)],14,'Copper')
    box('Side maintenance access',(side*747,351,267),(12,260,362),'Ceramic',5)
    beam('Access ladder',(side*752,472,74),(side*752,472,484),8,'Steel')
for x in (-600,0,600):box('Canopy work lighting',(x,-150,520),(170,170,7),'Light',2)
export('SM_RobotService','Original open three-berth robot charging and maintenance facility with charging contacts, flexible leads, articulated diagnostic arms, cooling plant and industrial canopy.')

for o in ASSETS.values():o.hide_render=False;o.location=(0,0,0)
path=SOURCE/'Seige_Construction_Assets.blend';bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
manifest={'units':'centimeters','license':'Original project work','generator':'Tools/create_construction_assets.py','materials':'Existing original Industry palette and PBR surfaces','meshes':records}
(OUT/'construction_manifest.json').write_text(json.dumps(manifest,indent=2))
print('CONSTRUCTION_ASSETS_COMPLETE',flush=True)

"""Original small wildflower accents: curved stems, leaves, and petal geometry.

Project-authored botanical forms; no reference game's assets or character art.
Muted cream, dusky violet, and occasional yellow flowers, 25-60 cm tall.
"""
from pathlib import Path
import bpy,json,random,math,hashlib
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
MANIFEST=json.loads((ART/'Exports/environment_manifest.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True);rng=random.Random(44103)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
palette={'Stem':(.08,.13,.032,1),'Leaf':(.095,.19,.045,1),'Cream':(.68,.67,.56,1),'Violet':(.24,.13,.33,1),'Gold':(.48,.31,.048,1)}
materials=[]
for label,color in palette.items():
    name='Original_Meadow_'+label;m=bpy.data.materials.new(name);m.use_nodes=True;p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=color;p.inputs['Roughness'].default_value=.85;p.inputs['Specular IOR Level'].default_value=.15
    materials.append(m);MANIFEST['materials'][name]={'original_tint':list(color),'foliage':True,'source':'Original project geometry and material'}
verts=[];faces=[];slots=[]
def surface(points,quads,slot):
    base=len(verts);verts.extend(points);faces.extend(tuple(base+i for i in face) for face in quads);slots.extend([slot]*len(quads))
def tube(points,radius,slot=0):
    pp=[];qq=[];sides=5
    for i,p in enumerate(points):
        tangent=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized();axis=tangent.cross(Vector((1,0,0))).normalized();other=tangent.cross(axis)
        for s in range(sides):pp.append(p+(axis*math.cos(s*math.tau/sides)+other*math.sin(s*math.tau/sides))*radius*(1-i/(len(points)*2)))
        if i:
            for s in range(sides):qq.append(((i-1)*sides+s,(i-1)*sides+(s+1)%sides,i*sides+(s+1)%sides,i*sides+s))
    surface(pp,qq,slot)
def leaf(base,angle,length,width):
    direction=Vector((math.cos(angle),math.sin(angle),.2));side=Vector((-math.sin(angle),math.cos(angle),0));pp=[]
    for i in range(5):
        t=i/4;c=base+direction*length*t+Vector((0,0,math.sin(t*math.pi)*length*.12));w=width*math.sin(t*math.pi)
        pp.extend((c-side*w,c,c+side*w))
    qq=[]
    for i in range(4):qq.extend(((i*3,(i+1)*3,(i+1)*3+1,i*3+1),(i*3+1,(i+1)*3+1,(i+1)*3+2,i*3+2)))
    surface(pp,qq,1)
def blossom(center,slot,size,petals):
    angle0=rng.random()*math.tau
    for petal in range(petals):
        a=angle0+petal*math.tau/petals;d=Vector((math.cos(a),math.sin(a),0));side=Vector((-math.sin(a),math.cos(a),0));pp=[]
        for j in range(4):
            t=j/3;rad=size*(.18+.82*t);c=center+d*rad+Vector((0,0,size*(.10-.22*t+.12*math.sin(t*math.pi))))
            width=size*(.14 if petals>8 else .32)*math.sin((.1+.85*t)*math.pi);pp.extend((c-side*width,c+side*width))
        surface(pp,[(j*2,j*2+1,j*2+3,j*2+2) for j in range(3)],slot)
    # Small faceted pollen disk, dense enough for close view without spheres.
    pp=[center+Vector((0,0,size*.10))]+[center+Vector((math.cos(i*math.tau/10)*size*.23,math.sin(i*math.tau/10)*size*.23,0)) for i in range(10)]
    surface(pp,[(0,i+1,(i+1)%10+1) for i in range(10)],4)
for plant in range(12):
    a=rng.random()*math.tau;r=rng.uniform(5,55);base=Vector((math.cos(a)*r,math.sin(a)*r,0));height=rng.uniform(28,58)
    bend=Vector((rng.uniform(-5,5),rng.uniform(-5,5),0));points=[base+bend*(i/6)**2+Vector((0,0,height*i/6)) for i in range(7)]
    tube(points,.12)
    for j in range(4):leaf(points[j+1],a+j*2.1,rng.uniform(4,8),rng.uniform(.35,.75))
    slot=2 if plant%3==0 else 4 if plant==11 else 3
    blossom(points[-1],slot,rng.uniform(1.5,2.4) if slot==2 else rng.uniform(1.1,1.65),13 if slot==2 else 5)
    if slot==3:
        for j in range(2):
            start=points[4+j];aa=a+j*2.5;end=start+Vector((math.cos(aa)*rng.uniform(4,7),math.sin(aa)*rng.uniform(4,7),rng.uniform(3,6)))
            tube([start,(start+end)/2+Vector((0,0,1)),end],.075);blossom(end,3,rng.uniform(1.1,1.5),5)
mesh=bpy.data.meshes.new('SM_WildflowerPatch_Mesh');mesh.from_pydata(verts,[],faces);mesh.update()
for m in materials:mesh.materials.append(m)
for p,slot in zip(mesh.polygons,slots):p.material_index=slot;p.use_smooth=True
mesh.uv_layers.new(name='UV0')
ob=bpy.data.objects.new('SM_WildflowerPatch',mesh);scene.collection.objects.link(ob);bpy.context.view_layer.objects.active=ob;ob.select_set(True)
low=[min(v.co[i] for v in mesh.vertices) for i in range(3)];high=[max(v.co[i] for v in mesh.vertices) for i in range(3)]
from mathutils import Matrix
mesh.transform(Matrix.Translation(Vector((-(low[0]+high[0])/2,-(low[1]+high[1])/2,-low[2]))));mesh.calc_loop_triangles()
file=ART/'Exports/SM_WildflowerPatch.fbx'
bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False,add_leaf_bones=False,path_mode='STRIP')
record={'source_asset':'original_meadow_wildflowers','source_object':'Seeded original curved stems, leaf surfaces and petal rosettes','license':'Original project work; project licensing applies',
    'fbx':file.name,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'materials':[m.name for m in materials],'dimensions_cm':[high[i]-low[i] for i in range(3)],
    'triangles':len(mesh.loop_triangles),'vertices':len(mesh.vertices),'pivot':'horizontal bounds center, ground z=0','nanite':True,'shape_preservation':'PRESERVE_AREA','unreal_path':'/Game/Art/NatureV04/SM_WildflowerPatch'}
MANIFEST['license']='CC0 natural sources and original project wildflowers; see individual records'
for r in MANIFEST['meshes'].values():r.setdefault('license','CC0-1.0')
MANIFEST['meshes'][ob.name]=record
(ART/'Exports/environment_manifest.json').write_text(json.dumps(MANIFEST,indent=2))
bpy.context.preferences.filepaths.save_version=0;bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source/Meadow_Wildflowers.blend'),compress=True)
print('ORIGINAL_WILDFLOWERS_PREPARED '+json.dumps(record),flush=True)

"""Build dense irregular swards from complete CC0 Grass Medium 02 tufts.

The new meshes retain curved source leaves and photographed alpha/UV maps.
No existing tree or grass mesh is changed. Units are centimetres for Unreal.
"""
from pathlib import Path
import bpy,json,math,random,hashlib
import numpy as np
from mathutils import Matrix,Vector
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
DATA=json.loads((ART/'sources.json').read_text());MANIFEST=json.loads((ART/'Exports/environment_manifest.json').read_text())
source=DATA['assets']['grass_medium_02'];bpy.ops.wm.open_mainfile(filepath=str(ROOT/source['original_source']['local_path']))
originals=[bpy.data.objects['grass_medium_02_'+letter] for letter in 'bcde']
# Prepared low grass contains 230 complete photographed Bermuda tufts. Four
# copies supply a real horizontal underlayer without extending the tall patch's
# existing bounds. It remains three-dimensional source foliage, not a billboard.
with bpy.data.libraries.load(str(ART/'Source/SM_MeadowGrassA.blend'),link=False) as (available,loaded):loaded.objects=['SM_MeadowGrassA']
low_template=loaded.objects[0]
if not low_template:raise RuntimeError('Prepare the source Bermuda grass first')
m=bpy.data.materials.new('PH_grass_medium_02_surface');m.use_nodes=True
n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF');p.inputs['Roughness'].default_value=.85;p.inputs['Specular IOR Level'].default_value=.15
for role,record in source['maps']['surface'].items():
    if role not in ('color','normal','roughness','alpha'):continue
    color_path=ART/'Textures/grass_medium_02/grass_medium_02_diff_padded_2k.png'
    t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(color_path if role=='color' and color_path.exists() else ROOT/record['local_path']),check_existing=True)
    if role!='color':t.image.colorspace_settings.name='Non-Color'
    if role=='color':
        tint=n.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1;tint.inputs[2].default_value=(.82,1,.70,1)
        l.new(t.outputs['Color'],tint.inputs[1]);l.new(tint.outputs[0],p.inputs['Base Color'])
    elif role=='normal':
        normal=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs['Normal'])
    elif role=='roughness':l.new(t.outputs['Color'],p.inputs['Roughness'])
    elif role=='alpha':alpha_texture=t
# Thin leaf transmission is approximated with a modest translucent lobe in the
# Blender review; Unreal uses its dedicated two-sided foliage shading model.
translucent=n.new('ShaderNodeBsdfTranslucent');l.new(tint.outputs[0],translucent.inputs['Color'])
mixed=n.new('ShaderNodeMixShader');mixed.inputs[0].default_value=.18;l.new(p.outputs[0],mixed.inputs[1]);l.new(translucent.outputs[0],mixed.inputs[2])
transparent=n.new('ShaderNodeBsdfTransparent');masked=n.new('ShaderNodeMixShader');l.new(alpha_texture.outputs['Color'],masked.inputs[0]);l.new(transparent.outputs[0],masked.inputs[1]);l.new(mixed.outputs[0],masked.inputs[2])
l.new(masked.outputs[0],n.get('Material Output').inputs['Surface'])
MANIFEST['materials'][m.name]={'source_asset':'grass_medium_02','texture_group':'surface','foliage':True}
retained=[]
for variant,seed in (('A',4407),('B',4419)):
    rng=random.Random(seed);copies=[]
    for i in range(16):
        src=originals[(i+seed)%len(originals)];ob=bpy.data.objects.new('source_tuft',src.data.copy());bpy.context.scene.collection.objects.link(ob)
        coords=np.array([v.co[:] for v in ob.data.vertices]);lo=coords.min(axis=0);hi=coords.max(axis=0)
        ob.data.transform(Matrix.Translation(Vector((float(-(lo[0]+hi[0])/2),float(-(lo[1]+hi[1])/2),float(-lo[2])))))
        x=(i%4-1.5)*.29+rng.uniform(-.08,.08);y=(i//4-1.5)*.29+rng.uniform(-.08,.08)
        ob.location=(x,y,rng.uniform(-.008,.005));ob.rotation_euler.z=rng.random()*math.tau
        size=rng.uniform(.92,1.35);ob.scale=(size,size,size*rng.uniform(1,1.25))
        ob.data.materials.clear();ob.data.materials.append(m)
        for poly in ob.data.polygons:poly.material_index=0
        for color in list(ob.data.color_attributes):ob.data.color_attributes.remove(color)
        if not ob.data.uv_layers:raise RuntimeError('Source tuft has no photographic UV layer')
        ob.data.uv_layers.active.name='UV0';copies.append(ob)
    for i,(x,y) in enumerate(((-.3,-.3),(.3,-.3),(-.3,.3),(.3,.3))):
        low=bpy.data.objects.new('low_meadow_turf',low_template.data.copy());bpy.context.scene.collection.objects.link(low)
        low.data.transform(Matrix.Scale(.01,4)) # Prepared centimetres back to assembly metres.
        low.location=(x,y,0);low.rotation_euler.z=((i+seed)%4)*math.pi/2;low.scale=(.9,.9,.6)
        copies.append(low)
    bpy.ops.object.select_all(action='DESELECT')
    for ob in copies:ob.select_set(True)
    bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();ob=bpy.context.object;mesh=ob.data
    mesh.transform(ob.matrix_world);ob.matrix_world=Matrix.Identity(4)
    coords=np.array([v.co[:] for v in mesh.vertices]);lo=coords.min(axis=0);hi=coords.max(axis=0)
    mesh.transform(Matrix.Scale(100,4)@Matrix.Translation(Vector((float(-(lo[0]+hi[0])/2),float(-(lo[1]+hi[1])/2),float(-lo[2])))))
    name='SM_MeadowSward'+variant;ob.name=name;mesh.name=name+'_Mesh';mesh.calc_loop_triangles();mesh.update()
    bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
    path=ART/'Exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False,add_leaf_bones=False,path_mode='STRIP')
    record={'source_asset':'grass_medium_02','source_assets':['grass_medium_02','grass_bermuda_01'],'source_object':'16 tall authored tufts plus 920 low Bermuda tufts; deterministic full 3D underlayer, original tall-patch bounds retained','fbx':path.name,
        'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'materials':[mat.name for mat in mesh.materials],'dimensions_cm':((hi-lo)*100).tolist(),'triangles':len(mesh.loop_triangles),'vertices':len(mesh.vertices),
        'pivot':'horizontal bounds center, ground z=0','nanite':True,'shape_preservation':'PRESERVE_AREA','unreal_path':'/Game/Art/NatureV04/'+name}
    MANIFEST['meshes'][name]=record;retained.append(ob);print('PREPARED_MEADOW_SWARD '+json.dumps(record),flush=True)
for ob in list(bpy.data.objects):
    if ob not in retained:bpy.data.objects.remove(ob,do_unlink=True)
for collection in list(bpy.data.collections):bpy.data.collections.remove(collection)
bpy.ops.outliner.orphans_purge(do_recursive=True)
for img in bpy.data.images:
    if img.packed_file:img.unpack(method='REMOVE')
bpy.context.preferences.filepaths.save_version=0
path=ART/'Source/Meadow_Swards.blend';bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(path),compress=True)
(ART/'Exports/environment_manifest.json').write_text(json.dumps(MANIFEST,indent=2))
print('MEADOW_SWARDS_PREPARED',flush=True)

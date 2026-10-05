"""Prepare full-crown CC0 trees and curved photographic grass for v0.4.

Run in Blender after download_environment_v04.py. Original source scale, UVs,
material assignments and complete LOD0 crown are retained. Runtime assets use
centimetres and grounded pivots. No whole-tree billboard or generated imagery.
"""
from pathlib import Path
import bpy, json, math, random, hashlib
import numpy as np
from mathutils import Matrix, Vector

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
SOURCE=ART/'Source';EXPORT=ART/'Exports';PREVIEW=ART/'Previews'
for p in (SOURCE,EXPORT,PREVIEW):p.mkdir(parents=True,exist_ok=True)
DATA=json.loads((ART/'sources.json').read_text(encoding='utf8'))
MANIFEST={'units':'centimeters','license':'CC0-1.0','meshes':{},'materials':{}}

def material(asset,group):
    name='PH_'+asset+'_'+group
    if bpy.data.materials.get(name):return bpy.data.materials[name]
    m=bpy.data.materials.new(name);m.use_nodes=True
    n=m.node_tree.nodes;l=m.node_tree.links;p=n.get('Principled BSDF')
    p.inputs['Roughness'].default_value=.8;p.inputs['Specular IOR Level'].default_value=.22
    maps=DATA['assets'][asset]['maps'][group]
    for role,record in maps.items():
        if role not in ('color','normal','roughness','alpha'):continue
        t=n.new('ShaderNodeTexImage');t.image=bpy.data.images.load(str(ROOT/record['local_path']),check_existing=True)
        if role!='color':t.image.colorspace_settings.name='Non-Color'
        if role=='color':
            if 'alpha' in maps:
                tint=n.new('ShaderNodeMixRGB');tint.blend_type='MULTIPLY';tint.inputs[0].default_value=1
                tint.inputs[2].default_value=(.55,.82,.50,1) if asset=='grass_bermuda_01' else (.45,.70,.42,1)
                l.new(t.outputs['Color'],tint.inputs[1]);l.new(tint.outputs[0],p.inputs['Base Color'])
            else:l.new(t.outputs['Color'],p.inputs['Base Color'])
        elif role=='alpha':l.new(t.outputs['Color'],p.inputs['Alpha'])
        elif role=='roughness':l.new(t.outputs['Color'],p.inputs['Roughness'])
        elif role=='normal':
            normal=n.new('ShaderNodeNormalMap');l.new(t.outputs['Color'],normal.inputs['Color']);l.new(normal.outputs['Normal'],p.inputs['Normal'])
    if 'alpha' in maps:p.inputs['Subsurface Weight'].default_value=.06
    MANIFEST['materials'][name]={'source_asset':asset,'texture_group':group,'foliage':'alpha' in maps}
    return m

def export(ob,name,asset,original):
    mesh=ob.data
    mesh.transform(ob.matrix_world);ob.matrix_world=Matrix.Identity(4)
    coords=np.empty(len(mesh.vertices)*3,np.float32);mesh.vertices.foreach_get('co',coords);coords=coords.reshape(-1,3)
    low=coords.min(axis=0);high=coords.max(axis=0)
    shift=Vector((float(-(low[0]+high[0])/2),float(-(low[1]+high[1])/2),float(-low[2])))
    mesh.transform(Matrix.Scale(100,4)@Matrix.Translation(shift));mesh.update()
    ob.name=name;mesh.name=name+'_Mesh';ob.hide_render=False;ob.hide_viewport=False;ob.hide_set(False)
    for c in list(ob.users_collection):c.objects.unlink(ob)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
    bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
    mesh.calc_loop_triangles()
    file=EXPORT/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',use_mesh_modifiers=True,
        mesh_smooth_type='FACE',use_tspace=False,bake_anim=False,add_leaf_bones=False,path_mode='STRIP')
    record={'source_asset':asset,'source_object':original,'fbx':file.name,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),
        'materials':[m.name for m in mesh.materials],'dimensions_cm':((high-low)*100).tolist(),
        'triangles':len(mesh.loop_triangles),'vertices':len(mesh.vertices),'pivot':'horizontal bounds center, ground z=0',
        'nanite':True,'shape_preservation':'PRESERVE_AREA','unreal_path':'/Game/Art/NatureV04/'+name}
    MANIFEST['meshes'][name]=record;print('ENVIRONMENT_PREPARED '+json.dumps(record),flush=True)
    for other in list(bpy.data.objects):
        if other!=ob:bpy.data.objects.remove(other,do_unlink=True)
    for collection in list(bpy.data.collections):bpy.data.collections.remove(collection)
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    for img in bpy.data.images:
        if img.packed_file:img.unpack(method='REMOVE')
    bpy.context.preferences.filepaths.save_version=0
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/(name+'.blend')),compress=True)
    bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/(name+'.blend')),compress=True)

# The complete 19.5-m mature canopy: no decimation of the author's LOD0 foliage.
bpy.ops.wm.open_mainfile(filepath=str(ROOT/DATA['assets']['jacaranda_tree']['original_source']['local_path']))
ob=bpy.data.objects['jacaranda_tree_LOD0'];mesh=ob.data
original=list(mesh.materials);groups=[m.name.removeprefix('jacaranda_tree_') for m in original]
indices=np.empty(len(mesh.polygons),np.int32);mesh.polygons.foreach_get('material_index',indices)
uv=np.empty(len(mesh.loops)*2,np.float32);mesh.uv_layers['UVMap'].data.foreach_get('uv',uv);uv=uv.reshape(-1,2)
# Bake the source trunk's painted bark transition while preserving photographic
# trunk UVs below it and the authored 15x repeating branch UVs above it.
col=mesh.color_attributes.get('Col')
if col and col.domain=='CORNER':
    colors=np.empty(len(col.data)*4,np.float32);col.data.foreach_get('color',colors)
    starts=np.empty(len(mesh.polygons),np.int32);counts=np.empty(len(mesh.polygons),np.int32)
    mesh.polygons.foreach_get('loop_start',starts);mesh.polygons.foreach_get('loop_total',counts)
    average=np.add.reduceat(colors.reshape(-1,4)[:,0],starts)/counts
    trunk=groups.index('trunk');branches=groups.index('branches')
    changed=(indices==trunk)&(average>.5)
    uv[np.repeat(changed,counts)]*=15;indices[changed]=branches
    print('BARK_TRANSITION_FACES',int(changed.sum()),flush=True)
while len(mesh.uv_layers):mesh.uv_layers.remove(mesh.uv_layers[0])
layer=mesh.uv_layers.new(name='UV0');layer.data.foreach_set('uv',uv.ravel());layer.active_render=True
for c in list(mesh.color_attributes):mesh.color_attributes.remove(c)
mats=[material('jacaranda_tree',g) for g in groups]
mesh.materials.clear()
for m in mats:mesh.materials.append(m)
mesh.polygons.foreach_set('material_index',indices)
export(ob,'SM_JacarandaA','jacaranda_tree','jacaranda_tree_LOD0')

# A compact, irregular meadow clump assembled from the provider's curved real
# blade/tuft meshes and photographic opacity atlas. Seedlings provide height.
bpy.ops.wm.open_mainfile(filepath=str(ROOT/DATA['assets']['grass_bermuda_01']['original_source']['local_path']))
templates=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('grass_bermuda_01_') and
    any(c.name=='grass_bermuda_01_static' for c in o.users_collection) and
    any(k in o.name for k in ('medium_','small_','seedling_'))]
if not templates:raise RuntimeError('No authored Bermuda grass tufts')
rng=random.Random(40423);copies=[]
for i in range(230):
    src=templates[i%len(templates)];o=bpy.data.objects.new('meadow_tuft',src.data.copy());bpy.context.scene.collection.objects.link(o)
    coords=np.array([v.co[:] for v in o.data.vertices]);lo=coords.min(axis=0);hi=coords.max(axis=0)
    o.data.transform(Matrix.Translation(Vector((float(-(lo[0]+hi[0])/2),float(-(lo[1]+hi[1])/2),float(-lo[2])))))
    angle=rng.random()*math.tau;radius=.33*math.sqrt(rng.random())
    o.location=(math.cos(angle)*radius,math.sin(angle)*radius,rng.uniform(-.006,.002));o.rotation_euler.z=rng.random()*math.tau
    scale=rng.uniform(.8,1.5);o.scale=(scale,scale,scale*rng.uniform(.9,1.3));copies.append(o)
bpy.ops.object.select_all(action='DESELECT')
for o in copies:o.select_set(True)
bpy.context.view_layer.objects.active=copies[0];bpy.ops.object.join();ob=bpy.context.object
ob.data.materials.clear();ob.data.materials.append(material('grass_bermuda_01','surface'))
for p in ob.data.polygons:p.material_index=0
for c in list(ob.data.color_attributes):ob.data.color_attributes.remove(c)
ob.data.uv_layers.active.name='UV0'
export(ob,'SM_MeadowGrassA','grass_bermuda_01','230 authored curved tufts, seeded spatial assembly')
(EXPORT/'environment_manifest.json').write_text(json.dumps(MANIFEST,indent=2),encoding='utf8')
print('ENVIRONMENT_V04_PREPARATION_COMPLETE',flush=True)

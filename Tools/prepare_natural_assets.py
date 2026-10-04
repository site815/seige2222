"""Convert verified CC0 Poly Haven sources to local centimetre FBXs and editable blends.

Run with the portable Blender binary. The source download script must run first.
FirA/B and broadleaf retain authored LOD1; FirC uses fuller authored LOD0 to retain
its canopy at gameplay distance. Unreal Nanite handles subsequent simplification.
Source material UV attributes and mapping scales are baked into an ordinary UV0.
"""
from pathlib import Path
import bpy, json, math, random, hashlib, sys
import numpy as np
from mathutils import Matrix, Vector

ROOT=Path(__file__).resolve().parents[1]
FIR_LOD0_EXPERIMENT='--fir-lod0-experiment' in sys.argv
ART=ROOT/('Saved/NatureLOD0' if FIR_LOD0_EXPERIMENT else 'Art/Nature')
EXPORT=ART/'Exports';SOURCE=ART/'Source';PREVIEW=ART/'Previews'
for folder in (EXPORT,SOURCE,PREVIEW):folder.mkdir(parents=True,exist_ok=True)
DATA=json.loads((ROOT/'Art/Nature/sources.json').read_text(encoding='utf-8'))
SELECTION={
 'fir_tree_01': [('SM_FirA','fir_tree_01_a_LOD1'),('SM_FirB','fir_tree_01_b_LOD1'),('SM_FirC','fir_tree_01_c_LOD0')],
 'tree_small_02':[('SM_BroadleafA','tree_small_02_LOD1')],
 'fern_02':[('SM_FernA','fern_02_a'),('SM_FernB','fern_02_b')],
 'rock_moss_set_01':[('SM_MossRockA','rock_moss_set_01_rock01'),('SM_MossRockB','rock_moss_set_01_rock03')],
}
if FIR_LOD0_EXPERIMENT:
    SELECTION={'fir_tree_01':[(name,original.replace('_LOD1','_LOD0')) for name,original in SELECTION['fir_tree_01']]}
MANIFEST={'units':'centimeters','provider':'Poly Haven','license':'CC0-1.0','meshes':{},'materials':{}}

def groups_for(asset,material):
    if asset=='fir_tree_01':
        if '_trunk_' in material:return 'trunk_'+material.rsplit('_',1)[-1]
        return 'twig' if material.endswith('_twig') else 'bark'
    if asset=='tree_small_02':
        return 'branch' if material.endswith('_branches') else ('leaves' if material.endswith('_leaves') else 'surface')
    return 'surface'

def material(asset,group):
    key=asset+'_'+group
    existing=bpy.data.materials.get('PH_'+key)
    if existing:return existing
    maps=DATA['assets'][asset]['maps'][group]
    m=bpy.data.materials.new('PH_'+key);m.use_nodes=True
    ns=m.node_tree.nodes;ls=m.node_tree.links;p=ns.get('Principled BSDF')
    p.inputs['Roughness'].default_value=.8;p.inputs['Specular IOR Level'].default_value=.22
    for role,record in maps.items():
        n=ns.new('ShaderNodeTexImage');n.image=bpy.data.images.load(str(ROOT/record['local_path']),check_existing=True)
        if role!='color':n.image.colorspace_settings.name='Non-Color'
        if role=='color':ls.new(n.outputs['Color'],p.inputs['Base Color'])
        elif role=='roughness':ls.new(n.outputs['Color'],p.inputs['Roughness'])
        elif role=='alpha':ls.new(n.outputs['Color'],p.inputs['Alpha'])
        elif role=='normal':
            normal=ns.new('ShaderNodeNormalMap');ls.new(n.outputs['Color'],normal.inputs['Color']);ls.new(normal.outputs['Normal'],p.inputs['Normal'])
    MANIFEST['materials'][m.name]={'source_asset':asset,'texture_group':group,'foliage':'alpha' in maps}
    return m

def uv_values(mesh,name):
    attribute=mesh.attributes.get(name)
    if not attribute:raise RuntimeError('Missing UV attribute '+name)
    width=3 if attribute.data_type=='FLOAT_VECTOR' else 2
    result=np.empty(len(attribute.data)*width,dtype=np.float32)
    attribute.data.foreach_get('vector',result)
    return result.reshape((-1,width))[:,:2].copy()

for asset,selection in SELECTION.items():
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/DATA['assets'][asset]['original_source']['local_path']))
    bpy.context.preferences.filepaths.save_version=0
    retained=[]
    for name,original in selection:
        ob=bpy.data.objects[original];ob.hide_render=False;ob.hide_viewport=False;ob.hide_set(False)
        # Keep only complete authored meshes, never source needle/leaf templates.
        mesh=ob.data.copy();ob.data=mesh
        original_materials=list(mesh.materials)
        uv=uv_values(mesh,'UVMap')
        branch_uv=uv_values(mesh,'UV_map_01') if asset=='tree_small_02' else None
        mats=[material(asset,groups_for(asset,m.name if m else asset+'_bark')) for m in original_materials]
        bark_index=next((i for i,m in enumerate(mats) if m.name=='PH_fir_tree_01_bark'),None)
        col=mesh.color_attributes.get('Col')
        upper_trunk_faces=0
        for poly in mesh.polygons:
            source_mat=original_materials[poly.material_index]
            old=source_mat.name if source_mat else asset+'_bark'
            ids=list(poly.loop_indices)
            if asset=='fir_tree_01' and old.endswith('_bark'):uv[ids]*=(1.2,.1)
            elif asset=='tree_small_02' and old.endswith('_branches'):uv[ids]=branch_uv[ids]*(3.0,.6)
            elif asset=='fir_tree_01' and '_trunk_' in old and col and bark_index is not None:
                # The original shader changes from the photographed trunk to bark
                # above a painted mask. Bake that selection and box UV projection.
                weight=sum(col.data[v].color[0] for v in poly.vertices)/len(poly.vertices)
                if weight>.5:
                    poly.material_index=bark_index;upper_trunk_faces+=1
                    axis=max(range(3),key=lambda i:abs(poly.normal[i]))
                    axes=((1,2),(0,2),(0,1))[axis]
                    for li in ids:
                        co=mesh.vertices[mesh.loops[li].vertex_index].co
                        uv[li]=(co[axes[0]]*(.7 if axes[0]==2 else 1.4),co[axes[1]]*(.7 if axes[1]==2 else 1.4))
        # Export a real UV layer even when the upstream source uses a vector attribute.
        while len(mesh.uv_layers):mesh.uv_layers.remove(mesh.uv_layers[0])
        legacy=mesh.attributes.get('UVMap')
        if legacy:mesh.attributes.remove(legacy)
        layer=mesh.uv_layers.new(name='UV0');layer.data.foreach_set('uv',uv.ravel());layer.active_render=True
        for color in list(mesh.color_attributes):mesh.color_attributes.remove(color)
        material_indices=np.empty(len(mesh.polygons),dtype=np.int32)
        mesh.polygons.foreach_get('material_index',material_indices)
        mesh.materials.clear()
        for mat in mats:mesh.materials.append(mat)
        mesh.polygons.foreach_set('material_index',material_indices)
        # Bake source object transforms, convert metres to centimetres and ground
        # the pivot. Horizontal bounds are centred for deterministic placement.
        mesh.transform(ob.matrix_world);ob.matrix_world=Matrix.Identity(4)
        mins=[min(v.co[i] for v in mesh.vertices) for i in range(3)]
        maxs=[max(v.co[i] for v in mesh.vertices) for i in range(3)]
        shift=Vector((-(mins[0]+maxs[0])/2,-(mins[1]+maxs[1])/2,-mins[2]))
        mesh.transform(Matrix.Scale(100,4)@Matrix.Translation(shift));mesh.update()
        ob.name=name;mesh.name=name+'_Mesh'
        # Link visibly into the master collection; source LOD collections can hide.
        for collection in list(ob.users_collection):collection.objects.unlink(ob)
        bpy.context.scene.collection.objects.link(ob)
        bpy.ops.object.select_all(action='DESELECT');ob.select_set(True);bpy.context.view_layer.objects.active=ob
        bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=.01
        mesh.calc_loop_triangles()
        record={'source_asset':asset,'source_object':original,'fbx':name+'.fbx','materials':[m.name for m in mats],
            'dimensions_cm':[(maxs[i]-mins[i])*100 for i in range(3)],'triangles':len(mesh.loop_triangles),'vertices':len(mesh.vertices),
            'pivot':'horizontal bounds center, ground z=0','source_uv_mapping_baked':True,'baked_upper_trunk_faces':upper_trunk_faces,
            'unreal_path':'/Game/Art/Nature/'+name,'nanite':not name.startswith('SM_Fern')}
        MANIFEST['meshes'][name]=record
        bpy.ops.export_scene.fbx(filepath=str(EXPORT/record['fbx']),use_selection=True,object_types={'MESH'},
            apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',
            use_mesh_modifiers=True,mesh_smooth_type='FACE',use_tspace=False,bake_anim=False,
            add_leaf_bones=False,path_mode='STRIP')
        retained.append(ob);print('PREPARED',name,record,flush=True)
    for ob in list(bpy.data.objects):
        if ob not in retained:bpy.data.objects.remove(ob,do_unlink=True)
    for collection in list(bpy.data.collections):bpy.data.collections.remove(collection)
    bpy.ops.outliner.orphans_purge(do_recursive=True)
    # Images stay externally referenced and versioned, without duplicated packs.
    for img in bpy.data.images:
        if img.packed_file:img.unpack(method='REMOVE')
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/(asset+'.blend')),compress=True)
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/(asset+'.blend')),compress=True)

(EXPORT/'nature_manifest.json').write_text(json.dumps(MANIFEST,indent=2),encoding='utf-8')
if FIR_LOD0_EXPERIMENT:
    print('NATURE_FIR_LOD0_EXPERIMENT_PREPARED '+str(ART),flush=True)
    sys.exit(0)

# A small physically scaled woodland review scene using the exact exported meshes.
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
objects={}
for asset in SELECTION:
    with bpy.data.libraries.load(str(SOURCE/(asset+'.blend')),link=True) as (source,target):
        target.objects=[name for name in source.objects if name.startswith('SM_')]
    for ob in target.objects:
        if ob:
            local=ob.copy();scene.collection.objects.link(local);objects[ob.name]=local
placements={'SM_FirA':(-540,360,0),'SM_FirB':(340,720,0),'SM_FirC':(720,150,0),
    'SM_BroadleafA':(-650,-310,0),'SM_FernA':(-130,-420,0),'SM_FernB':(170,-370,0),
    'SM_MossRockA':(260,-140,0),'SM_MossRockB':(-170,-190,0)}
for name,ob in objects.items():ob.location=placements[name]
rng=random.Random(114)
for i in range(24):
    ob=objects['SM_FernA' if i%2 else 'SM_FernB'].copy();scene.collection.objects.link(ob)
    ob.location=(rng.uniform(-950,1050),rng.uniform(-650,800),0);ob.rotation_euler.z=rng.random()*math.tau
    ob.scale*=rng.uniform(.6,1.2)
bpy.ops.mesh.primitive_plane_add(size=20000,location=(0,0,-2));ground=bpy.context.object
mat=bpy.data.materials.new('Preview forest floor');mat.use_nodes=True;n=mat.node_tree.nodes;l=mat.node_tree.links;p=n.get('Principled BSDF')
p.inputs['Roughness'].default_value=.92
tex=n.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(ROOT/'Art/Textures/PolyHaven/grass_ground_diff_2k.jpg'))
coord=n.new('ShaderNodeTexCoord');mapping=n.new('ShaderNodeVectorMath');mapping.operation='SCALE';mapping.inputs[3].default_value=55
l.new(coord.outputs['UV'],mapping.inputs[0]);l.new(mapping.outputs[0],tex.inputs[0]);l.new(tex.outputs[0],p.inputs['Base Color']);ground.data.materials.append(mat)
world=bpy.data.worlds.new('Temperate daylight');world.use_nodes=True;scene.world=world
sky=world.node_tree.nodes.new('ShaderNodeTexSky');sky.sky_type='NISHITA';sky.sun_elevation=.65;sky.sun_rotation=1.1
world.node_tree.links.new(sky.outputs[0],world.node_tree.nodes['Background'].inputs[0]);world.node_tree.nodes['Background'].inputs[1].default_value=.24
bpy.ops.object.light_add(type='SUN');bpy.context.object.rotation_euler=(.6,-.4,-.5);bpy.context.object.data.energy=2.4;bpy.context.object.data.angle=.045
bpy.ops.object.camera_add(location=(2350,-3000,1850));cam=bpy.context.object
cam.rotation_euler=(Vector((0,200,650))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=42;cam.data.clip_end=100000;scene.camera=cam
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=1500;scene.render.resolution_y=1150;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.view_settings.exposure=-1.3
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Nature_Review.blend'),compress=True)
bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Nature_Review.blend'),compress=True)
scene.render.filepath=str(PREVIEW/'licensed_woodland.png');bpy.ops.render.render(write_still=True)
print('NATURE_PREPARATION_COMPLETE',flush=True)

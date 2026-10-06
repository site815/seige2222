"""Reduce redundant meadow undergrowth, retaining photographed foreground blades.

Local CC0 sources only. The sixteen tall tufts stay; the old 920 overlapping
masked low tufts become four inexpensive opaque blade beds. Assemblies retain the authored patch
bounds for stable placement and proxy matching. Run with bundled Blender.
"""
from pathlib import Path
import bpy, json, math, random, hashlib
import numpy as np
from mathutils import Matrix, Vector

ROOT=Path(__file__).resolve().parents[1]
OLD=ROOT/'Art/EnvironmentV04'; ART=ROOT/'Art/EnvironmentV081'
for folder in ('Source','Exports'): (ART/folder).mkdir(parents=True,exist_ok=True)
data=json.loads((OLD/'sources.json').read_text())
baseline=json.loads((OLD/'Exports/environment_manifest.json').read_text())
bpy.ops.wm.open_mainfile(filepath=str(ROOT/data['assets']['grass_medium_02']['original_source']['local_path']))
tall=[bpy.data.objects['grass_medium_02_'+letter].data.copy() for letter in 'bcde']
with bpy.data.libraries.load(str(ROOT/'Art/EnvironmentV07/Source/Scenery_LODs.blend'),link=False) as (src,dst):
    dst.objects=['SM_GrassProxy']
ground=dst.objects[0].data.copy(); ground.transform(Matrix.Scale(.01,4))
for ob in list(bpy.data.objects): bpy.data.objects.remove(ob,do_unlink=True)
materials={}
for source in ('grass_medium_02','opaque_ground'):
    m=bpy.data.materials.new('PH_'+source+'_surface'); materials[source]=m
result={'units':'centimeters','license':'CC0-1.0','source_manifest':'Art/EnvironmentV04/sources.json',
        'description':'Sixteen photographic tall tufts plus four opaque curved blade beds; v0.8 had 920 masked low tufts.', 'meshes':{}}
retained=[]
for variant,seed in (('A',4407),('B',4419)):
    rng=random.Random(seed); copies=[]
    def add(mesh,source,location,rotation,scale):
        ob=bpy.data.objects.new('tuft',mesh.copy()); bpy.context.scene.collection.objects.link(ob)
        coords=np.array([v.co[:] for v in ob.data.vertices]); lo=coords.min(axis=0); hi=coords.max(axis=0)
        ob.data.transform(Matrix.Translation(Vector((float(-(lo[0]+hi[0])/2),float(-(lo[1]+hi[1])/2),float(-lo[2])))))
        ob.location=location; ob.rotation_euler.z=rotation; ob.scale=scale
        ob.data.materials.clear(); ob.data.materials.append(materials[source])
        for poly in ob.data.polygons: poly.material_index=0
        for color in list(ob.data.color_attributes): ob.data.color_attributes.remove(color)
        assert ob.data.uv_layers
        ob.data.uv_layers.active.name='UV0'; copies.append(ob)
    for i in range(16):
        x=(i%4-1.5)*.29+rng.uniform(-.08,.08); y=(i//4-1.5)*.29+rng.uniform(-.08,.08)
        z=rng.uniform(-.008,.005); angle=rng.random()*math.tau; size=rng.uniform(.92,1.35)
        add(tall[(i+seed)%len(tall)],'grass_medium_02',(x,y,z),angle,(size,size,size*rng.uniform(1,1.25)))
    for i,(x,y) in enumerate(((-.3,-.3),(.3,-.3),(-.3,.3),(.3,.3))):
        add(ground,'opaque_ground',(x,y,0),(i+seed)%4*math.pi/2,(.6,.6,.35))
    bpy.ops.object.select_all(action='DESELECT')
    for ob in copies: ob.select_set(True)
    bpy.context.view_layer.objects.active=copies[0]; bpy.ops.object.join(); ob=bpy.context.object
    mesh=ob.data; mesh.transform(ob.matrix_world); ob.matrix_world=Matrix.Identity(4)
    coords=np.array([v.co[:] for v in mesh.vertices]); lo=coords.min(axis=0); hi=coords.max(axis=0)
    target=np.array(baseline['meshes']['SM_MeadowSward'+variant]['dimensions_cm'])
    scale=target/(hi-lo)
    mesh.transform(Matrix.Diagonal(Vector((*scale,1)))@Matrix.Translation(Vector((float(-(lo[0]+hi[0])/2),float(-(lo[1]+hi[1])/2),float(-lo[2])))))
    name='SM_MeadowSwardV081'+variant; ob.name=name; mesh.name=name+'_Mesh'; mesh.calc_loop_triangles(); mesh.update()
    bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=.01
    path=ART/'Exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',mesh_smooth_type='FACE',bake_anim=False,add_leaf_bones=False,path_mode='STRIP')
    result['meshes'][name]={'fbx':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'triangles':len(mesh.loop_triangles),'baseline_triangles':baseline['meshes']['SM_MeadowSward'+variant]['triangles'],
        'dimensions_cm':target.tolist(),'materials':[mat.name for mat in mesh.materials], 'tall_tufts':16,'opaque_underlayer_blades':3072}
    retained.append(ob)
for ob in list(bpy.data.objects):
    if ob not in retained: bpy.data.objects.remove(ob,do_unlink=True)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(ART/'Source/Meadow_Swards.blend'),compress=True)
(ART/'Exports/environment_manifest.json').write_text(json.dumps(result,indent=2))
print('MEADOW_V081_PREPARED '+json.dumps(result),flush=True)

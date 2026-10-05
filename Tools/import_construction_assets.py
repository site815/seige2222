"""Import only v0.5 shuttle/service geometry and construction presentation materials."""
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Construction';DATA=json.loads((ART/'Exports/construction_manifest.json').read_text())
ED=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();ME=u.MaterialEditingLibrary
SME=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
DEST='/Game/Art/Construction';ED.make_directory(DEST)
def asset(name):return ED.load_asset(DEST+'/'+name) if ED.does_asset_exist(DEST+'/'+name) else AT.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
def node(m,kind,**props):
    n=ME.create_material_expression(m,kind)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def wire(a,b,pin,out=''):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError('Construction material link '+pin)
def output(a,prop,pin=''):
    if not ME.connect_material_property(a,pin,prop):raise RuntimeError('Construction material output '+str(prop))
holo=asset('M_ConstructionHologram');ME.delete_all_material_expressions(holo)
holo.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT);holo.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
holo.set_editor_property('two_sided',True);holo.set_editor_property('used_with_instanced_static_meshes',True)
tint=node(holo,u.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=u.LinearColor(.18,.9,.68,1))
glow=node(holo,u.MaterialExpressionMultiply,const_b=1.25);wire(tint,glow,'A');output(glow,u.MaterialProperty.MP_EMISSIVE_COLOR)
fresnel=node(holo,u.MaterialExpressionFresnel,exponent=3,base_reflect_fraction=.45)
opacity=node(holo,u.MaterialExpressionScalarParameter,parameter_name='Opacity',default_value=.25)
coverage=node(holo,u.MaterialExpressionMultiply);wire(fresnel,coverage,'A');wire(opacity,coverage,'B');output(coverage,u.MaterialProperty.MP_OPACITY)
ME.layout_material_expressions(holo);ME.recompile_material(holo);ED.save_loaded_asset(holo,False)

# Clone the actual PBR master, retaining every texture/scalar parameter. Runtime
# MIDs copy each original material instance's uniforms before applying this cut.
source=ED.load_asset('/Game/Art/Industry/M_IndustrySurface')
if not source:raise RuntimeError('Original industrial material required before construction import')
reveal=ED.load_asset(DEST+'/M_ConstructionReveal') if ED.does_asset_exist(DEST+'/M_ConstructionReveal') else None
if not reveal:reveal=AT.duplicate_asset('M_ConstructionReveal',DEST,source)
# Repeated imports replace only the construction cut, preserving the PBR graph.
if not reveal:raise RuntimeError('Could not create construction reveal material')
for n in list(ME.get_material_expressions(reveal)):
    if str(n.get_editor_property('desc')).startswith('ConstructionReveal:'):ME.delete_material_expression(reveal,n)
reveal.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);reveal.set_editor_property('opacity_mask_clip_value',.5)
world=node(reveal,u.MaterialExpressionWorldPosition,desc='ConstructionReveal: world position')
z=node(reveal,u.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False,desc='ConstructionReveal: height');wire(world,z,'')
height=node(reveal,u.MaterialExpressionScalarParameter,parameter_name='RevealHeight',default_value=1.e9,desc='ConstructionReveal: progress plane')
mask=node(reveal,u.MaterialExpressionSubtract,desc='ConstructionReveal: visible below plane');wire(height,mask,'A');wire(z,mask,'B')
output(mask,u.MaterialProperty.MP_OPACITY_MASK);ME.layout_material_expressions(reveal);ME.recompile_material(reveal);ED.save_loaded_asset(reveal,False)

tasks=[]
for name,r in DATA['meshes'].items():
    task=u.AssetImportTask();task.filename=str(ART/'Exports'/r['fbx']);task.destination_path='/Game/Art';task.destination_name=name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
    options=u.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
    options.automated_import_should_detect_type=False;options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    sm=options.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=False;sm.generate_lightmap_u_vs=False;sm.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
    task.options=options;tasks.append(task)
AT.import_asset_tasks(tasks)
report={'source':'Original project Blender geometry','materials':[holo.get_path_name(),reveal.get_path_name()],'meshes':{}}
for name,r in DATA['meshes'].items():
    mesh=ED.load_asset('/Game/Art/'+name)
    if not mesh:raise RuntimeError('Missing '+name)
    slots=[]
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        label=str(slot.get_editor_property('imported_material_slot_name'));material=ED.load_asset('/Game/Art/Industry/MI_Industry_'+label.removeprefix('IM_'))
        if not material:raise RuntimeError('Unknown original industrial material '+label)
        mesh.set_material(i,material);slots.append(label)
    if set(slots)!=set(r['materials']):raise RuntimeError('Material slot mismatch '+name)
    reduction=u.StaticMeshReductionOptions();reduction.set_editor_property('auto_compute_lod_screen_size',False);levels=[]
    for percent,screen in ((1,1),(.5,.20),(.2,.06)):
        setting=u.StaticMeshReductionSettings();setting.set_editor_property('percent_triangles',percent);setting.set_editor_property('screen_size',screen);levels.append(setting)
    reduction.set_editor_property('reduction_settings',levels)
    if SME.set_lods(mesh,reduction)!=3:raise RuntimeError('LOD generation failed '+name)
    ED.save_loaded_asset(mesh,False);box=mesh.get_bounding_box();lo,hi=box.min,box.max;size=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.5 or any(abs(a-b)>.5 for a,b in zip(size,r['dimensions_cm'])):raise RuntimeError('Bounds mismatch '+name)
    report['meshes'][name]={'path':mesh.get_path_name(),'dimensions_cm':size,'ground_z':lo.z,'lods':SME.get_lod_count(mesh),'materials':slots,'uv_channels':SME.get_num_uv_channels(mesh,0)}
(ART/'import_report.json').write_text(json.dumps(report,indent=2));u.log('SEIGE_CONSTRUCTION_IMPORT_SUCCESS '+json.dumps(report))

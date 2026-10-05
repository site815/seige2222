"""Import verified local CC0 v0.4 vegetation and terrain maps; no network calls.

Full authored mature tree crown uses Nanite PreserveArea. Leaf atlas coverage is
retained per material, with no global texture-quality override or vertex tint.
"""
from pathlib import Path
import json,unreal

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';DEST='/Game/Art/NatureV04'
DATA=json.loads((ART/'Exports/environment_manifest.json').read_text())
SOURCES=json.loads((ART/'sources.json').read_text())
MEADOW_ONLY=globals().get('MEADOW_ONLY',False)
if MEADOW_ONLY:
    DATA['meshes']={k:v for k,v in DATA['meshes'].items() if k.startswith('SM_MeadowSward') or k=='SM_WildflowerPatch'}
    wanted={m for r in DATA['meshes'].values() for m in r['materials']}
    DATA['materials']={k:v for k,v in DATA['materials'].items() if k in wanted}
    SOURCES['assets']={key:SOURCES['assets'][key] for key in ('grass_medium_02','grass_bermuda_01')}
AT=unreal.AssetToolsHelpers.get_asset_tools();ED=unreal.EditorAssetLibrary;ME=unreal.MaterialEditingLibrary
SME=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
ED.make_directory(DEST);ED.make_directory(DEST+'/Textures')

def asset(name,kind,factory):
    path=DEST+'/'+name
    result=ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name,DEST,kind,factory)
    if not result:raise RuntimeError('Failed creating '+path)
    return result
def node(mat,kind,**props):
    n=ME.create_material_expression(mat,kind,0,0)
    for key,value in props.items():n.set_editor_property(key,value)
    return n
def wire(source,target,pin,output=''):
    if not ME.connect_material_expressions(source,output,target,pin):raise RuntimeError('Material connection failed '+pin)
def output(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Material output failed '+str(prop))
def texture_name(source,group,role):return 'T_'+source+'_'+group+'_'+role

jobs=[];records={};TEXTURES={}
for source,data in SOURCES['assets'].items():
    for group,maps in data['maps'].items():
        for role,record in maps.items():
            if role=='height':continue # Retained source height map is not a claim of runtime displacement.
            name=texture_name(source,group,role);records[name]=(role,record)
            t=unreal.AssetImportTask();t.filename=str(ROOT/record['local_path']);t.destination_path=DEST+'/Textures';t.destination_name=name
            t.automated=True;t.replace_existing=True;t.save=True;jobs.append(t)
AT.import_asset_tasks(jobs)
for name,(role,record) in records.items():
    tex=ED.load_asset(DEST+'/Textures/'+name)
    if not isinstance(tex,unreal.Texture2D):raise RuntimeError('Missing '+name)
    tex.set_editor_property('srgb',role=='color')
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if role=='normal' else unreal.TextureCompressionSettings.TC_DEFAULT)
    if role=='normal':tex.set_editor_property('flip_green_channel',True)
    if role=='alpha':
        tex.set_editor_property('do_scale_mips_for_alpha_coverage',True)
        tex.set_editor_property('alpha_coverage_thresholds',unreal.Vector4(.33,0,0,0))
    ED.save_loaded_asset(tex,False);TEXTURES[name]=tex

def sample(mat,param,role,source,group,bias=None):
    sampler=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='normal' else
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='color' else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    n=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name=param,texture=TEXTURES[texture_name(source,group,role)],sampler_type=sampler)
    if bias is not None:
        n.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_MIP_BIAS);n.set_editor_property('const_mip_value',bias)
    return n

parents={}
for foliage,name,group in ((False,'M_V04Bark','branches'),(True,'M_V04Foliage','leaves')):
    if MEADOW_ONLY:
        parents[foliage]=ED.load_asset(DEST+'/'+name)
        if not parents[foliage]:raise RuntimeError('Import the base environment before meadow additions')
        continue
    m=asset(name,unreal.Material,unreal.MaterialFactoryNew());ME.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('used_with_nanite',True)
    m.set_editor_property('two_sided',foliage);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if foliage else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE if foliage else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    color=sample(m,'BaseColor','color','jacaranda_tree',group,-1 if foliage else None)
    normal=sample(m,'NormalMap','normal','jacaranda_tree',group);rough=sample(m,'RoughnessMap','roughness','jacaranda_tree',group)
    albedo=color
    if foliage:
        tint=node(m,unreal.MaterialExpressionVectorParameter,parameter_name='FoliageTint',default_value=unreal.LinearColor(.45,.70,.42,1))
        albedo=node(m,unreal.MaterialExpressionMultiply);wire(color,albedo,'A','RGB');wire(tint,albedo,'B')
    output(albedo,unreal.MaterialProperty.MP_BASE_COLOR,'' if foliage else 'RGB');output(normal,unreal.MaterialProperty.MP_NORMAL,'RGB');output(rough,unreal.MaterialProperty.MP_ROUGHNESS,'R')
    output(node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Specular',default_value=.15 if foliage else .22),unreal.MaterialProperty.MP_SPECULAR)
    ao=sample(m,'OcclusionMap','ao','jacaranda_tree',group);occlusion=node(m,unreal.MaterialExpressionLinearInterpolate,const_a=1,const_alpha=.65 if foliage else .4);wire(ao,occlusion,'B','R');output(occlusion,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if foliage:
        m.set_editor_property('opacity_mask_clip_value',.33)
        output(sample(m,'OpacityMap','alpha','jacaranda_tree',group,-2),unreal.MaterialProperty.MP_OPACITY_MASK,'R')
        transmission=node(m,unreal.MaterialExpressionMultiply,const_b=.18);wire(albedo,transmission,'A');output(transmission,unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        output(node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Thickness',default_value=.8),unreal.MaterialProperty.MP_OPACITY)
    ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,False);parents[foliage]=m

instances={};params={'color':'BaseColor','normal':'NormalMap','roughness':'RoughnessMap','alpha':'OpacityMap','ao':'OcclusionMap'}
flower_parent=asset('M_V04Wildflowers',unreal.Material,unreal.MaterialFactoryNew());ME.delete_all_material_expressions(flower_parent)
flower_parent.set_editor_property('used_with_instanced_static_meshes',True);flower_parent.set_editor_property('used_with_nanite',True);flower_parent.set_editor_property('two_sided',True)
flower_parent.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
flower_tint=node(flower_parent,unreal.MaterialExpressionVectorParameter,parameter_name='Tint',default_value=unreal.LinearColor(.1,.2,.04,1))
output(flower_tint,unreal.MaterialProperty.MP_BASE_COLOR)
output(node(flower_parent,unreal.MaterialExpressionConstant,r=.85),unreal.MaterialProperty.MP_ROUGHNESS)
output(node(flower_parent,unreal.MaterialExpressionConstant,r=.15),unreal.MaterialProperty.MP_SPECULAR)
flower_transmission=node(flower_parent,unreal.MaterialExpressionMultiply,const_b=.12);wire(flower_tint,flower_transmission,'A');output(flower_transmission,unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
ME.recompile_material(flower_parent);ED.save_loaded_asset(flower_parent,False)
for slot,s in DATA['materials'].items():
    mi=asset('MI_'+slot.removeprefix('PH_'),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    if 'original_tint' in s:
        ME.set_material_instance_parent(mi,flower_parent);ME.set_material_instance_vector_parameter_value(mi,'Tint',unreal.LinearColor(*s['original_tint']))
        ME.update_material_instance(mi);ED.save_loaded_asset(mi,False);instances[slot]=mi
        continue
    ME.set_material_instance_parent(mi,parents[s['foliage']])
    for role in SOURCES['assets'][s['source_asset']]['maps'][s['texture_group']]:
        ME.set_material_instance_texture_parameter_value(mi,params[role],TEXTURES[texture_name(s['source_asset'],s['texture_group'],role)])
    if s['source_asset']=='grass_bermuda_01':ME.set_material_instance_vector_parameter_value(mi,'FoliageTint',unreal.LinearColor(.55,.82,.5,1))
    if s['source_asset']=='grass_medium_02':ME.set_material_instance_vector_parameter_value(mi,'FoliageTint',unreal.LinearColor(.46,.72,.38,1))
    ME.update_material_instance(mi);ED.save_loaded_asset(mi,False);instances[slot]=mi

tasks=[]
for name,r in DATA['meshes'].items():
    t=unreal.AssetImportTask();t.filename=str(ART/'Exports'/r['fbx']);t.destination_path=DEST;t.destination_name=name
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=True
    o=unreal.FbxImportUI();o.import_mesh=True;o.import_as_skeletal=False;o.import_materials=False;o.import_textures=False
    o.automated_import_should_detect_type=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    s=o.static_mesh_import_data;s.combine_meshes=True;s.auto_generate_collision=False;s.generate_lightmap_u_vs=False
    s.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;s.convert_scene=True;s.convert_scene_unit=True;s.vertex_color_import_option=unreal.VertexColorImportOption.IGNORE
    t.options=o;tasks.append(t)
AT.import_asset_tasks(tasks)
report={'license':'CC0 natural sources; original project wildflowers','meshes':{},'textures':len(TEXTURES),'materials':len(instances),'runtime_network_required':False,
    'foliage_mip_bias':{'BaseColor':-1,'OpacityMap':-2},'alpha_coverage':.33,'normal_conversion':'OpenGL green channel inverted'}
for name,r in DATA['meshes'].items():
    mesh=ED.load_asset(DEST+'/'+name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Missing mesh '+name)
    assigned=[]
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        if key not in instances:key=str(slot.get_editor_property('material_slot_name'))
        if key not in instances:raise RuntimeError('Lost material '+name+':'+key)
        mesh.set_material(i,instances[key]);assigned.append(key)
    if set(assigned)!=set(r['materials']):raise RuntimeError('Material slot mismatch '+name)
    n=SME.get_nanite_settings(mesh);n.enabled=True;n.shape_preservation=unreal.NaniteShapePreservation.PRESERVE_AREA
    n.keep_percent_triangles=1;n.trim_relative_error=0;n.fallback_relative_error=.5;SME.set_nanite_settings(mesh,n,True)
    ED.save_loaded_asset(mesh,False)
    box=mesh.get_bounding_box();lo,hi=box.min,box.max;dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.2 or any(abs(a-b)>max(.5,b*.01) for a,b in zip(sorted(dims),sorted(r['dimensions_cm']))):raise RuntimeError('Mesh scale/pivot invalid '+name)
    uv=SME.get_num_uv_channels(mesh,0)
    if uv<1:raise RuntimeError('Missing UV '+name)
    report['meshes'][name]={'path':mesh.get_path_name(),'dimensions_cm':dims,'ground_z':lo.z,'materials':assigned,'source_triangles':r['triangles'],'uv_channels':uv,'nanite':True,'preserve_area':True}
ED.save_directory(DEST,False,True)
(ART/('meadow_import_report.json' if MEADOW_ONLY else 'import_report.json')).write_text(json.dumps(report,indent=2))
calibration=ROOT/'Tools/calibrate_meadow_materials.py'
exec(compile(calibration.read_text(),str(calibration),'exec'),{'__file__':str(calibration),'__name__':'__main__'})
if not MEADOW_ONLY:
    terrain_script=ROOT/'Tools/import_terrain_v04.py'
    exec(compile(terrain_script.read_text(),str(terrain_script),'exec'),{'__file__':str(terrain_script),'__name__':'__main__'})
unreal.log('SEIGE_ENVIRONMENT_V04_IMPORT_COMPLETE '+json.dumps(report))

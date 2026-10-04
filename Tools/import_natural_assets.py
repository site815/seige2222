"""Import prepared, local CC0 woodland assets into /Game/Art/Nature.

Run in Unreal 5.8 after prepare_natural_assets.py. No development download or
runtime network access occurs here. Foliage uses photographic alpha textures,
two-sided foliage shading and Nanite PreserveArea, with no leaf vertex tint.
"""
from pathlib import Path
import json
import unreal

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature';DEST='/Game/Art/Nature'
DATA=json.loads((ART/'Exports/nature_manifest.json').read_text(encoding='utf-8'))
SOURCES=json.loads((ART/'sources.json').read_text(encoding='utf-8'))
AT=unreal.AssetToolsHelpers.get_asset_tools();ED=unreal.EditorAssetLibrary;ME=unreal.MaterialEditingLibrary
SME=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
ED.make_directory(DEST);ED.make_directory(DEST+'/Textures')

def asset(name,kind,factory):
    path=DEST+'/'+name
    if ED.does_asset_exist(path):return ED.load_asset(path)
    result=AT.create_asset(name,DEST,kind,factory)
    if not result:raise RuntimeError('Failed creating '+path)
    return result

def node(mat,kind,**props):
    n=ME.create_material_expression(mat,kind,0,0)
    for key,value in props.items():n.set_editor_property(key,value)
    return n

def wire(source,target,pin,output=''):
    if not ME.connect_material_expressions(source,output,target,pin):raise RuntimeError('Material connection failed: '+pin)

def output(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Material output failed: '+str(prop))

def texture_name(source,group,role):return 'T_'+source+'_'+group+'_'+role

jobs=[];texture_records={}
for source,data in SOURCES['assets'].items():
    for group,maps in data['maps'].items():
        for role,record in maps.items():
            name=texture_name(source,group,role);texture_records[name]=(role,record)
            task=unreal.AssetImportTask();task.filename=str(ROOT/record['local_path']);task.destination_path=DEST+'/Textures';task.destination_name=name
            task.automated=True;task.replace_existing=True;task.save=True;jobs.append(task)
AT.import_asset_tasks(jobs)
TEXTURES={}
for name,(role,record) in texture_records.items():
    tex=ED.load_asset(DEST+'/Textures/'+name)
    if not isinstance(tex,unreal.Texture2D):raise RuntimeError('Missing texture '+name)
    tex.set_editor_property('srgb',role=='color')
    if role=='normal':
        tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
        # Source OpenGL normals use +Y; Unreal's tangent normal convention is -Y.
        tex.set_editor_property('flip_green_channel',True)
    else:tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT)
    if role=='alpha':
        tex.set_editor_property('do_scale_mips_for_alpha_coverage',True)
        tex.set_editor_property('alpha_coverage_thresholds',unreal.Vector4(.33,0,0,0))
    ED.save_loaded_asset(tex,only_if_is_dirty=False);TEXTURES[name]=tex

def sample(mat,param,role,source,group,mip_bias=None):
    sampler=(unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='normal' else
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='color' else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    result=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name=param,
        texture=TEXTURES[texture_name(source,group,role)],sampler_type=sampler)
    if mip_bias is not None:
        result.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_MIP_BIAS)
        result.set_editor_property('const_mip_value',mip_bias)
    return result

PARENTS={}
for foliage,name,default_source,default_group in ((False,'M_NatureSurface','rock_moss_set_01','surface'),(True,'M_NatureFoliage','fir_tree_01','twig')):
    m=asset(name,unreal.Material,unreal.MaterialFactoryNew());ME.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('used_with_nanite',True)
    m.set_editor_property('two_sided',foliage)
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED if foliage else unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE if foliage else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    color=sample(m,'BaseColor','color',default_source,default_group,mip_bias=-1 if foliage else None)
    normal=sample(m,'NormalMap','normal',default_source,default_group)
    rough=sample(m,'RoughnessMap','roughness',default_source,default_group)
    output(color,unreal.MaterialProperty.MP_BASE_COLOR,'RGB');output(normal,unreal.MaterialProperty.MP_NORMAL,'RGB');output(rough,unreal.MaterialProperty.MP_ROUGHNESS,'R')
    specular=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Specular',default_value=.22)
    output(specular,unreal.MaterialProperty.MP_SPECULAR)
    if foliage:
        m.set_editor_property('opacity_mask_clip_value',.33)
        alpha=sample(m,'OpacityMap','alpha',default_source,default_group,mip_bias=-2);output(alpha,unreal.MaterialProperty.MP_OPACITY_MASK,'R')
        strength=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Transmission',default_value=.32)
        subsurface=node(m,unreal.MaterialExpressionMultiply);wire(color,subsurface,'A','RGB');wire(strength,subsurface,'B')
        output(subsurface,unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        opacity=node(m,unreal.MaterialExpressionScalarParameter,parameter_name='Thickness',default_value=.65);output(opacity,unreal.MaterialProperty.MP_OPACITY)
    ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,only_if_is_dirty=False);PARENTS[foliage]=m

INSTANCES={}
role_params={'color':'BaseColor','normal':'NormalMap','roughness':'RoughnessMap','alpha':'OpacityMap'}
for slot,settings in DATA['materials'].items():
    instance=asset('MI_'+slot.removeprefix('PH_'),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    ME.set_material_instance_parent(instance,PARENTS[settings['foliage']])
    source=settings['source_asset'];group=settings['texture_group']
    for role in SOURCES['assets'][source]['maps'][group]:
        ME.set_material_instance_texture_parameter_value(instance,role_params[role],TEXTURES[texture_name(source,group,role)])
    ME.update_material_instance(instance);ED.save_loaded_asset(instance,only_if_is_dirty=False);INSTANCES[slot]=instance

tasks=[]
for name,record in DATA['meshes'].items():
    task=unreal.AssetImportTask();task.filename=str(ART/'Exports'/record['fbx']);task.destination_path=DEST;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
    options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
    options.automated_import_should_detect_type=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    sm=options.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=False;sm.generate_lightmap_u_vs=False
    sm.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
    sm.vertex_color_import_option=unreal.VertexColorImportOption.IGNORE
    task.options=options;tasks.append(task)
AT.import_asset_tasks(tasks)
REPORT={'provider':'Poly Haven','license':'CC0-1.0','meshes':{},'texture_count':len(TEXTURES),'material_instances':len(INSTANCES),
    'normal_conversion':'OpenGL green channel inverted on import','alpha_mips':'preserve red-channel coverage at 0.33',
    'foliage_texture_mip_bias':{'BaseColor':-1,'OpacityMap':-2,'NormalMap':0,'RoughnessMap':0},
    'runtime_network_required':False}
for name,record in DATA['meshes'].items():
    mesh=ED.load_asset(DEST+'/'+name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('Missing imported mesh '+name)
    slots=mesh.get_editor_property('static_materials');assigned=[]
    for index,slot in enumerate(slots):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        if key not in INSTANCES:key=str(slot.get_editor_property('material_slot_name'))
        if key not in INSTANCES:raise RuntimeError('Unrecognized material slot '+name+': '+key)
        mesh.set_material(index,INSTANCES[key]);assigned.append(key)
    expected=set(record['materials'])
    if set(assigned)!=expected:raise RuntimeError('Missing source material assignments '+name+str(assigned))
    nanite=SME.get_nanite_settings(mesh);nanite.set_editor_property('enabled',record['nanite'])
    if record['nanite']:
        nanite.set_editor_property('shape_preservation',unreal.NaniteShapePreservation.PRESERVE_AREA if name.startswith(('SM_Fir','SM_Broadleaf')) else unreal.NaniteShapePreservation.NONE)
        nanite.set_editor_property('keep_percent_triangles',1.0);nanite.set_editor_property('trim_relative_error',0.0)
        nanite.set_editor_property('fallback_relative_error',.5)
    SME.set_nanite_settings(mesh,nanite,True)
    ED.save_loaded_asset(mesh,only_if_is_dirty=False)
    box=mesh.get_bounding_box();lo,hi=box.min,box.max;dimensions=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.2:raise RuntimeError('Non-grounded mesh pivot '+name+': '+str(lo.z))
    if any(abs(a-b)>max(.5,b*.01) for a,b in zip(sorted(dimensions),sorted(record['dimensions_cm']))):
        raise RuntimeError('Incorrect metre/centimetre conversion '+name+str(dimensions))
    if SME.get_num_uv_channels(mesh,0)<1:raise RuntimeError('Mesh lost source UVs '+name)
    REPORT['meshes'][name]={'path':mesh.get_path_name(),'dimensions_cm':dimensions,'ground_z':lo.z,'material_slots':assigned,
        'source_triangles':record['triangles'],'nanite':record['nanite'],'shape_preservation':str(nanite.get_editor_property('shape_preservation')),
        'uv_channels':SME.get_num_uv_channels(mesh,0)}
ED.save_directory(DEST,only_if_is_dirty=False,recursive=True)
(ART/'import_report.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
unreal.log('SEIGE_NATURAL_ASSETS_IMPORTED '+json.dumps(REPORT))

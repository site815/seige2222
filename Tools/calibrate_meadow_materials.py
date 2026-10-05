"""Material-only meadow calibration; do not reimport any geometry or tree maps.

Called by the authoritative environment importer as well as the incremental
calibration commandlet. A separate grass parent keeps mature tree shading intact.
"""
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';DEST='/Game/Art/NatureV04'
ED=u.EditorAssetLibrary;ME=u.MaterialEditingLibrary;AT=u.AssetToolsHelpers.get_asset_tools()
derivative=ART/'Textures/grass_medium_02/grass_medium_02_diff_padded_2k.png'
if not derivative.exists():raise RuntimeError('Run pad_meadow_albedo.py in Blender first')
t=u.AssetImportTask();t.filename=str(derivative);t.destination_path=DEST+'/Textures';t.destination_name='T_grass_medium_02_surface_color_padded'
t.automated=True;t.replace_existing=True;t.save=True;AT.import_asset_tasks([t])
textures={}
for role in ('color','normal','roughness','ao','alpha'):
    texture=ED.load_asset(DEST+'/Textures/T_grass_medium_02_surface_'+role+('_padded' if role=='color' else ''))
    if not texture:raise RuntimeError('Missing grass texture '+role)
    texture.set_editor_property('never_stream',True)
    if role=='color':texture.set_editor_property('srgb',True)
    ED.save_loaded_asset(texture,False);textures[role]=texture
name='M_V04Grass';m=ED.load_asset(DEST+'/'+name) if ED.does_asset_exist(DEST+'/'+name) else AT.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
ME.delete_all_material_expressions(m)
m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('used_with_nanite',True)
m.set_editor_property('two_sided',True);m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
m.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE);m.set_editor_property('opacity_mask_clip_value',.33)
def node(kind,**props):
    n=ME.create_material_expression(m,kind)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def wire(a,b,pin,out=''):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError('Grass link '+pin)
def output(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Grass output '+str(prop))
def scalar(name,value):return node(u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)
def sample(param,role,bias=None):
    n=node(u.MaterialExpressionTextureSampleParameter2D,parameter_name=param,texture=textures[role],
        sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='normal' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='color' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    if bias is not None:n.set_editor_property('mip_value_mode',u.TextureMipValueMode.TMVM_MIP_BIAS);n.set_editor_property('const_mip_value',bias)
    return n
def multiply(a,b,ap='',bp=''):
    n=node(u.MaterialExpressionMultiply);wire(a,n,'A',ap);wire(b,n,'B',bp);return n
tint=node(u.MaterialExpressionVectorParameter,parameter_name='FoliageTint',default_value=u.LinearColor(.82,1.0,.70,1))
# Computed derivative-based color mips retain anisotropic filtering at grazing
# angles. Keep the existing alpha bias and coverage to preserve silhouettes.
albedo=multiply(sample('BaseColor','color'),tint,'RGB');output(albedo,u.MaterialProperty.MP_BASE_COLOR)
normal=sample('NormalMap','normal');output(normal,u.MaterialProperty.MP_NORMAL,'RGB')
output(sample('RoughnessMap','roughness'),u.MaterialProperty.MP_ROUGHNESS,'R')
output(sample('OpacityMap','alpha',-2),u.MaterialProperty.MP_OPACITY_MASK,'R')
output(multiply(albedo,scalar('Transmission',.75)),u.MaterialProperty.MP_SUBSURFACE_COLOR)
output(scalar('Thickness',.3),u.MaterialProperty.MP_OPACITY);output(scalar('Specular',.15),u.MaterialProperty.MP_SPECULAR)
ao=node(u.MaterialExpressionLinearInterpolate,const_a=1);wire(sample('OcclusionMap','ao'),ao,'B','R');wire(scalar('MicroOcclusion',.15),ao,'Alpha');output(ao,u.MaterialProperty.MP_AMBIENT_OCCLUSION)
ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,False)
mi=ED.load_asset(DEST+'/MI_grass_medium_02_surface')
if not mi:raise RuntimeError('Import meadow swards before their material calibration')
ME.set_material_instance_parent(mi,m)
for role,param in (('color','BaseColor'),('normal','NormalMap'),('roughness','RoughnessMap'),('alpha','OpacityMap'),('ao','OcclusionMap')):ME.set_material_instance_texture_parameter_value(mi,param,textures[role])
ME.set_material_instance_vector_parameter_value(mi,'FoliageTint',u.LinearColor(.82,1.0,.70,1))
for param,value in (('Transmission',.75),('Thickness',.3),('MicroOcclusion',.15)):ME.set_material_instance_scalar_parameter_value(mi,param,value)
ME.update_material_instance(mi);ED.save_loaded_asset(mi,False)
instances=[mi.get_path_name()]
# The dense low layer shares the tested thin-grass shader but keeps its own
# photographic maps. Padding is equally necessary for its narrow green blades.
low='grass_bermuda_01';low_file=ART/'Textures'/low/(low+'_diff_padded_2k.png')
if not low_file.exists():raise RuntimeError('Run pad_meadow_albedo.py for both grass atlases')
task=u.AssetImportTask();task.filename=str(low_file);task.destination_path=DEST+'/Textures';task.destination_name='T_'+low+'_surface_color_padded'
task.automated=True;task.replace_existing=True;task.save=True;AT.import_asset_tasks([task])
low_mi=ED.load_asset(DEST+'/MI_'+low+'_surface')
if not low_mi:raise RuntimeError('Missing low meadow material')
ME.set_material_instance_parent(low_mi,m)
for role,param in (('color','BaseColor'),('normal','NormalMap'),('roughness','RoughnessMap'),('alpha','OpacityMap'),('ao','OcclusionMap')):
    tex=ED.load_asset(DEST+'/Textures/T_'+low+'_surface_'+role+('_padded' if role=='color' else ''))
    if not tex:raise RuntimeError('Missing low meadow map '+role)
    tex.set_editor_property('never_stream',True);ED.save_loaded_asset(tex,False)
    ME.set_material_instance_texture_parameter_value(low_mi,param,tex)
ME.set_material_instance_vector_parameter_value(low_mi,'FoliageTint',u.LinearColor(.82,1,.70,1))
for param,value in (('Transmission',.75),('Thickness',.3),('MicroOcclusion',.15)):ME.set_material_instance_scalar_parameter_value(low_mi,param,value)
ME.update_material_instance(low_mi);ED.save_loaded_asset(low_mi,False);instances.append(low_mi.get_path_name())
report={'material':m.get_path_name(),'instance':mi.get_path_name(),'tint':[.82,1,.70],'transmission':.75,'thickness':.3,'micro_occlusion':.15,
    'mip_bias':{'OpacityMap':-2},'mip_mode':{'BaseColor':'computed_derivatives','OpacityMap':'bias'},'padded_colour':textures['color'].get_path_name(),'grass_maps_never_stream':True,
    'tree_material_unchanged':True,'emissive':False,'geometry_reimported':False,'instances':instances}
(ART/'meadow_calibration_report.json').write_text(json.dumps(report,indent=2))
u.log('SEIGE_MEADOW_CALIBRATION_COMPLETE '+json.dumps(report))

"""Rebuild the terrain material using local CC0 PBR surfaces. No downloads at import."""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[1]
ED=u.EditorAssetLibrary; ME=u.MaterialEditingLibrary; AT=u.AssetToolsHelpers.get_asset_tools()
DEST='/Game/Art/Textures'
tasks=[]
for suffix,name in [('diff','Color'),('nor_dx','Normal'),('rough','Roughness'),('ao','AO')]:
    t=u.AssetImportTask();t.filename=str(ROOT/'Art/Textures/PolyHaven'/f'leafy_grass_{suffix}_2k.jpg')
    t.destination_path=DEST;t.destination_name='T_Meadow_'+name;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
AT.import_asset_tasks(tasks)
for name in ['Color','Normal','Roughness','AO']:
    t=ED.load_asset(DEST+'/T_Meadow_'+name)
    if not t:raise RuntimeError('Missing ground map '+name)
    t.set_editor_property('srgb',name=='Color')
    t.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP if name=='Normal' else u.TextureCompressionSettings.TC_DEFAULT)
    ED.save_loaded_asset(t,False)
m=ED.load_asset('/Game/Art/M_Terrain')
ME.delete_all_material_expressions(m)
def node(kind,**props):
    n=ME.create_material_expression(m,kind)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def wire(a,b,pin,out=''):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError('Cannot connect '+pin)
def binary(kind,a,b,ap='',bp=''):
    n=node(kind);wire(a,n,'A',ap);wire(b,n,'B',bp);return n
def mul(a,b,ap='',bp=''):return binary(u.MaterialExpressionMultiply,a,b,ap,bp)
def add(a,b,ap='',bp=''):return binary(u.MaterialExpressionAdd,a,b,ap,bp)
def scalar(name,value):return node(u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)
def out(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Cannot connect output')
uv=node(u.MaterialExpressionTextureCoordinate)
detail=mul(uv,scalar('TextureScale',3.5))
secondary_uv=add(mul(detail,scalar('SecondaryTextureScale',1.371)),scalar('SecondaryTextureOffset',.417))
macro_uv=add(mul(detail,scalar('MacroTextureScale',.073)),scalar('MacroOffset',.371))
def sample(surface,channel,coords):
    texture=ED.load_asset(DEST+'/T_'+surface+'_'+channel)
    n=node(u.MaterialExpressionTextureSampleParameter2D,parameter_name=surface+channel+('Macro' if coords==macro_uv else ''),texture=texture)
    n.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='Normal' else (u.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='Color' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR))
    wire(coords,n,'UVs');return n
def detail_sample(surface,channel):
    # Break up the visible two-metre grid without changing the physical mesh.
    # Keeping both UV frames aligned also keeps their tangent-space normals valid.
    n=node(u.MaterialExpressionLinearInterpolate)
    wire(sample(surface,channel,detail),n,'A')
    wire(sample(surface,channel,secondary_uv),n,'B')
    wire(scalar('SecondaryTextureBlend',.45),n,'Alpha')
    return n
vc=node(u.MaterialExpressionVertexColor)
weight_sum=add(vc,vc,'R','G');sat=node(u.MaterialExpressionSaturate);wire(weight_sum,sat,'')
grass_weight=node(u.MaterialExpressionOneMinus);wire(sat,grass_weight,'')
def blend(channel):
    pin=''
    # Photographic grass in clearings, leaf litter beneath the woodland canopy.
    grassy=node(u.MaterialExpressionLinearInterpolate)
    wire(detail_sample('Grass',channel),grassy,'A',pin)
    wire(detail_sample('Meadow',channel),grassy,'B',pin)
    wire(vc,grassy,'Alpha','B')
    if channel=='Color':
        grassy=mul(grassy,node(u.MaterialExpressionVectorParameter,parameter_name='VegetationAlbedo',default_value=u.LinearColor(.68,.8,.63,1)))
    pin=''
    return add(add(mul(grassy,grass_weight,pin),mul(detail_sample('Dirt',channel),vc,pin,'R')),mul(detail_sample('Rock',channel),vc,pin,'G'))
base=blend('Color')
macro=sample('Meadow','Color',macro_uv)
desat=node(u.MaterialExpressionDesaturation);wire(macro,desat,'');wire(scalar('MacroDesaturation',1),desat,'Fraction')
variation=add(mul(desat,scalar('MacroStrength',.36)),scalar('MacroBase',.76))
out(mul(base,variation),u.MaterialProperty.MP_BASE_COLOR)
normal=node(u.MaterialExpressionNormalize);wire(blend('Normal'),normal,'');out(normal,u.MaterialProperty.MP_NORMAL)
out(blend('Roughness'),u.MaterialProperty.MP_ROUGHNESS)
ao=sample('Meadow','AO',detail);out(add(mul(ao,scalar('GroundOcclusionStrength',.3),'R'),scalar('GroundOcclusionBase',.7)),u.MaterialProperty.MP_AMBIENT_OCCLUSION)
ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,False)
(ROOT/'Art/ground_v03_import_report.json').write_text(json.dumps({'material':'/Game/Art/M_Terrain','texture_count':4,'source_manifest':'Textures/PolyHaven/ground_v03_sources.json'},indent=2),encoding='utf8')
u.log('SEIGE_GROUND_V03_IMPORT_SUCCESS')

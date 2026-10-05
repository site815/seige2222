"""Local v0.4 meadow/forest terrain, sampled in physical world centimetres.

Vertex R = soil/path, G = rock (priority), B = forest density. B=0 meadow,
B=1 forest litter. Terrain UVs are intentionally independent of render scale.
Two offset detail frequencies and broad noise reduce regular repeating tiles.
"""
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';DEST='/Game/Art/NatureV04'
ED=u.EditorAssetLibrary;ME=u.MaterialEditingLibrary;AT=u.AssetToolsHelpers.get_asset_tools()
path=DEST+'/M_TerrainV04'
m=ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset('M_TerrainV04',DEST,u.Material,u.MaterialFactoryNew())
ME.delete_all_material_expressions(m)
def node(kind,**props):
    n=ME.create_material_expression(m,kind)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def wire(a,b,pin,out=''):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError('Terrain link '+pin)
def binary(kind,a,b,ap='',bp=''):
    n=node(kind);wire(a,n,'A',ap);wire(b,n,'B',bp);return n
def mul(a,b,ap='',bp=''):return binary(u.MaterialExpressionMultiply,a,b,ap,bp)
def add(a,b,ap='',bp=''):return binary(u.MaterialExpressionAdd,a,b,ap,bp)
def sub(a,b,ap='',bp=''):return binary(u.MaterialExpressionSubtract,a,b,ap,bp)
def normalize(a):
    n=node(u.MaterialExpressionNormalize);wire(a,n,'');return n
def component(a,channel):
    n=node(u.MaterialExpressionComponentMask,r=channel=='R',g=channel=='G',b=channel=='B',a=False);wire(a,n,'');return n
def scalar(name,value):return node(u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)
def lerp(a,b,alpha,alpha_pin=''):
    n=node(u.MaterialExpressionLinearInterpolate);wire(a,n,'A');wire(b,n,'B');wire(alpha,n,'Alpha',alpha_pin);return n
def output(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Terrain output '+str(prop))
world=node(u.MaterialExpressionWorldPosition)
xy=node(u.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);wire(world,xy,'')
macro_pos=mul(world,scalar('VariationWorldScale',1/2400))
noise=node(u.MaterialExpressionNoise,scale=1,quality=1,levels=2,output_min=0,output_max=1);wire(macro_pos,noise,'')
# Most pixels favour one complete detail sample rather than averaging away the
# source contrast. The transition remains continuous across the non-repeating field.
detail_blend=node(u.MaterialExpressionSaturate);wire(add(mul(noise,scalar('DetailBlendRange',4)),scalar('DetailBlendFloor',-1.5)),detail_blend,'')
vc=node(u.MaterialExpressionVertexColor)
terrain_sources={'Meadow':('Grass004','surface',140),'EarthMeadow':(None,None,300),'Meso':(None,None,800),'Forest':('forest_leaves_02','surface',300),'Dirt':(None,None,300),'Rock':(None,None,250)}
role_names={'color':'Color','normal':'Normal','roughness':'Roughness','ao':'AO'}
sample_cache={};resident_textures=set()
def surface(name,role):
    key=(name,role)
    if key in sample_cache:return sample_cache[key]
    source,group,size=terrain_sources[name]
    path=(DEST+'/Textures/T_'+source+'_'+group+'_'+role if source else '/Game/Art/Textures/T_'+('Meadow' if name in ('EarthMeadow','Meso') else name)+'_'+role_names[role])
    texture=ED.load_asset(path)
    if not texture:raise RuntimeError('Missing terrain map '+path)
    # Absolute-world mapping does not match the procedural mesh UV density used
    # by streaming estimates. Keep these modest local maps resident, while still
    # allowing the renderer to choose correctly filtered mips at each pixel.
    if path not in resident_textures:
        texture.set_editor_property('never_stream',True);ED.save_loaded_asset(texture,False);resident_textures.add(path)
    uv=mul(xy,scalar(name+'WorldFrequency',1/size))
    secondary=add(mul(uv,scalar(name+'SecondaryScale',1.371)),node(u.MaterialExpressionConstant2Vector,r=.417,g=.193))
    samples=[]
    for i,coords in enumerate((uv,secondary)):
        n=node(u.MaterialExpressionTextureSampleParameter2D,parameter_name=name+role_names[role]+str(i),texture=texture,
            sampler_source=u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS,
            sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if role=='normal' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if role=='color' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        wire(coords,n,'UVs');samples.append(n)
    result=lerp(samples[0],samples[1],detail_blend)
    if role=='color' and name!='Meso':
        tint={'Meadow':(.64,.72,.58),'EarthMeadow':(.60,.70,.54),'Forest':(.65,.78,.60),'Dirt':(.65,.62,.57),'Rock':(.64,.66,.64)}[name]
        result=mul(result,node(u.MaterialExpressionVectorParameter,parameter_name=name+'Albedo',default_value=u.LinearColor(*tint,1)))
    sample_cache[key]=result;return result
soil_mask=node(u.MaterialExpressionSaturate);wire(mul(vc,scalar('SparseSoilStrength',2.8),'R'),soil_mask,'')
earth_meadow_weight=add(mul(soil_mask,scalar('EarthMeadowPatchRange',.40)),scalar('EarthMeadowFloor',.35))
def blend(role):
    grassy=lerp(surface('Meadow',role),surface('EarthMeadow',role),earth_meadow_weight)
    woodland=lerp(grassy,surface('Forest',role),vc,'B')
    path=lerp(woodland,surface('Dirt',role),soil_mask)
    return lerp(path,surface('Rock',role),vc,'G')
# The medium layer has an 8 m footprint: photographic tonal structure and normals
# remain readable after the 1.4 m close-detail layer reaches its filtered mips.
# No enlarged green/brown leaf colours are copied into the ground base colour.
meadow_mask=mul(sub(scalar('MesoCoverage',1),component(vc,'B')),sub(scalar('MesoRockExclusion',1),component(vc,'G')))
meso_gray=node(u.MaterialExpressionDesaturation);wire(surface('Meso','color'),meso_gray,'');wire(scalar('MesoDesaturation',1),meso_gray,'Fraction')
# Source analysis: linear mean .2499; at 64x64 effective mip resolution its
# standard deviation is .0265. The multiplier therefore restores useful medium
# contrast after filtering, without increasing the existing .86..1.14 limits.
meso_offset=mul(sub(meso_gray,scalar('MesoReferenceLuminance',.25)),scalar('MesoValueContrast',4))
meso_value=binary(u.MaterialExpressionMin,binary(u.MaterialExpressionMax,add(meso_offset,scalar('MesoValueBase',1)),scalar('MesoValueMinimum',.86)),scalar('MesoValueMaximum',1.14))
meso_value=lerp(scalar('MesoNeutralValue',1),meso_value,meadow_mask)
variation=add(mul(noise,scalar('MacroVariationRange',.08)),scalar('MacroVariationFloor',.96))
output(mul(mul(blend('color'),variation),meso_value),u.MaterialProperty.MP_BASE_COLOR)
# Reoriented normal mapping preserves both the fine and medium normal fields.
# t = n1 + (0,0,1); u = n2 * (-1,-1,1); r = t*dot(t,u)/t.z-u.
flat=node(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(0,0,1,1))
fine_normal=normalize(blend('normal'))
medium_normal=normalize(lerp(flat,normalize(surface('Meso','normal')),mul(meadow_mask,scalar('MesoNormalStrength',.4))))
t=add(fine_normal,flat);q=mul(medium_normal,node(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(-1,-1,1,1)))
dot=binary(u.MaterialExpressionDotProduct,t,q)
normal=normalize(sub(mul(t,binary(u.MaterialExpressionDivide,dot,component(t,'B'))),q));output(normal,u.MaterialProperty.MP_NORMAL)
rough=lerp(blend('roughness'),surface('Meso','roughness'),mul(meadow_mask,scalar('MesoRoughnessStrength',.25)))
output(component(rough,'R'),u.MaterialProperty.MP_ROUGHNESS)
# Photographic/procedural micro-occlusion remains modest; runtime foliage shadows
# and rolling terrain supply the large-scale light response.
ao=lerp(lerp(surface('Meadow','ao'),surface('EarthMeadow','ao'),earth_meadow_weight),surface('Forest','ao'),vc,'B');aomask=node(u.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);wire(ao,aomask,'')
output(add(mul(aomask,scalar('MicroOcclusion',.35)),scalar('OcclusionFloor',.65)),u.MaterialProperty.MP_AMBIENT_OCCLUSION)
output(scalar('GroundSpecular',.2),u.MaterialProperty.MP_SPECULAR)
ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,False)
report={'material':path,'vertex_channels':{'R':'soil/path','G':'rock','B':'forest density'},'physical_texture_size_cm':{'Meadow':140,'EarthMeadow':300,'Meso':800,'Forest':300,'Dirt':300,'Rock':250},
    'albedo_multipliers':{'Meadow':[.64,.72,.58],'EarthMeadow':[.60,.70,.54],'Forest':[.65,.78,.60],'Dirt':[.65,.62,.57],'Rock':[.64,.66,.64]},
    'soil_mask_strength':2.8,'photographic_meadow_blend':[.35,.75],'terrain_textures_never_stream':sorted(resident_textures),
    'medium_detail':{'source':'Poly Haven leafy_grass','size_cm':800,'base_color_use':'desaturated value modulation only, .86 to 1.14 multiplier','linear_luminance_center':.25,'filtered_contrast':4,'normal_strength':.4,'normal_combination':'reoriented normal mapping','roughness_blend':.25,'suppressed_on':'forest and rock','height_displacement':False},
    'coordinates':'world XY centimetres, two contrast-preserving detail frequencies, broad continuous noise','height_map_runtime_displacement':False}
(ART/'terrain_import_report.json').write_text(json.dumps(report,indent=2))
u.log('SEIGE_TERRAIN_V04_IMPORT_COMPLETE '+json.dumps(report))

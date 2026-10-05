"""Build a branched photographic terrain shader from the existing licensed maps.

Run in Unreal's Python commandlet. Only active material layers sample textures;
explicit gradients preserve filtering inside divergent shader branches. Geometry,
ground picking, blend weights and existing textures are not changed.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Art/NatureV07'
ED, ME = u.EditorAssetLibrary, u.MaterialEditingLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
ED.make_directory(DEST)
name = 'M_TerrainV07'
m = ED.load_asset(DEST+'/'+name) if ED.does_asset_exist(DEST+'/'+name) else AT.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
ME.delete_all_material_expressions(m)

def node(kind, **props):
    n = ME.create_material_expression(m,kind)
    for k,v in props.items(): n.set_editor_property(k,v)
    return n

def connect(a, b, pin, output=''):
    if not ME.connect_material_expressions(a,output,b,pin): raise RuntimeError('Link failed: '+pin)

def scalar(name, value):
    return node(u.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)

inputs = {'World':node(u.MaterialExpressionWorldPosition),
          'Weights':node(u.MaterialExpressionVertexColor),
          'Camera':node(u.MaterialExpressionCameraPositionWS)}
layers = {
    'Meadow':('/Game/Art/NatureV04/Textures/T_Grass004_surface_',140,(.70,.86,.67)),
    'EarthMeadow':('/Game/Art/Textures/T_Meadow_',300,(.70,.83,.68)),
    'Forest':('/Game/Art/NatureV04/Textures/T_forest_leaves_02_surface_',300,(.78,.90,.76)),
    'Dirt':('/Game/Art/Textures/T_Dirt_',300,(.88,.84,.76)),
    'Rock':('/Game/Art/Textures/T_Rock_',250,(.90,.93,.92)),
}
paths={}
for layer,(prefix,scale,tint) in layers.items():
    inputs[layer+'Scale']=scalar(layer+'WorldFrequency',1/scale)
    inputs[layer+'Tint']=node(u.MaterialExpressionVectorParameter,parameter_name=layer+'Albedo',default_value=u.LinearColor(*tint,1))
    for channel in ('Color','Normal','Roughness'):
        role=channel.lower() if layer in ('Meadow','Forest') else channel
        texture=ED.load_asset(prefix+role)
        if not texture: raise RuntimeError('Missing licensed texture: '+prefix+role)
        n=node(u.MaterialExpressionTextureObjectParameter,parameter_name=layer+channel,texture=texture,
               sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='Normal' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='Color' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        inputs[layer+channel]=n
        paths[layer+channel]=texture.get_path_name()
inputs['DetailDistance']=scalar('DetailDistanceCm',18000)
inputs['DetailBlendWidth']=scalar('DetailBlendWidthCm',18000)
inputs['SoilStrength']=scalar('SoilStrength',2.2)
inputs['SurfaceSaturation']=scalar('SurfaceSaturation',1.02)

# Branch weights are the same continuous vertex channels as the old material.
# A second offset color frequency hides repetition without sampling every PBR
# channel twice. Normal/roughness maps are skipped beyond the detail blend.
code='''
float2 dx=ddx(World.xy), dy=ddy(World.xy);
float detail=1-smoothstep(DetailDistance,DetailDistance+DetailBlendWidth,distance(World,Camera));
float dirt=saturate(Weights.r*SoilStrength), rock=saturate(Weights.g), forest=saturate(Weights.b);
float meadow=(1-dirt)*(1-forest)*(1-rock), earthMix=.45+.25*dirt;
float w[5]={meadow*(1-earthMix),meadow*earthMix,(1-dirt)*forest*(1-rock),dirt*(1-rock),rock};
float3 base=0, nsum=0; float rough=0;
'''
for i,(layer,(_,_,_)) in enumerate(layers.items()):
    code+=f'''
[branch] if(w[{i}]>.0001)
{{
    float2 uv=World.xy*{layer}Scale;
    float2 gx=dx*{layer}Scale, gy=dy*{layer}Scale;
    float3 a=Texture2DSampleGrad({layer}Color,{layer}ColorSampler,uv,gx,gy).rgb;
    float3 b=Texture2DSampleGrad({layer}Color,{layer}ColorSampler,uv*.371+float2(.417,.193),gx*.371,gy*.371).rgb;
    float3 c=lerp(a,b,.30)*{layer}Tint.rgb;
    float3 n=float3(0,0,1); float r=.88;
    [branch] if(detail>.001)
    {{
        float2 xy=Texture2DSampleGrad({layer}Normal,{layer}NormalSampler,uv,gx,gy).xy*2-1;
        n=normalize(lerp(float3(0,0,1),float3(xy,sqrt(saturate(1-dot(xy,xy)))),detail));
        r=lerp(.88,Texture2DSampleGrad({layer}Roughness,{layer}RoughnessSampler,uv,gx,gy).r,detail);
    }}
    base+=c*w[{i}]; nsum+=n*w[{i}]; rough+=r*w[{i}];
}}
'''
code+='''
float l=dot(base,float3(.2126,.7152,.0722));
base=max(0,lerp(l.xxx,base,SurfaceSaturation));
SurfaceNormal=normalize(nsum);
SurfaceRoughness=saturate(rough);
return base;
'''
custom=node(u.MaterialExpressionCustom,code=code,description='Only sample contributing landscape layers',output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
ci=[]
for key in inputs:
    p=u.CustomInput();p.set_editor_property('input_name',key);ci.append(p)
custom.set_editor_property('inputs',ci)
co=[]
for key,typ in [('SurfaceNormal',u.CustomMaterialOutputType.CMOT_FLOAT3),('SurfaceRoughness',u.CustomMaterialOutputType.CMOT_FLOAT1)]:
    p=u.CustomOutput();p.set_editor_property('output_name',key);p.set_editor_property('output_type',typ);co.append(p)
custom.set_editor_property('additional_outputs',co)
for key,value in inputs.items(): connect(value,custom,key)
for pin,prop in [('',u.MaterialProperty.MP_BASE_COLOR),('SurfaceNormal',u.MaterialProperty.MP_NORMAL),('SurfaceRoughness',u.MaterialProperty.MP_ROUGHNESS)]:
    if not ME.connect_material_property(custom,pin,prop): raise RuntimeError('Terrain output: '+pin)
ME.connect_material_property(scalar('MicroOcclusion',.92),'',u.MaterialProperty.MP_AMBIENT_OCCLUSION)
ME.connect_material_property(scalar('Specular',.2),'',u.MaterialProperty.MP_SPECULAR)
ME.layout_material_expressions(m)
errors=ME.recompile_material(m)
if errors: raise RuntimeError('Terrain compiler: '+str(errors))
if not ED.save_loaded_asset(m,False): raise RuntimeError('Failed to save terrain')
report={'material':m.get_path_name(),'texture_sources':paths,'opaque':True,'photographic_layers':list(layers),
        'samples_per_active_layer':{'near':4,'far':2},'normal_detail_blend_cm':[18000,36000],
        'spatial_branch_threshold':.0001,'albedo_multipliers':{k:v[2] for k,v in layers.items()},
        'procedural_noise_evaluations':0,'runtime_displacement':False,'geometry_changed':False,
        'notes':'Original v0.4 asset retained. Requires rendered review and timing; sample counts do not establish GPU gains.'}
out=ROOT/'Art/EnvironmentV07';out.mkdir(parents=True,exist_ok=True)
(out/'terrain_material_report.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('SEIGE_TERRAIN_V07_READY '+json.dumps(report))

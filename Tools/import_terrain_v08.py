"""Coordinate licensed ground surfaces and original far grass without geometry import.

Writes only NatureV08 assets. Existing v0.7 assets remain available for comparison.
The runtime terrain's vertex alpha supplies coherent meadow vigor; RGB retains
the existing soil/rock/woodland masks. No texture pixels, density or LOD distances
are changed by this material pass.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT/'Art/EnvironmentV08'
DEST = '/Game/Art/NatureV08'
ED, ME = u.EditorAssetLibrary, u.MaterialEditingLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
ED.make_directory(DEST)
palette = json.loads((ART/'surface_palette.json').read_text())
measured = json.loads((ART/'surface_measurements.json').read_text())


def material(name):
    path = DEST+'/'+name
    result = ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    if not result: raise RuntimeError('Cannot create '+path)
    ME.delete_all_material_expressions(result)
    return result


def node(m, kind, **props):
    result = ME.create_material_expression(m, kind)
    for key, value in props.items(): result.set_editor_property(key, value)
    return result


def connect(a, b, pin, output=''):
    if not ME.connect_material_expressions(a, output, b, pin):
        raise RuntimeError('Material link failed: '+pin)


def output(n, property, pin=''):
    if not ME.connect_material_property(n, pin, property):
        raise RuntimeError('Material output failed: '+str(property))


def scalar(m, name, value):
    return node(m, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


def color(m, name, rgb):
    return node(m, u.MaterialExpressionVectorParameter, parameter_name=name, default_value=u.LinearColor(*rgb, 1))


def custom(m, code, description, inputs, additional=()):
    result = node(m, u.MaterialExpressionCustom, code=code, description=description,
                  output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    definitions=[]
    for key in inputs:
        item=u.CustomInput(); item.set_editor_property('input_name', key); definitions.append(item)
    result.set_editor_property('inputs', definitions)
    outputs=[]
    for key, kind in additional:
        item=u.CustomOutput(); item.set_editor_property('output_name', key); item.set_editor_property('output_type', kind); outputs.append(item)
    result.set_editor_property('additional_outputs', outputs)
    for key, n in inputs.items(): connect(n, result, key, 'A' if key=='Vigor' else '')
    return result


def save(m):
    ME.layout_material_expressions(m)
    errors=ME.recompile_material(m)
    if errors: raise RuntimeError('Material compile failed: '+str(errors))
    if not ED.save_loaded_asset(m, False): raise RuntimeError('Material save failed')


terrain=material('M_TerrainV08')
inputs={'World':node(terrain,u.MaterialExpressionWorldPosition),
        'SurfaceUp':node(terrain,u.MaterialExpressionVertexNormalWS),
        'Weights':node(terrain,u.MaterialExpressionVertexColor),
        'Vigor':node(terrain,u.MaterialExpressionVertexColor),
        'Camera':node(terrain,u.MaterialExpressionCameraPositionWS)}
layers={
    'Meadow':('/Game/Art/NatureV04/Textures/T_Grass004_surface_',140),
    'EarthMeadow':('/Game/Art/Textures/T_Meadow_',300),
    'Forest':('/Game/Art/NatureV04/Textures/T_forest_leaves_02_surface_',300),
    'Dirt':('/Game/Art/Textures/T_Dirt_',300),
    'Rock':('/Game/Art/Textures/T_Rock_',250)}
paths={}
for layer,(prefix,scale) in layers.items():
    inputs[layer+'Scale']=scalar(terrain,layer+'WorldFrequency',1/scale)
    inputs[layer+'Mean']=color(terrain,layer+'MeasuredMean',measured[layer]['linear_mean'])
    inputs[layer+'Albedo']=color(terrain,layer+'Albedo',palette[layer])
    for channel in ('Color','Normal','Roughness'):
        suffix=channel.lower() if layer in ('Meadow','Forest') else channel
        texture=ED.load_asset(prefix+suffix)
        if not texture: raise RuntimeError('Missing retained licensed texture '+prefix+suffix)
        inputs[layer+channel]=node(terrain,u.MaterialExpressionTextureObjectParameter,
            parameter_name=layer+channel,texture=texture,
            sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=='Normal' else
                         u.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=='Color' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        paths[layer+channel]=texture.get_path_name()
inputs['DetailDistance']=scalar(terrain,'DetailDistanceCm',18000)
inputs['DetailBlendWidth']=scalar(terrain,'DetailBlendWidthCm',18000)
inputs['SoilStrength']=scalar(terrain,'SoilStrength',palette['SoilStrength'])
forest_fade_start=palette['ForestPatternFadeStartMeters']*100
forest_fade_end=palette['ForestPatternFadeEndMeters']*100
if not 0<=forest_fade_start<forest_fade_end:
    raise ValueError('Forest pattern fade must have increasing nonnegative distances')
inputs['ForestPatternFadeStart']=scalar(terrain,'ForestPatternFadeStartCm',forest_fade_start)
inputs['ForestPatternFadeWidth']=scalar(terrain,'ForestPatternFadeWidthCm',forest_fade_end-forest_fade_start)
code='''
float2 dx=ddx(World.xy), dy=ddy(World.xy);
float cameraDistance=distance(World,Camera);
float detail=1-smoothstep(DetailDistance,DetailDistance+DetailBlendWidth,cameraDistance);
float forestPatternFade=smoothstep(ForestPatternFadeStart,ForestPatternFadeStart+ForestPatternFadeWidth,cameraDistance);
float vigor=saturate(Vigor);
float dirt=saturate(Weights.r*SoilStrength), rock=saturate(Weights.g), forest=saturate(Weights.b);
float meadow=(1-dirt)*(1-forest)*(1-rock), earthMix=lerp(.42,.04,vigor);
float w[5]={meadow*(1-earthMix),meadow*earthMix,(1-dirt)*forest*(1-rock),dirt*(1-rock),rock};
float3 base=0, nsum=0; float rough=0;
'''
for i,layer in enumerate(layers):
    code+=f'''
[branch] if(w[{i}]>.0001)
{{
    float2 uv=World.xy*{layer}Scale;
    float2 gx=dx*{layer}Scale, gy=dy*{layer}Scale;
'''
    if layer=='Rock':
        code+='''
    // A top-down projection stretches into vertical ribbons on cliff faces.
    // Select the dominant physical surface plane without extra texture reads.
    float3 plane=abs(normalize(SurfaceUp));
    if(plane.x>plane.z && plane.x>=plane.y) uv=World.yz*RockScale;
    else if(plane.y>plane.z) uv=World.xz*RockScale;
    gx=ddx(uv); gy=ddy(uv);
    // Gently warp the rock plane across several texture repetitions. The same
    // coordinates drive colour/normal/roughness; explicit derivatives retain
    // ordinary mip filtering on the distorted face.
    uv+=float2(sin(uv.y*.61+sin(uv.x*.29)),cos(uv.x*.47+sin(uv.y*.37)))*.73;
    gx=ddx(uv); gy=ddy(uv);
'''
    if layer=='Forest':
        code+='''
    // The retained forest scan has a strong directional brightness band.
    // Keep its leaf/moss detail nearby; the distant surface uses a second
    // photographed ground sample instead of repeating that band or going flat.
    // Rotate both coordinates and gradients so mip filtering stays coherent.
    float2 uv2=float2(uv.x*.79863551-uv.y*.60181502,uv.x*.60181502+uv.y*.79863551)*.731+float2(.417,.193);
    float2 gx2=float2(gx.x*.79863551-gx.y*.60181502,gx.x*.60181502+gx.y*.79863551)*.731;
    float2 gy2=float2(gy.x*.79863551-gy.y*.60181502,gy.x*.60181502+gy.y*.79863551)*.731;
    float3 b=Texture2DSampleGrad(EarthMeadowColor,EarthMeadowColorSampler,uv2,gx2,gy2).rgb;
    float3 ratio=b/max(EarthMeadowMean.rgb,.001);
    [branch] if(forestPatternFade<.999)
    {
        float3 a=Texture2DSampleGrad(ForestColor,ForestColorSampler,uv,gx,gy).rgb;
        ratio=lerp(a/max(ForestMean.rgb,.001),ratio,lerp(.20,1,forestPatternFade));
    }
    ratio=clamp(ratio,.12,2.60);
    float surfaceDetail=detail*(1-forestPatternFade);
'''
    elif layer=='Rock':
        code+='''
    float2 uv2=float2(uv.x*.79863551-uv.y*.60181502,uv.x*.60181502+uv.y*.79863551)*.427+float2(.417,.193);
    float3 a=Texture2DSampleGrad(RockColor,RockColorSampler,uv,gx,gy).rgb;
    float3 b=Texture2DSampleGrad(RockColor,RockColorSampler,uv2,ddx(uv2),ddy(uv2)).rgb;
    // Equal rotated samples reduce the boulder's recognisable tile pattern.
    float3 ratio=clamp(lerp(a,b,.5)/max(RockMean.rgb,.001),.45,1.65);
    float surfaceDetail=detail;
'''
    else:
        code+=f'''
    float3 a=Texture2DSampleGrad({layer}Color,{layer}ColorSampler,uv,gx,gy).rgb;
    float3 b=Texture2DSampleGrad({layer}Color,{layer}ColorSampler,uv*.371+float2(.417,.193),gx*.371,gy*.371).rgb;
    // Retain photographed local detail while calibrating each surface to one
    // shared linear palette. This removes the old universal yellow soil wash.
    float3 ratio=clamp(lerp(a,b,.20)/max({layer}Mean.rgb,.001),.12,2.60);
    float surfaceDetail=detail;
'''
    code+=f'''
    float3 c=ratio*{layer}Albedo.rgb;
    float3 n=float3(0,0,1); float r=.88;
    [branch] if(surfaceDetail>.001)
    {{
        float2 xy=Texture2DSampleGrad({layer}Normal,{layer}NormalSampler,uv,gx,gy).xy*2-1;
        n=normalize(lerp(float3(0,0,1),float3(xy,sqrt(saturate(1-dot(xy,xy)))),surfaceDetail));
        r=lerp(.88,Texture2DSampleGrad({layer}Roughness,{layer}RoughnessSampler,uv,gx,gy).r,surfaceDetail);
    }}
    base+=c*w[{i}]; nsum+=n*w[{i}]; rough+=r*w[{i}];
}}
'''
code+='''
// One broad photographic modulation survives distant mips and breaks up
// uniform fields without adding geometry, alpha tests or repeated grass tiles.
float2 macroUV=World.xy/2200;
float3 macro=Texture2DSampleGrad(MeadowColor,MeadowColorSampler,macroUV,dx/2200,dy/2200).rgb;
float macroValue=dot(macro,float3(.2126,.7152,.0722))/max(dot(MeadowMean.rgb,float3(.2126,.7152,.0722)),.001);
base*=lerp(1,clamp(macroValue,.55,1.45)*lerp(.70,1.10,vigor),meadow);
SurfaceNormal=normalize(nsum);
SurfaceRoughness=saturate(rough);
return base;
'''
surface=custom(terrain,code,'Calibrated photographic layers; coherent meadow vigor',inputs,
    [('SurfaceNormal',u.CustomMaterialOutputType.CMOT_FLOAT3),('SurfaceRoughness',u.CustomMaterialOutputType.CMOT_FLOAT1)])
for pin,prop in [('',u.MaterialProperty.MP_BASE_COLOR),('SurfaceNormal',u.MaterialProperty.MP_NORMAL),('SurfaceRoughness',u.MaterialProperty.MP_ROUGHNESS)]: output(surface,prop,pin)
output(scalar(terrain,'MicroOcclusion',.96),u.MaterialProperty.MP_AMBIENT_OCCLUSION)
output(scalar(terrain,'Specular',.18),u.MaterialProperty.MP_SPECULAR)
save(terrain)

# Preserve the existing geometric sward, but give distant blades a coherent
# terrain-facing lighting normal. Instance space is essential: Local/Object
# orientation only sees the ISM component, not each terrain-aligned transform.
grass=material('M_GrassProxyV08')
for prop,value in [('used_with_instanced_static_meshes',True),('used_with_nanite',True),
                   ('two_sided',True),('tangent_space_normal',False)]: grass.set_editor_property(prop,value)
grass.set_editor_property('blend_mode',u.BlendMode.BLEND_OPAQUE)
grass.set_editor_property('shading_model',u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
albedo=color(grass,'MeanAlbedo',palette['GrassProxy'])
grass_texture=ED.load_asset(layers['Meadow'][0]+'color')
grass_color=custom(grass,'''
float2 uv=World.xy/140;
float3 sample=Texture2DSample(GrassColor,GrassColorSampler,uv).rgb;
float3 variation=clamp(sample/max(MeasuredMean,.001),.75,1.20);
return MeanAlbedo*variation;
''','World-aligned photographic color variation on retained far grass',
    {'World':node(grass,u.MaterialExpressionWorldPosition),
     'GrassColor':node(grass,u.MaterialExpressionTextureObjectParameter,parameter_name='GrassColor',texture=grass_texture,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR),
     'MeasuredMean':color(grass,'MeasuredMean',measured['Meadow']['linear_mean']),
     'MeanAlbedo':albedo})
output(grass_color,u.MaterialProperty.MP_BASE_COLOR)
output(grass_color,u.MaterialProperty.MP_SUBSURFACE_COLOR)
up=node(grass,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(0,0,1,1))
instance_up=node(grass,u.MaterialExpressionTransform,
    transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
    transform_type=u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
connect(up,instance_up,'')
normal=custom(grass,'return normalize(lerp(normalize(BladeNormal),normalize(GroundNormal),Blend));',
    'Terrain-aligned far sward lighting without changing geometry',
    {'BladeNormal':node(grass,u.MaterialExpressionVertexNormalWS),'GroundNormal':instance_up,
     'Blend':scalar(grass,'GroundNormalBlend',palette['GrassNormalBlend'])})
output(normal,u.MaterialProperty.MP_NORMAL)
for key,value,prop in [('Roughness',.9,u.MaterialProperty.MP_ROUGHNESS),('Specular',.1,u.MaterialProperty.MP_SPECULAR),
                       ('LeafThickness',.5,u.MaterialProperty.MP_OPACITY),('MicroOcclusion',1,u.MaterialProperty.MP_AMBIENT_OCCLUSION)]:
    output(scalar(grass,key,value),prop)
save(grass)
mi_path=DEST+'/MI_GrassProxyV08'
mi=ED.load_asset(mi_path) if ED.does_asset_exist(mi_path) else AT.create_asset('MI_GrassProxyV08',DEST,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
ME.set_material_instance_parent(mi,grass)
ME.set_material_instance_vector_parameter_value(mi,'MeanAlbedo',u.LinearColor(*palette['GrassProxy'],1))
ME.set_material_instance_scalar_parameter_value(mi,'GroundNormalBlend',palette['GrassNormalBlend'])
ME.update_material_instance(mi);ED.save_loaded_asset(mi,False)
source='/Game/Art/NatureV07/SM_GrassProxy'
mesh_path=DEST+'/SM_GrassProxyV08'
mesh=ED.load_asset(mesh_path) if ED.does_asset_exist(mesh_path) else ED.duplicate_asset(source,mesh_path)
if not isinstance(mesh,u.StaticMesh): raise RuntimeError('Cannot duplicate retained grass geometry')
for i in range(len(mesh.get_editor_property('static_materials'))): mesh.set_material(i,mi)
if not ED.save_loaded_asset(mesh,False): raise RuntimeError('Cannot save calibrated grass proxy')
report={'terrain_material':terrain.get_path_name(),'grass_material':grass.get_path_name(),
        'grass_proxy':mesh.get_path_name(),'retained_geometry_source':source,'source_triangles':1280,
        'palette':palette,'texture_sources':paths,'samples_per_active_layer':{'near':4,'far':2},'shared_macro_samples':1,
        'forest_pattern_control':{'near_source':'forest_leaves_02','far_source':'leafy_grass',
            'far_palette':'Forest','far_sampling':'rotated world coordinates with matching explicit gradients',
            'fade_start_m':forest_fade_start/100,'fade_end_m':forest_fade_end/100,
            'near_max_samples':4,'far_max_samples':1,'flat_color_fill':False,
            'source_analysis':'Art/EnvironmentV08/forest_pattern_analysis.json'},
        'ground_vertex_channels':{'r':'soil','g':'rock','b':'woodland','a':'coherent meadow vigor'},
        'grass_normal_space':'instance-up transformed to world, blended with blade normal',
        'geometry_or_density_changed':False,'opacity_masks_added':False,'dither':False,
        'prior_assets_preserved':True,'rendered_review_pending':True,'performance_verified':False}
(ART/'surface_import_report.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('SEIGE_TERRAIN_V08_READY '+json.dumps(report))

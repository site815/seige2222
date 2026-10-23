"""Original procedural water material; no downloaded artwork or texture copies.

Run in an allocated Unreal editor Python commandlet. Runtime has an opaque
fallback if this optional presentation material is unavailable.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Art/EnvironmentV09'
NAME = 'M_WaterV09'
ED, ME = u.EditorAssetLibrary, u.MaterialEditingLibrary
ED.make_directory(DEST)
path = DEST+'/'+NAME
m = ED.load_asset(path) if ED.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DEST, u.Material, u.MaterialFactoryNew())
assert m, path
ME.delete_all_material_expressions(m)
m.set_editor_property('two_sided', True)
m.set_editor_property('tangent_space_normal', False)

def node(kind, **props):
    result = ME.create_material_expression(m, kind)
    for key, value in props.items(): result.set_editor_property(key, value)
    return result

def output(n, target):
    assert ME.connect_material_property(n, '', target), target

def scalar(name, value):
    return node(u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)

color = node(u.MaterialExpressionVectorParameter, parameter_name='WaterColor', default_value=u.LinearColor(.025, .075, .085, 1))
shallow_color = node(u.MaterialExpressionVectorParameter, parameter_name='ShallowColor', default_value=u.LinearColor(.055, .090, .082, 1))
vertex = node(u.MaterialExpressionVertexColor)
world = node(u.MaterialExpressionWorldPosition)
depth_tint = node(u.MaterialExpressionCustom, description='Continuous physical shoal tint without separate rectangular faces',
    code='''float2 p=World.xy;
float grain=.5+.25*sin(p.x*.063+p.y*.029)*sin(p.y*.071-p.x*.017)+.25*sin(p.x*.013-p.y*.023);
return lerp(Deep.rgb,Shoal.rgb*(.88+.12*grain),saturate(Shallow));''',
    output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
depth_inputs=[]
for name in ('World','Deep','Shoal','Shallow'):
    item=u.CustomInput(); item.set_editor_property('input_name',name); depth_inputs.append(item)
depth_tint.set_editor_property('inputs',depth_inputs)
for source,source_pin,target in ((world,'','World'),(color,'','Deep'),(shallow_color,'','Shoal'),(vertex,'G','Shallow')):
    assert ME.connect_material_expressions(source,source_pin,depth_tint,target)
output(depth_tint, u.MaterialProperty.MP_BASE_COLOR)
output(scalar('Roughness', .48), u.MaterialProperty.MP_ROUGHNESS)
output(scalar('Specular', .12), u.MaterialProperty.MP_SPECULAR)
time = node(u.MaterialExpressionTime)
wave = node(u.MaterialExpressionCustom, description='Irregular broad wind ripples with pixel-footprint filtering',
    code='''float2 p=World.xy;
float warp=3.15*sin(dot(p,float2(.00031,.00017)))+2.8*sin(dot(p,float2(-.00013,.00041)));
float a=dot(p,float2(.0031,.0013))+warp+Time*.35;
float b=dot(p,float2(-.0008,.0047))+warp*.71-Time*.23;
float c=dot(p,float2(.0063,-.0021))-warp*.43+Time*.51;
float3 phase=float3(a,b,c);
float3 filter=exp(-2.0*(abs(ddx(phase))+abs(ddy(phase))));
float3 waves=sin(phase)*filter;
return normalize(float3(.009*waves.x+.004*waves.y+.003*waves.z,
                       .004*waves.x-.008*waves.y+.005*waves.z,1));''',
    output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ('World', 'Time'):
    item=u.CustomInput(); item.set_editor_property('input_name', name); inputs.append(item)
wave.set_editor_property('inputs', inputs)
assert ME.connect_material_expressions(world, '', wave, 'World')
assert ME.connect_material_expressions(time, '', wave, 'Time')
output(wave, u.MaterialProperty.MP_NORMAL)
ME.layout_material_expressions(m)
assert not ME.recompile_material(m)
assert ED.save_loaded_asset(m, False)
geology={
    'iron_ore':(.72,.40,.24), 'copper_ore':(.60,.49,.35),
    'silica':(1.12,1.04,.88), 'carbon':(.22,.30,.16),
    'water':(.25,.37,.39), 'hydrocarbons':(.14,.13,.12),
    'radioactive_ore':(.26,.31,.18), 'crystalline':(1.08,1.14,1.17),
}
directory=ROOT/'Art/EnvironmentV09';directory.mkdir(parents=True, exist_ok=True)
previous_report=directory/'water_import_report.json'
water_only='-wateronly' in u.SystemLibrary.get_command_line().lower()
geology_paths=json.loads(previous_report.read_text(encoding='utf-8')).get('deposit_materials',{}) if water_only and previous_report.exists() else {}
for resource,tint in ({} if water_only else geology).items():
    name='M_Deposit_'+resource; path=DEST+'/'+name
    m=ED.load_asset(path) if ED.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
    assert m; ME.delete_all_material_expressions(m)
    ME.set_material_usage(m, u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    ME.set_material_usage(m, u.MaterialUsage.MATUSAGE_NANITE)
    texture=ED.load_asset('/Game/Art/Textures/T_Rock_Color'); assert texture
    sample=node(u.MaterialExpressionTextureSample,texture=texture,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    shade=node(u.MaterialExpressionVectorParameter,parameter_name='MineralTint',default_value=u.LinearColor(*tint,1))
    multiply=node(u.MaterialExpressionMultiply)
    assert ME.connect_material_expressions(sample,'RGB',multiply,'A')
    assert ME.connect_material_expressions(shade,'',multiply,'B')
    output(multiply,u.MaterialProperty.MP_BASE_COLOR)
    output(scalar('Roughness',.24 if resource=='water' else .64),u.MaterialProperty.MP_ROUGHNESS)
    normal=ED.load_asset('/Game/Art/Textures/T_Rock_Normal'); assert normal
    normal_sample=node(u.MaterialExpressionTextureSample,texture=normal,sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    output(normal_sample,u.MaterialProperty.MP_NORMAL)
    ME.layout_material_expressions(m); assert not ME.recompile_material(m); assert ED.save_loaded_asset(m,False)
    geology_paths[resource]=m.get_path_name()
report={'asset':DEST+'/'+NAME+'.'+NAME, 'original_material':True, 'opaque':True, 'world_space_ripple_normals':True,'filtered_irregular_ripples':True,'continuous_shallow_vertex_tint':True,'roughness':.48,'specular':.12,'deposit_materials':geology_paths,
        'notes':'Surface geometry comes from shared physical river/lake profile. No refraction or wave displacement; ice simulation is not implemented.'}
(directory/'water_import_report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
u.log('V09_WATER_IMPORT_COMPLETE '+report['asset'])

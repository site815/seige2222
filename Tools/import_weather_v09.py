"""Add shared seasonal snow to retained project scenery; no source photographs copied.

Run with UnrealEditor-Cmd Python only under the parent's engine lease. This edits
the currently referenced game material masters in place, retaining their photo
textures, roughness, masking, normal maps and asset licensing.
"""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[1]
DEST='/Game/Art/WeatherV09'
ED,ME=u.EditorAssetLibrary,u.MaterialEditingLibrary
AT=u.AssetToolsHelpers.get_asset_tools()
ED.make_directory(DEST)
weather=json.loads((ROOT/'Graphics/weather.json').read_text())
scene=json.loads((ROOT/'Graphics/scene.json').read_text())
ambient_source='/Engine/MapTemplates/Sky/DaylightAmbientCubemap'
u.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Engine/MapTemplates/Sky'], True)
assert ED.load_asset(ambient_source), ambient_source
ambient_path=weather['ambient_cubemap'].split('.')[0]
ambient=ED.load_asset(ambient_path) if ED.does_asset_exist(ambient_path) else ED.duplicate_asset(ambient_source,ambient_path)
if not isinstance(ambient,u.TextureCube): raise RuntimeError('Weather requires the authored daylight texture cube')
if not ED.save_loaded_asset(ambient,False): raise RuntimeError('Could not save weather ambient cubemap')

def asset(name,kind,factory):
    path=DEST+'/'+name
    result=ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name,DEST,kind,factory)
    if not result: raise RuntimeError('Could not create '+path)
    return result

collection=asset('MPC_Weather',u.MaterialParameterCollection,u.MaterialParameterCollectionFactoryNew())
parameters=list(collection.get_editor_property('scalar_parameters'))
if not any(str(p.get_editor_property('parameter_name'))=='SnowCoverage' for p in parameters):
    p=u.CollectionScalarParameter()
    p.set_editor_property('parameter_name','SnowCoverage')
    p.set_editor_property('default_value',0)
    parameters.append(p)
    collection.set_editor_property('scalar_parameters',parameters)
ED.save_loaded_asset(collection,False)

def node(m,kind,**values):
    n=ME.create_material_expression(m,kind)
    for key,value in values.items(): n.set_editor_property(key,value)
    return n

def link(a,b,pin,output=''):
    if not ME.connect_material_expressions(a,output,b,pin): raise RuntimeError('Cannot link '+pin)

def master(m):
    while m and not isinstance(m,u.Material): m=m.get_editor_property('parent')
    return m

masters={}
terrain=ED.load_asset(scene['terrain_material']) if 'terrain_material' in scene else None
if not terrain:
    terrain=ED.load_asset('/Game/Art/NatureV08/M_TerrainV08')
if terrain: masters[terrain.get_path_name()]=(terrain,False)
for key,path in list(scene['nature_assets'].items())+[(k,scene[k]) for k in ['grass_proxy_asset','broadleaf_proxy_asset','conifer_proxy_asset'] if k in scene]:
    mesh=ED.load_asset(path)
    if not isinstance(mesh,u.StaticMesh): continue
    vegetation='grass' in key.lower() or 'tree' in key.lower() or 'proxy' in key.lower() or key.lower() in ['oaka','oakb','fira','firb','wildflowers']
    for slot in mesh.get_editor_property('static_materials'):
        m=master(slot.get_editor_property('material_interface'))
        if m and m.get_path_name().startswith('/Game/'):
            previous=masters.get(m.get_path_name())
            masters[m.get_path_name()]=(m,vegetation or (previous[1] if previous else False))

reports=[]
for path,(m,vegetation) in sorted(masters.items()):
    marker='Seige seasonal snow v0.9'
    expressions=ME.get_material_expressions(m)
    if any(str(n.get_editor_property('desc'))==marker for n in expressions):
        reports.append({'material':path,'status':'already wrapped'})
        continue
    base=ME.get_material_property_input_node(m,u.MaterialProperty.MP_BASE_COLOR)
    base_pin=ME.get_material_property_input_node_output_name(m,u.MaterialProperty.MP_BASE_COLOR)
    if not base: raise RuntimeError('Material has no retained base colour: '+path)
    coverage=node(m,u.MaterialExpressionCollectionParameter,collection=collection,parameter_name='SnowCoverage')
    normal=node(m,u.MaterialExpressionVertexNormalWS)
    snow=node(m,u.MaterialExpressionCustom,description=marker,
              code='float mask=saturate(Coverage)*'+('0.72' if vegetation else 'smoothstep(0.25,0.85,Normal.z)')+'; float detail=clamp(dot(Base,float3(.2126,.7152,.0722))*1.8+.72,.72,1.0); return lerp(Base,float3(.78,.84,.90)*detail,mask);',
              output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    snow.set_editor_property('desc',marker)
    inputs=[]
    for key in ['Base','Coverage','Normal']:
        entry=u.CustomInput();entry.set_editor_property('input_name',key);inputs.append(entry)
    snow.set_editor_property('inputs',inputs)
    link(base,snow,'Base',base_pin);link(coverage,snow,'Coverage');link(normal,snow,'Normal')
    if not ME.connect_material_property(snow,'',u.MaterialProperty.MP_BASE_COLOR):raise RuntimeError('Cannot install snow colour')
    # Mask/normal/roughness stay authored; snow never adds opacity or dither.
    ME.layout_material_expressions(m)
    errors=ME.recompile_material(m)
    if errors: raise RuntimeError('Snow material compile failed: '+str(errors))
    if not ED.save_loaded_asset(m,False): raise RuntimeError('Could not save '+path)
    reports.append({'material':path,'status':'wrapped','vegetation':vegetation})

flake=asset('M_Snowflake',u.Material,u.MaterialFactoryNew())
ME.delete_all_material_expressions(flake)
flake.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
ME.set_material_usage(flake,u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
colour=node(flake,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.45,.5,.56,1))
ME.connect_material_property(colour,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
ME.recompile_material(flake);ED.save_loaded_asset(flake,False)
out=ROOT/'Art/WeatherV09';out.mkdir(parents=True,exist_ok=True)
(out/'import_report.json').write_text(json.dumps({'collection':collection.get_path_name(),'snowflake_material':flake.get_path_name(),'ambient_cubemap':ambient.get_path_name(),'ambient_source':ambient_source,'materials':reports,'license':'Original shader wrapper; retained photographic texture licenses unchanged. Ambient cubemap is Epic Unreal Engine content copied into the cooked game art directory.','render_review_pending':True},indent=2)+'\n')
print('SEIGE_WEATHER_V09_OK '+str(len(reports))+' material masters')

"""Install calendar-driven GPU foliage wind without replacing scenery art.

Run under the parent engine lease with -AllowCommandletRendering -RenderOffscreen
(not NullRHI): the report includes the actual D3D material compile diagnostics.
Preserves color/normal/roughness/opacity and existing seasonal snow connections.
"""
from pathlib import Path
import json,hashlib
import unreal as u

ROOT=Path(__file__).resolve().parents[1]
ED,ME=u.EditorAssetLibrary,u.MaterialEditingLibrary
weather=json.loads((ROOT/'Graphics/weather.json').read_text())
scene=json.loads((ROOT/'Graphics/scene.json').read_text())
collection=ED.load_asset(weather['snow_collection'])
if not isinstance(collection,u.MaterialParameterCollection): raise RuntimeError('Missing existing weather collection')
defaults={
 'WindPhase':0.,'WindPreviousPhase':0.,
 'WindTreeSwayCm':weather['tree_wind_sway_centimeters'],
 'WindLeafRustleCm':weather['leaf_wind_rustle_centimeters'],
 'WindGrassSwayCm':weather['grass_wind_sway_centimeters'],
 'WindTreeDistanceCm':weather['tree_wind_distance_meters']*100.,
 'WindGrassDistanceCm':scene['grass_wind_distance_m']*100.,
 'WindFadeStartFraction':weather['wind_fade_start_fraction']}
parameters=list(collection.get_editor_property('scalar_parameters'))
for name,value in defaults.items():
 p=next((p for p in parameters if str(p.get_editor_property('parameter_name'))==name),None)
 if p is None:
  p=u.CollectionScalarParameter();p.set_editor_property('parameter_name',name);parameters.append(p)
 p.set_editor_property('default_value',value)
collection.set_editor_property('scalar_parameters',parameters)
if not ED.save_loaded_asset(collection,False): raise RuntimeError('Cannot save weather wind parameters')

marker='Seige calendar wind v0.10.6'
targets={
 '/Game/Art/NatureV091/M_GrassCardV091':'grass_cards',
 '/Game/Art/NatureV04/M_V04Foliage':'tree',
 '/Game/Art/Nature/M_NatureFoliage':'tree',
 '/Game/Art/NatureV08/M_BroadleafSpraysV08':'tree',
 '/Game/Art/NatureV08/M_ConiferSpraysV08':'tree',
 '/Game/Art/NatureV04/M_V04Wildflowers':'grass',
 '/Game/Art/NatureV08/M_GrassProxyV08':'grass'}
report=[]
for path,kind in targets.items():
 m=ED.load_asset(path)
 if not isinstance(m,u.Material):raise RuntimeError('Missing foliage master: '+path)
 protected={str(prop):(ME.get_material_property_input_node(m,prop),str(ME.get_material_property_input_node_output_name(m,prop)))
            for prop in (u.MaterialProperty.MP_BASE_COLOR,u.MaterialProperty.MP_NORMAL,u.MaterialProperty.MP_OPACITY_MASK,u.MaterialProperty.MP_ROUGHNESS)}
 for expression in list(ME.get_material_expressions(m)):
  if str(expression.get_editor_property('desc')).startswith('Seige calendar wind v0.10.'):ME.delete_material_expression(m,expression)
 def node(kind,**values):
  n=ME.create_material_expression(m,kind)
  for key,value in values.items():n.set_editor_property(key,value)
  n.set_editor_property('desc',marker)
  return n
 def connect(a,b,pin,output=''):
  if not ME.connect_material_expressions(a,output,b,pin):raise RuntimeError('Wind link failed: '+path+' '+pin)
 def parameter(name):return node(u.MaterialExpressionCollectionParameter,collection=collection,parameter_name=name)
 phase=node(u.MaterialExpressionPreviousFrameSwitch)
 connect(parameter('WindPhase'),phase,'Current Frame')
 connect(parameter('WindPreviousPhase'),phase,'Previous Frame')
 world=node(u.MaterialExpressionWorldPosition,world_position_shader_offset=u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
 local=node(u.MaterialExpressionTransformPosition,
            transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
            transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE)
 connect(world,local,'')
 inputs={'WorldPos':(world,''),'LocalPos':(local,''),'CameraPos':(node(u.MaterialExpressionCameraPositionWS),''),
         'Phase':(phase,''),'Amplitude':(parameter('WindTreeSwayCm' if kind=='tree' else 'WindGrassSwayCm'),''),
         'Rustle':(parameter('WindLeafRustleCm'),''),'Distance':(parameter('WindTreeDistanceCm' if kind=='tree' else 'WindGrassDistanceCm'),''),
         'FadeStart':(parameter('WindFadeStartFraction'),'')}
 height='smoothstep(0.0, 65.0, LocalPos.z)'
 if kind=='tree':
  # The ISM implementation returns StaticMeshBounds here, not the bounding box
  # of the entire forest batch. Matched source/proxy bounds give the same bend.
  bounds=node(u.MaterialExpressionPreSkinnedLocalBounds)
  inputs['BoundsMin']=(bounds,'Min')
  inputs['BoundsSize']=(bounds,'Extents')
  origin=node(u.MaterialExpressionTransformPosition,
              transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE,
              transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
  connect(node(u.MaterialExpressionConstant3Vector,constant=u.LinearColor(0,0,0,0)),origin,'')
  inputs['RootWorld']=(origin,'')
 if kind=='grass_cards':
  inputs['Vertex']=(node(u.MaterialExpressionVertexColor),'')
  height='saturate(Vertex.r) * saturate(Vertex.r)'
 code='''
// World-position waves agree between detailed trees and canopy proxies. Ground
// anchors and bark never move. All temporal terms use integer phase harmonics.
float fade = 1.0 - smoothstep(max(Distance * FadeStart, 0.01), max(Distance, 0.02), length(WorldPos - CameraPos));
float anchor = HEIGHT;
float broadPhase = dot(WorldPos.xy, float2(.00071, .00043));
float finePhase = dot(WorldPos.xyz, float3(.0121, .0097, .0163));
float sway = (sin(Phase + broadPhase) + .2 * sin(Phase * 2.0 + broadPhase * 2.0)) * Amplitude;
float rustle = sin(Phase * 3.0 + finePhase) * RUSTLE;
return float3(sway * .8 + rustle * .6, sway * .6 - rustle * .8, rustle * .18) * anchor * fade;
'''.replace('HEIGHT',height).replace('RUSTLE','Rustle' if kind=='tree' else 'Amplitude * .25')
 if kind=='tree':
  code='''
// Broad crown bend persists at all visible distances, including opaque canopy
// proxies. Only small leaf flutter fades out. Roots/bark remain rigid; height
// weighting prevents the old nearly uniform translation of the entire canopy.
float detailFade = 1.0 - smoothstep(max(Distance * FadeStart, 0.01), max(Distance, 0.02), length(WorldPos - CameraPos));
float height = saturate((LocalPos.z - BoundsMin.z) / max(BoundsSize.z, 1.0));
float bend = smoothstep(.08, 1.0, height);
bend *= bend;
float rootPhase = dot(RootWorld.xy, float2(.00071, .00043));
float branchPhase = dot(LocalPos.xy / max(BoundsSize.xy, float2(1,1)), float2(3.7,2.3));
float broad = (sin(Phase + rootPhase) + .18 * sin(Phase * 2.0 + rootPhase * 1.7)) * Amplitude;
float branch = sin(Phase * 2.0 + rootPhase + branchPhase) * Amplitude * .12;
float finePhase = dot(WorldPos.xyz, float3(.0121, .0097, .0163));
float flutter = sin(Phase * 3.0 + finePhase) * Rustle * detailFade;
float2 offset = float2(.8,.6) * broad + float2(-.6,.8) * (branch + flutter);
return float3(offset, flutter * .12 - abs(broad) * .025) * bend;
'''
 wind=node(u.MaterialExpressionCustom,code=code,description=marker,output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
 definitions=[]
 for name in inputs:
  entry=u.CustomInput();entry.set_editor_property('input_name',name);definitions.append(entry)
 wind.set_editor_property('inputs',definitions)
 for name,(n,pin) in inputs.items():connect(n,wind,name,pin)
 if not ME.connect_material_property(wind,'',u.MaterialProperty.MP_WORLD_POSITION_OFFSET):raise RuntimeError('Cannot connect wind offset: '+path)
 # Bounds are extended by Unreal itself. Even maximum editable amplitudes stay
 # within these limits; no full-tree CPU transforms or persistent shadow churn.
 m.set_editor_property('max_world_position_offset_displacement',48. if kind=='tree' else 16.)
 m.set_editor_property('always_evaluate_world_position_offset',False)
 ME.layout_material_expressions(m)
 errors=ME.recompile_material(m)
 if errors:raise RuntimeError('Foliage shader compile failed '+path+': '+str(errors))
 for prop in (u.MaterialProperty.MP_BASE_COLOR,u.MaterialProperty.MP_NORMAL,u.MaterialProperty.MP_OPACITY_MASK,u.MaterialProperty.MP_ROUGHNESS):
  now=(ME.get_material_property_input_node(m,prop),str(ME.get_material_property_input_node_output_name(m,prop)))
  if now!=protected[str(prop)]:raise RuntimeError('Existing scenery surface was changed: '+path)
 if not ED.save_loaded_asset(m,False):raise RuntimeError('Cannot save wind material: '+path)
 stats=ME.get_statistics(m)
 report.append({'material':path,'kind':kind,'shader_errors':list(errors),'surface_inputs_preserved':True,
                'uses_engine_time':False,'previous_frame_phase':True,'bounded_offset_cm':48 if kind=='tree' else 16,
                'coarse_motion_distance_cutoff':False if kind=='tree' else True,
                'height_weighting':'per-mesh local bounds; squared crown bend' if kind=='tree' else height,
                'highest_temporal_harmonic':3,
                'vertex_instructions':stats.get_editor_property('num_vertex_shader_instructions')})

# Validate the actual configured near trees and survey proxies resolve to these
# masters; a successful shader import alone would miss an unused replacement.
mesh_coverage=[]
for label,path in {**{k:v for k,v in scene['nature_assets'].items() if k.startswith(('Oak','Pine'))},
                   'broadleaf_proxy':scene['broadleaf_proxy_asset'],'conifer_proxy':scene['conifer_proxy_asset']}.items():
 mesh=ED.load_asset(path)
 if not isinstance(mesh,u.StaticMesh):raise RuntimeError('Missing configured tree: '+path)
 wind_masters=[]
 for slot in mesh.get_editor_property('static_materials'):
  material=slot.get_editor_property('material_interface')
  while isinstance(material,u.MaterialInstance):material=material.get_editor_property('parent')
  if material and material.get_path_name().split('.')[0] in targets and targets[material.get_path_name().split('.')[0]]=='tree':
   wind_masters.append(material.get_path_name())
 if not wind_masters:raise RuntimeError('Configured tree has no animated crown master: '+path)
 mesh_coverage.append({'kind':label,'mesh':path,'crown_materials':sorted(set(wind_masters))})

out=ROOT/'Art/WeatherV09/wind_import_report.json'
asset_paths=[*targets,weather['snow_collection'].split('.')[0]]
asset_hashes={path:hashlib.sha256((ROOT/'Content'/Path(path.removeprefix('/Game/')).with_suffix('.uasset')).read_bytes()).hexdigest() for path in asset_paths}
out.write_text(json.dumps({'materials':report,'configured_tree_coverage':mesh_coverage,'collection':collection.get_path_name(),
 'asset_sha256':asset_hashes,
 'source_sha256':{path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in ('Tools/import_foliage_wind.py','Graphics/weather.json')},
 'gpu_only':True,'trunks':'unchanged rigid bark materials','shadow_cache':'rigid; does not follow individual leaf rustle',
 'timing':'integrated calendar delta, square-root time-lapse rate capped by editable maximum; frozen on pause; bounded phase and previous-frame phase',
 'maximum_playback_rate':weather['wind_maximum_playback_rate'],
 'license':'Original material code; retained asset and texture licenses unchanged.'},indent=2)+'\n')
print('SEIGE_FOLIAGE_WIND_READY '+str(len(report))+' material masters')

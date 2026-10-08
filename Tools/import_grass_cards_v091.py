"""Import the v0.9.1 grass card atlas and clump meshes; build their non-Nanite material.

Run through the editor commandlet after Tools/prepare_grass_cards_v091.py:
  UnrealEditor-Cmd.exe seige2222.uproject -run=pythonscript -script=Tools/import_grass_cards_v091.py -unattended -nop4 -nosplash -NullRHI

Writes only /Game/Art/NatureV091. The material deliberately does not use Nanite:
masked blades in Nanite fall back to the programmable rasterizer, which was the
single largest GPU pass at 3840x1600. Cards use the ordinary masked pass with a
dithered per-instance distance fade, wind offset, terrain-matched vigor tint and
two-sided foliage transmission.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV091'
DEST = '/Game/Art/NatureV091'
manifest = json.loads((ART / 'Exports/grass_cards_manifest.json').read_text())
ED, ME = u.EditorAssetLibrary, u.MaterialEditingLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
ED.make_directory(DEST); ED.make_directory(DEST + '/Textures')
# The legacy FBX route honours the vertex colour import option (see v0.7 proxies).
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
if u.SystemLibrary.get_console_variable_int_value('Interchange.FeatureFlags.Import.FBX') != 0:
    raise RuntimeError('Cannot select the FBX importer that honors vertex colors')

# ------------------------------------------------------------------ atlas
atlas_task = u.AssetImportTask()
atlas_task.filename = str(ART / 'Exports' / manifest['atlas']['file'])
atlas_task.destination_path = DEST + '/Textures'
atlas_task.destination_name = 'T_GrassCardAtlasV091'
atlas_task.automated = True; atlas_task.replace_existing = True; atlas_task.save = True
AT.import_asset_tasks([atlas_task])
atlas = ED.load_asset(DEST + '/Textures/T_GrassCardAtlasV091')
if not isinstance(atlas, u.Texture2D):
    raise RuntimeError('Atlas import failed')
atlas.set_editor_property('srgb', True)
atlas.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
atlas.set_editor_property('do_scale_mips_for_alpha_coverage', True)
atlas.set_editor_property('alpha_coverage_thresholds', u.Vector4(0, 0, 0, .35))
atlas.set_editor_property('lod_group', u.TextureGroup.TEXTUREGROUP_WORLD)
atlas.set_editor_property('never_stream', False)
ED.save_loaded_asset(atlas, False)

macro_texture = ED.load_asset('/Game/Art/NatureV04/Textures/T_Grass004_surface_color')
if not macro_texture:
    raise RuntimeError('Missing retained meadow macro texture')
measured = json.loads((ROOT / 'Art/EnvironmentV08/surface_measurements.json').read_text())
def srgb_to_linear(c):
    return c / 12.92 if c <= .04045 else ((c + .055) / 1.055) ** 2.4
atlas_mean_linear = tuple(srgb_to_linear(c) for c in manifest['atlas']['mean_blade_colour_srgb'])
palette = json.loads((ROOT / 'Art/EnvironmentV08/surface_palette.json').read_text())


# ------------------------------------------------------------------ material
def material(name):
    path = DEST + '/' + name
    result = ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    if not result:
        raise RuntimeError('Cannot create ' + path)
    ME.delete_all_material_expressions(result)
    return result


def node(m, kind, **props):
    result = ME.create_material_expression(m, kind)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result


def connect(a, b, pin, output=''):
    if not ME.connect_material_expressions(a, output, b, pin):
        raise RuntimeError('Material link failed: ' + pin)


def output(n, prop, pin=''):
    if not ME.connect_material_property(n, pin, prop):
        raise RuntimeError('Material output failed: ' + str(prop))


def scalar(m, name, value):
    return node(m, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


def color(m, name, rgb):
    return node(m, u.MaterialExpressionVectorParameter, parameter_name=name, default_value=u.LinearColor(*rgb, 1))


def custom(m, code, description, inputs, additional=(), output_type=u.CustomMaterialOutputType.CMOT_FLOAT3):
    result = node(m, u.MaterialExpressionCustom, code=code, description=description, output_type=output_type)
    definitions = []
    for key in inputs:
        item = u.CustomInput(); item.set_editor_property('input_name', key); definitions.append(item)
    result.set_editor_property('inputs', definitions)
    outputs = []
    for key, kind in additional:
        item = u.CustomOutput(); item.set_editor_property('output_name', key); item.set_editor_property('output_type', kind); outputs.append(item)
    result.set_editor_property('additional_outputs', outputs)
    for key, (n, pin) in inputs.items():
        connect(n, result, key, pin)
    return result


grass = material('M_GrassCardV091')
for prop, value in [('used_with_instanced_static_meshes', True), ('used_with_nanite', False), ('two_sided', True),
                    ('tangent_space_normal', False), ('blend_mode', u.BlendMode.BLEND_MASKED),
                    ('opacity_mask_clip_value', .33), ('shading_model', u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE),
                    ('dithered_lod_transition', False)]:
    grass.set_editor_property(prop, value)
up = node(grass, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(0, 0, 1, 1))
instance_up = node(grass, u.MaterialExpressionTransform,
                   transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
                   transform_type=u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
connect(up, instance_up, '')
surface_inputs = {
    'Atlas': (node(grass, u.MaterialExpressionTextureObjectParameter, parameter_name='CardAtlas', texture=atlas,
                   sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR), ''),
    'UV': (node(grass, u.MaterialExpressionTextureCoordinate, coordinate_index=0), ''),
    'Vertex': (node(grass, u.MaterialExpressionVertexColor), ''),
    'WorldPos': (node(grass, u.MaterialExpressionWorldPosition), ''),
    'InstanceUp': (instance_up, ''),
    'VertexNormal': (node(grass, u.MaterialExpressionVertexNormalWS), ''),
    'Vigor': (node(grass, u.MaterialExpressionPerInstanceCustomData, data_index=0), ''),
    'Rand': (node(grass, u.MaterialExpressionPerInstanceRandom), ''),
    'Fade': (node(grass, u.MaterialExpressionPerInstanceFadeAmount), ''),
    'Macro': (node(grass, u.MaterialExpressionTextureObjectParameter, parameter_name='MeadowColor', texture=macro_texture,
                   sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR), ''),
    'MacroMean': (color(grass, 'MeadowMeasuredMean', measured['Meadow']['linear_mean']), ''),
    # Calibrate the photographed cards to the terrain palette the same way the
    # terrain layers are calibrated: divide by the measured atlas mean, then
    # multiply by an authored albedo slightly lusher than the meadow surface.
    'AtlasMean': (color(grass, 'AtlasMeasuredMean', atlas_mean_linear), ''),
    'Albedo': (color(grass, 'GrassAlbedo', (.09, .125, .04)), ''),
    'Tint': (color(grass, 'LushTint', (.9, 1.04, 1.0)), ''),
    'DryTint': (color(grass, 'DryTint', (1.06, 1.0, .85)), ''),
    'BaseShade': (scalar(grass, 'BaseShade', .68), ''),
    'NormalBlend': (scalar(grass, 'GroundNormalBlend', .7), ''),
    'Transmission': (scalar(grass, 'Transmission', .32), ''),
}
surface = custom(grass, '''
float4 tex = Texture2DSample(Atlas, AtlasSampler, UV);
float height = saturate(Vertex.r);
// Match M_TerrainV08: the same macro photograph and coherent vigor modulate the
// meadow surface, so clumps never float as a differently lit green over the ground.
float2 macroUV = WorldPos.xy / 2200;
float3 macro = Texture2DSample(Macro, MacroSampler, macroUV).rgb;
float macroValue = dot(macro, float3(.2126, .7152, .0722)) / max(dot(MacroMean, float3(.2126, .7152, .0722)), .001);
float vigor = saturate(Vigor);
float ground = clamp(macroValue, .55, 1.45) * lerp(.70, 1.10, vigor);
float3 ratio = clamp(tex.rgb / max(AtlasMean, .001), .3, 2.4);
float3 albedo = ratio * Albedo * lerp(DryTint, Tint, vigor) * ground;
albedo *= lerp(BaseShade, 1.0, saturate(height * 1.35));
albedo *= lerp(.86, 1.12, Rand);
albedo.g *= lerp(.97, 1.03, frac(Rand * 7.31));
// Per-instance dithered distance fade (interleaved gradient noise, temporally jittered).
float2 pixel = Parameters.SvPosition.xy;
float dither = frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715)) + float(View.StateFrameIndexMod8) * 0.125));
Mask = tex.a * (Fade > dither ? 1.0 : 0.0);
Normal = normalize(lerp(normalize(VertexNormal), normalize(InstanceUp), NormalBlend));
Subsurface = albedo * Transmission;
return albedo;
''', 'Card albedo matched to the terrain meadow, dithered per-instance fade, ground-blended normal', surface_inputs,
    [('Mask', u.CustomMaterialOutputType.CMOT_FLOAT1), ('Normal', u.CustomMaterialOutputType.CMOT_FLOAT3),
     ('Subsurface', u.CustomMaterialOutputType.CMOT_FLOAT3)])
output(surface, u.MaterialProperty.MP_BASE_COLOR)
output(surface, u.MaterialProperty.MP_OPACITY_MASK, 'Mask')
output(surface, u.MaterialProperty.MP_NORMAL, 'Normal')
output(surface, u.MaterialProperty.MP_SUBSURFACE_COLOR, 'Subsurface')
output(scalar(grass, 'Roughness', .82), u.MaterialProperty.MP_ROUGHNESS)
output(scalar(grass, 'Specular', .12), u.MaterialProperty.MP_SPECULAR)
output(scalar(grass, 'LeafThickness', .6), u.MaterialProperty.MP_OPACITY)
wind = custom(grass, '''
float h = saturate(Vertex.r);
float phase = dot(WorldPos.xy, float2(.011, .007)) + Vertex.g * 6.2831853;
float gust = sin(Time * .9 + WorldPos.x * .0015 + WorldPos.y * .0011) * .5 + .5;
float sway = sin(Time * 2.1 + phase) * .6 + sin(Time * 3.7 + phase * 1.6) * .25 + gust * .55;
return float3(sway * Direction.r, sway * Direction.g, -abs(sway) * .12) * h * h * Strength;
''', 'Cheap two-tone wind sway weighted by card height', {
    'Vertex': (node(grass, u.MaterialExpressionVertexColor), ''),
    'WorldPos': (node(grass, u.MaterialExpressionWorldPosition), ''),
    'Time': (node(grass, u.MaterialExpressionTime), ''),
    'Direction': (color(grass, 'WindDirection', (.75, .66, 0)), ''),
    'Strength': (scalar(grass, 'WindStrengthCm', 5.0), '')})
output(wind, u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
ME.layout_material_expressions(grass)
errors = ME.recompile_material(grass)
if errors:
    raise RuntimeError('Grass card material failed to compile: ' + str(errors))
if not ED.save_loaded_asset(grass, False):
    raise RuntimeError('Grass card material save failed')
mi_path = DEST + '/MI_GrassCardV091'
mi = ED.load_asset(mi_path) if ED.does_asset_exist(mi_path) else AT.create_asset('MI_GrassCardV091', DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
ME.set_material_instance_parent(mi, grass)
ME.update_material_instance(mi); ED.save_loaded_asset(mi, False)

# ------------------------------------------------------------------ meshes
tasks = []
for name, record in manifest['meshes'].items():
    t = u.AssetImportTask(); t.filename = str(ART / 'Exports' / record['fbx']); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    o = u.FbxImportUI(); o.import_mesh = True; o.import_as_skeletal = False; o.import_materials = False; o.import_textures = False
    o.automated_import_should_detect_type = False; o.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    s = o.static_mesh_import_data; s.combine_meshes = True; s.auto_generate_collision = False; s.generate_lightmap_u_vs = False
    s.normal_import_method = u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS; s.convert_scene = True; s.convert_scene_unit = True
    s.vertex_color_import_option = u.VertexColorImportOption.REPLACE
    t.options = o; tasks.append(t)
AT.import_asset_tasks(tasks)
report = {'atlas': atlas.get_path_name(), 'material': grass.get_path_name(), 'meshes': {}, 'nanite': False, 'atlas_mean_linear': list(atlas_mean_linear),
          'rendering_path': 'masked non-Nanite ISM cards with per-instance dithered fade',
          'source': 'CC0 grass_medium_02 tufts rendered orthographically; no new downloads'}
for name, record in manifest['meshes'].items():
    mesh = ED.load_asset(DEST + '/' + name)
    if not isinstance(mesh, u.StaticMesh):
        raise RuntimeError('Missing mesh ' + name)
    if not SME.has_vertex_colors(mesh):
        raise RuntimeError('Imported clump lost its vertex colours: ' + name)
    for i in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(i, mi)
    n = SME.get_nanite_settings(mesh); n.enabled = False; SME.set_nanite_settings(mesh, n, True)
    mesh.set_editor_property('allow_cpu_access', False)
    ED.save_loaded_asset(mesh, False)
    box = mesh.get_bounding_box(); lo, hi = box.min, box.max; dims = [hi.x - lo.x, hi.y - lo.y, hi.z - lo.z]
    if abs(lo.z) > 1.5 or any(abs(a - b) > max(1.0, b * .02) for a, b in zip(sorted(dims), sorted(record['dimensions_cm']))):
        raise RuntimeError('Mesh scale/pivot invalid ' + name + ' ' + str(dims))
    report['meshes'][name] = {'path': mesh.get_path_name(), 'dimensions_cm': dims, 'ground_z': lo.z,
                              'triangles': record['triangles'], 'cards': record['cards'], 'nanite': False,
                              'uv_channels': SME.get_num_uv_channels(mesh, 0)}
ED.save_directory(DEST, False, True)
(ART / 'import_report.json').write_text(json.dumps(report, indent=2) + '\n')
u.log('GRASS_CARDS_V091_IMPORTED ' + json.dumps(report))

"""Import the revision-5 Rex mesh, clips and card-fur materials (same asset paths as before).

Run in the Unreal Editor commandlet:
    UnrealEditor-Cmd.exe <project> -run=pythonscript -script=Tools/import_companion_dog_v2.py
Reads Art/CompanionDog/dog_manifest.json written by Tools/create_companion_dog_v2.py.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art/CompanionDog'; DEST = '/Game/Art/CompanionDog'
DATA = json.loads((ART / 'dog_manifest.json').read_text())
if DATA.get('revision') != 5:
    raise RuntimeError('dog_manifest.json is not the revision-5 export')
ED = u.EditorAssetLibrary; AT = u.AssetToolsHelpers.get_asset_tools(); ME = u.MaterialEditingLibrary
SME = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem) or u.get_default_object(u.SkeletalMeshEditorSubsystem)
ED.make_directory(DEST); ED.make_directory(DEST + '/Textures')
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')


def node(m, kind, **props):
    result = ME.create_material_expression(m, kind)
    for key, value in props.items():
        result.set_editor_property(key, value)
    return result


def link(a, b, pin, output=''):
    if not ME.connect_material_expressions(a, output, b, pin):
        raise RuntimeError('Dog material link failed: ' + pin)


def output(n, prop, pin=''):
    if not ME.connect_material_property(n, pin, prop):
        raise RuntimeError('Dog material output failed: ' + str(prop))


def custom(m, code, description, inputs, output_type=u.CustomMaterialOutputType.CMOT_FLOAT3):
    result = node(m, u.MaterialExpressionCustom, code=code, description=description, output_type=output_type)
    definitions = []
    for key in inputs:
        item = u.CustomInput(); item.set_editor_property('input_name', key); definitions.append(item)
    result.set_editor_property('inputs', definitions)
    for key, (n, pin) in inputs.items():
        link(n, result, key, pin)
    return result


def scalar(m, name, value):
    return node(m, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


# ------------------------------------------------------------------ textures
textures = {}
for name, kind in [('T_Dog_FurAtlas', 'atlas'), ('T_Dog_FurDetail', 'mask'), ('T_Dog_FurNormal', 'normal')]:
    task = u.AssetImportTask(); task.filename = str(ART / 'Textures' / (name + '.png')); task.destination_path = DEST + '/Textures'
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True
    AT.import_asset_tasks([task])
    tex = ED.load_asset(DEST + '/Textures/' + name)
    if not isinstance(tex, u.Texture2D):
        raise RuntimeError('Missing dog texture ' + name)
    tex.set_editor_property('srgb', False)
    if kind == 'normal':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == 'mask':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS)
    else:
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_DEFAULT)
        tex.set_editor_property('do_scale_mips_for_alpha_coverage', True)
        tex.set_editor_property('alpha_coverage_thresholds', u.Vector4(0, 0, 0, .4))
    ED.save_loaded_asset(tex, False); textures[name] = tex

# ------------------------------------------------------------------ materials
DECODE = 'return lerp(Color/12.92,pow((Color+.055)/1.055,2.4),step(.04045,Color));'
materials = {}
for slot, settings in DATA['materials'].items():
    name = 'M_' + slot.removeprefix('DM_'); path = DEST + '/' + name
    m = ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    ME.delete_all_material_expressions(m); m.set_editor_property('used_with_skeletal_mesh', True)
    m.set_editor_property('two_sided', False); m.set_editor_property('blend_mode', u.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT)
    if slot == 'DM_Coat':
        vc = node(m, u.MaterialExpressionVertexColor)
        decoded = custom(m, DECODE, 'Decode FBX sRGB coat colours', {'Color': (vc, '')})
        uv0 = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=0)
        uv1 = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=1)
        normal_ws = node(m, u.MaterialExpressionVertexNormalWS)
        detail_obj = node(m, u.MaterialExpressionTextureObjectParameter, parameter_name='FurDetail', texture=textures['T_Dog_FurDetail'],
                          sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        normal_obj = node(m, u.MaterialExpressionTextureObjectParameter, parameter_name='FurNormal', texture=textures['T_Dog_FurNormal'],
                          sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        scale = scalar(m, 'DetailScaleCm', 42.0)
        strength = scalar(m, 'DetailStrength', .75)
        # Rest-pose position from the authored UV channels keeps the streaks fixed
        # to the skin while the dog animates; two planar projections blend by the
        # vertex normal (top view on the back, side view on the flanks and legs).
        code = '''
float3 P = float3(UV0.x*260.0-120.0, UV0.y*120.0-60.0, UV1.x*120.0);
float2 top = P.xy / Scale, side = float2(P.x, P.z) / Scale;
float w = saturate(abs(N.z) * 1.4 - .2);
float dTop = Texture2DSample(Detail, DetailSampler, top).r;
float dSide = Texture2DSample(Detail, DetailSampler, side).r;
float d = lerp(dSide, dTop, w);
float3 nTop = Texture2DSample(NormalMap, NormalMapSampler, top).rgb * 2 - 1;
float3 nSide = Texture2DSample(NormalMap, NormalMapSampler, side).rgb * 2 - 1;
float3 nm = normalize(lerp(nSide, nTop, w));
DetailOut = lerp(1.0, d, Strength);
return float3(nm.xy, 1.0);
'''
        tri = node(m, u.MaterialExpressionCustom, code=code.strip(), description='Rest-pose planar fur detail',
                   output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
        inputs = []
        for key in ['UV0', 'UV1', 'N', 'Detail', 'NormalMap', 'Scale', 'Strength']:
            item = u.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
        tri.set_editor_property('inputs', inputs)
        out = u.CustomOutput(); out.set_editor_property('output_name', 'DetailOut'); out.set_editor_property('output_type', u.CustomMaterialOutputType.CMOT_FLOAT1)
        tri.set_editor_property('additional_outputs', [out])
        for key, (n, pin) in {'UV0': (uv0, ''), 'UV1': (uv1, ''), 'N': (normal_ws, ''), 'Detail': (detail_obj, ''), 'NormalMap': (normal_obj, ''),
                              'Scale': (scale, ''), 'Strength': (strength, '')}.items():
            link(n, tri, key, pin)
        base = node(m, u.MaterialExpressionMultiply); link(decoded, base, 'A'); link(tri, base, 'B', 'DetailOut')
        output(base, u.MaterialProperty.MP_BASE_COLOR)
        # Tangent-space detail normal is only approximate under the planar UVs; keep it mild.
        nstrength = node(m, u.MaterialExpressionLinearInterpolate, const_a=0, const_alpha=.35)
        flat = node(m, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(0, 0, 1, 1))
        nmix = node(m, u.MaterialExpressionLinearInterpolate); link(flat, nmix, 'A'); link(tri, nmix, 'B'); link(scalar(m, 'DetailNormalStrength', .35), nmix, 'Alpha')
        output(nmix, u.MaterialProperty.MP_NORMAL)
        output(scalar(m, 'Roughness', .62), u.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m, 'Specular', .32), u.MaterialProperty.MP_SPECULAR)
        m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_SUBSURFACE)
        sss = node(m, u.MaterialExpressionMultiply); link(decoded, sss, 'A'); link(node(m, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(.9, .55, .35, 1)), sss, 'B')
        output(sss, u.MaterialProperty.MP_SUBSURFACE_COLOR)
        output(scalar(m, 'SubsurfaceOpacity', .85), u.MaterialProperty.MP_OPACITY)
    elif slot == 'DM_FurCard':
        for prop, value in [('two_sided', True), ('blend_mode', u.BlendMode.BLEND_MASKED), ('opacity_mask_clip_value', .36),
                            ('shading_model', u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE), ('dithered_lod_transition', False)]:
            m.set_editor_property(prop, value)
        vc = node(m, u.MaterialExpressionVertexColor)
        decoded = custom(m, DECODE, 'Decode FBX sRGB coat colours', {'Color': (vc, '')})
        atlas = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='FurAtlas', texture=textures['T_Dog_FurAtlas'],
                     sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        uv0 = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=0); link(uv0, atlas, 'UVs')
        tint = node(m, u.MaterialExpressionVectorParameter, parameter_name='StrandTint', default_value=u.LinearColor(1.0, .96, .9, 1))
        base = node(m, u.MaterialExpressionMultiply); link(decoded, base, 'A'); link(atlas, base, 'B', 'RGB')
        tinted = node(m, u.MaterialExpressionMultiply); link(base, tinted, 'A'); link(tint, tinted, 'B')
        output(tinted, u.MaterialProperty.MP_BASE_COLOR)
        output(atlas, u.MaterialProperty.MP_OPACITY_MASK, 'A')
        sss = node(m, u.MaterialExpressionMultiply); link(tinted, sss, 'A'); link(scalar(m, 'Transmission', .55), sss, 'B')
        output(sss, u.MaterialProperty.MP_SUBSURFACE_COLOR)
        output(scalar(m, 'Roughness', .58), u.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m, 'Specular', .25), u.MaterialProperty.MP_SPECULAR)
    else:
        color = node(m, u.MaterialExpressionVectorParameter, parameter_name='Color', default_value=u.LinearColor(*settings['color']))
        output(color, u.MaterialProperty.MP_BASE_COLOR)
        if slot == 'DM_Eye':
            m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_CLEAR_COAT)
            for prop, value in (('MP_CUSTOM_DATA0', .9), ('MP_CUSTOM_DATA1', .05)):
                if hasattr(u.MaterialProperty, prop):
                    output(scalar(m, prop, value), getattr(u.MaterialProperty, prop))
        for parameter, value, prop in [('Roughness', settings['roughness'], u.MaterialProperty.MP_ROUGHNESS),
                                       ('Metallic', settings['metallic'], u.MaterialProperty.MP_METALLIC),
                                       ('Specular', .45 if slot in ('DM_Eye', 'DM_Nose', 'DM_Tongue') else .25, u.MaterialProperty.MP_SPECULAR)]:
            output(scalar(m, parameter, value), prop)
    ME.layout_material_expressions(m)
    if ME.recompile_material(m):
        raise RuntimeError('Dog material compile failed ' + name)
    ED.save_loaded_asset(m, False); materials[slot] = m

# ------------------------------------------------------------------ skeletal mesh


def task_for(filename, name):
    task = u.AssetImportTask(); task.filename = str(ART / 'Exports' / filename); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.replace_existing_settings = True; task.save = True
    return task


options = u.FbxImportUI(); options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_mesh = True; options.import_as_skeletal = True; options.import_materials = False; options.import_textures = False; options.import_animations = False
options.create_physics_asset = False
data = options.skeletal_mesh_import_data
for key, value in {'convert_scene': True, 'convert_scene_unit': True, 'normal_import_method': u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
                   'vertex_color_import_option': u.VertexColorImportOption.REPLACE, 'import_morph_targets': False, 'use_t0_as_ref_pose': False}.items():
    data.set_editor_property(key, value)
for key in ('use_full_precision_u_vs', 'use_full_precision_uvs'):
    try:
        data.set_editor_property(key, True); u.log('Dog import: ' + key + ' enabled'); break
    except Exception:
        pass
mesh_path = DEST + '/SK_CompanionDog'
if ED.does_asset_exist(mesh_path):
    previous = ED.load_asset(mesh_path); stored = previous.get_editor_property('asset_import_data')
    if isinstance(stored, u.FbxSkeletalMeshImportData):
        stored.set_editor_property('vertex_color_import_option', u.VertexColorImportOption.REPLACE)
task = task_for('SK_CompanionDog.fbx', 'SK_CompanionDog'); task.options = options; AT.import_asset_tasks([task])
mesh = ED.load_asset(mesh_path)
if not isinstance(mesh, u.SkeletalMesh):
    raise RuntimeError('Skeletal dog mesh was not imported')
if not mesh.has_vertex_colors():
    raise RuntimeError('Dog coat vertex colors missing')
skeleton = mesh.get_editor_property('skeleton')
if not isinstance(skeleton, u.Skeleton):
    raise RuntimeError('Dog skeleton missing')
slots = list(mesh.get_editor_property('materials')); seen = []
for slot in slots:
    name = str(slot.get_editor_property('imported_material_slot_name'))
    if name not in materials:
        raise RuntimeError('Unknown dog material slot ' + name)
    slot.set_editor_property('material_interface', materials[name]); seen.append(name)
required = set(DATA.get('used_materials', materials))
if required - set(seen):
    raise RuntimeError('Incomplete dog material assignments: ' + str(sorted(required - set(seen))))
mesh.set_editor_property('materials', slots)
if not SME.regenerate_lod(mesh, 3, False, False):
    raise RuntimeError('Dog skeletal LOD generation failed')
for lod in range(SME.get_lod_count(mesh)):
    for section in range(SME.get_num_sections(mesh, lod)):
        slot = SME.get_lod_material_slot(mesh, lod, section)
        if 0 <= slot < len(seen) and seen[slot] == 'DM_FurCard':
            SME.set_section_cast_shadow(mesh, lod, section, False)
bounds = mesh.get_imported_bounds(); extent = bounds.box_extent; dimensions = [extent.x * 2, extent.y * 2, extent.z * 2]
expected = [b - a for a, b in zip(DATA['reference_pose_bounds_cm']['min'], DATA['reference_pose_bounds_cm']['max'])]
if any(abs(a - b) > 2 for a, b in zip(dimensions, expected)):
    raise RuntimeError('Dog unit/axis bounds mismatch: ' + str(dimensions) + ' expected ' + str(expected))
found = set(); pending = ['root']
while pending:
    name = pending.pop()
    if name in found:
        continue
    found.add(name); pending.extend(str(x) for x in SME.get_bone_children(mesh, name))
if set(DATA['bones']) - found:
    raise RuntimeError('Dog rig bones missing: ' + str(sorted(set(DATA['bones']) - found)))
ED.save_loaded_asset(mesh, False); ED.save_loaded_asset(skeleton, False)

# ------------------------------------------------------------------ animations
animations = {}
for name, definition in DATA['animations'].items():
    options = u.FbxImportUI(); options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION; options.import_mesh = False; options.import_as_skeletal = True
    options.import_animations = True; options.import_materials = False; options.import_textures = False; options.skeleton = skeleton
    options.override_animation_name = name
    data = options.anim_sequence_import_data
    for key, value in {'convert_scene': True, 'convert_scene_unit': True, 'animation_length': u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
                       'import_bone_tracks': True, 'use_default_sample_rate': False, 'custom_sample_rate': 30, 'remove_redundant_keys': False}.items():
        data.set_editor_property(key, value)
    task = task_for(name + '.fbx', name); task.options = options; AT.import_asset_tasks([task])
    clip = ED.load_asset(DEST + '/' + name)
    if not isinstance(clip, u.AnimSequence):
        raise RuntimeError('Dog animation was not imported: ' + name)
    if clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Dog clip skeleton mismatch')
    length = clip.get_play_length()
    if abs(length - definition['duration_seconds']) > .04:
        raise RuntimeError('Dog animation duration mismatch: ' + name + ' ' + str(length))
    clip.set_editor_property('enable_root_motion', False); ED.save_loaded_asset(clip, False)
    animations[name] = {'path': clip.get_path_name(), 'seconds': length, 'root_motion': False}

report = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'animations': animations, 'revision': 5,
          'lod_count': SME.get_lod_count(mesh), 'lod_vertices': [SME.get_num_verts(mesh, i) for i in range(SME.get_lod_count(mesh))],
          'material_slots': seen, 'vertex_colors': mesh.has_vertex_colors(), 'vertex_color_encoding': 'FBX sRGB decoded by material',
          'source_triangles': DATA['triangles'], 'source_bones': DATA['bones'], 'source_reference_bounds_cm': DATA['reference_pose_bounds_cm'],
          'imported_dimensions_cm': dimensions, 'verified_bones': sorted(found), 'fur_card_shadow_casting': False,
          'runtime_review_pending': True, 'reference_photographs_imported': False}
(ART / 'import_report.json').write_text(json.dumps(report, indent=2) + '\n')
u.log('SEIGE_COMPANION_DOG_IMPORTED ' + json.dumps(report))

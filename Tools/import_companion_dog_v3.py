"""Import the revision-6 Rex mesh (generated base, baked photo albedo), clips and card-fur materials.

Run in the Unreal Editor commandlet:
    UnrealEditor-Cmd.exe <project> -run=pythonscript -script=Tools/import_companion_dog_v3.py
Reads Art/CompanionDog/dog_manifest.json written by Tools/build_rex_from_generated.py.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art/CompanionDog'; DEST = '/Game/Art/CompanionDog'
DATA = json.loads((ART / 'dog_manifest.json').read_text())
if DATA.get('revision') != 6:
    raise RuntimeError('dog_manifest.json is not the revision-6 export')
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
for name, kind in [('T_Dog_Albedo', 'color'), ('T_Dog_FurAtlas', 'atlas'), ('T_Dog_FurDetail', 'mask'), ('T_Dog_FurNormal', 'normal')]:
    task = u.AssetImportTask(); task.filename = str(ART / 'Textures' / (name + '.png')); task.destination_path = DEST + '/Textures'
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True
    AT.import_asset_tasks([task])
    tex = ED.load_asset(DEST + '/Textures/' + name)
    if not isinstance(tex, u.Texture2D):
        raise RuntimeError('Missing dog texture ' + name)
    tex.set_editor_property('srgb', kind == 'color')
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
        uv0 = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=0)
        albedo = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='Albedo', texture=textures['T_Dog_Albedo'],
                      sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        link(uv0, albedo, 'UVs')
        # Fur flow: the streak mask is box-projected in the mesh's local space so
        # strands run nose-to-tail along the flanks and back instead of following
        # the arbitrary orientation of each smart-unwrapped albedo island.
        wpos = node(m, u.MaterialExpressionWorldPosition)
        lpos = node(m, u.MaterialExpressionTransformPosition, transform_source_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                    transform_type=u.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
        link(wpos, lpos, '')
        scaled = node(m, u.MaterialExpressionDivide, const_b=9.0); link(lpos, scaled, 'A')
        side_uv = node(m, u.MaterialExpressionComponentMask, r=True, g=False, b=True, a=False); link(scaled, side_uv, '')
        top_uv = node(m, u.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False); link(scaled, top_uv, '')
        detail_side = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='FurDetailSide', texture=textures['T_Dog_FurDetail'],
                           sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        detail_top = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='FurDetailTop', texture=textures['T_Dog_FurDetail'],
                          sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        link(side_uv, detail_side, 'UVs'); link(top_uv, detail_top, 'UVs')
        wnorm = node(m, u.MaterialExpressionVertexNormalWS)
        lnorm = node(m, u.MaterialExpressionTransform, transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                     transform_type=u.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
        link(wnorm, lnorm, '')
        up = node(m, u.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(lnorm, up, '')
        up_abs = node(m, u.MaterialExpressionAbs); link(up, up_abs, '')
        up_w = node(m, u.MaterialExpressionMultiply); link(up_abs, up_w, 'A'); link(up_abs, up_w, 'B')
        detail = node(m, u.MaterialExpressionLinearInterpolate); link(detail_side, detail, 'A', 'R'); link(detail_top, detail, 'B', 'R'); link(up_w, detail, 'Alpha')
        # Fine strand variation around unity: the baked photo albedo must keep
        # its brightness, so the detail mask neither darkens nor tints the coat.
        strength = node(m, u.MaterialExpressionLinearInterpolate, const_a=.84, const_b=1.12); link(detail, strength, 'Alpha')
        base = node(m, u.MaterialExpressionMultiply); link(albedo, base, 'A', 'RGB'); link(strength, base, 'B')
        # The photo was taken in shade; under the colony sun (9 lux-equivalent,
        # +0.4 EV) the same albedo clips to white on the face and pushes the
        # tan body to orange through the tonemapper. Darken and desaturate the
        # bake so the rendered coat lands near the photograph's pale gold.
        toned = node(m, u.MaterialExpressionMultiply); link(base, toned, 'A'); link(scalar(m, 'CoatBrightness', .7), toned, 'B')
        desat = node(m, u.MaterialExpressionDesaturation); link(toned, desat, ''); link(scalar(m, 'CoatDesaturation', .22), desat, 'Fraction')
        # Grazing-angle lift stands in for light scattering through the guard
        # hairs at the silhouette, which is most of what reads as fur at 200 px.
        fres = node(m, u.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.0)
        rim = node(m, u.MaterialExpressionLinearInterpolate, const_a=1.0, const_b=1.14); link(fres, rim, 'Alpha')
        lifted = node(m, u.MaterialExpressionMultiply); link(desat, lifted, 'A'); link(rim, lifted, 'B')
        output(lifted, u.MaterialProperty.MP_BASE_COLOR)
        tiled = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=0, u_tiling=6, v_tiling=6)
        normal = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='FurNormal', texture=textures['T_Dog_FurNormal'],
                      sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        link(tiled, normal, 'UVs')
        flat = node(m, u.MaterialExpressionConstant3Vector, constant=u.LinearColor(0, 0, 1, 1))
        nmix = node(m, u.MaterialExpressionLinearInterpolate); link(flat, nmix, 'A'); link(normal, nmix, 'B', 'RGB'); link(scalar(m, 'DetailNormalStrength', .4), nmix, 'Alpha')
        output(nmix, u.MaterialProperty.MP_NORMAL)
        output(scalar(m, 'Roughness', .8), u.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m, 'Specular', .2), u.MaterialProperty.MP_SPECULAR)
        # Default lit: revision 6 showed the subsurface model saturating the
        # whole coat orange under the colony sun and blowing out the pale face.
    elif slot == 'DM_FurCard':
        # Plain two-sided lit cards: the foliage model's transmission darkened
        # every card in the body's shadow in revision 6. The atlas is only a
        # brightness mask around unity, so a card matches the coat beneath it.
        for prop, value in [('two_sided', True), ('blend_mode', u.BlendMode.BLEND_MASKED), ('opacity_mask_clip_value', .4),
                            ('shading_model', u.MaterialShadingModel.MSM_DEFAULT_LIT), ('dithered_lod_transition', False)]:
            m.set_editor_property(prop, value)
        vc = node(m, u.MaterialExpressionVertexColor)
        decoded = custom(m, DECODE, 'Decode FBX sRGB coat colours', {'Color': (vc, '')})
        atlas = node(m, u.MaterialExpressionTextureSampleParameter2D, parameter_name='FurAtlas', texture=textures['T_Dog_FurAtlas'],
                     sampler_type=u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        uv0 = node(m, u.MaterialExpressionTextureCoordinate, coordinate_index=0); link(uv0, atlas, 'UVs')
        strand = node(m, u.MaterialExpressionLinearInterpolate, const_a=.86, const_b=1.1); link(atlas, strand, 'Alpha', 'R')
        tint = node(m, u.MaterialExpressionVectorParameter, parameter_name='StrandTint', default_value=u.LinearColor(1.0, .98, .94, 1))
        base = node(m, u.MaterialExpressionMultiply); link(decoded, base, 'A'); link(strand, base, 'B')
        tinted = node(m, u.MaterialExpressionMultiply); link(base, tinted, 'A'); link(tint, tinted, 'B')
        output(tinted, u.MaterialProperty.MP_BASE_COLOR)
        output(atlas, u.MaterialProperty.MP_OPACITY_MASK, 'A')
        output(scalar(m, 'Roughness', .7), u.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m, 'Specular', .2), u.MaterialProperty.MP_SPECULAR)
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
        # Re-import keeps slots of earlier revisions; they reference no sections now.
        u.log_warning('Stale dog material slot kept from an earlier revision: ' + name)
        slot.set_editor_property('material_interface', materials['DM_Coat']); seen.append(name); continue
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

report = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'animations': animations, 'revision': 6,
          'lod_count': SME.get_lod_count(mesh), 'lod_vertices': [SME.get_num_verts(mesh, i) for i in range(SME.get_lod_count(mesh))],
          'material_slots': seen, 'vertex_colors': mesh.has_vertex_colors(), 'vertex_color_encoding': 'FBX sRGB decoded by material',
          'source_triangles': DATA['triangles'], 'source_bones': DATA['bones'], 'source_reference_bounds_cm': DATA['reference_pose_bounds_cm'],
          'imported_dimensions_cm': dimensions, 'verified_bones': sorted(found), 'fur_card_shadow_casting': False,
          'runtime_review_pending': True, 'reference_photographs_imported': False}
(ART / 'import_report.json').write_text(json.dumps(report, indent=2) + '\n')
u.log('SEIGE_COMPANION_DOG_IMPORTED ' + json.dumps(report))

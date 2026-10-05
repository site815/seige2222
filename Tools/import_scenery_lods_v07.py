"""Import original grass and CC0 source-derived tree proxies; preserve old assets."""
from pathlib import Path
import json, re
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV07'
DEST = '/Game/Art/NatureV07'
manifest = json.loads((ART / 'proxy_manifest.json').read_text(encoding='utf-8'))
palette = json.loads((ART / 'proxy_palette.json').read_text(encoding='utf-8'))
AT = u.AssetToolsHelpers.get_asset_tools()
ED = u.EditorAssetLibrary
ME = u.MaterialEditingLibrary
SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
ED.make_directory(DEST)
# The default Interchange FBX route ignores the legacy FbxImportUI color option.
# Select the legacy route explicitly for these tiny, authored colored proxies.
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
if u.SystemLibrary.get_console_variable_int_value('Interchange.FeatureFlags.Import.FBX') != 0:
    raise RuntimeError('Cannot select the FBX importer that honors vertex colors')

def verify_imported_colors(mesh, name):
    if not SME.has_vertex_colors(mesh):
        raise RuntimeError('Imported proxy has only default white vertex colors: ' + name)
    # Inspect the actual Unreal mesh rather than trusting Blender or import flags.
    # The source-mesh FBX exporter writes its imported linear colors as sRGB.
    folder = ROOT / 'Saved/ProxyColorVerification'
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / (name + '.fbx')
    task = u.AssetExportTask()
    task.object = mesh
    task.filename = str(path)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = u.StaticMeshExporterFBX()
    options = u.FbxExportOption()
    options.ascii = True
    options.vertex_color = True
    options.export_source_mesh = True
    options.level_of_detail = False
    options.collision = False
    options.bake_material_inputs = u.FbxMaterialBakeMode.DISABLED
    task.options = options
    if not u.Exporter.run_asset_export_task(task):
        raise RuntimeError('Unreal color-verification export failed: ' + name)
    arrays = re.findall(r'\bColors:\s*\*\d+\s*\{\s*a:\s*([^}]+)', path.read_text(errors='strict'))
    channels = [[], [], []]
    for array in arrays:
        values = [float(x) for x in array.replace('\n', '').replace('\r', '').split(',') if x.strip()]
        for i in range(0, len(values), 4):
            for c in range(3):
                s = values[i+c]
                channels[c].append(s/12.92 if s <= .04045 else ((s+.055)/1.055)**2.4)
    if not channels[0]: raise RuntimeError('No exported Unreal vertex-color data: ' + name)
    result = {'count': len(channels[0]), 'linear_min': [min(c) for c in channels],
              'linear_max': [max(c) for c in channels],
              'linear_mean': [sum(c)/len(c) for c in channels]}
    if max(result['linear_mean']) > .7 or max(result['linear_mean']) < .005:
        raise RuntimeError('Imported Unreal color values are invalid: ' + name + ' ' + str(result))
    return result

def material(name, tint, leaf_source=None):
    path = DEST + '/' + name
    m = ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name, DEST, u.Material, u.MaterialFactoryNew())
    ME.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('used_with_nanite', True)
    grass = 'Grass' in name
    m.set_editor_property('two_sided', grass)
    m.set_editor_property('blend_mode', u.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model', u.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE if grass else u.MaterialShadingModel.MSM_DEFAULT_LIT)
    def node(kind, **props):
        n = ME.create_material_expression(m, kind)
        for key, value in props.items(): n.set_editor_property(key, value)
        return n
    def connect(a, b, pin, output=''):
        # VertexColor's combined output is unnamed in UE 5.8. 'RGB' silently
        # fails to connect it and leaves Multiply A at zero (black foliage).
        if not ME.connect_material_expressions(a, output, b, pin):
            raise RuntimeError('Proxy material link failed: ' + name + ' / ' + pin)
    def output(n, prop):
        if not ME.connect_material_property(n, '', prop):
            raise RuntimeError('Proxy material output failed: ' + name + ' / ' + str(prop))
    vertex = node(u.MaterialExpressionVertexColor)
    color = node(u.MaterialExpressionVectorParameter, parameter_name='GrassTint' if 'Grass' in name else 'CanopyTint', default_value=u.LinearColor(*tint, 1))
    # The source mesh's colors can survive import while a Nanite path supplies
    # default white to the shader. Keep physical albedo explicit; vertex color
    # may vary brightness by only 15%, never determine the base color itself.
    mean_rgb = palette['grass' if grass else 'conifer' if 'Conifer' in name else 'broadleaf']
    mean = node(u.MaterialExpressionVectorParameter, parameter_name='MeanAlbedo', default_value=u.LinearColor(*mean_rgb, 1))
    vertex_ratio = node(u.MaterialExpressionDivide, const_b=.143 if grass else .083 if 'Conifer' in name else .144)
    connect(vertex, vertex_ratio, 'A', 'G')
    vertex_variation = node(u.MaterialExpressionClamp, min_default=.85, max_default=1.15)
    connect(vertex_ratio, vertex_variation, '')
    tinted = node(u.MaterialExpressionMultiply)
    connect(mean, tinted, 'A'); connect(color, tinted, 'B')
    multiply = node(u.MaterialExpressionMultiply)
    connect(tinted, multiply, 'A'); connect(vertex_variation, multiply, 'B')
    albedo = multiply
    if leaf_source:
        # Opaque geometric crowns can retain fine photographic leaf variation
        # without rerunning an opacity mask or showing the atlas's dark padding.
        prefix, mean = leaf_source
        uv = node(u.MaterialExpressionTextureCoordinate, u_tiling=40, v_tiling=40)
        def sample(role, sampler):
            texture = ED.load_asset(prefix + role)
            if not texture: raise RuntimeError('Missing retained foliage map: ' + prefix + role)
            n = node(u.MaterialExpressionTextureSampleParameter2D, parameter_name='Leaf' + role.title(), texture=texture, sampler_type=sampler)
            connect(uv, n, 'UVs')
            return n
        photograph = sample('color', u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        alpha = sample('alpha', u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        normal = sample('normal', u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        normalization = node(u.MaterialExpressionVectorParameter, parameter_name='LeafColorNormalization', default_value=u.LinearColor(*[1/v for v in mean], 1))
        ratio = node(u.MaterialExpressionMultiply)
        connect(photograph, ratio, 'A', 'RGB'); connect(normalization, ratio, 'B')
        bounded = node(u.MaterialExpressionClamp, min_default=.7, max_default=1.3)
        connect(ratio, bounded, '')
        valid = node(u.MaterialExpressionMultiply)
        connect(alpha, valid, 'A', 'R'); connect(vertex, valid, 'B', 'A')
        strength = node(u.MaterialExpressionMultiply, const_b=.55)
        connect(valid, strength, 'A')
        variation = node(u.MaterialExpressionLinearInterpolate, const_a=1)
        connect(bounded, variation, 'B'); connect(strength, variation, 'Alpha')
        albedo = node(u.MaterialExpressionMultiply)
        connect(multiply, albedo, 'A'); connect(variation, albedo, 'B')
        flat = node(u.MaterialExpressionConstant3Vector, constant=u.LinearColor(0, 0, 1, 1))
        normal_strength = node(u.MaterialExpressionMultiply, const_b=.35)
        connect(valid, normal_strength, 'A')
        normal_mix = node(u.MaterialExpressionLinearInterpolate)
        connect(flat, normal_mix, 'A'); connect(normal, normal_mix, 'B', 'RGB'); connect(normal_strength, normal_mix, 'Alpha')
        output(normal_mix, u.MaterialProperty.MP_NORMAL)
    output(albedo, u.MaterialProperty.MP_BASE_COLOR)
    if grass: output(multiply, u.MaterialProperty.MP_SUBSURFACE_COLOR)
    for parameter, value, prop in (('Roughness', .9, u.MaterialProperty.MP_ROUGHNESS),
                                   ('Specular', .1, u.MaterialProperty.MP_SPECULAR),
                                   ('LeafThickness', .5, u.MaterialProperty.MP_OPACITY)):
        n = node(u.MaterialExpressionScalarParameter, parameter_name=parameter, default_value=value)
        output(n, prop)
    ME.layout_material_expressions(m)
    ME.recompile_material(m)
    ED.save_loaded_asset(m, False)
    instance_name = 'MI_' + name[2:]
    instance_path = DEST + '/' + instance_name
    mi = ED.load_asset(instance_path) if ED.does_asset_exist(instance_path) else AT.create_asset(instance_name, DEST, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    ME.set_material_instance_parent(mi, m)
    ME.set_material_instance_vector_parameter_value(mi, 'GrassTint' if grass else 'CanopyTint', u.LinearColor(*tint, 1))
    ME.set_material_instance_vector_parameter_value(mi, 'MeanAlbedo', u.LinearColor(*mean_rgb, 1))
    ME.update_material_instance(mi)
    ED.save_loaded_asset(mi, False)
    return mi

materials = {
    'M_GrassProxy': material('M_GrassProxy', (1, 1, 1)),
    'M_CanopyProxy': material('M_CanopyProxy', (1, 1, 1), ('/Game/Art/NatureV04/Textures/T_jacaranda_tree_leaves_', [.1667764, .2025094, .0542335])),
    'M_ConiferProxy': material('M_ConiferProxy', (1, 1, 1), ('/Game/Art/Nature/Textures/T_fir_tree_01_twig_', [.0945283, .0876884, .0328738]))}
tasks = []
for name, entry in manifest['meshes'].items():
    task = u.AssetImportTask()
    task.filename = str(ART / 'Exports' / entry['fbx'])
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    options = u.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    settings = options.static_mesh_import_data
    settings.combine_meshes = True
    settings.auto_generate_collision = False
    settings.generate_lightmap_u_vs = False
    settings.convert_scene = True
    settings.convert_scene_unit = True
    settings.normal_import_method = u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    settings.vertex_color_import_option = u.VertexColorImportOption.REPLACE
    existing = ED.load_asset(DEST + '/' + name)
    if existing:
        # UE 5.8's unattended atomic reimport replaces the task's import data
        # with Mesh.AssetImportData (EditorFactories.cpp). Update that actual
        # object as well; replace_existing_settings alone cannot override it.
        stored = existing.get_editor_property('asset_import_data')
        if not isinstance(stored, u.FbxStaticMeshImportData):
            raise RuntimeError('Proxy has unexpected reimport data: ' + name)
        previous_color_option = str(stored.get_editor_property('vertex_color_import_option'))
        for property_name in ('combine_meshes', 'auto_generate_collision',
                              'generate_lightmap_u_vs', 'convert_scene',
                              'convert_scene_unit', 'normal_import_method',
                              'vertex_color_import_option'):
            stored.set_editor_property(property_name, settings.get_editor_property(property_name))
        u.log('SEIGE_PROXY_REIMPORT_COLORS ' + name + ': ' + previous_color_option + ' -> REPLACE')
    task.options = options
    tasks.append(task)
AT.import_asset_tasks(tasks)
report = {'license': manifest['license'], 'meshes': {}, 'opaque': True,
          'opacity_mask': False, 'dither': False, 'rendered_review_pending': True,
          'all_material_links_checked': True, 'combined_color_pin': 'unnamed_first_output',
          'canopy_shading': 'one_sided_default_lit_outward_normals',
          'vertex_albedo': 'explicit MeanAlbedo; vertex green only changes brightness within 0.85..1.15',
          'mean_albedo': palette,
          'fbx_importer': 'legacy; Interchange disabled for this commandlet',
          'canopy_detail': 'existing licensed leaf color/normal; alpha only controls bounded surface variation, never opacity'}
for name, entry in manifest['meshes'].items():
    mesh = ED.load_asset(DEST + '/' + name)
    if not isinstance(mesh, u.StaticMesh): raise RuntimeError('Missing proxy ' + name)
    imported_colors = verify_imported_colors(mesh, name)
    material_key = 'M_ConiferProxy' if name == 'SM_ConiferProxy' else entry['material']
    assigned_materials = []
    for i, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('imported_material_slot_name'))
        if not slot_name: slot_name = str(slot.get_editor_property('material_slot_name'))
        if slot_name.startswith('PH_jacaranda_tree_'):
            assigned = ED.load_asset('/Game/Art/NatureV04/MI_'+slot_name.removeprefix('PH_'))
        elif slot_name.startswith('PH_fir_tree_01_'):
            assigned = ED.load_asset('/Game/Art/Nature/MI_'+slot_name.removeprefix('PH_'))
        else: assigned = materials[material_key]
        if not assigned: raise RuntimeError('Missing retained canopy/wood material: '+slot_name)
        mesh.set_material(i, assigned)
        assigned_materials.append(assigned.get_path_name())
    settings = SME.get_nanite_settings(mesh)
    settings.enabled = True
    settings.shape_preservation = u.NaniteShapePreservation.PRESERVE_AREA
    settings.keep_percent_triangles = 1
    settings.trim_relative_error = 0
    SME.set_nanite_settings(mesh, settings, True)
    ED.save_loaded_asset(mesh, False)
    box = mesh.get_bounding_box()
    dimensions = [box.max.x-box.min.x, box.max.y-box.min.y, box.max.z-box.min.z]
    if abs(box.min.z) > .2 or any(abs(x-y) > max(.5, y*.001) for x,y in zip(sorted(dimensions), sorted(entry['dimensions_cm']))):
        raise RuntimeError('Proxy bounds or pivot mismatch: ' + name)
    report['meshes'][name] = {'path': mesh.get_path_name(), 'source_triangles': entry['triangles'],
        'dimensions_cm': dimensions, 'ground_z': box.min.z, 'nanite': True,
        'material': materials[material_key].get_path_name(), 'material_slots': assigned_materials,
        'imported_vertex_colors': imported_colors}
(ART / 'proxy_import_report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
u.log('SEIGE_SCENERY_LODS_V07_IMPORTED ' + json.dumps(report))

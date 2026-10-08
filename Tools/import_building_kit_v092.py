"""Import the v0.9.2 building kit: distinct meshes for families that shared the factory hall.

Run after Tools/create_building_kit_v092.py (meshes) and Tools/render_building_kit_ui_v092.py
(portraits) in the Unreal commandlet:
    UnrealEditor-Cmd.exe <project> -run=pythonscript -script=Tools/import_building_kit_v092.py
Meshes go to /Game/Art/SM_<Kind> with the existing MI_Industry_* instances, three LODs and
the industry importer's bounds/ground checks; portraits go to /Game/Art/Interface/T_Building_<Kind>.
Rules are untouched: Graphics/building_visuals.json maps building ids to the new kinds.
"""
from pathlib import Path
import json, unreal

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art'; KIT = ART / 'BuildingKitV092'
DATA = json.loads((KIT / 'kit_manifest.json').read_text(encoding='utf-8'))
PORTRAITS = json.loads((KIT / 'UI/portraits.json').read_text(encoding='utf-8')) if (KIT / 'UI/portraits.json').exists() else {}
DEST = '/Game/Art/Industry'; MESH_DEST = '/Game/Art'; UI_DEST = '/Game/Art/Interface'
ED = unreal.EditorAssetLibrary; TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
instances = {}
for slot in json.loads((ART / 'IndustryExports/industry_manifest.json').read_text(encoding='utf-8'))['palette']:
    mi = ED.load_asset(DEST + '/MI_Industry_' + slot.removeprefix('IM_'))
    if mi is None:
        raise RuntimeError('Missing industrial material instance for ' + slot + '; run Tools/import_industry_assets.py first')
    instances[slot] = mi

tasks = []
for name, record in DATA['meshes'].items():
    t = unreal.AssetImportTask(); t.filename = str(KIT / 'Exports' / record['fbx']); t.destination_path = MESH_DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.replace_existing_settings = True; t.save = True
    opts = unreal.FbxImportUI(); opts.import_mesh = True; opts.import_as_skeletal = False; opts.import_materials = False; opts.import_textures = False
    opts.automated_import_should_detect_type = False; opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = opts.static_mesh_import_data; sm.combine_meshes = True; sm.auto_generate_collision = False; sm.generate_lightmap_u_vs = False
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS; sm.convert_scene = True; sm.convert_scene_unit = True
    t.options = opts; tasks.append(t)
TOOLS.import_asset_tasks(tasks)
editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
report = {'source': 'Original Blender-authored building kit v0.9.2 on the industrial palette', 'meshes': {}, 'portraits': {}}
for name, record in DATA['meshes'].items():
    mesh = ED.load_asset(MESH_DEST + '/' + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError('Missing imported building ' + name)
    slots = mesh.get_editor_property('static_materials')
    imported_names = [str(s.get_editor_property('imported_material_slot_name')) for s in slots]
    if not set(record['materials']).issubset(imported_names):
        raise RuntimeError('Missing material slots ' + name + ': ' + str(imported_names))
    for i, slot in enumerate(slots):
        key = str(slot.get_editor_property('imported_material_slot_name'))
        if key in instances:
            mesh.set_material(i, instances[key])
    active = [editor.get_lod_material_slot(mesh, 0, i) for i in range(mesh.get_num_sections(0))]
    if any(index < 0 or index >= len(slots) or imported_names[index] not in instances for index in active):
        raise RuntimeError('Unknown material on active geometry ' + name + ': ' + str([(index, imported_names[index]) for index in active]))
    reduction = unreal.StaticMeshReductionOptions(); reduction.set_editor_property('auto_compute_lod_screen_size', False)
    levels = []
    for triangles, screen in ((1, 1), (.45, .25), (.14, .08)):
        setting = unreal.StaticMeshReductionSettings(); setting.set_editor_property('percent_triangles', triangles); setting.set_editor_property('screen_size', screen); levels.append(setting)
    reduction.set_editor_property('reduction_settings', levels)
    if editor.set_lods(mesh, reduction) != 3:
        raise RuntimeError('Failed building LODs for ' + name)
    ED.save_loaded_asset(mesh, only_if_is_dirty=False)
    bounds = mesh.get_bounding_box(); lo, hi = bounds.min, bounds.max; dims = [hi.x - lo.x, hi.y - lo.y, hi.z - lo.z]
    if abs(lo.z) > .5 or any(abs(a - b) > max(.5, b * .01) for a, b in zip(dims, record['dimensions'])):
        raise RuntimeError('Physical scale/ground pivot mismatch ' + name + ': ' + str(dims))
    report['meshes'][name] = {'dimensions_cm': dims, 'ground_z': lo.z, 'source_triangles': record['triangles'], 'lod_count': editor.get_lod_count(mesh),
                             'material_slots': len(slots), 'active_material_slots': [imported_names[index] for index in active], 'building_families': record.get('building_families', [])}

ED.make_directory(UI_DEST); ui_tasks = []
for key, entry in PORTRAITS.items():
    png = KIT / 'UI' / ('T_Building_' + key + '.png')
    if not png.exists():
        continue
    t = unreal.AssetImportTask(); t.filename = str(png); t.destination_path = UI_DEST; t.destination_name = 'T_Building_' + key
    t.automated = True; t.replace_existing = True; t.save = True; ui_tasks.append(t)
if ui_tasks:
    TOOLS.import_asset_tasks(ui_tasks)
for key in PORTRAITS:
    tex = ED.load_asset(UI_DEST + '/T_Building_' + key)
    if not isinstance(tex, unreal.Texture2D):
        if (KIT / 'UI' / ('T_Building_' + key + '.png')).exists():
            raise RuntimeError('Portrait import failed for ' + key)
        continue
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI); tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS); tex.set_editor_property('never_stream', True); tex.set_editor_property('srgb', True)
    ED.save_loaded_asset(tex, only_if_is_dirty=False); report['portraits'][key] = UI_DEST + '/T_Building_' + key
(KIT / 'import_report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
unreal.log('SEIGE_BUILDING_KIT_IMPORT_SUCCESS ' + json.dumps({k: v['lod_count'] for k, v in report['meshes'].items()}))

"""Import the separate authored LOD1 candidate using existing photographic materials."""
from pathlib import Path
import json, unreal as u
ROOT = Path(__file__).resolve().parents[1]
ART = ROOT/'Art/EnvironmentV07'
DEST, NAME = '/Game/Art/NatureV07', 'SM_JacarandaNearV07'
record = json.loads((ART/'jacaranda_lod1_manifest.json').read_text())
ED = u.EditorAssetLibrary
SME = u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
task = u.AssetImportTask()
task.filename = str(ART/'Exports'/record['fbx'])
task.destination_path, task.destination_name = DEST, NAME
task.automated = task.replace_existing = task.replace_existing_settings = task.save = True
options = u.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = options.import_materials = options.import_textures = False
options.automated_import_should_detect_type = False
options.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
settings = options.static_mesh_import_data
settings.combine_meshes = True
settings.auto_generate_collision = settings.generate_lightmap_u_vs = False
settings.normal_import_method = u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
settings.convert_scene = settings.convert_scene_unit = True
settings.vertex_color_import_option = u.VertexColorImportOption.IGNORE
task.options = options
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = ED.load_asset(DEST+'/'+NAME)
if not isinstance(mesh, u.StaticMesh): raise RuntimeError('Missing LOD1 candidate')
assigned = []
for i, slot in enumerate(mesh.get_editor_property('static_materials')):
    key = str(slot.get_editor_property('imported_material_slot_name'))
    if key not in record['materials']: key = str(slot.get_editor_property('material_slot_name'))
    if key not in record['materials']: raise RuntimeError('Unknown candidate material: '+key)
    path = '/Game/Art/NatureV04/MI_'+key.removeprefix('PH_')
    material = ED.load_asset(path)
    if not material: raise RuntimeError('Missing retained photographic material: '+path)
    mesh.set_material(i, material)
    assigned.append(path)
nanite = SME.get_nanite_settings(mesh)
nanite.enabled = True
nanite.shape_preservation = u.NaniteShapePreservation.PRESERVE_AREA
nanite.keep_percent_triangles = 1
nanite.trim_relative_error = 0
nanite.fallback_relative_error = .5
SME.set_nanite_settings(mesh, nanite, True)
ED.save_loaded_asset(mesh, False)
box = mesh.get_bounding_box()
dims = [box.max.x-box.min.x, box.max.y-box.min.y, box.max.z-box.min.z]
if abs(box.min.z) > .2 or any(abs(a-b) > .5 for a,b in zip(sorted(dims), sorted(record['dimensions_cm']))):
    raise RuntimeError('Candidate bounds/pivot differ from existing tree')
if SME.get_num_uv_channels(mesh, 0) != 1: raise RuntimeError('Candidate UV0 invalid')
report = {'path': mesh.get_path_name(), 'dimensions_cm': dims, 'ground_z': box.min.z,
          'source_triangles': record['triangles'], 'materials': assigned, 'nanite_preserve_area': True,
          'candidate_only': True, 'existing_runtime_asset_unchanged': True, 'rendered_review_pending': True}
(ART/'jacaranda_lod1_import_report.json').write_text(json.dumps(report, indent=2)+'\n')
u.log('SEIGE_JACARANDA_LOD1_V07_IMPORTED '+json.dumps(report))

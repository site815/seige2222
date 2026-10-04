"""Reimport only the approved FirC mesh; reuse all existing photographic materials."""
from pathlib import Path
import json,unreal
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature';DEST='/Game/Art/Nature';NAME='SM_FirC'
data=json.loads((ART/'Exports/nature_manifest.json').read_text(encoding='utf-8'));record=data['meshes'][NAME]
if record['source_object']!='fir_tree_01_c_LOD0':raise RuntimeError('Expected approved FirC source LOD0')
ed=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
sme=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
task=unreal.AssetImportTask();task.filename=str(ART/'Exports'/record['fbx']);task.destination_path=DEST;task.destination_name=NAME
task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
options.automated_import_should_detect_type=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
sm=options.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=False;sm.generate_lightmap_u_vs=False
sm.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
sm.vertex_color_import_option=unreal.VertexColorImportOption.IGNORE;task.options=options;tools.import_asset_tasks([task])
mesh=ed.load_asset(DEST+'/'+NAME)
if not mesh:raise RuntimeError('FirC failed to import')
assigned=[]
for i,slot in enumerate(mesh.get_editor_property('static_materials')):
    key=str(slot.get_editor_property('imported_material_slot_name'))
    if key not in data['materials']:key=str(slot.get_editor_property('material_slot_name'))
    if key not in data['materials']:raise RuntimeError('Unexpected FirC material '+key)
    mat=ed.load_asset(DEST+'/MI_'+key.removeprefix('PH_'))
    if not mat:raise RuntimeError('Existing photographic material missing '+key)
    mesh.set_material(i,mat);assigned.append(key)
if set(assigned)!=set(record['materials']):raise RuntimeError('Missing FirC material region')
settings=sme.get_nanite_settings(mesh);settings.set_editor_property('enabled',True)
settings.set_editor_property('shape_preservation',unreal.NaniteShapePreservation.PRESERVE_AREA)
settings.set_editor_property('keep_percent_triangles',1.0);settings.set_editor_property('trim_relative_error',0.0)
settings.set_editor_property('fallback_relative_error',.5);sme.set_nanite_settings(mesh,settings,True)
ed.save_loaded_asset(mesh,only_if_is_dirty=False)
box=mesh.get_bounding_box();lo,hi=box.min,box.max;dimensions=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
if abs(lo.z)>.2 or any(abs(a-b)>max(.5,b*.01) for a,b in zip(sorted(dimensions),sorted(record['dimensions_cm']))):raise RuntimeError('Unexpected FirC dimensions/pivot')
if sme.get_num_uv_channels(mesh,0)!=1:raise RuntimeError('FirC UV0 missing')
report=json.loads((ART/'import_report.json').read_text(encoding='utf-8'))
report['meshes'][NAME]={'path':mesh.get_path_name(),'dimensions_cm':dimensions,'ground_z':lo.z,'material_slots':assigned,
    'source_triangles':record['triangles'],'nanite':True,'shape_preservation':str(settings.get_editor_property('shape_preservation')),'uv_channels':1,
    'source_object':record['source_object'],'verified_from_fresh_process':False}
report['verified_from_fresh_process']=False
(ART/'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('SEIGE_FIR_C_LOD0_IMPORTED '+json.dumps(report['meshes'][NAME]))

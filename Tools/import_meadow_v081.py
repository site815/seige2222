"""Import local optimized meadow meshes; reuse the existing photographic maps."""
from pathlib import Path
import json, unreal as u
ROOT=Path(__file__).resolve().parents[1]; ART=ROOT/'Art/EnvironmentV081'; DEST='/Game/Art/NatureV081'
data=json.loads((ART/'Exports/environment_manifest.json').read_text())
ed=u.EditorAssetLibrary; at=u.AssetToolsHelpers.get_asset_tools()
sm=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
ed.make_directory(DEST); tasks=[]
for name,record in data['meshes'].items():
    t=u.AssetImportTask(); t.filename=str(ART/'Exports'/record['fbx']); t.destination_path=DEST; t.destination_name=name
    t.automated=True; t.replace_existing=True; t.replace_existing_settings=True; t.save=True
    o=u.FbxImportUI(); o.import_mesh=True; o.import_as_skeletal=False; o.import_materials=False; o.import_textures=False
    o.automated_import_should_detect_type=False; o.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    s=o.static_mesh_import_data; s.combine_meshes=True; s.auto_generate_collision=False; s.generate_lightmap_u_vs=False
    s.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS; s.convert_scene=True; s.convert_scene_unit=True
    s.vertex_color_import_option=u.VertexColorImportOption.IGNORE; t.options=o; tasks.append(t)
at.import_asset_tasks(tasks)
report={}
for name,record in data['meshes'].items():
    mesh=ed.load_asset(DEST+'/'+name); assert isinstance(mesh,u.StaticMesh)
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=str(slot.get_editor_property('imported_material_slot_name'))
        if key not in record['materials']: key=str(slot.get_editor_property('material_slot_name'))
        assert key in record['materials'],key
        material=ed.load_asset('/Game/Art/NatureV08/MI_GrassProxyV08' if key=='PH_opaque_ground_surface' else '/Game/Art/NatureV04/MI_'+key.removeprefix('PH_')); assert material
        mesh.set_material(i,material)
    n=sm.get_nanite_settings(mesh); n.enabled=True; n.shape_preservation=u.NaniteShapePreservation.PRESERVE_AREA
    n.keep_percent_triangles=1; n.trim_relative_error=0; n.fallback_relative_error=.5; sm.set_nanite_settings(mesh,n,True)
    ed.save_loaded_asset(mesh,False)
    box=mesh.get_bounding_box(); lo,hi=box.min,box.max; dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    assert abs(lo.z)<.2 and all(abs(a-b)<.5 for a,b in zip(sorted(dims),sorted(record['dimensions_cm'])))
    report[name]={'path':mesh.get_path_name(),'triangles':record['triangles'],'dimensions_cm':dims,'nanite':True,'materials_reused':True}
(ART/'import_report.json').write_text(json.dumps(report,indent=2))
u.log('MEADOW_V081_IMPORTED '+json.dumps(report))

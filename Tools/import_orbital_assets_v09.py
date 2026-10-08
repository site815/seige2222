"""Import authored orbital assets and baked portraits; no runtime scene captures."""
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/OrbitalV09';DATA=json.loads((ART/'Exports/orbital_manifest.json').read_text())
ED=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();SME=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
tasks=[]
for name,r in DATA['meshes'].items():
    t=u.AssetImportTask();t.filename=str(ART/'Exports'/r['fbx']);t.destination_path='/Game/Art';t.destination_name=name
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=True
    opts=u.FbxImportUI();opts.import_mesh=True;opts.import_as_skeletal=False;opts.import_materials=False;opts.import_textures=False
    opts.automated_import_should_detect_type=False;opts.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    sm=opts.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=False;sm.generate_lightmap_u_vs=False
    sm.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
    t.options=opts;tasks.append(t)
AT.import_asset_tasks(tasks);report={'meshes':{},'portraits':[]}
for name,r in DATA['meshes'].items():
    mesh=ED.load_asset('/Game/Art/'+name);slots=mesh.get_editor_property('static_materials')
    labels=[str(s.get_editor_property('imported_material_slot_name')) for s in slots]
    for i,label in enumerate(labels):
        if label not in r['materials']:continue
        mat=ED.load_asset('/Game/Art/Industry/MI_Industry_'+label.removeprefix('IM_'))
        if not mat:raise RuntimeError('Missing material '+label)
        mesh.set_material(i,mat)
    active=[SME.get_lod_material_slot(mesh,0,i) for i in range(mesh.get_num_sections(0))]
    if any(labels[i] not in r['materials'] for i in active):raise RuntimeError('Stale material on active geometry '+name)
    reduction=u.StaticMeshReductionOptions();reduction.set_editor_property('auto_compute_lod_screen_size',False);levels=[]
    for pct,screen in ((1,1),(.45,.25),(.14,.08)):
        s=u.StaticMeshReductionSettings();s.set_editor_property('percent_triangles',pct);s.set_editor_property('screen_size',screen);levels.append(s)
    reduction.set_editor_property('reduction_settings',levels)
    if SME.set_lods(mesh,reduction)!=3:raise RuntimeError('Failed LODs '+name)
    ED.save_loaded_asset(mesh,False);bounds=mesh.get_bounding_box();size=[bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y,bounds.max.z-bounds.min.z]
    if abs(bounds.min.z)>.5 or any(abs(a-b)>.6 for a,b in zip(size,r['dimensions_cm'])):raise RuntimeError('Scale mismatch '+name)
    report['meshes'][name]={'dimensions_cm':size,'lods':SME.get_lod_count(mesh),'triangles':r['triangles']}
ED.make_directory('/Game/Art/Interface');tasks=[]
for p in sorted((ART/'UI').glob('*.png')):
    t=u.AssetImportTask();t.filename=str(p);t.destination_path='/Game/Art/Interface';t.destination_name=p.stem;t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
AT.import_asset_tasks(tasks)
for task in tasks:
    tex=ED.load_asset('/Game/Art/Interface/'+task.destination_name)
    tex.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI);tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_NO_MIPMAPS);tex.set_editor_property('never_stream',True);tex.set_editor_property('srgb',True)
    ED.save_loaded_asset(tex,False);report['portraits'].append(tex.get_path_name())
(ART/'import_report.json').write_text(json.dumps(report,indent=2)+'\n');u.log('SEIGE_ORBITAL_IMPORT_COMPLETE '+json.dumps(report))

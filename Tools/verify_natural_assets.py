"""Verify saved local nature assets from a fresh Unreal process."""
from pathlib import Path
import json,unreal
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature';DEST='/Game/Art/Nature'
manifest=json.loads((ART/'Exports/nature_manifest.json').read_text(encoding='utf-8'))
report=json.loads((ART/'import_report.json').read_text(encoding='utf-8'))
ed=unreal.EditorAssetLibrary
sme=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
for name,record in manifest['meshes'].items():
    mesh=ed.load_asset(DEST+'/'+name)
    if not mesh or sme.get_num_uv_channels(mesh,0)!=1:raise RuntimeError('Missing mesh/UV0 '+name)
    settings=sme.get_nanite_settings(mesh)
    if settings.get_editor_property('enabled')!=record['nanite']:raise RuntimeError('Nanite setting not saved '+name)
    if name.startswith(('SM_Fir','SM_Broadleaf')) and settings.get_editor_property('shape_preservation')!=unreal.NaniteShapePreservation.PRESERVE_AREA:
        raise RuntimeError('Tree area preservation not saved '+name)
    for slot in mesh.get_editor_property('static_materials'):
        m=slot.get_editor_property('material_interface')
        if not m:raise RuntimeError('Missing material '+name)
        parent=m.get_editor_property('parent')
        if not parent.get_editor_property('used_with_instanced_static_meshes'):raise RuntimeError('Missing instancing shader usage '+name)
        if record['nanite'] and not parent.get_editor_property('used_with_nanite'):raise RuntimeError('Missing Nanite shader usage '+name)
    bounds=mesh.get_bounding_box();lo,hi=bounds.min,bounds.max
    dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.2 or any(abs(a-b)>max(.5,b*.01) for a,b in zip(sorted(dims),sorted(record['dimensions_cm']))):
        raise RuntimeError('Saved bounds/pivot incorrect '+name)
    report['meshes'][name]['verified_from_fresh_process']=True
sources=json.loads((ART/'sources.json').read_text(encoding='utf-8'))
for source,data in sources['assets'].items():
    for group,maps in data['maps'].items():
        for role in maps:
            tex=ed.load_asset(DEST+'/Textures/T_'+source+'_'+group+'_'+role)
            if not tex or tex.get_editor_property('srgb')!=(role=='color'):raise RuntimeError('Incorrect texture color space '+source+'/'+group+'/'+role)
            if role=='normal' and not tex.get_editor_property('flip_green_channel'):raise RuntimeError('Missing normal convention conversion')
            if role=='alpha' and not tex.get_editor_property('do_scale_mips_for_alpha_coverage'):raise RuntimeError('Missing alpha coverage preservation')
material=ed.load_asset(DEST+'/M_NatureFoliage')
if abs(material.get_editor_property('opacity_mask_clip_value')-.33)>.0001:raise RuntimeError('Incorrect foliage alpha threshold')
expected_bias={'BaseColor':-1,'OpacityMap':-2,'NormalMap':0,'RoughnessMap':0};actual_bias={}
for node in unreal.MaterialEditingLibrary.get_material_expressions(material):
    if not isinstance(node,unreal.MaterialExpressionTextureSampleParameter2D):continue
    name=str(node.get_editor_property('parameter_name'))
    if name not in expected_bias:continue
    mode=node.get_editor_property('mip_value_mode')
    expected_mode=unreal.TextureMipValueMode.TMVM_MIP_BIAS if expected_bias[name] else unreal.TextureMipValueMode.TMVM_NONE
    if mode!=expected_mode:raise RuntimeError('Incorrect foliage mip mode '+name)
    actual_bias[name]=node.get_editor_property('const_mip_value') if expected_bias[name] else 0
if actual_bias!=expected_bias:raise RuntimeError('Foliage texture bias was not saved '+str(actual_bias))
report['foliage_texture_mip_bias']=actual_bias
report['verified_from_fresh_process']=True
(ART/'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('SEIGE_NATURAL_ASSETS_VERIFIED '+str(len(manifest['meshes'])))

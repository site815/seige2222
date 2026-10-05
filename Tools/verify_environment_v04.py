"""Fresh Unreal process validation of saved v0.4 meshes and material contracts."""
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04';DEST='/Game/Art/NatureV04'
ED=u.EditorAssetLibrary;ME=u.MaterialEditingLibrary
SME=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.get_default_object(u.StaticMeshEditorSubsystem)
manifest=json.loads((ART/'Exports/environment_manifest.json').read_text())
report={'meshes':{},'materials':{},'errors':[]}
for name,r in manifest['meshes'].items():
    mesh=ED.load_asset(DEST+'/'+name)
    if not isinstance(mesh,u.StaticMesh):raise RuntimeError('Missing '+name)
    box=mesh.get_bounding_box();lo,hi=box.min,box.max;dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.2 or any(abs(a-b)>max(.5,b*.01) for a,b in zip(sorted(dims),sorted(r['dimensions_cm']))):raise RuntimeError('Invalid bounds '+name)
    nanite=SME.get_nanite_settings(mesh)
    if not nanite.enabled or nanite.shape_preservation!=u.NaniteShapePreservation.PRESERVE_AREA:raise RuntimeError('Canopy preservation not saved '+name)
    uv=SME.get_num_uv_channels(mesh,0)
    if uv<1:raise RuntimeError('Missing UV '+name)
    slots=[s.material_interface.get_path_name() for s in mesh.static_materials]
    if len(slots)!=len(r['materials']):raise RuntimeError('Lost material slots '+name)
    report['meshes'][name]={'path':mesh.get_path_name(),'dimensions_cm':dims,'ground_z':lo.z,'uv_channels':uv,'nanite':True,'preserve_area':True,'material_slots':slots}
for name in ('M_V04Bark','M_V04Foliage','M_V04Grass','M_V04Wildflowers','M_TerrainV04'):
    m=ED.load_asset(DEST+'/'+name)
    if not isinstance(m,u.Material):raise RuntimeError('Missing material '+name)
    expressions=ME.get_material_expressions(m);samples=[]
    for n in expressions:
        if isinstance(n,u.MaterialExpressionTextureSampleParameter2D):
            texture=n.get_editor_property('texture');parameter=str(n.get_editor_property('parameter_name'))
            if not texture:raise RuntimeError('Unresolved texture '+name+':'+parameter)
            if name in ('M_V04Grass','M_TerrainV04') and not texture.get_editor_property('never_stream'):raise RuntimeError('World-mapped/grass texture residency not saved '+name+':'+parameter)
            samples.append({'parameter':parameter,'path':texture.get_path_name(),'never_stream':texture.get_editor_property('never_stream'),'mip_bias':n.get_editor_property('const_mip_value') if n.get_editor_property('mip_value_mode')==u.TextureMipValueMode.TMVM_MIP_BIAS else 0})
    report['materials'][name]={'expressions':len(expressions),'texture_samples':samples}
    if name in ('M_V04Foliage','M_V04Grass'):
        biases={s['parameter']:s['mip_bias'] for s in samples}
        if biases.get('BaseColor')!=(-2 if name=='M_V04Grass' else -1) or biases.get('OpacityMap')!=-2:raise RuntimeError('Local foliage mip settings not saved')
        if not m.get_editor_property('two_sided') or abs(m.get_editor_property('opacity_mask_clip_value')-.33)>.001:raise RuntimeError('Invalid foliage shading')
    if name=='M_TerrainV04':
        defaults={str(n.get_editor_property('parameter_name')):n.get_editor_property('default_value') for n in expressions if isinstance(n,u.MaterialExpressionScalarParameter)}
        for key,value in (('MesoWorldFrequency',1/800),('MesoNormalStrength',.4),('MesoRoughnessStrength',.25),('MesoDesaturation',1),('MesoReferenceLuminance',.25),('MesoValueContrast',4)):
            if abs(defaults.get(key,-100)-value)>.00001:raise RuntimeError('Medium terrain detail missing '+key)
        report['materials'][name]['medium_detail']={key:defaults[key] for key in ('MesoWorldFrequency','MesoNormalStrength','MesoRoughnessStrength','MesoDesaturation','MesoReferenceLuminance','MesoValueContrast')}
for slot,r in manifest['materials'].items():
    mi=ED.load_asset(DEST+'/MI_'+slot.removeprefix('PH_'))
    if not isinstance(mi,u.MaterialInstanceConstant):raise RuntimeError('Missing '+slot)
    if 'original_tint' in r:
        tint=ME.get_material_instance_vector_parameter_value(mi,'Tint')
        if any(abs(a-b)>.001 for a,b in zip((tint.r,tint.g,tint.b),r['original_tint'])):raise RuntimeError('Original flower tint lost '+slot)
        continue
    for role,param in (('color','BaseColor'),('normal','NormalMap'),('roughness','RoughnessMap'),('ao','OcclusionMap')):
        tex=ME.get_material_instance_texture_parameter_value(mi,param)
        expected='T_'+r['source_asset']+'_'+r['texture_group']+'_'+role+('_padded' if r['source_asset'] in ('grass_medium_02','grass_bermuda_01') and role=='color' else '')
        if not tex or tex.get_path_name().split('.')[-1]!=expected:raise RuntimeError('Incorrect texture '+slot+':'+param)
    if r['foliage']:
        alpha=ME.get_material_instance_texture_parameter_value(mi,'OpacityMap')
        if not alpha or not alpha.get_editor_property('do_scale_mips_for_alpha_coverage'):raise RuntimeError('Missing alpha coverage '+slot)
    if r['source_asset'] in ('grass_medium_02','grass_bermuda_01'):
        tint=ME.get_material_instance_vector_parameter_value(mi,'FoliageTint')
        if any(abs(a-b)>.001 for a,b in zip((tint.r,tint.g,tint.b),(.82,1,.70))):raise RuntimeError('Meadow material calibration not saved')
        if mi.get_editor_property('parent').get_path_name().split('.')[-1]!='M_V04Grass':raise RuntimeError('Dedicated grass shading not saved')
        for param,value in (('Transmission',.75),('Thickness',.3),('MicroOcclusion',.15)):
            if abs(ME.get_material_instance_scalar_parameter_value(mi,param)-value)>.001:raise RuntimeError('Grass parameter missing '+param)
        report['meadow_foliage_tint']=[tint.r,tint.g,tint.b]
(ART/'fresh_verification.json').write_text(json.dumps(report,indent=2))
u.log('SEIGE_ENVIRONMENT_V04_FRESH_VERIFICATION_PASS '+json.dumps(report))

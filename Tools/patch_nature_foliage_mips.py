"""Materials-only foliage coverage update; no mesh or texture reimport.

Negative mip bias is confined to foliage alpha (-2) and color (-1), following
runtime diagnostics. Normals, roughness, 0.33 mask cutoff and alpha coverage
preservation remain unchanged. The full importer creates identical settings.
"""
from pathlib import Path
import json,unreal
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature'
ed=unreal.EditorAssetLibrary;me=unreal.MaterialEditingLibrary
material=ed.load_asset('/Game/Art/Nature/M_NatureFoliage')
if not material:raise RuntimeError('Foliage master is missing')
expected={'BaseColor':-1,'OpacityMap':-2};found={}
for node in me.get_material_expressions(material):
    if not isinstance(node,unreal.MaterialExpressionTextureSampleParameter2D):continue
    name=str(node.get_editor_property('parameter_name'))
    if name in expected:
        node.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_MIP_BIAS)
        node.set_editor_property('const_mip_value',expected[name]);found[name]=expected[name]
    elif name in ('NormalMap','RoughnessMap'):
        if node.get_editor_property('mip_value_mode')!=unreal.TextureMipValueMode.TMVM_NONE:
            raise RuntimeError('Unexpected existing normal/roughness bias')
if found!=expected:raise RuntimeError('Expected foliage texture parameters were not found')
if abs(material.get_editor_property('opacity_mask_clip_value')-.33)>.0001:raise RuntimeError('Unexpected foliage mask threshold')
me.recompile_material(material);ed.save_loaded_asset(material,only_if_is_dirty=False)
report=json.loads((ART/'import_report.json').read_text(encoding='utf-8'))
report['foliage_texture_mip_bias']={'BaseColor':-1,'OpacityMap':-2,'NormalMap':0,'RoughnessMap':0}
report['verified_from_fresh_process']=False
(ART/'import_report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('SEIGE_NATURE_FOLIAGE_MIPS_PATCHED '+json.dumps(found))

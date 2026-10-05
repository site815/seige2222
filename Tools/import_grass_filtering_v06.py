"""Narrow v0.6 grass-color filtering candidate; run through UnrealEditor-Cmd.

Changes only the BaseColor texture-sample mip mode in the existing grass parent.
Opacity, alpha coverage, all texture files, instances and geometry stay intact.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / 'Art/EnvironmentV06/grass_filtering_report.json'
PATH = '/Game/Art/NatureV04/M_V04Grass'
material = u.EditorAssetLibrary.load_asset(PATH)
if not isinstance(material, u.Material):
    raise RuntimeError('Missing existing grass material: ' + PATH)

samples = {}
parameters = {}
for expression in u.MaterialEditingLibrary.get_material_expressions(material):
    if isinstance(expression, u.MaterialExpressionTextureSampleParameter2D):
        name = str(expression.get_editor_property('parameter_name'))
        identity = expression.get_path_name()
        samples[identity] = expression
        parameters[identity] = name
if 'BaseColor' not in parameters.values() or 'OpacityMap' not in parameters.values():
    raise RuntimeError('Expected grass BaseColor and OpacityMap samples')

def state(sample):
    return {
        'mip_mode': str(sample.get_editor_property('mip_value_mode')),
        'constant_mip_value': sample.get_editor_property('const_mip_value'),
        'texture': sample.get_editor_property('texture').get_path_name(),
    }

before = {key: {'parameter': parameters[key], **state(value)} for key, value in samples.items()}
clip = material.get_editor_property('opacity_mask_clip_value')
for key, expression in samples.items():
    if parameters[key] == 'BaseColor':
        expression.set_editor_property('mip_value_mode', u.TextureMipValueMode.TMVM_NONE)
        expression.set_editor_property('const_mip_value', 0)
errors = u.MaterialEditingLibrary.recompile_material(material)
if errors:
    raise RuntimeError('Grass filtering material compile errors: ' + '; '.join(errors))
after = {key: {'parameter': parameters[key], **state(value)} for key, value in samples.items()}
for key in samples:
    if parameters[key] != 'BaseColor' and before[key] != after[key]:
        raise RuntimeError('Unintended sample change: ' + key)
if material.get_editor_property('opacity_mask_clip_value') != clip:
    raise RuntimeError('Opacity clip value changed')
if not u.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError('Could not save grass filtering material')
REPORT.parent.mkdir(parents=True, exist_ok=True)
report = {
    'material': material.get_path_name(),
    'before': before,
    'after': after,
    'opacity_clip_value': clip,
    'opacity_unchanged': True,
    'texture_files_changed': False,
    'geometry_changed': False,
    'tree_materials_changed': False,
    'rendered_review_pending': True,
    'purpose': 'Restore derivative-based BaseColor mips and anisotropic filtering; preserve masked silhouettes.',
}
REPORT.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
u.log('SEIGE_GRASS_FILTERING_V06_COMPLETE ' + json.dumps(report))

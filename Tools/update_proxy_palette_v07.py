"""Apply the authoritative proxy palette without reimporting any geometry."""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV07'
palette = json.loads((ART / 'proxy_palette.json').read_text(encoding='utf-8'))
for role, asset in (('grass', 'MI_GrassProxy'), ('broadleaf', 'MI_CanopyProxy'),
                    ('conifer', 'MI_ConiferProxy')):
    rgb = palette[role]
    if len(rgb) != 3 or not all(isinstance(x, (int, float)) and 0 < x < 1 for x in rgb):
        raise RuntimeError('Invalid linear proxy albedo: ' + role)
    instance = u.EditorAssetLibrary.load_asset('/Game/Art/NatureV07/' + asset)
    if not isinstance(instance, u.MaterialInstanceConstant):
        raise RuntimeError('Missing existing proxy material instance: ' + asset)
    u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        instance, 'MeanAlbedo', u.LinearColor(*rgb, 1))
    actual = u.MaterialEditingLibrary.get_material_instance_vector_parameter_value(instance, 'MeanAlbedo')
    if any(abs(a-b) > 1e-6 for a, b in zip((actual.r, actual.g, actual.b), rgb)):
        raise RuntimeError('Proxy palette parameter verification failed: ' + role)
    u.MaterialEditingLibrary.update_material_instance(instance)
    u.EditorAssetLibrary.save_loaded_asset(instance, False)

report_path = ART / 'proxy_import_report.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
report['mean_albedo'] = palette
report['palette_update'] = 'Material-instance parameters only; geometry and quantitative imported-color data unchanged'
report['rendered_review_pending'] = True
report_path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
u.log('SEIGE_PROXY_PALETTE_UPDATED ' + json.dumps(palette))

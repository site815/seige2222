"""Export the provider's authored Jacaranda LOD1 as a separate near-tree candidate.

Keeps the v0.4 LOD0 asset untouched. Reuses its UV/material preparation and
normalizes the candidate to exactly the existing runtime bounds and pivot.
"""
from pathlib import Path
import bpy, numpy as np, json
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/EnvironmentV07'
NAME = 'SM_JacarandaNearV07'
# Reuse only the shared material/export definitions, never the old script's
# asset-generation entry point. This retains the same photographic mapping.
source_script = ROOT / 'Tools/prepare_environment_v04.py'
definitions = source_script.read_text().split('# The complete 19.5-m mature canopy:')[0]
namespace = {'__file__': str(source_script)}
exec(compile(definitions, str(source_script), 'exec'), namespace)
namespace['SOURCE'] = ART / 'Source'
namespace['EXPORT'] = ART / 'Exports'
for folder in (namespace['SOURCE'], namespace['EXPORT']): folder.mkdir(parents=True, exist_ok=True)
data = namespace['DATA']
bpy.ops.wm.open_mainfile(filepath=str(ROOT / data['assets']['jacaranda_tree']['original_source']['local_path']))
ob = bpy.data.objects['jacaranda_tree_LOD1']
mesh = ob.data
original = list(mesh.materials)
groups = [m.name.removeprefix('jacaranda_tree_') for m in original]
indices = np.empty(len(mesh.polygons), np.int32)
mesh.polygons.foreach_get('material_index', indices)
uv = np.empty(len(mesh.loops)*2, np.float32)
mesh.uv_layers['UVMap'].data.foreach_get('uv', uv)
uv = uv.reshape(-1, 2)
col = mesh.color_attributes.get('Col')
if col and col.domain == 'CORNER':
    colors = np.empty(len(col.data)*4, np.float32)
    col.data.foreach_get('color', colors)
    starts, counts = np.empty(len(mesh.polygons), np.int32), np.empty(len(mesh.polygons), np.int32)
    mesh.polygons.foreach_get('loop_start', starts)
    mesh.polygons.foreach_get('loop_total', counts)
    average = np.add.reduceat(colors.reshape(-1, 4)[:, 0], starts) / counts
    changed = (indices == groups.index('trunk')) & (average > .5)
    uv[np.repeat(changed, counts)] *= 15
    indices[changed] = groups.index('branches')
while mesh.uv_layers: mesh.uv_layers.remove(mesh.uv_layers[0])
layer = mesh.uv_layers.new(name='UV0')
layer.data.foreach_set('uv', uv.ravel())
layer.active_render = True
for color in list(mesh.color_attributes): mesh.color_attributes.remove(color)
materials = [namespace['material']('jacaranda_tree', group) for group in groups]
mesh.materials.clear()
for material in materials: mesh.materials.append(material)
mesh.polygons.foreach_set('material_index', indices)

mesh.transform(ob.matrix_world)
ob.matrix_world = Matrix.Identity(4)
coords = np.empty(len(mesh.vertices)*3, np.float32)
mesh.vertices.foreach_get('co', coords)
coords = coords.reshape(-1, 3)
low, high = coords.min(axis=0), coords.max(axis=0)
target = json.loads((ROOT/'Art/EnvironmentV04/Exports/environment_manifest.json').read_text())['meshes']['SM_JacarandaA']['dimensions_cm']
scale = np.asarray(target) / 100 / (high-low)
coords = (coords-low)*scale
mesh.vertices.foreach_set('co', coords.astype(np.float32).ravel())
mesh.update()
namespace['export'](ob, NAME, 'jacaranda_tree', 'jacaranda_tree_LOD1')
record = namespace['MANIFEST']['meshes'][NAME]
record.update({'unreal_path': '/Game/Art/NatureV07/'+NAME, 'license': 'CC0-1.0',
               'source_page': 'https://polyhaven.com/a/jacaranda_tree',
               'matches_existing_bounds': 'SM_JacarandaA', 'bounds_scale_from_authored_lod1': scale.tolist(),
               'candidate_only': True, 'existing_runtime_asset_unchanged': True})
(ART/'jacaranda_lod1_manifest.json').write_text(json.dumps(record, indent=2)+'\n')
print('JACARANDA_LOD1_V07_READY '+json.dumps(record), flush=True)

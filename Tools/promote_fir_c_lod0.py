"""Promote the approved staged FirC into the authoritative source, export and manifest.

One-time migration after prepare_natural_assets.py --fir-lod0-experiment. Normal
full regeneration now selects the same FirC LOD0 directly from the original.
"""
from pathlib import Path
import bpy,json,re,shutil
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature';CANDIDATE=ROOT/'Saved/NatureLOD0'
name='SM_FirC'
baseline=json.loads((ART/'Exports/nature_manifest.json').read_text(encoding='utf-8'))
candidate=json.loads((CANDIDATE/'Exports/nature_manifest.json').read_text(encoding='utf-8'))
if candidate['meshes'][name]['source_object']!='fir_tree_01_c_LOD0':raise RuntimeError('Incorrect staged candidate')
target=ART/'Source/fir_tree_01.blend'
bpy.ops.wm.open_mainfile(filepath=str(target));bpy.context.preferences.filepaths.save_version=0
materials={m.name:m for m in bpy.data.materials}
bpy.data.objects.remove(bpy.data.objects[name],do_unlink=True)
with bpy.data.libraries.load(str(CANDIDATE/'Source/fir_tree_01.blend'),link=False) as (source,loaded):
    loaded.objects=[name]
ob=loaded.objects[0]
if not ob:raise RuntimeError('Staged FirC absent')
ob.name=name;bpy.context.scene.collection.objects.link(ob)
for i,mat in enumerate(ob.data.materials):
    canonical=re.sub(r'\.\d+$','',mat.name)
    if canonical not in materials:raise RuntimeError('Unexpected material '+canonical)
    ob.data.materials[i]=materials[canonical]
bpy.ops.outliner.orphans_purge(do_recursive=True)
ob.data.name=name+'_Mesh'
bpy.ops.wm.save_as_mainfile(filepath=str(target),compress=True)
bpy.ops.file.make_paths_relative();bpy.ops.wm.save_as_mainfile(filepath=str(target),compress=True)
shutil.copyfile(CANDIDATE/'Exports'/candidate['meshes'][name]['fbx'],ART/'Exports'/candidate['meshes'][name]['fbx'])
baseline['meshes'][name]=candidate['meshes'][name]
(ART/'Exports/nature_manifest.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')
print('FIR_C_LOD0_PROMOTED',candidate['meshes'][name],flush=True)

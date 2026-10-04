"""Check converted Blender sources and record distributable-file hashes."""
from pathlib import Path
import bpy,json,hashlib
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/Nature'
manifest=json.loads((ART/'Exports/nature_manifest.json').read_text(encoding='utf-8'))
checked={}
for source in sorted((ART/'Source').glob('*.blend')):
    if source.name=='Nature_Review.blend':continue
    bpy.ops.wm.open_mainfile(filepath=str(source))
    for ob in bpy.data.objects:
        if ob.type!='MESH':continue
        if ob.name not in manifest['meshes']:raise RuntimeError('Unexpected derivative mesh '+ob.name)
        if len(ob.data.uv_layers)!=1:raise RuntimeError('Expected one baked UV0 '+ob.name)
        if len(ob.data.color_attributes):raise RuntimeError('Unexpected vertex color dependence '+ob.name)
        slots=set(p.material_index for p in ob.data.polygons)
        if len(slots)!=len(set(manifest['meshes'][ob.name]['materials'])):
            # Multiple source slots intentionally share the same bark material.
            actual={ob.data.materials[i].name for i in slots}
            if actual!=set(manifest['meshes'][ob.name]['materials']):raise RuntimeError('Missing material coverage '+ob.name)
        for material in ob.data.materials:
            if not material or not material.use_nodes:raise RuntimeError('Missing material '+ob.name)
            for n in material.node_tree.nodes:
                if n.type=='TEX_IMAGE' and n.image:
                    path=Path(bpy.path.abspath(n.image.filepath)).resolve()
                    if not path.is_file() or not path.is_relative_to(ART/'Textures'):
                        raise RuntimeError('Missing/non-local texture '+str(path))
        checked[ob.name]={'uv_channels':len(ob.data.uv_layers),'materials':sorted({ob.data.materials[i].name for i in slots}),
            'leaf_vertex_colors':False,'source':source.relative_to(ROOT).as_posix()}
if set(checked)!=set(manifest['meshes']):raise RuntimeError('Some source meshes are missing')
bpy.ops.wm.open_mainfile(filepath=str(ART/'Source/Nature_Review.blend'))
for name in manifest['meshes']:
    if not any(o.type=='MESH' and o.data.name==name+'_Mesh' and len(o.data.vertices)>0 for o in bpy.context.scene.objects):
        raise RuntimeError('Review scene has a missing linked mesh '+name)
report={'source_checks':checked,'linked_review_meshes_verified':len(manifest['meshes']),'sha256':{}}
for folder in ('Source','Exports','Previews'):
    for path in sorted((ART/folder).rglob('*')):
        if path.is_file() and path.suffix in ('.blend','.fbx','.json','.png'):
            # Git stores JSON as LF. Canonicalize before hashing so provenance
            # remains valid after a Windows-to-Linux checkout.
            if path.suffix=='.json':
                path.write_text(path.read_text(encoding='utf-8'),encoding='utf-8',newline='\n')
            report['sha256'][path.relative_to(ROOT).as_posix()]=hashlib.sha256(path.read_bytes()).hexdigest()
(ART/'prepared_hashes.json').write_text(json.dumps(report,indent=2),encoding='utf-8',newline='\n')
print('NATURE_SOURCES_VERIFIED',len(checked),flush=True)

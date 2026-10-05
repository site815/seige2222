from pathlib import Path
import bpy,json
ROOT=Path(__file__).resolve().parents[1];result={}
for asset in ('jacaranda_tree','grass_bermuda_01'):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/'.tools/nature-downloads'/asset/(asset+'_1k.blend')))
    objects=[]
    for ob in bpy.data.objects:
        if ob.type!='MESH':continue
        objects.append({'name':ob.name,'dimensions':list(ob.dimensions),'polygons':len(ob.data.polygons),'vertices':len(ob.data.vertices),
            'modifiers':[(m.type,m.name,m.show_render) for m in ob.modifiers],'materials':[m.name if m else None for m in ob.data.materials],
            'attributes':[(a.name,a.data_type,a.domain) for a in ob.data.attributes if not a.name.startswith('.')],
            'collection':[c.name for c in ob.users_collection]})
    materials={}
    for m in bpy.data.materials:
        if m.use_nodes:materials[m.name]=[{'type':n.type,'name':n.name,'image':n.image.filepath if n.type=='TEX_IMAGE' and n.image else None,
            'uv_attribute':getattr(n,'attribute_name',''),'scale':list(n.inputs['Scale'].default_value) if n.type=='MAPPING' else None} for n in m.node_tree.nodes if n.type in ('TEX_IMAGE','ATTRIBUTE','MAPPING')]
    result[asset]={'objects':objects,'materials':materials,'units':bpy.context.scene.unit_settings.scale_length}
(ROOT/'Art/EnvironmentV04/source_inspection.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('ENVIRONMENT_SOURCE_INSPECTION '+json.dumps(result),flush=True)

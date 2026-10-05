from pathlib import Path
import bpy,json
ROOT=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'.tools/nature-downloads/jacaranda_tree/jacaranda_tree_1k.blend'))
result={}
for m in bpy.data.materials:
    if m.name.startswith('jacaranda_tree') and m.use_nodes:
        result[m.name]={'nodes':[{'name':n.name,'type':n.type,'attribute':getattr(n,'attribute_name',''),'inputs':{i.name:list(i.default_value) if hasattr(i.default_value,'__len__') else str(i.default_value) for i in n.inputs if hasattr(i,'default_value') and not i.is_linked}} for n in m.node_tree.nodes], 'links':[(l.from_node.name,l.from_socket.name,l.to_node.name,l.to_socket.name) for l in m.node_tree.links]}
(ROOT/'Saved/jacaranda_material_inspection.json').write_text(json.dumps(result,indent=2))

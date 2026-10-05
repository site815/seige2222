from pathlib import Path
import bpy,json
ROOT=Path(__file__).resolve().parents[1];CACHE=ROOT/'.tools/meadow-research';result={}
for asset in ('grass_medium_01','grass_medium_02'):
    bpy.ops.wm.open_mainfile(filepath=str(CACHE/asset/(asset+'.blend')))
    objects=[]
    for ob in bpy.data.objects:
        if ob.type!='MESH':continue
        objects.append({'name':ob.name,'dimensions':list(ob.dimensions),'polygons':len(ob.data.polygons),'vertices':len(ob.data.vertices),
            'modifiers':[(m.type,m.name,m.show_render) for m in ob.modifiers],'materials':[m.name if m else None for m in ob.data.materials],
            'collections':[c.name for c in ob.users_collection]})
    result[asset]=objects
(CACHE/'inspection.json').write_text(json.dumps(result,indent=2))
print('MEADOW_CANDIDATE_INSPECTION '+json.dumps(result),flush=True)

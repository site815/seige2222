"""Offline Rex anatomy/photographic repair. See Art/CompanionDog/README.md.
The owner-authorized prototype license exception remains unresolved.
"""
import bpy,bmesh,numpy as np,json,math
from pathlib import Path
P=Path(__file__).resolve().parents[1]/'Saved/rex-repair-v011'
bpy.ops.wm.open_mainfile(filepath=str(P/'Textured-repair.blend'));dog=bpy.data.objects['Rex_repaired'];rig=bpy.data.objects['CompanionDogRig'];scene=bpy.context.scene
bm=bmesh.new();bm.from_mesh(dog.data);boundary=sum(e.is_boundary for e in bm.edges);nonmanifold=sum(not e.is_manifold for e in bm.edges);bm.free()
invalid=[];tailerrors=[];crosserrors=[]
for v in dog.data.vertices:
 w=[(dog.vertex_groups[g.group].name,g.weight)for g in v.groups]
 if not all(math.isfinite(f)for f in v.co)or abs(sum(f for n,f in w)-1)>.001 or len(w)>4:invalid.append(v.index)
 if v.co.x< -30 and any(n.startswith(('fore','hind'))and f>.001 for n,f in w):tailerrors.append(v.index)
 if v.co.z<25 and abs(v.co.y)>3:
  wrong='_R'if v.co.y>0 else'_L'
  if any(n.endswith(wrong)and n.startswith(('fore','hind'))and f>.001 for n,f in w):crosserrors.append(v.index)
rest=np.array([v.co for v in dog.data.vertices]);frames=[];rig.data.pose_position='POSE';rig.animation_data.action=bpy.data.actions['A_DogWalk']
for f in range(1,15):
 scene.frame_set(f);obj=dog.evaluated_get(bpy.context.evaluated_depsgraph_get());me=obj.to_mesh();v=np.array([q.co for q in me.vertices]);obj.to_mesh_clear()
 assert np.isfinite(v).all();frames.append({'frame':f,'min_cm':v.min(0).tolist(),'max_cm':v.max(0).tolist(),'max_displacement_cm':float(np.linalg.norm(v-rest,axis=1).max())})
report={'boundary_edges':boundary,'nonmanifold_edges':nonmanifold,'invalid_weight_vertices':len(invalid),'tail_vertices_with_leg_weights':len(tailerrors),'lower_limb_vertices_with_opposite_side_weights':len(crosserrors),'walk_frames':frames,'bone_count':len(rig.data.bones)}
(P/'deformation_validation.json').write_text(json.dumps(report,indent=2))
assert boundary==0 and nonmanifold==0, report
assert not invalid and not tailerrors and not crosserrors,report
assert max(f['max_displacement_cm']for f in frames)<60,report
print('REX_GEOMETRY_VALIDATION',json.dumps(report),flush=True)

"""Offline Rex anatomy/photographic repair. See Art/CompanionDog/README.md.
The owner-authorized prototype license exception remains unresolved.
"""
import bpy,math,json
from pathlib import Path
from mathutils import Vector
ROOT=Path(__file__).resolve().parents[1];P=ROOT/'Saved/rex-repair-v011'
bpy.ops.wm.open_mainfile(filepath=str(P/'Textured-repair.blend'))
dog=bpy.data.objects['Rex_repaired'];rig=bpy.data.objects['CompanionDogRig'];scene=bpy.context.scene
old={v.index:{dog.vertex_groups[g.group].name:g.weight for g in v.groups}for v in dog.data.vertices};groups={g.name:g for g in dog.vertex_groups}
def s(a,b,x):
 t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def add(w,n,a):
 if a>1e-7:w[n]=w.get(n,0)+a
for v in dog.data.vertices:
 x,y,z=v.co;side='L'if y>=0 else'R';w={};fore=s(12,22,x)*(1-s(46,54,x));hind=(1-s(-2,8,x))*s(-35,-28,x)
 lateral=(1-s(23,29,z))+s(23,29,z)*s(1,7,abs(y))
 amount=(1-s(23,42,z))*lateral;fw=fore*amount;hw=hind*amount
 tail=(1-s(-34,-22,x))*s(26,34,z)
 body=max(0,1-fw-hw-tail);pb=1-s(-12,20,x);add(w,'pelvis',body*pb);add(w,'spine',body*(1-pb))
 for pref,total in [('fore',fw),('hind',hw)]:
  upper=s(25,35 if pref=='fore'else 36,z);paw=1-s(5 if pref=='fore'else 10,11 if pref=='fore'else 17,z)
  add(w,pref+'_upper_'+side,total*upper);add(w,pref+'_lower_'+side,total*(1-upper)*(1-paw));add(w,pref+'_paw_'+side,total*(1-upper)*paw)
 if tail>0:
  scores=[]
  for n in ['tail_1','tail_2','tail_3','tail_4']:
   b=rig.data.bones[n];d=b.tail_local-b.head_local;t=max(0,min(1,(v.co-b.head_local).dot(d)/d.length_squared));dist=(v.co-b.head_local-d*t).length;scores.append((1/max(2,dist)**3,n))
  den=sum(a for a,n in scores)
  for a,n in scores:add(w,n,tail*a/den)
 # Preserve the detailed facial controls without transferring defective old
 # fore/hind/tail weights back into the torso attachment regions.
 face=s(45,57,z)*s(25,40,x)
 if face>0:
  donor={n:a for n,a in old[v.index].items()if not n.startswith(('fore','hind','tail'))};den=sum(donor.values())
  if den>0:
   w={n:a*(1-face)for n,a in w.items()}
   for n,a in donor.items():add(w,n,face*a/den)
 weights=sorted(((a,n)for n,a in w.items()if a>1e-6),reverse=True)[:4];den=sum(a for a,n in weights)
 for g in dog.vertex_groups:g.remove([v.index])
 for a,n in weights:groups[n].add([v.index],a/den,'REPLACE')
dog['continuous_skinning_revision']=1
rig.animation_data.action=bpy.data.actions['A_DogIdle'];scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(P/'Textured-repair.blend'))
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=1400;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
rig.animation_data.action=bpy.data.actions['A_DogWalk']
for frame in [4,7,10]:
 scene.frame_set(frame);scene.camera.location=(250,-65,105);scene.camera.rotation_euler=(Vector((15,0,43))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=58;scene.render.filepath=str(P/f'textured-walk-{frame}.png');bpy.ops.render.render(write_still=True)
print('CONTINUOUS_SKINNING_DONE',flush=True)

"""Offline Rex anatomy/photographic repair. See Art/CompanionDog/README.md.
The owner-authorized prototype license exception remains unresolved.
"""
import bpy,bmesh,math,json
from mathutils import Vector
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/rex-repair-v011';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'Art/CompanionDog/Source/RepairDonor.blend'))
rig=bpy.data.objects['CompanionDogRig'];rig.data.pose_position='REST';rig.animation_data.action=None
orig=bpy.data.objects['SK_CompanionDog'];orig.hide_render=True
# The photographed side has usable eye/ear/muzzle anatomy. Reconstruct the
# occluded side symmetrically instead of retaining its stretched generated shell.
bm=bmesh.new();bm.from_mesh(orig.data)
for v in bm.verts:v.co.y-=1.2
bmesh.ops.symmetrize(bm,input=list(bm.verts)+list(bm.edges)+list(bm.faces),direction='-Y',dist=.0001)
for v in bm.verts:v.co.y+=1.2
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(orig.data);bm.free()
orig['rex_bilateral_repair']=True
body=orig.copy();body.data=orig.data.copy();bpy.context.collection.objects.link(body);body.name='Rex_repaired';body.hide_render=False
for m in list(body.modifiers):body.modifiers.remove(m)
body.parent=None
# Clip corrupt tail and fused lower anatomy, then cap both cut surfaces.
bm=bmesh.new();bm.from_mesh(body.data)
for point,normal in [((-29,0,0),(1,0,0)),((0,0,30),(0,0,1))]:
 r=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=.0001,plane_co=point,plane_no=normal,clear_inner=True,clear_outer=False)
 edges=[e for e in bm.edges if e.is_boundary]
 if edges:bmesh.ops.holes_fill(bm,edges=edges,sides=0)
bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(body.data);bm.free()
print('RETAINED_BODY',len(body.data.vertices),len(body.data.polygons),flush=True)
# Blend the original scan's cut underside inward before attaching new limbs.
for v in body.data.vertices:
 x,y,z=v.co
 if z<43:
  f=max(0,min(1,(43-z)/13));f=f*f*(3-2*f)
  v.co.y=(y-1)* (1-.72*f)+1
  belly=max(0,1-abs((x-2)/30))
  v.co.z+=6*belly*f
for v in body.data.vertices:
 x,y,z=v.co
 if x>18 and z<38:
  v.co.z=z+(38-z)*min(1,(x-18)/10)
 if x < -21 and z<43:
  t=min(1,(-21-x)/6);v.co.z=z+(43-z)*t
 if 24<x<42 and 39<z<62:
  f=min(1,(x-24)/5,(42-x)/4,(z-39)/6,(62-z)/5)
  yy=y-1;limit=9.3
  if abs(yy)>limit:v.co.y=1+math.copysign(limit+(abs(yy)-limit)*(1-f),yy)
parts=[body]
def loft(name,sections,side):
    vs=[];fs=[];segments=32
    # Smooth cubic centre/radius interpolation, preserving organic joint taper.
    points=[]
    for i in range(len(sections)-1):
      p0=sections[max(0,i-1)];p1=sections[i];p2=sections[i+1];p3=sections[min(len(sections)-1,i+2)]
      for j in range(7):
        t=j/7;points.append([.5*((2*p1[k])+(-p0[k]+p2[k])*t+(2*p0[k]-5*p1[k]+4*p2[k]-p3[k])*t*t+(-p0[k]+3*p1[k]-3*p2[k]+p3[k])*t*t*t)for k in range(5)])
    points.append(sections[-1])
    for x,z,y,rx,ry in points:
      for j in range(segments):
        a=j*2*math.pi/segments
        # Small contour asymmetry along the shin is anatomy, not a cylinder.
        vs.append((x+rx*math.cos(a),side*(y+ry*math.sin(a)),z))
    for i in range(len(points)-1):
      for j in range(segments):fs.append((i*segments+j,i*segments+(j+1)%segments,(i+1)*segments+(j+1)%segments,(i+1)*segments+j))
    fs.append(tuple(reversed(range(segments))));fs.append(tuple((len(points)-1)*segments+j for j in range(segments)))
    me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update();o=bpy.data.objects.new(name,me);bpy.context.collection.objects.link(o);parts.append(o)
def ellipsoid(name,loc,scale):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=16,location=loc);o=bpy.context.object;o.name=name;o.scale=scale;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);parts.append(o)
# A continuous rounded underline is retained without a hanging added volume.
ellipsoid('Brisket blend',(29,1,44),(7,8.5,7.5))
for side in [-1,1]:
  loft('Foreleg',[(25,54,3,2.5,2.5),(28,46,6,4.5,4.0),(29,41,7,5.2,4.8),(30.6,36,8,4.8,4.2),(30.7,32,8,4.3,3.8),(31.0,25,8,3.55,3.25),(32.0,17,8,3.0,2.9),(33.1,9,8,2.9,2.9),(34.8,4,8,4.1,3.5),(35.8,1.1,8,4.6,3.7)],side)
  ellipsoid('Front paw',(35.7,side*8,3.0),(5.3,3.65,2.6))
  for off in [-2.2,-.72,.72,2.2]:ellipsoid('Fore toe',(39.0,side*(8+off),2.35),(2.5,1.0,1.85))
  loft('Hindleg',[(-16,54,3,2.5,2.5),(-15,48,5,4.5,4),(-13,42,7,6.8,5.3),(-11,36,8.1,5.8,4.6),(-10,32,8.1,5.1,4.3),(-12.0,26,8.1,4.2,3.4),(-17.2,20,8.1,3.2,2.85),(-21.8,14,8.1,2.8,2.6),(-20.2,9,8.1,2.5,2.6),(-17.5,4,8.1,3.6,3.1),(-16.7,1.1,8.1,4.1,3.3)],side)
  ellipsoid('Hind paw',(-16.3,side*8.1,2.7),(4.9,3.2,2.4))
  for off in [-1.8,-.6,.6,1.8]:ellipsoid('Hind toe',(-13.0,side*(8.1+off),2.25),(2.05,.83,1.65))
# Tail rings run across Y/Z, giving soft feathered volume instead of a split slab.
sections=[(-22,46,3.5,5),(-29,46,5.0,6.5),(-38,44,4.7,7),(-48,44,3.6,6),(-58,46,2.2,3.5),(-64,48,.6,1)]
vs=[];fs=[];N=32
for x,z,ry,rz in sections:
 for j in range(N):
  a=2*math.pi*j/N;vs.append((x,ry*math.cos(a),z+rz*math.sin(a)))
for i in range(len(sections)-1):
 for j in range(N):fs.append((i*N+j,i*N+(j+1)%N,(i+1)*N+(j+1)%N,(i+1)*N+j))
fs.append(tuple(range(N)));fs.append(tuple(reversed([(len(sections)-1)*N+j for j in range(N)])))
me=bpy.data.meshes.new('Feathered tail');me.from_pydata(vs,[],fs);o=bpy.data.objects.new('Feathered tail',me);bpy.context.collection.objects.link(o);parts.append(o)
bpy.context.view_layer.objects.active=o;o.select_set(True);mod=o.modifiers.new('Tail contour','SUBSURF');mod.levels=2;bpy.ops.object.modifier_apply(modifier=mod.name)
bpy.ops.object.select_all(action='DESELECT')
for o in parts:o.select_set(True)
bpy.context.view_layer.objects.active=body;bpy.ops.object.join()
bm=bmesh.new();bm.from_mesh(body.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(body.data);bm.free()
mod=body.modifiers.new('Watertight anatomy','REMESH');mod.mode='VOXEL';mod.voxel_size=.22;mod.use_smooth_shade=True;bpy.ops.object.modifier_apply(modifier=mod.name)
print('REMESH',len(body.data.polygons),flush=True)
# Local smoothing blends the seam and voxel stair steps; the face keeps its shape.
bm=bmesh.new();bm.from_mesh(body.data)
vs=[v for v in bm.verts if v.co.z<55]
for _ in range(65):bmesh.ops.smooth_vert(bm,verts=vs,factor=.38,use_axis_x=True,use_axis_y=True,use_axis_z=True)
bm.normal_update();bm.to_mesh(body.data);bm.free()
# Keep enough density for a fine face silhouette and rounded paws.
body.data.calc_loop_triangles();mod=body.modifiers.new('Runtime budget','DECIMATE');mod.ratio=min(1,90000/len(body.data.loop_triangles));bpy.ops.object.modifier_apply(modifier=mod.name)
for p in body.data.polygons:p.use_smooth=True
# Clay view deliberately exposes anatomy from every angle before texturing.
mat=bpy.data.materials.new('Anatomy clay');mat.diffuse_color=(.38,.30,.2,1);mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.38,.30,.2,1);bs.inputs['Roughness'].default_value=.7
body.data.materials.clear();body.data.materials.append(mat)
# Rebuild local weights; no vertex can be assigned to the opposite side's limbs.
for g in list(body.vertex_groups):body.vertex_groups.remove(g)
groups={b.name:body.vertex_groups.new(name=b.name)for b in rig.data.bones}
def smooth(a,b,x):
 t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
def dist(p,a,b):
 d=b-a;t=max(0,min(1,(p-a).dot(d)/d.length_squared));return (p-a-d*t).length
for v in body.data.vertices:
 p=v.co;x,y,z=p;side='L'if y>=0 else'R';weights=[]
 if z<43 and abs(y)>3 and x>-26 and (x>16 or x<1):
  pref='fore'if x>16 else'hind';chain=[pref+'_'+part+'_'+side for part in ['upper','lower','paw']]
  scores=[(1/max(1.4,dist(p,rig.data.bones[n].head_local,rig.data.bones[n].tail_local))**3,n)for n in chain];den=sum(w for w,n in scores)
  limb=1-smooth(34,45,z)
  weights=[(limb*w/den,n)for w,n in scores]+[(1-limb,'spine'if pref=='fore'else'pelvis')]
 elif x>36 and z>57:weights=[(1,'head')]
 elif x>20 and z>47:weights=[(1,'neck')]
 elif x< -26 and z>32:
  chain=['tail_'+str(i)for i in range(1,5)];scores=[(1/max(2,dist(p,rig.data.bones[n].head_local,rig.data.bones[n].tail_local))**3,n)for n in chain];den=sum(w for w,n in scores);weights=[(w/den,n)for w,n in scores]
 else:
  blend=smooth(-12,20,x);weights=[(1-blend,'pelvis'),(blend,'spine')]
 weights=[(w,n)for w,n in weights if w>1e-5];weights=sorted(weights,reverse=True)[:4];den=sum(w for w,n in weights)
 for w,n in weights:groups[n].add([v.index],w/den,'REPLACE')
body.parent=rig;mod=body.modifiers.new('Skinning','ARMATURE');mod.object=rig
rig.data.pose_position='POSE';rig.animation_data.action=bpy.data.actions['A_DogIdle'];scene=bpy.context.scene;scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Anatomy-repair.blend'))
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=20;scene.cycles.use_denoising=True;scene.render.resolution_x=1100;scene.render.resolution_y=825;scene.render.resolution_percentage=100
for name,pos,tgt,lens in [('front',(300,-40,95),(30,0,50),60),('side',(6,-285,80),(-8,0,44),48),('rear',(-240,-55,95),(-14,0,40),50),('three_quarter',(200,-235,118),(5,0,46),50)]:
 scene.camera.location=pos;scene.camera.rotation_euler=(Vector(tgt)-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=lens;scene.render.filepath=str(OUT/('anatomy-'+name+'.png'));bpy.ops.render.render(write_still=True)
print('ANATOMY_REPAIRED',len(body.data.polygons),len(body.data.vertices),flush=True)

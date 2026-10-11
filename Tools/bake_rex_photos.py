"""Offline Rex anatomy/photographic repair. See Art/CompanionDog/README.md.
The owner-authorized prototype license exception remains unresolved.
"""
import bpy,bmesh,math,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
from mathutils.geometry import barycentric_transform
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Saved/rex-repair-v011'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Anatomy-repair.blend'))
body=bpy.data.objects['Rex_repaired'];orig=bpy.data.objects['SK_CompanionDog'];rig=bpy.data.objects['CompanionDogRig'];rig.data.pose_position='REST'
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True
# Mirrored donor topology must also swap paired bone names.
if orig.get('rex_bilateral_repair',False):
 for v in orig.data.vertices:
  if v.co.y<=1.2001:continue
  ws=[(orig.vertex_groups[g.group].name,g.weight)for g in v.groups]
  for g in orig.vertex_groups:g.remove([v.index])
  for n,w in ws:
   if n.endswith('_R'):n=n[:-2]+'_L'
   elif n.endswith('_L'):n=n[:-2]+'_R'
   orig.vertex_groups[n].add([v.index],w,'REPLACE')
orig.data.calc_loop_triangles();tris=list(orig.data.loop_triangles);ov=[v.co.copy()for v in orig.data.vertices];ot=[t.vertices[:]for t in tris]
bvh=BVHTree.FromPolygons(ov,ot,all_triangles=True);ouv=orig.data.uv_layers[0].data
# Original body/neck/head weighting is transferred; only repaired limbs use
# local ipsilateral chains. This prevents opposite legs pulling the same skin.
oldnames={g.index:g.name for g in orig.vertex_groups};newgroups={g.name:g for g in body.vertex_groups}
def smooth(a,b,x):
 t=max(0,min(1,(x-a)/(b-a)));return t*t*(3-2*t)
for v in body.data.vertices:
 x,y,z=v.co
 if x<-28:continue
 blend=smooth(38,48,z)
 if blend<=0:continue
 q,n,ti,d=bvh.find_nearest(v.co);t=tris[ti]
 weights=barycentric_transform(q,*(ov[i]for i in t.vertices),Vector((1,0,0)),Vector((0,1,0)),Vector((0,0,1)))
 w={newgroups[g.group].name:g.weight*(1-blend)for g in []}
 for g in v.groups:w[body.vertex_groups[g.group].name]=g.weight*(1-blend)
 for vi,b in zip(t.vertices,weights):
  for g in orig.data.vertices[vi].groups:w[oldnames[g.group]]=w.get(oldnames[g.group],0)+blend*max(0,b)*g.weight
 for g in body.vertex_groups:g.remove([v.index])
 w=sorted(((a,n)for n,a in w.items()if a>1e-5),reverse=True)[:4];den=sum(a for a,n in w)
 for a,n in w:newgroups[n].add([v.index],a/den,'REPLACE')
# Coefficient channels: original face, pale shin, longer chest hair.
attr=body.data.color_attributes.new(name='PhotoRegion',type='FLOAT_COLOR',domain='POINT')
for v in body.data.vertices:
 x,y,z=v.co
 head=smooth(52,61,z)*smooth(33,42,x)
 if head>0:
  _,_,_,distance=bvh.find_nearest(v.co);head*=1-smooth(.35,1.1,distance)
 legs=1-smooth(16,40,z)
 chest=smooth(15,35,x)*(1-smooth(53,63,z))
 attr.data[v.index].color=(head,legs,chest,1)
# Ray transfer of the original photo coat does not interpolate across UV seams.
for layer in list(body.data.uv_layers):body.data.uv_layers.remove(layer)
buv=body.data.uv_layers.new(name='UV0');body.data.uv_layers.active=buv
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.uv.smart_project(angle_limit=math.radians(68),island_margin=.009,area_weight=.15);bpy.ops.object.mode_set(mode='OBJECT')
oldmat=bpy.data.materials.new('Original photo emission');oldmat.use_nodes=True;nn=oldmat.node_tree.nodes;ll=oldmat.node_tree.links;nn.clear();otex=nn.new('ShaderNodeTexImage');otex.image=bpy.data.images.load(str(ROOT/'Art/CompanionDog/Source/PhotoPatches/ReferenceCoat.png'),check_existing=True);ouvn=nn.new('ShaderNodeUVMap');ouvn.uv_map=orig.data.uv_layers[0].name;ll.new(ouvn.outputs[0],otex.inputs['Vector']);oe=nn.new('ShaderNodeEmission');ll.new(otex.outputs['Color'],oe.inputs['Color']);oo=nn.new('ShaderNodeOutputMaterial');ll.new(oe.outputs[0],oo.inputs['Surface']);orig.data.materials.clear();orig.data.materials.append(oldmat)
transfer=bpy.data.images.new('RayTransferredCoat',width=4096,height=4096,alpha=False);transfer.colorspace_settings.name='sRGB'
tm=bpy.data.materials.new('Transfer target');tm.use_nodes=True;tn=tm.node_tree.nodes.new('ShaderNodeTexImage');tn.image=transfer;tm.node_tree.nodes.active=tn;body.data.materials.clear();body.data.materials.append(tm)
orig.hide_render=False;orig.hide_set(False);orig.select_set(True);body.select_set(True);bpy.context.view_layer.objects.active=body
scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.45;scene.render.bake.max_ray_distance=1.5;scene.render.bake.margin=18
bpy.ops.object.bake(type='EMIT');transfer.filepath_raw=str(OUT/'RayTransferredCoat.png');transfer.file_format='PNG';transfer.save();orig.hide_render=True;orig.select_set(False)
scene.render.bake.use_selected_to_active=False
print('RAY_TRANSFER_DONE',flush=True)
# A calibrated frontal photograph corrects the previously one-sided face.
fuv=body.data.uv_layers.new(name='FrontPhoto');fattr=body.data.color_attributes.new(name='FrontBlend',type='FLOAT_COLOR',domain='POINT')
for v in body.data.vertices:
 x,y,z=v.co;f=(1-smooth(2,5,abs(y-1.2)))*smooth(70.5,72,z)*smooth(41,45,x)
 mouth=smooth(48,56,x)*smooth(.15,.6,v.normal.x)*smooth(57,59,z)*(1-smooth(63.5,65,z));f=max(f,mouth);fattr.data[v.index].color=(f,f,f,1)
for li,loop in enumerate(body.data.loops):
 x,y,z=body.data.vertices[loop.vertex_index].co;px=497+(y-1.2)*8;py=310+(76-z)*5
 if z<67:px=497+(y-1.2)*11.8;py=425+(64-z)*17
 fuv.data[li].uv=((px-370)/242,1-(py-297)/248)
body.data.uv_layers.active=body.data.uv_layers['UV0'];body.data.uv_layers['UV0'].active_render=True
mat=bpy.data.materials.new('DM_Coat_Repaired');mat.use_nodes=True;nodes=mat.node_tree.nodes;links=mat.node_tree.links;nodes.clear()
def node(kind):return nodes.new(kind)
def link(a,op,b,ip):links.new(a.outputs[op],b.inputs[ip])
output=node('ShaderNodeOutputMaterial');em=node('ShaderNodeEmission');link(em,0,output,'Surface')
uvnode=node('ShaderNodeUVMap');uvnode.uv_map='UV0';oldtex=node('ShaderNodeTexImage');oldtex.image=transfer;link(uvnode,0,oldtex,'Vector')
geo=node('ShaderNodeNewGeometry');scale=node('ShaderNodeVectorMath');scale.operation='MULTIPLY';scale.inputs[1].default_value=(1/16,1/14,1/20);link(geo,'Position',scale,0)
patch={}
for name in ['body','leg','chest','paw']:
 tex=node('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(ROOT/f'Art/CompanionDog/Source/PhotoPatches/tile-{name}.png'),check_existing=True);tex.projection='BOX';tex.projection_blend=.35;link(scale,0,tex,'Vector');patch[name]=tex
vc=node('ShaderNodeVertexColor');vc.layer_name='PhotoRegion';sep=node('ShaderNodeSeparateColor');link(vc,'Color',sep,0)
# Keep the outdoor pale-gold colour. Indoor paw texture contributes only subtle
# hair detail, rather than carrying the warm room illumination into daylight.
chestmix=node('ShaderNodeMixRGB');chestmix.blend_type='MIX';link(sep,'Blue',chestmix,0);link(patch['body'],'Color',chestmix,1);link(patch['chest'],'Color',chestmix,2)
legmix=node('ShaderNodeMixRGB');link(sep,'Green',legmix,0);link(chestmix,0,legmix,1);link(patch['leg'],'Color',legmix,2)
face=node('ShaderNodeMixRGB');link(sep,'Red',face,0);link(legmix,0,face,1);link(oldtex,'Color',face,2)
ftex=node('ShaderNodeTexImage');ftex.image=bpy.data.images.load(str(ROOT/'Art/CompanionDog/Source/PhotoPatches/FrontFacePhoto.png'),check_existing=True);fu=node('ShaderNodeUVMap');fu.uv_map='FrontPhoto';link(fu,0,ftex,'Vector');fc=node('ShaderNodeVertexColor');fc.layer_name='FrontBlend';fmix=node('ShaderNodeMixRGB');link(fc,'Color',fmix,0);link(face,0,fmix,1);link(ftex,'Color',fmix,2);link(fmix,0,em,'Color')
body.data.materials.clear();body.data.materials.append(mat)
# Generate a fresh atlas for the repaired topology, then bake the multi-source
# photo material into it. Runtime needs one ordinary albedo texture.
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
img=bpy.data.images.new('RepairedPhotoCoat',width=4096,height=4096,alpha=False);img.colorspace_settings.name='sRGB';target=node('ShaderNodeTexImage');target.image=img;nodes.active=target
scene.render.bake.use_selected_to_active=False;scene.render.bake.margin=12
bpy.ops.object.bake(type='EMIT');img.filepath_raw=str(OUT/'T_Dog_Albedo.png');img.file_format='PNG';img.save()
print('BAKED_PHOTO_COAT',flush=True)
# Final runtime-equivalent material, with photographic fur variation.
nodes.clear();out=node('ShaderNodeOutputMaterial');bs=node('ShaderNodeBsdfPrincipled');link(bs,0,out,'Surface');bs.inputs['Roughness'].default_value=.8;bs.inputs['Specular IOR Level'].default_value=.2
tex=node('ShaderNodeTexImage');tex.image=img;uv=node('ShaderNodeUVMap');uv.uv_map='UV0';link(uv,0,tex,'Vector');link(tex,'Color',bs,'Base Color')
noise=node('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=9;noise.inputs['Detail'].default_value=2;geo=node('ShaderNodeNewGeometry');mapping=node('ShaderNodeVectorMath');mapping.operation='MULTIPLY';mapping.inputs[1].default_value=(.12,1,1);link(geo,'Position',mapping,0);link(mapping,0,noise,'Vector');bump=node('ShaderNodeBump');bump.inputs['Strength'].default_value=.18;bump.inputs['Distance'].default_value=.05;link(noise,'Fac',bump,'Height');link(bump,0,bs,'Normal')
# Retain height UV required by the importer without temporary source UVs.
body.data.uv_layers.remove(body.data.uv_layers['FrontPhoto']);body.data.uv_layers.active=body.data.uv_layers['UV0'];body.data.uv_layers['UV0'].active_render=True
rest=body.data.uv_layers.new(name='RestHeight')
for li,loop in enumerate(body.data.loops):rest.data[li].uv=(body.data.vertices[loop.vertex_index].co.z/100,0)
# A proper rounded nose restores the broad black nose visible in all five photos.
# It deforms with the head bone and has an independent wet surface response.
bpy.ops.mesh.primitive_uv_sphere_add(segments=48,ring_count=24,location=(61.8,1.2,67.35));nose=bpy.context.object;nose.name='Rex nose';nose.scale=(1.05,2.65,1.65);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
for v in nose.data.vertices:
 # Slightly taper the lower nose into the central philtrum.
 if v.co.z<0:v.co.y*=1+.16*v.co.z/1.72
# Shallow real nostril cavities avoid a featureless button silhouette.
for sign in [-1,1]:
 bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=16,location=(62.66,1.2+sign*1.37,67.4));cutter=bpy.context.object;cutter.scale=(.5,.64,.45);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
 bpy.context.view_layer.objects.active=nose;mod=nose.modifiers.new('Nostril','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter;bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
for p in nose.data.polygons:p.use_smooth=True
nm=bpy.data.materials.new('DM_Nose');nm.use_nodes=True;ns=nm.node_tree.nodes;nl=nm.node_tree.links;nb=ns.get('Principled BSDF');nb.inputs['Roughness'].default_value=.46;nb.inputs['Specular IOR Level'].default_value=.3;nt=ns.new('ShaderNodeTexImage');nt.image=bpy.data.images.load(str(ROOT/'Art/CompanionDog/Source/PhotoPatches/NosePhoto.png'),check_existing=True);nu=ns.new('ShaderNodeUVMap');nu.uv_map='UV0';nl.new(nu.outputs[0],nt.inputs['Vector']);nl.new(nt.outputs['Color'],nb.inputs['Base Color']);noise=ns.new('ShaderNodeTexNoise');noise.inputs['Scale'].default_value=20;noise.inputs['Detail'].default_value=2;ng=ns.new('ShaderNodeNewGeometry');nl.new(ng.outputs['Position'],noise.inputs['Vector']);bb=ns.new('ShaderNodeBump');bb.inputs['Strength'].default_value=.24;bb.inputs['Distance'].default_value=.035;nl.new(noise.outputs['Fac'],bb.inputs['Height']);nl.new(bb.outputs[0],nb.inputs['Normal']);nose.data.materials.append(nm)
nose.data.uv_layers.active.name='UV0'
for li,loop in enumerate(nose.data.loops):
 p=nose.data.vertices[loop.vertex_index].co;nose.data.uv_layers[0].data[li].uv=(.5+p.y/5.8,.5+p.z/4.0)
g=nose.vertex_groups.new(name='head');g.add([v.index for v in nose.data.vertices],1,'REPLACE')
# Joining preserves the nose UVs, material slot and head weights.
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);nose.select_set(True);bpy.context.view_layer.objects.active=body;bpy.ops.object.join()
rig.data.pose_position='POSE';rig.animation_data.action=bpy.data.actions['A_DogIdle'];scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Textured-repair.blend'))
scene.render.resolution_x=1400;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
views=[('front',(300,0,85),(30,0,47),64),('left',(3,-280,85),(-3,0,43),50),('right',(3,280,85),(-3,0,43),50),('rear',(-265,25,88),(-10,0,40),55),('three_quarter',(205,-240,120),(4,0,44),55),('face',(175,0,78),(47,0,66),110)]
for name,pos,tgt,lens in views:
 scene.camera.location=pos;scene.camera.rotation_euler=(Vector(tgt)-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=lens;scene.render.filepath=str(OUT/('textured-'+name+'.png'));bpy.ops.render.render(write_still=True)
rig.animation_data.action=bpy.data.actions['A_DogWalk']
for frame in [4,7,10]:
 scene.frame_set(frame);scene.camera.location=(250,-65,105);scene.camera.rotation_euler=(Vector((15,0,43))-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.lens=58;scene.render.filepath=str(OUT/f'textured-walk-{frame}.png');bpy.ops.render.render(write_still=True)
print('TEXTURE_REPAIR_DONE',flush=True)

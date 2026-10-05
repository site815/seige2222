"""Original photo-guided companion dog; run with the bundled Blender in background.

The reference photographs are deliberately never read, copied or packed here.
Geometry, markings, rig and clips are authored for this project. The retained
T_Dog_CoatDetail swatch was generated with OpenAI imagegen from a text prompt;
the other three maps are procedural. No reference-photo pixels are sampled.
Centimeters; +X forward, +Z up; ground-pivot root; in-place 30 fps clips.
"""
from pathlib import Path
import bpy, math, random, json, hashlib
import numpy as np
from mathutils import Vector, Quaternion, Matrix

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/CompanionDog'
for d in ('Source','Exports','Textures','Previews'): (ART/d).mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.context.preferences.filepaths.save_version=0
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=.01
scene.render.fps=30
R=random.Random(8152222)
PARTS=[];BODY=[];MATS={};PALETTE={
    'Coat':((.48,.285,.125,1),.79,0),
    'FurCard':((.48,.285,.125,1),.83,0),
    'Nose':((.009,.008,.007,1),.44,0),
    'Eye':((.022,.009,.003,1),.34,0),
    'Iris':((.048,.018,.006,1),.30,0),
    'Mouth':((.018,.009,.012,1),.50,0),
    'Tongue':((.38,.11,.14,1),.48,0),
    'Teeth':((.74,.70,.56,1),.31,0),
    'Collar':((.025,.29,.265,1),.75,0),
    'CollarEdge':((.012,.022,.028,1),.67,0),
    'Metal':((.56,.60,.58,1),.26,.85),
}

# Original seamless fine coat relief, with no photographed pixels.
n=1024;rng=np.random.default_rng(815)
noise=rng.random((n,n)).astype(np.float32)
grain=sum(np.roll(noise,i,axis=0) for i in range(-8,9))/17
grain=(grain-grain.mean())*2.7
dx=(np.roll(grain,-1,axis=1)-np.roll(grain,1,axis=1))*.60
dy=(np.roll(grain,-1,axis=0)-np.roll(grain,1,axis=0))*.60
normal=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1)
normal/=np.linalg.norm(normal,axis=-1)[...,None]
def image(name,rgb):
    img=bpy.data.images.new(name,width=n,height=n,alpha=True)
    img.colorspace_settings.name='Non-Color'
    rgba=np.ones((n,n,4),np.float32);rgba[:,:,:3]=rgb
    img.pixels.foreach_set(rgba.reshape(-1));img.filepath_raw=str(ART/'Textures'/(name+'.png'));img.file_format='PNG';img.save()
    return img
fur_normal=image('T_Dog_FurNormal',normal*.5+.5)
fur_detail=image('T_Dog_FurDetail',np.repeat(np.clip(.78+grain*.23,.56,.96)[...,None],3,axis=-1))
u=np.arange(n,dtype=np.float32)[None,:]/n;v=np.arange(n,dtype=np.float32)[:,None]/n
alpha=np.zeros((n,n),np.float32)
for i in range(13):
    center=(i+.5)/13+.012*np.sin(v*5+i*2.1)+.035*(v-.2)**2*math.sin(i*3)
    thickness=.012*np.clip((1-v)*3,0,1)
    strand=np.clip((thickness-np.abs(u-center))*150,0,1)*np.clip((1-v)*14,0,1)
    alpha=np.maximum(alpha,strand)
fur_alpha=image('T_Dog_FurAlpha',np.repeat(alpha[...,None],3,axis=-1))
for name,(color,rough,metal) in PALETTE.items():
    m=bpy.data.materials.new('DM_'+name);m.use_nodes=True;m.diffuse_color=color
    p=m.node_tree.nodes.get('Principled BSDF');p.inputs['Base Color'].default_value=color
    p.inputs['Roughness'].default_value=rough;p.inputs['Metallic'].default_value=metal
    if name in ('Coat','FurCard'):
        vc=m.node_tree.nodes.new('ShaderNodeVertexColor');vc.layer_name='CoatColor'
        detail=m.node_tree.nodes.new('ShaderNodeTexImage');detail.image=fur_detail
        mix=m.node_tree.nodes.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=.55
        m.node_tree.links.new(vc.outputs['Color'],mix.inputs[1]);m.node_tree.links.new(detail.outputs['Color'],mix.inputs[2]);m.node_tree.links.new(mix.outputs[0],p.inputs['Base Color'])
        tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=fur_normal
        bump=m.node_tree.nodes.new('ShaderNodeNormalMap');bump.inputs['Strength'].default_value=.42
        m.node_tree.links.new(tex.outputs['Color'],bump.inputs['Color']);m.node_tree.links.new(bump.outputs['Normal'],p.inputs['Normal'])
        p.inputs['Sheen Weight'].default_value=.24;p.inputs['Sheen Roughness'].default_value=.6
        if name=='FurCard':
            tex=m.node_tree.nodes.new('ShaderNodeTexImage');tex.image=fur_alpha
            cut=m.node_tree.nodes.new('ShaderNodeMath');cut.operation='GREATER_THAN';cut.inputs[1].default_value=.33
            m.node_tree.links.new(tex.outputs['Color'],cut.inputs[0]);m.node_tree.links.new(cut.outputs[0],p.inputs['Alpha'])
            # Match the engine's non-shadow-casting fur section: the continuous
            # skin casts the silhouette shadow, individual close ribbons do not
            # form rectangular self-shadow patches on the coat beneath them.
            rays=m.node_tree.nodes.new('ShaderNodeLightPath')
            transparent=m.node_tree.nodes.new('ShaderNodeBsdfTransparent')
            shadow_mix=m.node_tree.nodes.new('ShaderNodeMixShader')
            m.node_tree.links.new(rays.outputs['Is Shadow Ray'],shadow_mix.inputs[0])
            m.node_tree.links.new(p.outputs['BSDF'],shadow_mix.inputs[1])
            m.node_tree.links.new(transparent.outputs[0],shadow_mix.inputs[2])
            m.node_tree.links.new(shadow_mix.outputs[0],m.node_tree.nodes.get('Material Output').inputs['Surface'])
    if name in ('Eye','Nose'):p.inputs['Coat Weight'].default_value=.3 if name=='Eye' else .12
    MATS[name]=m

def smooth(o):
    for p in o.data.polygons:p.use_smooth=True
    return o
def active(o):
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
def finish(o,name,mat='Coat',bone=None,body=False):
    o.name=name;o.data.materials.append(MATS[mat]);smooth(o)
    if bone:
        group=o.vertex_groups.new(name=bone);group.add(list(range(len(o.data.vertices))),1,'REPLACE')
    (BODY if body else PARTS).append(o);return o
def ellipsoid(name,p,s,mat='Coat',bone=None,body=False,segments=28,rings=18):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=p)
    o=bpy.context.object;o.scale=s;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    return finish(o,name,mat,bone,body)
def loft(name,centers,radii,mat='Coat',bone=None,body=False,sides=16):
    points=[Vector(x) for x in centers];verts=[];faces=[]
    for i,p in enumerate(points):
        tangent=(points[min(i+1,len(points)-1)]-points[max(0,i-1)]).normalized()
        reference=Vector((0,1,0)) if abs(tangent.y)<.95 else Vector((1,0,0))
        side=(reference-tangent*reference.dot(tangent)).normalized();up=tangent.cross(side).normalized()
        ry,rz=radii[i] if isinstance(radii[i],tuple) else (radii[i],radii[i])
        for j in range(sides):
            a=2*math.pi*j/sides;verts.append(p+side*math.cos(a)*ry+up*math.sin(a)*rz)
        if i:
            for j in range(sides):
                a=(i-1)*sides+j;b=(i-1)*sides+(j+1)%sides;c=i*sides+(j+1)%sides;d=i*sides+j;faces.append((a,b,c,d))
    faces.append(tuple(reversed(range(sides))));faces.append(tuple((len(points)-1)*sides+j for j in range(sides)))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update();o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o)
    return finish(o,name,mat,bone,body)
def line(name,points,r,mat,bone):
    curve=bpy.data.curves.new(name,'CURVE');curve.dimensions='3D';curve.resolution_u=3;curve.bevel_depth=r;curve.bevel_resolution=2
    s=curve.splines.new('BEZIER');s.bezier_points.add(len(points)-1)
    for p,co in zip(s.bezier_points,points):p.co=co;p.handle_left_type='AUTO';p.handle_right_type='AUTO'
    o=bpy.data.objects.new(name,curve);bpy.context.collection.objects.link(o);active(o);bpy.ops.object.convert(target='MESH')
    return finish(bpy.context.object,name,mat,bone)

# Continuous anatomical surface: ribs, chest, tucked abdomen, neck and head.
loft('Ribcage',[(-48,0,45),(-40,0,46),(-29,0,46),(-10,0,45),(10,0,45),(25,0,46),(36,0,48)],[(2,4),(12,13),(16,16),(17,17),(17,17),(17,17),(10,14)],body=True,sides=40)
ellipsoid('Chest',(26,0,46),(14.5,14.3,17),body=True)
loft('Neck',[(23,0,52),(31,0,62),(41,0,73),(47,0,77)],[(16,16),(15,16),(12,13),(9,10)],body=True,sides=28)
ellipsoid('Skull',(48,0,78),(14,11.5,11.5),body=True)
ellipsoid('Brow',(55,0,79.5),(9,10.7,7.8),body=True)
loft('Muzzle',[(55,0,73),(62,0,73),(70,0,72),(76,0,72.3)],[(8.7,6.2),(8.9,5.6),(8.2,5.1),(6.3,4)],body=True,sides=28)
for side in (-1,1):
    ellipsoid('Soft integrated brow',(55,side*8.1,81.7),(3.2,2.8,1.3),body=True)
    ellipsoid('Upper jowl',(68,side*5.7,69.9),(8.6,4,5),body=True)
    y=side*12.7
    ellipsoid('Shoulder',(23,y,43),(9,8,17),body=True)
    loft('Foreleg',[(27,y,47),(23,y,30),(24.5,y,14),(26,y,6)],[(6.7,7.5),(4.2,4.6),(3.1,3.6),(3.6,3.4)],body=True,sides=20)
    ellipsoid('Forepaw',(28.6,y,4.1),(5.6,4.6,3.8),body=True)
    for toe in (-2.7,-.9,.9,2.7):
        ellipsoid('Forepaw rounded toe',(33.0,y+toe,3.1),(3.15,1.35,2.55),body=True,segments=16,rings=12)
    ellipsoid('Haunch',(-32,y,43),(11,8.5,15),body=True)
    loft('Hindleg',[(-34,y,44),(-24,y,30),(-37,y,15),(-36,y,6)],[(7.5,8.5),(5,6),(3.7,4),(3.2,3)],body=True,sides=20)
    ellipsoid('Hindpaw',(-34.0,y,3.8),(4.8,4.0,3.6),body=True)
    for toe in (-2.4,-.8,.8,2.4):
        ellipsoid('Hindpaw rounded toe',(-29.6,y+toe,2.9),(2.75,1.2,2.4),body=True,segments=16,rings=12)

# Voxel union removes intersecting-primitive seams; relaxed, reduced manifold
# skin is subsequently weighted to the anatomical rig.
bpy.ops.object.select_all(action='DESELECT')
for o in BODY:o.select_set(True)
bpy.context.view_layer.objects.active=BODY[0];bpy.ops.object.join();skin=bpy.context.object;skin.name='Continuous anatomical coat'
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
remesh=skin.modifiers.new('Unified sculpt surface','REMESH');remesh.mode='VOXEL';remesh.voxel_size=.78;remesh.use_smooth_shade=True
bpy.ops.object.modifier_apply(modifier=remesh.name)
relax=skin.modifiers.new('Anatomical surface relaxation','SMOOTH');relax.factor=.9;relax.iterations=4;bpy.ops.object.modifier_apply(modifier=relax.name)
reduce=skin.modifiers.new('Efficient smooth skin','DECIMATE');reduce.ratio=.66;bpy.ops.object.modifier_apply(modifier=reduce.name)
PARTS.append(skin);smooth(skin)

# Rig, with explicit joint placement and deterministic smooth skin weighting.
spec=[('root',(0,0,0),(0,0,8),None),('pelvis',(-33,0,43),(-14,0,44),'root'),('spine',(-14,0,44),(23,0,48),'pelvis'),('neck',(23,0,48),(42,0,74),'spine'),('head',(42,0,74),(66,0,75),'neck'),('jaw',(53,0,68),(73,0,64),'head')]
for side,label in ((-1,'R'),(1,'L')):
    y=side*12.7
    spec.extend([(f'fore_upper_{label}',(26,y,47),(23,y,29),'spine'),(f'fore_lower_{label}',(23,y,29),(26,y,6),f'fore_upper_{label}'),(f'fore_paw_{label}',(26,y,6),(34,y,3.8),f'fore_lower_{label}'),(f'hind_upper_{label}',(-33,y,44),(-24,y,30),'pelvis'),(f'hind_lower_{label}',(-24,y,30),(-37,y,14),f'hind_upper_{label}'),(f'hind_paw_{label}',(-37,y,14),(-30,y,3.8),f'hind_lower_{label}'),(f'ear_{label}',(45,side*10.5,82),(41,side*15,61),'head')])
tail_points=[(-43,0,49),(-57,-1,47),(-73,-2,40),(-90,-2.5,30),(-105,-2,25)]
for i in range(4):spec.append((f'tail_{i+1}',tail_points[i],tail_points[i+1],'pelvis' if i==0 else f'tail_{i}'))
arm=bpy.data.armatures.new('CompanionDog_Skeleton');rig=bpy.data.objects.new('CompanionDogRig',arm);bpy.context.collection.objects.link(rig);active(rig);bpy.ops.object.mode_set(mode='EDIT')
for name,a,b,parent in spec:
    bone=arm.edit_bones.new(name);bone.head=a;bone.tail=b
    if parent:bone.parent=arm.edit_bones[parent]
    bone.use_connect=False
bpy.ops.object.mode_set(mode='OBJECT');rig.show_in_front=True
segments={name:(Vector(a),Vector(b)) for name,a,b,parent in spec}
def distance_segment(p,a,b):
    delta=b-a;t=max(0,min(1,(p-a).dot(delta)/delta.length_squared));return (p-a-delta*t).length
def weights(o):
    groups={n:o.vertex_groups.get(n) or o.vertex_groups.new(name=n) for n in segments}
    for v in o.data.vertices:
        p=o.matrix_world@v.co;side='L' if p.y>=0 else 'R'
        def ease(t):t=max(0,min(1,t));return t*t*(3-2*t)
        fore=ease((50-p.z)/23)*ease((abs(p.y)-4)/7)*math.exp(-((p.x-25)/19)**4)
        hind=ease((53-p.z)/24)*ease((abs(p.y)-4)/7)*math.exp(-((p.x+31)/20)**4)
        head=ease((p.x-38)/20)*ease((p.z-57)/18)
        neck=ease((p.x-19)/23)*ease((p.z-44)/19)*(1-head)
        base=max(0,1-fore-hind-head-neck)
        values=[]
        for amount,candidates in [(fore,[f'fore_upper_{side}',f'fore_lower_{side}',f'fore_paw_{side}']),
                                   (hind,[f'hind_upper_{side}',f'hind_lower_{side}',f'hind_paw_{side}']),
                                   (head,['head']),(neck,['neck']),(base,['pelvis','spine'])]:
            raw=[(1/max(distance_segment(p,*segments[n]),2)**3,n) for n in candidates];den=sum(a for a,n in raw)
            values.extend((amount*a/den,n) for a,n in raw if amount>.0001)
        values=sorted(values,reverse=True)[:4]
        total=sum(x[0] for x in values)
        for amount,name in values:groups[name].add([v.index],amount/total,'REPLACE')
weights(skin)

# Individual drooping ears have a folded root, rounded lobes and fine feathering.
for side,label in ((-1,'R'),(1,'L')):
    loft('Feathered drop ear '+label,[(45,side*10.7,82),(44,side*14.3,78),(41,side*15.2,70),(40,side*14.4,62),(40,side*13,59)],[(1.2,3),(1.8,6),(2,6.5),(1.6,4.6),(.15,.35)],bone=f'ear_{label}',sides=28)

tail_surface=[(-43,0,49),(-57,-1,44.5),(-73,-2,36),(-90,-2.5,27),(-105,-2,25)]
tail=loft('Long feathered tail',tail_surface,[(5.0,4.8),(4.7,7.0),(3.6,8.0),(2.1,5.5),(.06,.08)],sides=28)
for i,v in enumerate(tail.data.vertices):
    p=v.co;values=sorted([(1/max(distance_segment(p,*segments[f'tail_{j}']),2)**4,f'tail_{j}') for j in range(1,5)],reverse=True)[:2];total=sum(a for a,b in values)
    for value,name in values:(tail.vertex_groups.get(name) or tail.vertex_groups.new(name=name)).add([i],value/total,'REPLACE')

# Facial planes, open smiling mouth, dark lip rims, rounded nose and tongue.
loft('Open mouth cavity',[(55,0,66),(65,0,65),(74,0,66),(76,0,67)],[(5,3),(6.5,3),(5.5,2.3),(2.8,1)],'Mouth','head',sides=24)
loft('Lower jaw',[(54,0,64),(64,0,60.8),(72,0,61.7),(77,0,64)],[(5.6,3.5),(6.4,2.8),(6,2.5),(3.2,1.4)],bone='jaw',sides=24)
for side in (-1,1):
    line('Natural dark lip',[(56,side*6.3,66.7),(62,side*7.6,64.8),(71,side*6.5,65.3),(76,side*4.2,68.1)],.22,'Mouth','head')
    line('Lower lip',[(56,side*4.5,62.6),(65,side*6.1,61.3),(74,side*4.9,63.2)],.22,'Mouth','jaw')
    for x,z in ((61,65.3),(70,66.8)):
        loft('Canine tooth',[(x,side*6.8,z+1),(x+.5,side*6.6,z-1.3),(x+1,side*6.2,z-2.1)],[.9,.6,.08],'Teeth','head',sides=12)
loft('Relaxed pink tongue',[(60,0,63),(68,0,62),(76,0,60.8),(79,0,60.5),(81,0,60.8)],[(2.9,.8),(4.2,.8),(4.4,.7),(3.7,.6),(.2,.2)],'Tongue','jaw',sides=24)
line('Tongue center fold',[(67,0,62.9),(72,0,62.3),(77,0,61.7)],.075,'Mouth','jaw')
nose=ellipsoid('Broad black nose',(78.1,0,73.2),(3.6,5.1,3.3),'Nose','head',segments=32,rings=20)
for v in nose.data.vertices:v.co.y*=.68+.32*(v.co.z/3.3+1)*.5
for side in (-1,1):
    ellipsoid('Nostril',(80.6,side*2.9,73.4),(.65,1.24,.75),'Mouth','head',segments=20,rings=12)
line('Philtrum',[(79,0,71.2),(77.7,0,68.1)],.24,'Mouth','head')
for side in (-1,1):
    # Small dark eyes under soft pale brows, not enlarged cartoon eyeballs.
    center=Vector((57.5,side*9.9,80.0));normal=Vector((.67,side*.74,.08)).normalized()
    eye=ellipsoid('Dark brown eye',center,(1,1,1),'Eye','head',segments=28,rings=18)
    tangent=Vector((-normal.y,normal.x,0));up=Vector((0,0,1))
    for v in eye.data.vertices:
        q=v.co.copy();v.co=tangent*q.x*1.7+up*q.z*1.22+normal*q.y*.68
    rim=[center+normal*.20+tangent*(math.cos(a)*1.86)+up*(math.sin(a)*1.42) for a in [j*2*math.pi/20 for j in range(21)]]
    line('Pigmented eyelid',rim,.17,'Nose','head')
    for fore,x in ((True,31),(False,-31)):
        y=side*12.7;bone=f'{"fore" if fore else "hind"}_paw_{"L" if side>0 else "R"}'
        for toe in (-1,0,1):
            line('Toe crease',[(x+1,y+toe*2.2,6.6),(x+4.2,y+toe*2.2,3.7)],.10,'Nose',bone)
            ellipsoid('Short dark claw',(x+4.7,y+toe*2.2,2.5),(1.1,.46,.42),'Nose',bone,segments=12,rings=8)

# Turquoise woven collar over the lower neck, black edge and plain bone tag.
neck_axis=Vector((.48,0,.88)).normalized();center=Vector((33,0,61));side_axis=Vector((0,1,0));cross=neck_axis.cross(side_axis)
def collar_loop(name,along,radius,width,material):
    pts=[center+neck_axis*along+side_axis*(math.cos(a)*radius)+cross*(math.sin(a)*(radius+.7)) for a in [i*2*math.pi/64 for i in range(65)]]
    line(name,pts,width,material,'neck')
collar_loop('Turquoise collar',0,14.7,1.8,'Collar')
for edge in (-1.5,1.5):collar_loop('Black collar binding',edge,14.9,.35,'CollarEdge')
tagpos=center-cross*16+Vector((1.5,0,-4))
line('Tag attachment ring',[tagpos+Vector((0,math.cos(a)*1.2,math.sin(a)*1.2+2)) for a in [i*2*math.pi/20 for i in range(21)]],.22,'Metal','neck')
loft('Plain bone tag',[(tagpos.x,tagpos.y-2,tagpos.z-1),(tagpos.x,tagpos.y+2,tagpos.z-1)],[.72,.72],'Metal','neck',sides=12)
for y in (-2.,2.):
    for z in (-.7,.7):ellipsoid('Bone tag lobe',tagpos+Vector((0,y,z-1)),(.4,1.0,.8),'Metal','neck',segments=14,rings=8)
letters=bpy.data.curves.new('Rex tag inscription','FONT');letters.body='Rex';letters.size=1.35;letters.align_x='CENTER';letters.extrude=.035
text=bpy.data.objects.new('Rex tag inscription',letters);bpy.context.collection.objects.link(text)
text.location=tagpos+Vector((.76,0,-1.45));text.rotation_euler=Matrix(((0,0,1),(1,0,0),(0,1,0))).to_euler()
active(text);bpy.ops.object.convert(target='MESH');finish(bpy.context.object,'Rex tag inscription','Nose','neck')

# Final opaque coat and feathering are authored by Source/likeness.py.
# The superseded masked-ribbon pass was removed; it was discarded by that pass.
def coat_color(p):
    x,y,z=p;gold=Vector((.48,.29,.135));cream=Vector((.77,.69,.55))
    face=max(0,min(1,(x-47)/17))*max(0,min(1,(86-z)/15))
    blaze=max(0,1-abs(y)/8)*max(0,min(1,(x-48)/12))*.48
    chest=max(0,min(1,(x-20)/18))*max(0,min(1,(58-z)/15))*.62
    feet=max(0,min(1,(12-z)/10))*.78
    ear=max(0,min(1,(abs(y)-10)/5))*max(0,1-abs(x-41)/9)*max(0,1-abs(z-70)/15)
    c=gold.lerp(cream,max(face,blaze,chest,feet));c*=1-ear*.14
    varied=1+.035*math.sin(x*1.41+y*.79+z*2.12)+.025*math.sin(x*.21-z*.38)
    return (*[max(0,min(1,a*varied)) for a in c],1)
for o in PARTS:
    active(o);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    attr=o.data.color_attributes.new(name='CoatColor',type='FLOAT_COLOR',domain='CORNER')
    o.data.color_attributes.active_color=attr
    uv=o.data.uv_layers.new(name='UVMap')
    for loop in o.data.loops:
        p=o.data.vertices[loop.vertex_index].co
        attr.data[loop.index].color=coat_color(p) if o.data.materials[0] in (MATS['Coat'],MATS['FurCard']) else (1,1,1,1)
        uv.data[loop.index].uv=((loop.vertex_index%2),(loop.vertex_index//2)/2) if o.data.materials[0]==MATS['FurCard'] else (p.y/8+p.z/31,p.x/8+p.z/19)

bpy.ops.object.select_all(action='DESELECT')
for o in PARTS:o.select_set(True)
bpy.context.view_layer.objects.active=skin;bpy.ops.object.join();dog=bpy.context.object;dog.name='SK_CompanionDog';dog.data.name='CompanionDog_Geometry'

def refine_rex_face_and_coat(dog):
    """Deterministic cosmetic finish, also usable on the saved editable source."""
    import bmesh
    mesh=dog.data
    parents=list(range(len(mesh.vertices)))
    def find(i):
        while parents[i]!=i:parents[i]=parents[parents[i]];i=parents[i]
        return i
    for edge in mesh.edges:
        a,b=map(find,edge.vertices)
        if a!=b:parents[b]=a
    components={}
    for vertex in mesh.vertices:components.setdefault(find(vertex.index),[]).append(vertex.index)
    material={}
    for face in mesh.polygons:material[find(face.vertices[0])]=mesh.materials[face.material_index].name
    remove=set()
    for key,indices in components.items():
        name=material[key];points=[mesh.vertices[i].co.copy() for i in indices]
        center=sum(points,Vector())/len(points)
        span=[max(p[j] for p in points)-min(p[j] for p in points) for j in range(3)]
        if name=='DM_Mouth' and span[0]>8 and span[2]<6:
            # Separate drawn lip tubes and tongue stripe looked detached.
            remove.update(indices);continue
        if name=='DM_FurCard':
            # Retain close coat and tail feathering, remove dangling leg/chest
            # sheets. Ears keep only short contour feathering.
            if (center.x>26 and center.z<60) or (center.z<34 and center.x>-45) or (35<center.x<46 and 64<center.z<79 and abs(center.y)>15):
                remove.update(indices);continue
            if len(indices)==6:
                root=(points[0]+points[1])*.5
                # Short dense ribbons avoid isolated long strands over skin.
                for i,p in zip(indices,points):mesh.vertices[i].co=root+(p-root)*(.75 if center.x<-43 else .55)
        elif name=='DM_Tongue':
            for index in indices:
                p=mesh.vertices[index].co
                p.x=60+(p.x-60)*.67
                p.y*=.65
                p.z+=3.8
                # Rounded soft tip, instead of a flat protruding rectangle.
                tip=max(0,min(1,(p.x-70)/3.5))
                p.y*=1-.32*tip
        elif name=='DM_Coat':
            jaw=dog.vertex_groups.get('jaw')
            for index in indices:
                vertex=mesh.vertices[index]
                if jaw and any(g.group==jaw.index and g.weight>.99 for g in vertex.groups):
                    vertex.co.z+=3.5
                    vertex.co.y*=.94
        elif name=='DM_Mouth' and span[0]>18 and span[1]>9:
            for index in indices:
                p=mesh.vertices[index].co;p.z=67.2+(p.z-66)*.43
        elif name=='DM_Teeth':
            for index in indices:
                p=mesh.vertices[index].co;p.z=67+(p.z-67)*.5
    bm=bmesh.new();bm.from_mesh(mesh);bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm,geom=[bm.verts[i] for i in sorted(remove)],context='VERTS')
    bm.to_mesh(mesh);bm.free();mesh.update()
    attr=mesh.color_attributes.get('CoatColor')
    for face in mesh.polygons:
        if mesh.materials[face.material_index].name not in ('DM_Coat','DM_FurCard'):continue
        for li in face.loop_indices:
            color=attr.data[li].color
            # Warmer honey-gold body with the pale muzzle/chest preserved.
            cream=max(0,min(1,(color[1]-.35)/.30))
            attr.data[li].color=(min(1,color[0]*(1.14-.10*cream)),min(1,color[1]*1.04),color[2]*.97,1)
    mesh.calc_loop_triangles()

refine_rex_face_and_coat(dog)
# Keep the focused likeness pass independently editable without regenerating the
# initial anatomical union during art iteration.
import importlib.util
likeness_spec=importlib.util.spec_from_file_location('rex_likeness',ART/'Source/likeness.py')
likeness=importlib.util.module_from_spec(likeness_spec);likeness_spec.loader.exec_module(likeness);likeness.apply(dog)
dog.parent=rig
mod=dog.modifiers.new('Anatomical skinning','ARMATURE');mod.object=rig;mod.use_deform_preserve_volume=True
# FBX exports plain linear blend weights; limiting influences also keeps runtime cheap.
active(dog);bpy.ops.object.vertex_group_limit_total(limit=4);bpy.ops.object.vertex_group_normalize_all(lock_active=False)
dog.data.calc_loop_triangles()

def local_rotation(name,world_axis,radians):
    bone=rig.pose.bones[name];axis=rig.data.bones[name].matrix_local.to_3x3().inverted()@Vector(world_axis)
    bone.rotation_mode='QUATERNION';bone.rotation_quaternion=Quaternion(axis,radians)
def key(name,frame):
    bone=rig.pose.bones[name];bone.keyframe_insert('rotation_quaternion',frame=frame);bone.keyframe_insert('location',frame=frame)
def make_clip(name,frames,walk):
    for b in rig.pose.bones:b.rotation_mode='QUATERNION';b.rotation_quaternion=Quaternion();b.location=(0,0,0)
    action=bpy.data.actions.new(name);rig.animation_data_create();rig.animation_data.action=action
    for frame in range(1,frames+1):
        phase=(frame-1)/(frames-1)*2*math.pi
        for b in rig.pose.bones:b.rotation_quaternion=Quaternion();b.location=(0,0,0)
        local_rotation('spine',(0,1,0),math.sin(phase*(2 if walk else 1))*(.014 if walk else .006))
        local_rotation('neck',(0,1,0),math.sin(phase+.3)*(.025 if walk else .018))
        local_rotation('head',(0,0,1),math.sin(phase*.999)*(.027 if walk else .05))
        local_rotation('jaw',(0,1,0),math.sin(phase*2)*.012)
        if walk:
            for limb,offset in [('fore_L',0),('hind_R',.05),('fore_R',math.pi),('hind_L',math.pi+.05)]:
                kind,label=limb.split('_');s=math.sin(phase+offset)
                local_rotation(f'{kind}_upper_{label}',(0,1,0),s*(.34 if kind=='fore' else .30))
                local_rotation(f'{kind}_lower_{label}',(0,1,0),max(0,-s)*(.53 if kind=='fore' else -.58))
                local_rotation(f'{kind}_paw_{label}',(0,1,0),-s*.18)
        for side,label in ((-1,'R'),(1,'L')):local_rotation(f'ear_{label}',(1,0,0),math.sin(phase*2+side*.4)*.035)
        for i in range(1,5):local_rotation(f'tail_{i}',(0,0,1),math.sin(phase*2-i*.48)*(.15 if walk else .12))
        for b in rig.pose.bones:key(b.name,frame)
    action.use_fake_user=True
    return action
clips={'A_DogIdle':make_clip('A_DogIdle',91,False),'A_DogWalk':likeness.author_walk(rig)}
rig.animation_data.action=None
for b in rig.pose.bones:b.rotation_quaternion=Quaternion();b.location=(0,0,0)
scene.frame_set(1)

def export(path,animation=False):
    bpy.ops.object.select_all(action='DESELECT');dog.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH','ARMATURE'} if not animation else {'ARMATURE'},
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',axis_forward='-Y',axis_up='Z',add_leaf_bones=False,
        armature_nodetype='NULL',use_armature_deform_only=True,mesh_smooth_type='FACE',colors_type='SRGB',
        bake_anim=animation,bake_anim_use_all_actions=False,bake_anim_use_nla_strips=False,bake_anim_simplify_factor=0,
        bake_anim_force_startend_keying=True,path_mode='STRIP')
export(ART/'Exports/SK_CompanionDog.fbx')
for name,action in clips.items():
    rig.animation_data.action=action;scene.frame_start=1;scene.frame_end=91 if name.endswith('Idle') else likeness.WALK_FRAMES
    export(ART/'Exports'/(name+'.fbx'),True)
rig.animation_data.action=clips['A_DogIdle'];scene.frame_start=1;scene.frame_end=91;scene.frame_set(1)

lo=Vector(tuple(min(v.co[i] for v in dog.data.vertices) for i in range(3)));hi=Vector(tuple(max(v.co[i] for v in dog.data.vertices) for i in range(3)))
report={'asset':'SK_CompanionDog','source':'Original authored geometry, rig, animation and generated coat texture guided by private reference photographs; photographs are not included or sampled into textures',
    'forward_axis':'+X','up_axis':'+Z','units':'centimeters','root_motion':False,'reference_pose_bounds_cm':{'min':list(lo),'max':list(hi)},
    'triangles':len(dog.data.loop_triangles),'vertices':len(dog.data.vertices),'bones':[n for n,a,b,p in spec],
    'materials':{m.name:{'color':list(PALETTE[m.name[3:]][0]),'roughness':PALETTE[m.name[3:]][1],'metallic':PALETTE[m.name[3:]][2]} for m in dog.data.materials},
    'used_materials':sorted({dog.data.materials[p.material_index].name for p in dog.data.polygons}),
    'coat_texture_provenance':{'file':'Textures/T_Dog_CoatDetail.png','method':'OpenAI imagegen, text-to-image; no reference-image inputs','prompt_record':'TEXTURE_PROVENANCE.md','reference_photo_pixels_used':False},
    'animations':{'A_DogIdle':{'duration_seconds':3,'fps':30,'loop':True},'A_DogWalk':likeness.walk_metadata(clips['A_DogWalk'])},
    'coat_vertex_attribute':'CoatColor','max_bone_influences':4,'dog_name':'Rex','name_on_tag':True,
    'cosmetic_revision':'Rex likeness revision4: broader shorter cream muzzle, smaller recessed almond eyes with fitted lids, fuller panting jowls and chin, leaner chest, shaped toes and redistributed chest/ear/leg feathering; original geometry and retained planted-foot walk',
    'exports':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ART/'Exports').glob('*.fbx')},'unreal_import_pending':True,'runtime_review_pending':True}
(ART/'dog_manifest.json').write_text(json.dumps(report,indent=2)+'\n')

# Review lighting and cameras are saved separately from exported geometry.
floor_mat=bpy.data.materials.new('PreviewOnly_Backdrop');floor_mat.diffuse_color=(.06,.075,.08,1)
bpy.ops.mesh.primitive_plane_add(size=2000);floor=bpy.context.object;floor.name='PreviewOnly_Floor';floor.data.materials.append(floor_mat)
world=bpy.data.worlds.new('Companion studio') if not scene.world else scene.world;scene.world=world;world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(.15,.18,.21,1);world.node_tree.nodes['Background'].inputs[1].default_value=.5
def area(name,p,power,size):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    o=bpy.data.objects.new(name,data);bpy.context.collection.objects.link(o);o.location=p;o.rotation_euler=(Vector((5,0,45))-o.location).to_track_quat('-Z','Y').to_euler()
area('Large warm key',(110,-130,220),240000,150);area('Soft fill',(-50,150,140),180000,140);area('Coat rim',(-130,-20,180),220000,100)
camera_data=bpy.data.cameras.new('Review camera');cam=bpy.data.objects.new('Review camera',camera_data);bpy.context.collection.objects.link(cam);scene.camera=cam
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1400;scene.render.resolution_y=1050;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
def render(name,position,target,lens=55):
    cam.location=position;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='PERSP';cam.data.lens=lens
    scene.render.filepath=str(ART/'Previews'/(name+'.png'));bpy.ops.render.render(write_still=True)
cam.location=(210,-245,125);cam.rotation_euler=(Vector((0,0,43))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.lens=55
for img in bpy.data.images:
    if img.filepath:img.filepath=bpy.path.relpath(img.filepath,start=str(ART/'Source'))
source_path=ART/'Source/CompanionDog.blend'
try:
    bpy.ops.wm.save_as_mainfile(filepath=str(source_path))
except RuntimeError:
    # Windows can refuse Blender's atomic rename while permitting the existing
    # file to be overwritten. Blender preserves its complete new file with @.
    pending=Path(str(source_path)+'@')
    if not pending.is_file():raise
    contents=pending.read_bytes()
    if not contents.startswith(b'BLENDER'):raise
    source_path.write_bytes(contents);pending.unlink()
    print('Recovered complete Blender save after Windows rename failure',flush=True)
render('dog_three_quarter',(210,-245,125),(0,0,43),55)
render('dog_face',(180,-115,106),(55,0,72),70)
render('dog_profile',(8,-280,83),(-10,0,43),48)
rig.animation_data.action=clips['A_DogWalk'];scene.frame_set(8)
render('dog_walk_pose',(140,-240,92),(0,0,40),55)
print('COMPANION_DOG_READY '+json.dumps(report),flush=True)

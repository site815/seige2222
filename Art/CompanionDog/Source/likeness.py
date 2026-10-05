"""Photo-guided Rex anatomical and short-coat refinement; original geometry only."""
import bpy,bmesh,math,random
from mathutils import Vector, Quaternion
from mathutils.bvhtree import BVHTree

def apply(dog):
    if dog.get('rex_likeness_revision')==4:return
    mesh=dog.data;mesh.update()
    slots={m.name:i for i,m in enumerate(mesh.materials)}
    parents=list(range(len(mesh.vertices)))
    def find(i):
        while parents[i]!=i:parents[i]=parents[parents[i]];i=parents[i]
        return i
    for edge in mesh.edges:
        a,b=map(find,edge.vertices)
        if a!=b:parents[b]=a
    comps={};mats={}
    for v in mesh.vertices:comps.setdefault(find(v.index),[]).append(v.index)
    for face in mesh.polygons:mats[find(face.vertices[0])]=mesh.materials[face.material_index].name
    skin=max(comps.values(),key=len);skin_set=set(skin)
    remove=set()
    for key,indices in comps.items():
        mat=mats[key];p=sum((mesh.vertices[i].co for i in indices),Vector())/len(indices)
        jaw=dog.vertex_groups.get('jaw')
        is_jaw=jaw and all(any(g.group==jaw.index and g.weight>.99 for g in mesh.vertices[i].groups) for i in indices)
        if mat in ('DM_FurCard','DM_Eye','DM_Teeth','DM_Tongue','DM_Mouth','DM_Collar','DM_CollarEdge') or (is_jaw and mat=='DM_Coat') or (mat=='DM_Nose' and p.z>68):remove.update(indices)
    # Less spherical cranium and jowls, a firm long bridge, lean adult trunk.
    for v in mesh.vertices:
        p=v.co
        if v.index in skin_set:
            if p.x>27 and p.z>62:
                if p.z>80:p.z=80+(p.z-80)*.69
                t=max(0,min(1,(p.x-54)/23))
                if p.x>54:
                    p.x=54+(p.x-54)*.90
                    p.y*=1+.06*t
                    p.z=72+(p.z-72)*(1-.15*t)-1.3*t
                # Distinct nasal bridge without two inflated spherical jowls.
                p.z-=.45*math.exp(-((p.x-60)/7)**2-(p.y/5)**2)
            elif p.z>25 and p.x<40:
                leg=min(1,abs(p.y)/10)*max(0,min(1,(39-p.z)/14))
                t=max(0,min(1,(40-p.x)/22));t=t*t*(3-2*t)
                p.y*=1-.18*(1-leg)*t*max(0,min(1,(p.z-25)/10))
                p.z+=7.0*math.exp(-((p.x+12)/24)**2)*max(0,min(1,(42-p.z)/18))*t
                # A broad rounded loin, not the previous pointed pelvic hump.
                if -47<p.x<22 and p.z>57:
                    cap=61.4+1.0*math.exp(-((p.x-12)/25)**2)
                    p.z=cap-math.log1p(math.exp(min(30,(cap-p.z)*1.1)))/1.1
        else:
            # Reconnect the back of the lower jaw to the cheek.
            jaw=dog.vertex_groups.get('jaw')
            if jaw and any(g.group==jaw.index and g.weight>.99 for g in v.groups):
                p.z+=max(0,1-(p.x-54)/10)*1.6
    bm=bmesh.new();bm.from_mesh(mesh);bm.verts.ensure_lookup_table()
    bmesh.ops.delete(bm,geom=[bm.verts[i] for i in remove],context='VERTS');bm.to_mesh(mesh);bm.free();mesh.update()
    # Compact source so real fine fur geometry has a sensible total budget.
    dog.select_set(True);bpy.context.view_layer.objects.active=dog
    dec=dog.modifiers.new('Close-view surface budget','DECIMATE');dec.ratio=.76
    bpy.ops.object.modifier_apply(modifier=dec.name)
    mesh=dog.data;mesh.update()
    coat=mesh.color_attributes.get('CoatColor')
    def color(p):
        x,y,z=p;gold=Vector((.58,.29,.09));cream=Vector((.90,.84,.73))
        face=max(0,min(1,(x-47)/16))*max(0,min(1,(89-z)/10))
        eyes=max(0,1-abs(x-58)/10)*max(0,1-abs(z-80)/6)
        chest=max(0,min(1,(x-19)/16))*max(0,min(1,(66-z)/18))*.9
        feet=max(0,min(1,(29-z)/22))*.97
        # Golden ear feathering frames a much paler face, as in Rex's photos.
        ear=max(0,min(1,(abs(y)-11)/4))*max(0,1-abs(x-41)/10)
        pale=max(face,eyes*.72,chest,feet)*(1-ear*.9)
        c=gold.lerp(cream,pale)
        c*=1+.025*math.sin(x*.31-z*.37+y*.7)
        return (*c,1)
    for f in mesh.polygons:
        if mesh.materials[f.material_index].name not in ('DM_Coat','DM_FurCard'):continue
        for li in f.loop_indices:coat.data[li].color=color(mesh.vertices[mesh.loops[li].vertex_index].co)
    bvh=BVHTree.FromPolygons([v.co for v in mesh.vertices],[list(p.vertices) for p in mesh.polygons])
    parts=[]
    def obj(name,verts,faces,material,bone='head',colors=None,weights=None,normals=None):
        m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
        o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o);m.materials.append(bpy.data.materials[material])
        for f in m.polygons:f.use_smooth=True
        if normals:m.normals_split_custom_set_from_vertices(normals)
        attr=m.color_attributes.new(name='CoatColor',type='FLOAT_COLOR',domain='CORNER');m.color_attributes.active_color=attr
        uv=m.uv_layers.new(name='UVMap')
        for loop in m.loops:
            p=m.vertices[loop.vertex_index].co
            attr.data[loop.index].color=colors[loop.vertex_index] if colors else color(p) if material=='DM_Coat' else (1,1,1,1)
            uv.data[loop.index].uv=(p.y/8+p.z/31,p.x/8+p.z/19)
        if weights:
            groups={n:o.vertex_groups.new(name=n) for n in dog.vertex_groups.keys()}
            for i,values in enumerate(weights):
                for n,w in values:groups[n].add([i],w,'REPLACE')
        else:o.vertex_groups.new(name=bone).add(list(range(len(verts))),1,'REPLACE')
        parts.append(o);return o
    def ellipsoid(name,center,scale,material,bone='head'):
        verts=[Vector(center)+Vector((scale[0],0,0))];faces=[]
        for i in range(1,16):
            phi=math.pi*i/16
            for j in range(32):
                a=math.tau*j/32;verts.append(Vector(center)+Vector((math.cos(phi)*scale[0],math.sin(phi)*math.cos(a)*scale[1],math.sin(phi)*math.sin(a)*scale[2])))
        end=len(verts);verts.append(Vector(center)-Vector((scale[0],0,0)))
        for j in range(32):faces.append((0,1+j,1+(j+1)%32))
        for i in range(14):
            for j in range(32):faces.append((1+i*32+j,1+(i+1)*32+j,1+(i+1)*32+(j+1)%32,1+i*32+(j+1)%32))
        for j in range(32):faces.append((end,1+14*32+(j+1)%32,1+14*32+j))
        return obj(name,verts,faces,material,bone)
    def tube(name,points,radius,material,bone='head'):
        if len(points)<12:
            controls=points;points=[]
            for i in range(len(controls)-1):
                a=controls[max(0,i-1)];b=controls[i];c=controls[i+1];d=controls[min(len(controls)-1,i+2)]
                for j in range(8):
                    t=j/8;points.append(.5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t))
            points.append(controls[-1])
        verts=[];faces=[]
        for i,p in enumerate(points):
            tangent=(points[min(len(points)-1,i+1)]-points[max(0,i-1)]).normalized()
            a=tangent.cross(Vector((0,0,1))).normalized()
            if a.length<.1:a=Vector((0,1,0))
            b=tangent.cross(a)
            for j in range(6):verts.append(p+(a*math.cos(j*math.tau/6)+b*math.sin(j*math.tau/6))*radius)
            if i:
                for j in range(6):faces.append(((i-1)*6+j,(i-1)*6+(j+1)%6,i*6+(j+1)%6,i*6+j))
        return obj(name,verts,faces,material,bone)
    def loft(name,points,radii,material,bone='head',sides=24):
        verts=[];faces=[];points=[Vector(p) for p in points]
        for i,p in enumerate(points):
            tangent=(points[min(len(points)-1,i+1)]-points[max(0,i-1)]).normalized()
            side=Vector((0,1,0));side=(side-tangent*side.dot(tangent)).normalized();up=tangent.cross(side)
            ry,rz=radii[i]
            for j in range(sides):
                a=j*math.tau/sides;verts.append(p+side*math.cos(a)*ry+up*math.sin(a)*rz)
            if i:
                for j in range(sides):faces.append(((i-1)*sides+j,(i-1)*sides+(j+1)%sides,i*sides+(j+1)%sides,i*sides+j))
        faces.extend([tuple(reversed(range(sides))),tuple((len(points)-1)*sides+j for j in range(sides))])
        return obj(name,verts,faces,material,bone)
    # Readable curved corneas fitted to the sculpt, with a warm brown iris and
    # dark pupil. The old planar eye was almost entirely buried in the brow.
    for side in (-1,1):
        n=Vector((.81,side*.58,.045)).normalized();t=Vector((-n.y,n.x,0)).normalized();up=n.cross(t).normalized()
        aim=Vector((57.0,side*9.0,79.1));hit=bvh.ray_cast(aim+n*20,-n,40)
        center=(hit[0] if hit[0] is not None else aim)+n*.07
        boundary=[];outer=[]
        for j in range(48):
            a=j*math.tau/48;u=math.cos(a);v=math.sin(a)
            q=center+t*u*2.04+up*v*(1.38 if v>0 else 1.08)*(1-.20*abs(u))
            contact=bvh.ray_cast(q+n*6,-n,12)
            q=(contact[0] if contact[0] is not None else q)+n*.09
            boundary.append(q)
            q2=center+t*u*2.95+up*v*(2.20 if v>0 else 1.66)
            contact=bvh.ray_cast(q2+n*6,-n,12)
            outer.append((contact[0] if contact[0] is not None else q2)+n*.025)
        verts=[center+n*.48];faces=[]
        for ring,r in enumerate((.33,.66,1)):
            for p in boundary:verts.append(center.lerp(p,r)+n*.48*(1-r*r))
            if ring==0:
                for j in range(48):faces.append((0,1+j,1+(j+1)%48))
            else:
                for j in range(48):faces.append((1+(ring-1)*48+j,1+ring*48+j,1+ring*48+(j+1)%48,1+(ring-1)*48+(j+1)%48))
        obj('Curved brown eye',verts,faces,'DM_Eye')
        for name,radius,depth,material in [('Warm brown iris',1.00,.54,'DM_Iris'),('Dark round pupil',.67,.58,'DM_Eye')]:
            verts=[center+n*depth];faces=[]
            for j in range(48):
                a=j*math.tau/48;verts.append(center+t*math.cos(a)*radius+up*math.sin(a)*radius+n*(depth-.17))
                faces.append((0,j+1,(j+1)%48+1))
            obj(name,verts,faces,material)
        middle=[a.lerp(b,.48)+n*.13 for a,b in zip(boundary,outer)]
        socket_colors=[(.18,.105,.052,1)]*48+[tuple(Vector(color(p)[:3])*.89)+(1,) for p in middle]+[color(p) for p in outer]
        obj('Soft fitted eyelid socket',boundary+middle+outer,
            [(ring*48+j,(ring+1)*48+j,(ring+1)*48+(j+1)%48,ring*48+(j+1)%48) for ring in range(2) for j in range(48)],
            'DM_Coat',colors=socket_colors)
        tube('Soft dark waterline',boundary+[boundary[0]],.09,'DM_Mouth')
    nose=ellipsoid('Sculpted triangular nose',(75.6,0,71.1),(1.8,5.0,3.1),'DM_Nose')
    for v in nose.data.vertices:
        p=v.co;local=p-Vector((75.6,0,71.1));p.y*=.70+.30*max(0,min(1,(local.z+3.1)/6.2))
        if local.z>1.5:p.z=72.6+(local.z-1.5)*.62
        for side in (-1,1):p.x-=.65*math.exp(-((local.y-side*2.8)/.95)**2-((local.z-.2)/.85)**2)*max(0,local.x/1.8)
    for side in (-1,1):
        # Recessed dark cavities sit inside the nose, never as raised knobs.
        ellipsoid('Recessed nostril',(76.95,side*2.6,71.25),(.05,.83,.57),'DM_Mouth')
    # Open pant: a dark continuous oral cavity, integrated chin and a soft
    # rounded tongue. The tongue stays inside the muzzle width, with no slabs.
    jaw=loft('Connected lower jaw',[(50,0,66),(55,0,62.5),(62,0,59.0),(69,0,58.8),(73,0,60.0),(74.6,0,61.0)],[(4.0,5.0),(6.3,4.3),(6.6,3.2),(6.1,2.8),(4.8,2.2),(.4,.5)],'DM_Coat','jaw')
    bpy.ops.object.select_all(action='DESELECT');jaw.select_set(True);bpy.context.view_layer.objects.active=jaw
    sub=jaw.modifiers.new('Rounded chin and cheek continuity','SUBSURF');sub.levels=1;bpy.ops.object.modifier_apply(modifier=sub.name)
    # A recessed hollow shell leaves actual space for the tongue; the previous
    # convex black ellipsoid filled the opening like a solid rubber slab.
    cavity_verts=[];cavity_faces=[];sides=32
    for x,width,floor,roof in [(51,3.7,63,68),(56,5.2,61,69),(63,5.7,60.2,69),(70,3.7,60.6,67.0)]:
        for j in range(sides):
            a=j*math.tau/sides;cavity_verts.append(Vector((x,math.cos(a)*width,(floor+roof)/2+math.sin(a)*(roof-floor)/2)))
    for ring in range(3):
        for j in range(sides):cavity_faces.append((ring*sides+j,(ring+1)*sides+j,(ring+1)*sides+(j+1)%sides,ring*sides+(j+1)%sides))
    cavity_faces.append(tuple(range(sides)))
    obj('Recessed open oral shell',cavity_verts,cavity_faces,'DM_Mouth','jaw')
    for side in (-1,1):
        # The mouth cavity itself supplies the lip boundary. A separate dark
        # line across the pale chin read as a drawn cartoon smile.
        loft('Small upper canine',[(61,side*5.5,67),(61.2,side*5.4,66.0),(61.35,side*5.25,65.6)],[(.38,.42),(.22,.25),(.035,.045)],'DM_Teeth')
    tongue=ellipsoid('Rounded panting tongue',(69.1,0,61.8),(6.5,3.9,1.0),'DM_Tongue','jaw')
    for v in tongue.data.vertices:
        v.co.z-=1.35*max(0,min(1,(v.co.x-70)/5))**2
    # Tapered overlapping feather locks form the tail silhouette in actual
    # geometry; they remain bound to the same four animated tail bones.
    feather_rng=random.Random(2222606);tail_verts=[];tail_faces=[];tail_weights=[]
    for i in range(2200):
        t=feather_rng.uniform(.12,3.82);k=min(3,int(t));f=t-k
        anchors=[Vector(p) for p in [(-43,0,49),(-57,-1,47),(-73,-2,40),(-90,-2.5,30),(-105,-2,25)]]
        p=anchors[k].lerp(anchors[k+1],f)
        side=-1 if i%2 else 1;p.y+=side*feather_rng.uniform(.9,4.2)*(1-t*.15);p.z-=feather_rng.uniform(1,5)
        length=feather_rng.uniform(7,14)*math.sin(math.pi*(t+.1)/4.4)
        end=p+Vector((-length*.7,side*length*.09,-length*.70))
        across=Vector((0,1,side*.2)).normalized()*feather_rng.uniform(.045,.12)
        mid=p.lerp(end,.54)+Vector((0,0,.5));offset=len(tail_verts)
        tail_verts.extend([p-across,p+across,mid+across*.65,end]);tail_faces.extend([(offset,offset+1,offset+2),(offset,offset+2,offset+3)])
        tail_weights.extend([[(f'tail_{k+1}',1)]]*4)
    obj('Fine flowing tail feathering',tail_verts,tail_faces,'DM_Coat',weights=tail_weights)
    # Thin flat woven strap rather than a padded tube. The dark bindings are
    # part of the same flat profile and skin with the neck.
    axis=Vector((.48,0,.88)).normalized();side=Vector((0,1,0));up=axis.cross(side)
    center=Vector((33,0,61))
    def band(name,along,width,radius,thickness,material):
        verts=[];faces=[];segments=96
        for offset,r in ((along-width/2,radius),(along+width/2,radius),(along+width/2,radius-thickness),(along-width/2,radius-thickness)):
            for j in range(segments):
                a=j*math.tau/segments;radial=side*math.cos(a)+up*math.sin(a)*1.0476
                verts.append(center+axis*offset+radial*r)
        for ring in range(4):
            for j in range(segments):faces.append((ring*segments+j,ring*segments+(j+1)%segments,((ring+1)%4)*segments+(j+1)%segments,((ring+1)%4)*segments+j))
        return obj(name,verts,faces,material,'neck')
    band('Flat turquoise woven collar',0,3.25,16.05,.32,'DM_Collar')
    for edge in (-1.48,1.48):band('Fine black collar binding',edge,.28,16.16,.18,'DM_CollarEdge')
    # Original fine opaque strands. Each follows the surface and inherits its
    # exact skin weights, avoiding long disconnected cards and masked overdraw.
    mesh.calc_loop_triangles();R=random.Random(2222815)
    triangles=[f for f in mesh.loop_triangles if mesh.materials[f.material_index].name=='DM_Coat']
    areas=[];total=0
    for f in triangles:total+=f.area;areas.append(total)
    import bisect
    verts=[];faces=[];colors=[];weights=[];normals=[]
    for i in range(30000):
        f=triangles[bisect.bisect_left(areas,R.random()*total)];vs=[mesh.vertices[k] for k in f.vertices]
        a=math.sqrt(R.random());b=R.random();bary=(1-a,a*(1-b),a*b)
        p=sum((v.co*w for v,w in zip(vs,bary)),Vector());n=sum((v.normal*w for v,w in zip(vs,bary)),Vector()).normalized()
        if p.z<5:continue
        is_face=p.x>45 and p.z>64
        ear=abs(p.y)>11 and 33<p.x<47 and p.z>58
        flow=Vector((-1,0,-.22)) if p.x<22 else Vector((-.1,p.y*.018,-1))
        if is_face:flow=Vector((.3,p.y*.045,-.7))
        tangent=flow-n*flow.dot(n)
        if tangent.length<.1:tangent=n.cross(Vector((0,1,0)))
        tangent.normalize();across=n.cross(tangent).normalized()
        feather=(19<p.x<39 and 27<p.z<61) or (-33<p.x<12 and p.z<34) or (ear and p.z<75)
        length=R.uniform(.28,.64) if is_face else R.uniform(2.2,4.8) if feather else R.uniform(.9,2.4) if ear or p.x<-43 else R.uniform(.55,1.25)
        width=R.uniform(.014,.030) if feather else R.uniform(.006,.016);root=p+n*.025
        end=root+tangent*length+n*length*.10
        mid=root.lerp(end,.58)+n*length*.035
        offset=len(verts);points=[root-across*width,root+across*width,mid+across*width*.7,end]
        verts.extend(points);faces.extend([(offset+2,offset+1,offset),(offset+3,offset+2,offset)])
        values={}
        for vertex,w in zip(vs,bary):
            for group in vertex.groups:
                key=dog.vertex_groups[group.group].name;values[key]=values.get(key,0)+group.weight*w
        values=sorted(values.items(),key=lambda p:p[1],reverse=True)[:4];den=sum(w for n,w in values);values=[(n,w/den) for n,w in values]
        c=color(p);variation=R.uniform(.98,1.09);c=tuple(v*variation for v in c[:3])+(1,)
        for q in points:colors.append(c);weights.append(values);normals.append(n)
    obj('Dense short flowing coat',verts,faces,'DM_Coat',colors=colors,weights=weights,normals=normals)
    # Eye colour is dark brown with restrained highlights, not polished black beads.
    p=bpy.data.materials['DM_Eye'].node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(.022,.009,.003,1);p.inputs['Roughness'].default_value=.34;p.inputs['Coat Weight'].default_value=.08
    bpy.ops.object.select_all(action='DESELECT');dog.select_set(True)
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=dog;bpy.ops.object.join()
    # A generated coat swatch supplies coherent fine fibers. Body coloration
    # remains authored vertex data; no photographic reference pixels are used.
    from pathlib import Path
    path=Path(__file__).resolve().parents[1]/'Textures/T_Dog_CoatDetail.png'
    if path.exists():
        image=bpy.data.images.load(str(path),check_existing=True);image.colorspace_settings.name='Non-Color'
        material=bpy.data.materials['DM_Coat'];nodes=material.node_tree.nodes;links=material.node_tree.links
        p=nodes.get('Principled BSDF');detail=nodes.new('ShaderNodeTexImage');detail.image=image
        vc=nodes.new('ShaderNodeVertexColor');vc.layer_name='CoatColor'
        mix=nodes.new('ShaderNodeMixRGB');mix.blend_type='MULTIPLY';mix.inputs[0].default_value=.85
        links.new(vc.outputs['Color'],mix.inputs[1]);links.new(detail.outputs['Color'],mix.inputs[2]);links.new(mix.outputs[0],p.inputs['Base Color'])
        bump=nodes.new('ShaderNodeBump');bump.inputs['Strength'].default_value=.38;bump.inputs['Distance'].default_value=.09
        links.new(detail.outputs['Color'],bump.inputs['Height']);links.new(bump.outputs['Normal'],p.inputs['Normal'])
        # Explicit first UV map avoids inherited primitive UVs being exported.
        while dog.data.uv_layers:dog.data.uv_layers.remove(dog.data.uv_layers[0])
        uv=dog.data.uv_layers.new(name='UVMap')
        for f in dog.data.polygons:
            axis=max(range(3),key=lambda i:abs(f.normal[i]))
            for li in f.loop_indices:
                q=dog.data.vertices[dog.data.loops[li].vertex_index].co
                uv.data[li].uv=(q.y/14,q.z/14) if axis==0 else (q.x/14,q.z/14) if axis==1 else (q.y/14,q.x/14)
    dog['rex_likeness_revision']=4;dog.data.calc_loop_triangles()


WALK_FRAMES=14
WALK_FPS=30
WALK_DURATION=(WALK_FRAMES-1)/WALK_FPS
WALK_SPEED_M_S=5/3.6
WALK_STRIDE_M=WALK_SPEED_M_S*WALK_DURATION
WALK_STANCE=.60

def author_walk(rig):
    """An in-place four-beat walk with fixed-height, constant-speed stance feet.

    Runtime advances the root through the world. Sampling animation by travelled
    distance / WALK_STRIDE_M preserves foot contact through speed changes.
    """
    old=bpy.data.actions.get('A_DogWalk')
    if old:
        if rig.animation_data and rig.animation_data.action==old:rig.animation_data.action=None
        bpy.data.actions.remove(old)
    action=bpy.data.actions.new('A_DogWalk');action.use_fake_user=True
    rig.animation_data_create();rig.animation_data.action=action
    scene=bpy.context.scene;scene.render.fps=WALK_FPS
    def pose_matrix(name,head,tail=None):
        rest=rig.data.bones[name];q=rest.matrix_local.to_quaternion()
        if tail is not None:q=(rest.tail_local-rest.head_local).normalized().rotation_difference((tail-head).normalized())@q
        matrix=q.to_matrix().to_4x4();matrix.translation=head
        rig.pose.bones[name].matrix=matrix;bpy.context.view_layer.update()
    checks=[];step=WALK_STRIDE_M*100*WALK_STANCE
    phases={'fore_L':0,'hind_L':.25,'fore_R':.5,'hind_R':.75}
    for frame in range(1,WALK_FRAMES+1):
        scene.frame_set(frame);phase=(frame-1)/(WALK_FRAMES-1)
        for p in rig.pose.bones:p.rotation_mode='QUATERNION';p.rotation_quaternion=Quaternion();p.location=(0,0,0);p.scale=(1,1,1)
        # A modest flexed walking stance gives knees/elbows room to articulate.
        dz=-5.5+.35*(1-math.cos(phase*math.tau*2))
        rig.pose.bones['pelvis'].location=rig.data.bones['pelvis'].matrix_local.to_3x3().inverted()@Vector((0,0,dz))
        for i in range(1,5):
            p=rig.pose.bones[f'tail_{i}'];axis=rig.data.bones[p.name].matrix_local.to_3x3().inverted()@Vector((0,0,1))
            p.rotation_quaternion=Quaternion(axis,math.sin(phase*math.tau-i*.45)*.08)
        bpy.context.view_layer.update()
        for limb,offset in phases.items():
            kind,side=limb.split('_');u=(phase+offset)%1
            if u<WALK_STANCE:x=step*(.5-u/WALK_STANCE);lift=0
            else:
                t=(u-WALK_STANCE)/(1-WALK_STANCE);smooth=t*t*(3-2*t)
                x=step*(-.5+smooth);lift=6*math.sin(math.pi*t)**1.3
            upper=f'{kind}_upper_{side}';lower=f'{kind}_lower_{side}';paw=f'{kind}_paw_{side}'
            target=rig.data.bones[paw].head_local+Vector((x,0,lift));hip=rig.pose.bones[upper].head.copy()
            delta=target-hip;distance=delta.length;l1=rig.data.bones[upper].length;l2=rig.data.bones[lower].length
            if not abs(l1-l2)<distance<l1+l2:raise RuntimeError(f'Walk target beyond reach: {limb}, frame {frame}')
            direction=delta/distance;a=(l1*l1-l2*l2+distance*distance)/(2*distance)
            height=math.sqrt(max(0,l1*l1-a*a));perp=Vector((direction.z,0,-direction.x)).normalized()
            if kind=='hind':perp=-perp
            knee=hip+direction*a+perp*height
            pose_matrix(upper,hip,knee);pose_matrix(lower,knee,target);pose_matrix(paw,target)
            error=(rig.pose.bones[lower].tail-target).length
            checks.append({'frame':frame,'limb':limb,'stance':u<WALK_STANCE,'target_error_cm':error,'ankle_z_cm':target.z})
        for p in rig.pose.bones:
            p.keyframe_insert('rotation_quaternion',frame=frame);p.keyframe_insert('location',frame=frame)
    # Linear samples keep the planted portion at the authored travel rate.
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for curve in bag.fcurves:
                    for key in curve.keyframe_points:key.interpolation='LINEAR'
    error=max(x['target_error_cm'] for x in checks)
    if error>.005:raise RuntimeError(f'Walk IK target error {error} cm')
    action['stride_meters']=WALK_STRIDE_M;action['duration_seconds']=WALK_DURATION
    action['reference_speed_m_s']=WALK_SPEED_M_S;action['max_ik_target_error_cm']=error
    scene.frame_start=1;scene.frame_end=WALK_FRAMES
    return action

def walk_metadata(action):
    return {'duration_seconds':WALK_DURATION,'fps':WALK_FPS,'loop':True,
            'stride_meters':WALK_STRIDE_M,'reference_speed_m_s':WALK_SPEED_M_S,
            'reference_speed_cm_s':WALK_SPEED_M_S*100,'stance_fraction':WALK_STANCE,
            'phase_source':'distance_walked_meters / stride_meters',
            'max_ik_target_error_cm':action['max_ik_target_error_cm']}

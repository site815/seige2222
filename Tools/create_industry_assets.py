"""Original full-scale industrial colony architecture. Run with Blender --background --python.

No marketplace models or game assets are used. Generates six editable meshes, FBX exports,
original tileable PBR maps, and rendered review images. Dimensions are centimeters.
Pass -- --core-only to replace only the command hub export and refresh the retained
review scene, preserving the other five meshes and all texture files.
"""
from pathlib import Path
import bpy, math, json, random, sys, shutil
import numpy as np
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/"Art"; OUT=ART/"IndustryExports"; TEX=ART/"Textures/Industry"
CORE_ONLY="--core-only" in sys.argv
SOURCE=ART/"Source/Seige_Industry_Architecture.blend"
previous_manifest=json.loads((OUT/"industry_manifest.json").read_text(encoding="utf-8")) if CORE_ONLY else None
if CORE_ONLY and not SOURCE.exists():raise RuntimeError("Core-only refresh requires the retained six-building Blender source")
if CORE_ONLY:
    retained_source=ROOT/"Saved/AssetBuild/industry_before_core_refresh.blend"
    retained_source.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(SOURCE,retained_source)
for p in (OUT,TEX,ART/"Source",ART/"Previews"): p.mkdir(parents=True,exist_ok=True)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.object.select_all(action="SELECT");bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene;scene.unit_settings.system="METRIC";scene.unit_settings.scale_length=.01
PARTS=[];ASSETS={};PALETTE={
    "Ceramic":((.60,.64,.63,1),.38,.43,"Paint",0),
    "Slate":((.105,.155,.16,1),.65,.43,"Paint",0),
    "Steel":((.40,.44,.46,1),.92,.31,"Steel",0),
    "Concrete":((.43,.43,.39,1),0,.86,"Concrete",0),
    "Carbon":((.025,.035,.039,1),.36,.62,"Paint",0),
    "Glass":((.025,.095,.125,1),.65,.17,"Steel",0),
    "Copper":((.34,.16,.075,1),.80,.46,"Steel",0),
    "Yellow":((.76,.43,.065,1),.35,.50,"Paint",0),
    "Blue":((.10,.27,.38,1),.45,.48,"Paint",0),
    "Light":((.54,.84,.87,1),.1,.25,"",2.0),
    "Amber":((1,.23,.025,1),.1,.25,"",1.5),
}
MATS={};manifest={"version":1,"units":"centimeters","forward":"-Y","generator":"Tools/create_industry_assets.py","palette":{},"meshes":dict(previous_manifest["meshes"]) if CORE_ONLY else {},"textures":{}}

def save_image(name,array):
    if CORE_ONLY:
        im=bpy.data.images.load(str(TEX/(name+".png")),check_existing=True);im.name=name;return im
    h,w=array.shape[:2];im=bpy.data.images.new(name,width=w,height=h,alpha=True)
    rgba=np.ones((h,w,4),dtype=np.float32);rgba[:,:,:array.shape[2]]=array
    im.pixels.foreach_set(rgba.ravel());im.filepath_raw=str(TEX/(name+".png"));im.file_format="PNG";im.save();return im

# Authored material surfaces: multiple periodic frequency bands keep every map tileable.
N=1024;yy,xx=np.mgrid[0:N,0:N].astype(np.float32)/N;rng=np.random.default_rng(2222)
for kind in ("Paint","Steel","Concrete"):
    broad=np.zeros((N,N),np.float32)
    for k in range(22):
        fx,fy=rng.integers(1,42,size=2);phase=rng.random()*math.tau
        broad+=np.sin(math.tau*(xx*fx+yy*fy)+phase)/(1+fx+fy)**.45
    broad=(broad-broad.min())/(broad.max()-broad.min());fine=rng.random((N,N)).astype(np.float32)
    if kind=="Concrete":
        height=broad*.74+fine*.26;shade=.74+.22*broad;rough=.70+.25*broad
        pits=(fine>.996).astype(np.float32);height-=pits*.34;shade-=pits*.22
    elif kind=="Steel":
        brushed=.5+.5*np.sin(math.tau*yy*277+.35*np.sin(math.tau*xx*4))
        height=broad*.30+brushed*.56+fine*.14;shade=.80+.13*broad+.04*brushed;rough=.32+.25*broad+.12*brushed
    else:
        height=broad*.48+fine*.18;shade=.84+.15*broad;rough=.42+.16*broad
    dx=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))*1.3
    dy=(np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))*1.3
    length=np.sqrt(dx*dx+dy*dy+1);normal=np.stack((-dx/length,dy/length,1/length),axis=2)*.5+.5
    maps={"Color":save_image("T_Industry_"+kind+"_Color",np.repeat(shade[:,:,None],3,2)),
          "Roughness":save_image("T_Industry_"+kind+"_Roughness",np.repeat(rough[:,:,None],3,2)),
          "Normal":save_image("T_Industry_"+kind+"_Normal",normal)}
    manifest["textures"][kind]={c:str(Path(im.filepath_raw).relative_to(ART)).replace("\\","/") for c,im in maps.items()}
for name,(color,metal,rough,surface,emission) in PALETTE.items():
    mat=bpy.data.materials.new("IM_"+name);mat.diffuse_color=color;mat.use_nodes=True
    ns=mat.node_tree.nodes;ls=mat.node_tree.links;p=ns.get("Principled BSDF")
    p.inputs["Base Color"].default_value=color;p.inputs["Metallic"].default_value=metal;p.inputs["Roughness"].default_value=rough
    p.inputs["Emission Color"].default_value=color;p.inputs["Emission Strength"].default_value=emission
    if surface:
        color_tex=ns.new("ShaderNodeTexImage");color_tex.image=bpy.data.images.get("T_Industry_"+surface+"_Color")
        mix=ns.new("ShaderNodeMixRGB");mix.blend_type="MULTIPLY";mix.inputs[0].default_value=1;mix.inputs[2].default_value=color
        ls.new(color_tex.outputs["Color"],mix.inputs[1]);ls.new(mix.outputs[0],p.inputs["Base Color"])
        rt=ns.new("ShaderNodeTexImage");rt.image=bpy.data.images.get("T_Industry_"+surface+"_Roughness");rt.image.colorspace_settings.name="Non-Color"
        rm=ns.new("ShaderNodeMath");rm.operation="MULTIPLY";rm.inputs[1].default_value=rough/.55;ls.new(rt.outputs[0],rm.inputs[0]);ls.new(rm.outputs[0],p.inputs["Roughness"])
        nt=ns.new("ShaderNodeTexImage");nt.image=bpy.data.images.get("T_Industry_"+surface+"_Normal");nt.image.colorspace_settings.name="Non-Color"
        nm=ns.new("ShaderNodeNormalMap");nm.inputs["Strength"].default_value=.35;ls.new(nt.outputs["Color"],nm.inputs["Color"]);ls.new(nm.outputs["Normal"],p.inputs["Normal"])
    MATS[name]=mat;manifest["palette"]["IM_"+name]={"color":color,"metallic":metal,"roughness":rough,"surface":surface,"emission":emission}

def finish(o,name,mat):
    o.name=name;o.data.materials.append(MATS[mat]);PARTS.append(o);return o
def bevel(o,width=3,segments=2):
    if width:
        m=o.modifiers.new("Manufactured edge radii","BEVEL");m.width=width;m.segments=segments;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=m.name)
    m=o.modifiers.new("Weighted face normals","WEIGHTED_NORMAL");m.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=m.name)
def box(name,p,d,mat="Ceramic",edge=3,rotation=0):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);bevel(o,edge);o.rotation_euler.z=rotation;return finish(o,name,mat)
def beam(name,a,b,width,depth=None,mat="Steel"):
    a,b=Vector(a),Vector(b);o=box(name,(a+b)/2,(width,depth or width,(b-a).length),mat,min(width*.15,2));o.rotation_euler=(b-a).to_track_quat("Z","Y").to_euler();return o
def tube(name,a,b,r,mat="Steel",r2=None,sides=20):
    a,b=Vector(a),Vector(b);bpy.ops.mesh.primitive_cone_add(vertices=sides,radius1=r,radius2=r if r2 is None else r2,depth=(b-a).length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=(b-a).to_track_quat("Z","Y").to_euler()
    if sides>=16:
        for face in o.data.polygons:face.use_smooth=len(face.vertices)<=4
    return finish(o,name,mat)
def cyl(name,p,r,h,mat="Steel",sides=32,r2=None):return tube(name,(p[0],p[1],p[2]-h/2),(p[0],p[1],p[2]+h/2),r,mat,r2,sides)
def torus(name,p,r,t,mat="Steel",rotation=None):
    bpy.ops.mesh.primitive_torus_add(major_segments=48,minor_segments=8,major_radius=r,minor_radius=t,location=p,rotation=rotation or (0,0,0))
    for face in bpy.context.object.data.polygons:face.use_smooth=True
    return finish(bpy.context.object,name,mat)
def pipe(name,points,r=9,mat="Steel"):
    cu=bpy.data.curves.new(name,"CURVE");cu.dimensions="3D";cu.resolution_u=2;cu.bevel_depth=r;cu.bevel_resolution=3
    sp=cu.splines.new("POLY");sp.points.add(len(points)-1)
    for q,p in zip(sp.points,points):q.co=(*p,1)
    ob=bpy.data.objects.new(name,cu);bpy.context.collection.objects.link(ob);bpy.context.view_layer.objects.active=ob;ob.select_set(True);bpy.ops.object.convert(target="MESH");return finish(bpy.context.object,name,mat)
def text(name,word,p,size=70,rotation=(math.pi/2,0,0),mat="Ceramic"):
    cu=bpy.data.curves.new(name,"FONT");cu.body=word;cu.size=size;cu.extrude=.7;cu.align_x="CENTER";cu.align_y="CENTER"
    o=bpy.data.objects.new(name,cu);bpy.context.collection.objects.link(o);o.location=p;o.rotation_euler=rotation
    bpy.ops.object.select_all(action="DESELECT");o.select_set(True);bpy.context.view_layer.objects.active=o;bpy.ops.object.convert(target="MESH");return finish(bpy.context.object,name,mat)
def panel_y(x,y,z,w,h,mat="Ceramic"):
    box("Removable facade cassette",(x,y,z),(w,7,h),mat,2)
    for sx in (-1,1):
        for sz in (-1,1):tube("Captive panel fixing",(x+sx*(w/2-10),y-5,z+sz*(h/2-10)),(x+sx*(w/2-10),y-10,z+sz*(h/2-10)),3,"Steel",sides=8)
def window_y(x,y,z,w,h):
    box("Window thermally broken frame",(x,y,z),(w+15,14,h+15),"Carbon",3)
    box("Blue low-reflection glazing",(x,y-9,z),(w,5,h),"Glass",1)
    box("Window sill flashing",(x,y-15,z-h/2-7),(w+24,28,7),"Steel",1)
    for a in range(1,max(1,round(w/110))):box("Glazing mullion",(x-w/2+a*w/max(1,round(w/110)),y-13,z),(5,10,h),"Steel",.5)
def vent_y(x,y,z,w,h):
    box("Ventilation recess",(x,y,z),(w,12,h),"Carbon",2)
    for k in range(max(3,int(h/14))):box("Angled ventilation louver",(x,y-9,z-h/2+8+k*14),(w-10,10,4),"Steel",.4)
def door_y(x,y,floor,w=115,h=215):
    box("Personnel airlock frame",(x,y,floor+h/2),(w+26,26,h+24),"Slate",5)
    box("Personnel airlock seal",(x,y-16,floor+h/2),(w+6,8,h+4),"Carbon",2)
    box("Personnel airlock leaf",(x,y-22,floor+h/2),(w,8,h),"Ceramic",3)
    window_y(x,y-29,floor+h*.70,w*.60,h*.16)
    box("Door access control",(x+w*.66,y-29,floor+135),(18,10,30),"Carbon",2)
    box("Door access display",(x+w*.66,y-35,floor+140),(12,2,12),"Light",.4)
    beam("Door pull handle",(x+w*.32,y-39,floor+90),(x+w*.32,y-39,floor+120),4,mat="Steel")
    box("Airlock downlight",(x,y-28,floor+h+20),(w-12,8,6),"Light",1)
def rail(a,b,floor=60):
    a,b=Vector(a),Vector(b);count=max(1,math.ceil((b-a).length/170))
    for i in range(count+1):
        p=a+(b-a)*i/count;beam("Safety rail stanchion",(p.x,p.y,floor),(p.x,p.y,floor+110),6,mat="Steel")
    for z in (floor+55,floor+110):tube("Safety handrail",(a.x,a.y,z),(b.x,b.y,z),4,"Yellow")
    beam("Deck toe board",(a.x,a.y,floor+9),(b.x,b.y,floor+9),4,16,"Slate")
def stairs(x,y,w=170,steps=6,rise=18,run=28):
    for i in range(steps):
        box("Non-slip stair tread",(x,y+i*run,9+i*rise),(w,run+4,10),"Steel",1)
        box("Yellow stair nosing",(x,y+i*run-run/2,15+i*rise),(w,5,2),"Yellow",.2)
    for side in (-1,1):
        beam("Stair stringer",(x+side*w/2,y-12,4),(x+side*w/2,y+steps*run,steps*rise),12,18,"Slate")
        tube("Stair handrail",(x+side*(w/2+4),y-12,108),(x+side*(w/2+4),y+steps*run,steps*rise+105),4,"Yellow")
        for i in (0,steps-1):beam("Stair rail post",(x+side*w/2,y+i*run,i*rise),(x+side*w/2,y+i*run,i*rise+105),5,mat="Steel")
def foundation(w,d):
    box("Reinforced concrete grade slab",(0,0,24),(w,d,48),"Concrete",10)
    for x in range(int(-w/2+250),int(w/2),450):box("Concrete expansion joint",(x,0,48),(2,d-10,1),"Carbon",0)
    for y in (-d/2+18,d/2-18):box("Foundation drainage slot",(0,y,48),(w-110,12,1),"Carbon",0)
def fan(x,y,z,r=60):
    cyl("Cooling fan shroud",(x,y,z),r,20,"Carbon");torus("Fan rim",(x,y,z+11),r-4,3,"Steel")
    cyl("Fan motor",(x,y,z+13),r*.22,12,"Steel",16)
    for a in range(0,360,45):
        t=math.radians(a);o=box("Fan blade",(x+math.cos(t)*r*.49,y+math.sin(t)*r*.49,z+10),(r*.61,r*.15,3),"Steel",.5,rotation=t+.2)
    for q in (-.7,-.35,0,.35,.7):
        l=math.sqrt(1-q*q)*r*.92;tube("Fan guard grille",(x-l,y+q*r,z+22),(x+l,y+q*r,z+22),1.3,"Steel",sides=6)
def hvac(x,y,z,w=200,d=290,h=150):
    box("Air handling plant",(x,y,z+h/2),(w,d,h),"Ceramic",6);vent_y(x,y-d/2-5,z+h*.47,w-32,h-25)
    for offset in (-d*.24,d*.24):fan(x,y+offset,z+h+8,min(w*.34,d*.20))
    for a in (-1,1):box("HVAC mounting foot",(x+a*w*.35,y,z-8),(20,d+35,16),"Steel",1)
def tank(x,y,z,r=110,h=330):
    cyl("Pressurized process vessel",(x,y,z+h/2),r,h,"Steel");cyl("Vessel dished top",(x,y,z+h+20),r,40,"Steel",32,r*.3)
    for zz in (z+25,z+h-25):torus("Vessel reinforcement ring",(x,y,zz),r+2,6,"Slate")
    for a in range(0,360,120):t=math.radians(a);beam("Vessel support leg",(x+math.cos(t)*r*.73,y+math.sin(t)*r*.73,z-70),(x+math.cos(t)*r*.73,y+math.sin(t)*r*.73,z+35),16,mat="Slate")
def roof_barrel(x,y,z,w,d,rise=90):
    verts=[];faces=[];steps=16
    for j in (0,1):
        for i in range(steps+1):
            u=i/steps;verts.append((x+(u-.5)*w,y+(j-.5)*d,z+math.sin(u*math.pi)*rise))
    for i in range(steps):faces.append((i,i+1,i+steps+2,i+steps+1))
    me=bpy.data.meshes.new("Curved standing seam canopy");me.from_pydata(verts,[],faces);me.update();ob=bpy.data.objects.new(me.name,me);bpy.context.collection.objects.link(ob);finish(ob,ob.name,"Ceramic")
    for i in range(0,steps+1,2):
        u=i/steps;tube("Barrel roof standing seam",(x+(u-.5)*w,y-d/2,z+math.sin(u*math.pi)*rise+2),(x+(u-.5)*w,y+d/2,z+math.sin(u*math.pi)*rise+2),2,"Steel",sides=6)
def export(name,description):
    bpy.ops.object.select_all(action="DESELECT")
    for o in PARTS:o.select_set(True)
    bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type="ORIGIN_CURSOR");bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    lo=[min(v.co[i] for v in o.data.vertices) for i in range(3)];hi=[max(v.co[i] for v in o.data.vertices) for i in range(3)]
    shift=Vector(((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2]))
    for v in o.data.vertices:v.co-=shift
    uv=o.data.uv_layers.active or o.data.uv_layers.new(name="UVMap")
    for face in o.data.polygons:
        axis=max(range(3),key=lambda k:abs(face.normal[k]));axes=([1,2],[0,2],[0,1])[axis]
        for i in face.loop_indices:
            p=o.data.vertices[o.data.loops[i].vertex_index].co;uv.data[i].uv=(p[axes[0]]/160,p[axes[1]]/160)
    o.data.calc_loop_triangles();dims=[hi[i]-lo[i] for i in range(3)]
    manifest["meshes"][name]={"fbx":name+".fbx","dimensions":dims,"triangles":len(o.data.loop_triangles),"materials":[m.name for m in o.data.materials],"description":description,"unreal_path":"/Game/Art/"+name}
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+".fbx")),use_selection=True,object_types={"MESH"},apply_unit_scale=True,axis_forward="-Y",axis_up="Z",bake_anim=False,add_leaf_bones=False,mesh_smooth_type="FACE")
    ASSETS[name]=o;PARTS.clear();print("INDUSTRY_MESH "+name+" "+str(dims),flush=True)

# COMMAND HUB: a recognizable operations campus rather than a vertical stack of crates.
foundation(2560,2600)
cyl("Octagonal armored operations hull",(0,100,320),810,535,"Slate",8)
cyl("Cast facade sill",(0,100,96),848,52,"Concrete",8)
cyl("Main deck roof edge",(0,100,608),840,40,"Steel",8)
for i in range(8):
    a=(i+.5)*math.tau/8;r=755;x,y=math.cos(a)*r,100+math.sin(a)*r
    o=box("Octagonal facade armor panel",(x,y,375),(535,16,395),"Ceramic",5,rotation=a-math.pi/2)
    for k in (-1,1):
        dx,dy=math.cos(a-math.pi/2)*k*164,math.sin(a-math.pi/2)*k*164
        q=box("Operations slit window",(x+dx+math.cos(a)*12,y+dy+math.sin(a)*12,453),(137,7,108),"Glass",3,rotation=a-math.pi/2)
cyl("Upper command gallery",(0,100,744),596,245,"Slate",12)
for i in range(12):
    a=(i+.5)*math.tau/12;r=578
    box("Command panoramic glazing",(math.cos(a)*r,100+math.sin(a)*r,760),(280,14,144),"Glass",2,rotation=a-math.pi/2)
    box("Command glazing pier",(math.cos(i*math.tau/12)*597,100+math.sin(i*math.tau/12)*597,759),(22,22,178),"Ceramic",2)
# Pressed curved roof cassettes: a 48-sided shell with six meridian rings avoids
# the old twelve broad conical faces while retaining the exact roof envelope.
roof_profile=((644,872),(635,892),(610,919),(570,945),(515,965),(445,972))
roof_sides=48;roof_verts=[];roof_faces=[]
for radius,z in roof_profile:
    for i in range(roof_sides):
        a=i*math.tau/roof_sides;roof_verts.append((math.cos(a)*radius,100+math.sin(a)*radius,z))
for j in range(len(roof_profile)-1):
    for i in range(roof_sides):
        k=(i+1)%roof_sides;roof_faces.append((j*roof_sides+i,j*roof_sides+k,(j+1)*roof_sides+k,(j+1)*roof_sides+i))
roof_faces.append(tuple(range((len(roof_profile)-1)*roof_sides,len(roof_profile)*roof_sides)))
roof_faces.append(tuple(reversed(range(roof_sides))))
roof_mesh=bpy.data.meshes.new("Curved armored roof shell");roof_mesh.from_pydata(roof_verts,[],roof_faces);roof_mesh.update()
roof_object=bpy.data.objects.new(roof_mesh.name,roof_mesh);bpy.context.collection.objects.link(roof_object)
for face in roof_mesh.polygons:face.use_smooth=len(face.vertices)==4
finish(roof_object,roof_object.name,"Ceramic")
for i in range(24):
    a=i*math.tau/24
    pipe("Raised radial cassette seam",[(math.cos(a)*radius,100+math.sin(a)*radius,z+3.5) for radius,z in roof_profile],2.6,"Steel")
    for radius,z in ((635,895),(515,968),(452,975)):
        cyl("Roof seam captive fixing",(math.cos(a)*radius,100+math.sin(a)*radius,z+3),3.6,3,"Steel",8)
torus("Rolled eaves flashing",(0,100,874),643,4,"Steel")
torus("Roof annular expansion joint",(0,100,966),515,3,"Slate")
torus("Equipment terrace flange",(0,100,975),445,4,"Steel")
cyl("Roof equipment well",(0,100,979),340,25,"Slate",12)
for x in (-165,165):hvac(x,105,995,175,250,112)
for x in (-962,962):
    box("Single-storey service wing",(x,230,205),(575,1260,305),"Slate",12)
    roof_barrel(x,230,369,600,1290,82)
    for yy in (-260,200,660):
        box("Wing exterior rib",(x,yy,213),(596,22,313),"Steel",2)
    window_y(x,-408,241,435,114);door_y(x,-422,48,102,206)
    for y in (80,350,620):hvac(x,y,442,130,190,78)
    for sign in (-1,1):pipe("External coolant distribution",[(x+sign*235,-260,80),(x+sign*235,-260,325),(x+sign*235,760,325)],11,"Copper")
box("Glazed entrance pavilion",(0,-843,226),(755,555,350),"Slate",10)
for x in (-280,280):window_y(x,-1124,251,214,220)
door_y(0,-1136,91,143,236)
box("Cantilever entrance canopy",(0,-1005,427),(890,505,28),"Ceramic",7)
box("Recessed canopy light",(0,-1229,409),(730,10,5),"Light",1)
box("Entry deck",(0,-1202,59),(880,365,32),"Steel",4)
stairs(0,-1530,252,5,15,31)
rail((-431,-1369),(-157,-1369),75);rail((157,-1369),(431,-1369),75)
text("Embossed colony identity","C O M M A N D   /   0 1",(0,-1142,376),54,mat="Ceramic")
for x in (-525,525):
    tank(x,935,110,117,289);pipe("Reactor heat exchanger manifold",[(x,1010,120),(x,1160,120),(x,1160,500),(x*.55,490,625)],13,"Steel")
for a in range(0,360,60):
    t=math.radians(a);beam("Roof communication support",(math.cos(t)*70,100+math.sin(t)*70,991),(0,100,1290),7,mat="Steel")
tube("Aerial mast",(0,100,1230),(0,100,1480),8,"Steel")
for z,w in ((1320,130),(1390,90),(1450,55)):beam("Communications cross aerial",(-w,100,z),(w,100,z),5,mat="Steel")
cyl("Shuttle docking collar",(0,935,670),245,45,"Carbon",32)
torus("Escape-shuttle docking seal",(0,935,696),208,10,"Steel")
for a in range(0,360,60):
    t=math.radians(a);box("Docking clamp",(math.cos(t)*215,935+math.sin(t)*215,710),(52,32,20),"Yellow",3,rotation=t)
text("Docking bay identifier","ESCAPE / SERVICE",(0,1165,728),30,rotation=(0,0,0),mat="Ceramic")
export("SM_Core","Full-scale octagonal command campus, panoramic gallery, curved 48-sided armored roof with standing seams and flange joints, twin maintenance wings, glazed entry, docking collar, HVAC and communications equipment.")

def build_other_assets():
    # EXTRACTION: open machinery, a tall lattice mast and a long physically legible conveyor.
    foundation(1710,1750)
    for x in (-230,230):
        for y in (-200,200):beam("Drill derrick leg",(x,y,50),(x*.61,y*.61,1460),34,mat="Slate")
    for z in (200,460,720,980,1240):
        r=230-z*.061
        for y in (-1,1):beam("Derrick diagonal cross brace",(-r,y*r,z),(r*.94,y*r*.94,z+220),15,mat="Steel")
        for x in (-1,1):beam("Derrick side cross brace",(x*r,-r,z),(x*r*.94,r*.94,z+220),15,mat="Steel")
    box("Drill crown gearbox",(0,0,1470),(440,410,115),"Ceramic",14)
    cyl("Top-drive drilling motor",(0,0,1230),137,310,"Slate",32)
    for a in range(0,360,30):
        t=math.radians(a);box("Electric motor cooling fin",(math.cos(t)*137,math.sin(t)*137,1250),(18,18,250),"Steel",1,rotation=t)
    cyl("Hydraulic drill string",(0,0,656),44,910,"Steel",32)
    for z in range(130,850,110):torus("Drill string coupling",(0,0,z),46,10,"Slate")
    cyl("Extraction annulus",(0,0,122),206,147,"Carbon",32)
    torus("Wellhead pressure seal",(0,0,201),171,15,"Copper")
    for x in (-625,625):
        box("Pump machinery pod",(x,-285,240),(350,830,365),"Slate",10)
        for yy in (-490,-140):
            tank(x,yy,150,94,230);pipe("Hydraulic pressure line",[(x,yy,240),(x*.61,yy,240),(x*.61,-70,860)],13,"Copper")
        vent_y(x,-710,250,258,200)
    box("Operator instrumentation cabin",(-540,540,281),(480,460,410),"Ceramic",12);window_y(-540,303,341,338,158);door_y(-540,293,51,94,210)
    roof_barrel(-540,540,497,513,490,38)
    for y in (390,640):hvac(-535,y,536,135,173,92)
    for x in (-230,230):beam("Conveyor stringer",(x,230,212),(x,810,405),21,30,"Steel")
    for y in range(260,821,70):tube("Conveyor return roller",(-222,y,214+(y-230)/3),(222,y,214+(y-230)/3),18,"Steel",sides=16)
    beam("Ribbed ore conveyor belt",(0,240,235),(0,800,419),445,13,"Carbon")
    for y in range(275,801,90):beam("Conveyor transverse cleat",(-207,y,244+(y-230)/3),(207,y,244+(y-230)/3),10,12,"Yellow")
    box("Ore discharge hopper",(0,775,165),(603,150,225),"Ceramic",8)
    text("Extraction bay number","EX / 04",(0,-222,1425),60,mat="Yellow")
    export("SM_Extractor","Open braced drilling derrick, top-drive motor, hydraulic pump pods, operator cabin, wellhead and inclined ore conveyor.")

    # PRODUCTION: sawtooth monitor roof, a machine hall and a separate process-vessel gallery.
    foundation(2460,2050)
    box("Factory machine hall",(-190,0,319),(1700,1650,540),"Slate",8)
    for x in range(-930,561,215):
        panel_y(x,-833,333,200,505);window_y(x,-844,484,150,94)
    for y in range(-680,681,225):box("Factory side pilaster",(-1048,y,338),(21,26,580),"Steel",2)
    # Asymmetric roof monitors define the factory silhouette and admit northern light.
    for y in (-635,-220,195,610):
        beam("Sawtooth opaque roof plane",(-185,y-170,621),(-185,y+120,751),1770,16,"Ceramic")
        box("Roof monitor glazing",(-185,y+129,683),(1680,12,140),"Glass",2)
        for x in range(-960,601,155):box("Monitor glazing mullion",(x,y+116,684),(8,16,145),"Steel",.5)
    for x in (-640,60):
        box("Industrial vehicle bay",(x,-848,228),(467,22,359),"Carbon",4)
        for z in range(76,405,32):box("Roller shutter slat",(x,-864,z),(449,12,25),"Steel",1)
        box("Loading dock canopy",(x,-946,438),(526,276,30),"Slate",4)
        for xx in (x-259,x+259):cyl("Loading dock safety bollard",(xx,-957,118),15,140,"Yellow",16)
    door_y(-995,-855,48,96,208)
    box("Factory identification band",(-190,-866,603),(1780,13,46),"Blue",1)
    text("Production hall sign","FABRICATION   /   03",(-190,-877,603),37,mat="Ceramic")
    for y in (-570,0,570):
        tank(892,y,190,190,517)
        pipe("Process vessel discharge",[(892,y,694),(1060,y,694),(1060,y,250),(653,y,250)],18,"Copper")
        for x in (690,1100):beam("Process gantry column",(x,y,51),(x,y,465),14,mat="Steel")
    box("Process gallery walkway",(895,0,466),(489,1580,19),"Steel",2)
    rail((1152,-771),(1152,771),476)
    for y in (-370,375):
        cyl("Industrial exhaust plenum",(-865,y,782),119,265,"Slate",24)
        cyl("Insulated exhaust stack",(-865,y,1034),79,328,"Steel",32)
        for z in (900,1100,1210):torus("Exhaust stack bolted flange",(-865,y,z),84,6,"Steel")
        cyl("Stack rain cowl",(-865,y,1232),105,30,"Slate",32)
    for x in (-200,270):
        box("Raised roof plant maintenance deck",(x,215,813),(208,338,15),"Steel",2)
        for dx in (-80,80):
            for dy in (-125,125):beam("Roof plant support stanchion",(x+dx,215+dy,593),(x+dx,215+dy,810),12,mat="Steel")
        hvac(x,215,833,160,280,117)
    export("SM_Factory","Sawtooth-roof machine hall with glazed monitors, loading shutters, external process-vessel gallery, pipework and flanged exhaust stacks.")

    # LOGISTICS: open loading canopy, a closed stores block, charging alcoves and cargo racks.
    foundation(2470,2060)
    box("Logistics stores module",(-715,140,311),(850,1630,521),"Slate",12)
    for y in range(-580,781,226):box("Stores module shell rib",(-715,y,330),(871,17,539),"Ceramic",2)
    roof_barrel(-715,140,588,893,1690,68)
    door_y(-710,-689,48,118,221);window_y(-955,-689,346,230,105)
    for x in (-230,1150):
        for y in (-755,835):beam("Loading canopy steel column",(x,y,48),(x,y,717),26,36,"Steel")
    for y in (-755,835):
        beam("Loading canopy truss bottom chord",(-277,y,646),(1190,y,646),16,22,"Steel")
        beam("Loading canopy truss upper chord",(-277,y,748),(1190,y,748),16,22,"Steel")
        for x in range(-220,1160,172):beam("Triangulated canopy girder",(x,y,648),(x+160,y,746),11,mat="Steel")
    roof_barrel(460,42,761,1550,1740,110)
    for y in range(-770,850,135):beam("Canopy roof purlin",(-300,y,773),(1205,y,773),9,14,"Slate")
    for x,y,z in ((120,560,162),(530,560,162),(940,560,162),(120,560,402),(530,560,402),(120,40,162),(940,45,162)):
        box("Intermodal robot freight crate",(x,y,z),(358,333,214),"Blue" if x<600 else "Ceramic",9)
        for k in range(-140,151,47):box("Freight crate rib",(x+k,y-173,z),(9,14,185),"Steel",1)
        for k in (-1,1):
            for q in (-1,1):box("Container corner casting",(x+k*163,y-174,z+q*91),(28,24,28),"Yellow",1)
        text("Container logistics label","22 / C",(x,y-185,z),34,mat="Ceramic")
    for x in (-935,-570):
        box("Robot charging alcove",(x,-791,173),(281,202,240),"Carbon",5)
        box("Charging dock recess",(x,-899,175),(220,6,171),"Steel",2)
        box("Charging contact screen",(x,-905,232),(164,4,51),"Light",1)
        for dx in (-66,66):box("Charging induction shoe",(x+dx,-902,82),(35,29,38),"Yellow",2)
    for x in (12,400,800,1170):cyl("Vehicle protection bollard",(x,-900,110),15,125,"Yellow",16)
    text("Logistics sign","LOGISTICS / 02",(-713,-710,523),62,mat="Ceramic")
    export("SM_Depot","High open-span loading canopy with steel trusses, closed stores module, detailed intermodal freight crates and robot charging alcoves.")

    # SENSOR: triangulated steel structure with a service cabin and visible phased-array panels.
    foundation(1060,1000)
    for x in (-230,230):
        for y in (-230,230):
            box("Tower bolted concrete footing",(x,y,86),(150,150,77),"Concrete",6)
            for sx in (-1,1):
                for sy in (-1,1):cyl("Tower anchor stud",(x+sx*48,y+sy*48,143),9,42,"Steel",8)
            beam("Tapering sensor tower column",(x,y,132),(x*.37,y*.37,2090),28,mat="Slate")
    for z in range(200,1910,285):
        r=230-(z-132)*.074
        for side in (-1,1):
            beam("Tower X bracing",(-r,side*r,z),(r*.87,side*r*.87,z+270),12,mat="Steel")
            beam("Tower opposite bracing",(r,side*r,z),(-r*.87,side*r*.87,z+270),12,mat="Steel")
            beam("Tower side lattice",(side*r,-r,z),(side*r*.87,r*.87,z+270),12,mat="Steel")
    box("Tower service platform",(0,0,1928),(420,420,22),"Steel",3)
    for y in (-210,210):rail((-210,y),(210,y),1940)
    cyl("Sensor gimbal azimuth drive",(0,0,2101),104,117,"Steel",32)
    for y in (-146,146):
        box("Phased-array armor backing",(0,y,2301),(580,76,340),"Ceramic",12)
        box("Phased-array dark active surface",(0,y-43,2301),(532,10,289),"Carbon",2)
        for x in range(-230,231,46):
            for z in range(2190,2420,46):box("Phased-array ceramic antenna tile",(x,y-51,z),(36,6,35),"Slate",2)
    tube("Lightning finial",(0,0,2420),(0,0,2720),5,"Steel");cyl("Navigation warning beacon",(0,0,2548),16,30,"Amber",16)
    box("Sensor control shelter",(275,-270,214),(415,402,332),"Ceramic",9);door_y(275,-480,48,99,214)
    roof_barrel(275,-270,389,450,430,34);vent_y(275,-489,318,230,58)
    for z in range(175,1800,38):beam("Tower maintenance ladder rung",(-52,235-z*.068,z),(52,235-z*.068,z),6,mat="Yellow")
    for x in (-56,56):beam("Tower ladder stringer",(x,230,125),(x,111,1860),8,mat="Steel")
    text("Sensor shelter sign","SENSOR / 05",(275,-487,387),33,mat="Slate")
    export("SM_Sensor","Full-height braced sensor tower, maintenance ladder and platform, bolted footings, phased-array tiles and ground-level control shelter.")

    # DEFENSE: heavy armored emplacement, external magazines and a clear articulated gun assembly.
    foundation(1300,1250)
    cyl("Turret reinforced concrete plinth",(0,0,122),446,148,"Concrete",12)
    cyl("Armored turret base",(0,0,278),350,233,"Slate",12)
    for i in range(12):
        a=(i+.5)*math.tau/12;box("Replaceable angled pedestal armor",(math.cos(a)*340,math.sin(a)*340,273),(174,24,189),"Ceramic",5,rotation=a-math.pi/2)
    cyl("Exposed azimuth bearing",(0,0,419),289,46,"Steel",48);torus("Bearing seal",(0,0,444),268,8,"Carbon")
    box("Sloped turret receiver",(0,22,557),(531,414,214),"Slate",20)
    for x in (-235,235):
        box("Armored ammunition magazine",(x,69,568),(145,420,270),"Ceramic",14)
        for y in (-62,22,106,190):box("Magazine rib",(x,y,568),(153,9,248),"Steel",1)
        tube("Elevation trunnion",(x-21,0,565),(x+21,0,565),64,"Steel",sides=24)
    for x in (-92,92):
        tube("Autocannon cooling jacket",(x,-151,584),(x,-522,603),42,"Slate",sides=24)
        tube("Precision gun barrel",(x,-485,601),(x,-842,620),25,"Steel",sides=24)
        for y in (-256,-396,-536):tube("Barrel thermal collar",(x,y,586+(-y-151)*.051),(x,y-23,587+(-y-151)*.051),47,"Steel",sides=24)
        tube("Ported muzzle brake",(x,-815,619),(x,-910,624),39,"Carbon",sides=24)
        for yy in (-842,-874):box("Muzzle vent",(x-38,yy,621),(3,15,25),"Steel",.5)
    box("Optical targeting module",(0,-193,725),(199,130,132),"Ceramic",10)
    for x in (-56,46):
        tube("Targeting lens housing",(x,-251,737),(x,-280,737),29,"Carbon",sides=24)
        tube("Targeting lens",(x,-278,737),(x,-283,737),23,"Glass",sides=24)
    box("Turret service cabinet",(0,321,535),(306,208,270),"Ceramic",10);vent_y(0,209,542,220,155)
    for x in (-170,170):pipe("Flexible ammunition feed",[(x,308,459),(x,460,458),(x,460,588),(x,268,620)],31,"Carbon")
    text("Defense unit marking","D / 06",(0,-193,533),55,mat="Ceramic")
    export("SM_Turret","Armored autonomous twin-cannon emplacement with reinforced pedestal, magazines, cooling jackets, azimuth bearing and optical targeting module.")

if CORE_ONLY:
    with bpy.data.libraries.load(str(retained_source), link=False) as (data_from,data_to):
        data_to.objects=[name for name in manifest["meshes"] if name!="SM_Core"]
    for obj in data_to.objects:
        if obj is None:raise RuntimeError("Retained source is missing an industrial mesh")
        bpy.context.collection.objects.link(obj)
        for slot in obj.material_slots:
            if slot.material:
                key=slot.material.name.removeprefix("IM_").split(".")[0]
                if key in MATS:slot.material=MATS[key]
        ASSETS[obj.name]=obj
    # Blender retains an append provenance library even for fully local objects;
    # clear that unused bookkeeping before saving the refreshed source in place.
    for library in list(bpy.data.libraries):
        if library.users==0:bpy.data.libraries.remove(library)
else:build_other_assets()

# A retained six-asset review scene, plus close-ups of the hero hub and machine hall.
(OUT/"industry_manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")
placements={"SM_Core":(-1850,-1600,0),"SM_Factory":(1650,-1550,0),"SM_Depot":(1750,1250,0),"SM_Extractor":(-1700,1550,0),"SM_Sensor":(-3600,750,0),"SM_Turret":(1000,-3670,0)}
for name,p in placements.items():ASSETS[name].location=p
bpy.ops.mesh.primitive_plane_add(size=80000,location=(0,0,-2));ground=bpy.context.object;ground.name="Preview studio ground";ground.data.materials.append(MATS["Concrete"])
world=bpy.data.worlds.new("Soft industrial daylight");world.use_nodes=True;scene.world=world
sky=world.node_tree.nodes.new("ShaderNodeTexSky");sky.sky_type="NISHITA";sky.sun_elevation=.66;sky.sun_rotation=2.3
world.node_tree.links.new(sky.outputs[0],world.node_tree.nodes["Background"].inputs[0]);world.node_tree.nodes["Background"].inputs[1].default_value=.30
bpy.ops.object.light_add(type="SUN",location=(0,0,12000));sun=bpy.context.object;sun.rotation_euler=(.61,-.34,-.64);sun.data.energy=2.4;sun.data.angle=.09
bpy.ops.object.camera_add(location=(9400,-12600,10600));cam=bpy.context.object;cam.data.type="PERSP";cam.data.lens=48;cam.data.clip_end=150000
cam.rotation_euler=(Vector((-500,-650,600))-cam.location).to_track_quat("-Z","Y").to_euler();scene.camera=cam
scene.render.engine="CYCLES";scene.cycles.samples=32;scene.cycles.use_denoising=True;scene.render.resolution_x=2000;scene.render.resolution_y=1450;scene.render.resolution_percentage=100
scene.view_settings.view_transform="AgX";scene.view_settings.exposure=-1.6
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(ART/"Source/Seige_Industry_Architecture.blend"),compress=True)
scene.render.filepath=str(ART/"Previews/industry_architecture_set.png");bpy.ops.render.render(write_still=True)
for name,image_name,offset,target in (("SM_Core","industry_command_closeup",(3800,-4900,3000),(0,-60,500)),("SM_Factory","industry_factory_closeup",(3500,-4200,2500),(0,0,500))):
    if CORE_ONLY and name!="SM_Core":continue
    for obj in ASSETS.values():obj.hide_render=obj.name!=name
    obj=ASSETS[name];cam.location=obj.location+Vector(offset);cam.rotation_euler=(obj.location+Vector(target)-cam.location).to_track_quat("-Z","Y").to_euler();cam.data.lens=52
    scene.render.resolution_x=1900;scene.render.resolution_y=1350;scene.render.filepath=str(ART/"Previews"/(image_name+".png"));bpy.ops.render.render(write_still=True)
print("SEIGE_INDUSTRY_ASSETS_CREATED "+json.dumps({k:v["triangles"] for k,v in manifest["meshes"].items()}),flush=True)

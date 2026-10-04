"""Original industrial colony and temperate vegetation assets; run in Blender background mode."""
from pathlib import Path
import bpy, math, random, json
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "Art"
OUT = ART / "RealisticExports"
OUT.mkdir(parents=True, exist_ok=True)
(ART / "Source").mkdir(exist_ok=True)
(ART / "Previews").mkdir(exist_ok=True)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = .01

PALETTE = {
    "Concrete": ((.28,.29,.27,1),0,.88,0), "Paint": ((.53,.56,.54,1),.5,.5,0),
    "DarkPaint": ((.10,.135,.13,1),.55,.52,0), "Steel": ((.26,.29,.30,1),.85,.32,0),
    "Black": ((.018,.024,.025,1),.3,.63,0), "Window": ((.022,.055,.073,1),.55,.16,0),
    "Rust": ((.24,.105,.045,1),.45,.8,0), "Ochre": ((.42,.27,.075,1),.4,.55,0),
    "Light": ((.6,.81,.9,1),0,.3,.6), "Amber": ((.9,.28,.035,1),0,.35,.7),
    "Bark": ((.15,.105,.065,1),0,.95,0), "Leaf": ((.12,.23,.055,1),0,.82,0),
    "Pine": ((.055,.13,.055,1),0,.87,0), "Grass": ((.14,.22,.055,1),0,.9,0),
    "Rock": ((.27,.285,.26,1),0,.93,0),
}
MATS = {}
for key,(color,metal,rough,emit) in PALETTE.items():
    mat=bpy.data.materials.new("RM_"+key); mat.diffuse_color=color; mat.use_nodes=True
    nodes=mat.node_tree.nodes; links=mat.node_tree.links; p=nodes.get("Principled BSDF")
    p.inputs["Base Color"].default_value=color; p.inputs["Metallic"].default_value=metal
    p.inputs["Roughness"].default_value=rough; p.inputs["Emission Color"].default_value=color
    p.inputs["Emission Strength"].default_value=emit
    if key not in ("Light","Amber","Window"):
        noise=nodes.new("ShaderNodeTexNoise"); noise.inputs["Scale"].default_value=24
        noise.inputs["Detail"].default_value=3
        bump=nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value=.18
        bump.inputs["Distance"].default_value=.07
        links.new(noise.outputs["Fac"],bump.inputs["Height"]);links.new(bump.outputs["Normal"],p.inputs["Normal"])
    if key in ("Bark","Rock"):
        source="bark_brown_02" if key=="Bark" else "rock_boulder_dry"
        file=ART/"Textures"/"PolyHaven"/(source+"_diff_2k.jpg")
        tex=nodes.new("ShaderNodeTexImage");tex.image=bpy.data.images.load(str(file))
        links.new(tex.outputs["Color"],p.inputs["Base Color"])
    if key in ("Leaf","Pine","Grass"):
        attr=nodes.new("ShaderNodeVertexColor");attr.layer_name="Col"
        links.new(attr.outputs["Color"],p.inputs["Base Color"])
    MATS[key]=mat

PARTS=[]; ASSETS={}; manifest={"units":"centimeters","forward":"+X","palette":{},"meshes":{}}
for k,(c,m,r,e) in PALETTE.items():
    manifest["palette"]["RM_"+k]={"color":c,"metallic":m,"roughness":r,"emission":e,"foliage":k in ("Leaf","Pine","Grass")}

def finish(o,name,mat):
    o.name=name;o.data.materials.append(MATS[mat]);PARTS.append(o);return o

def box(name,p,d,mat="Paint",bevel=.7):
    bpy.ops.mesh.primitive_cube_add(size=1,location=p);o=bpy.context.object;o.dimensions=d
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        b=o.modifiers.new("Edge chamfer","BEVEL");b.width=bevel;b.segments=2
        bpy.ops.object.modifier_apply(modifier=b.name)
        n=o.modifiers.new("Weighted normals","WEIGHTED_NORMAL");bpy.ops.object.modifier_apply(modifier=n.name)
    return finish(o,name,mat)

def tube(name,a,b,r,mat="Steel",r2=None,sides=12):
    a=Vector(a);b=Vector(b);v=b-a
    bpy.ops.mesh.primitive_cone_add(vertices=sides,radius1=r,radius2=r if r2 is None else r2,depth=v.length,location=(a+b)/2)
    o=bpy.context.object;o.rotation_euler=v.to_track_quat("Z","Y").to_euler()
    return finish(o,name,mat)

def cyl(name,p,r,h,mat="Steel",sides=24):
    return tube(name,(p[0],p[1],p[2]-h/2),(p[0],p[1],p[2]+h/2),r,mat,sides=sides)

def uv_sphere(name,p,d,mat="Paint",segments=24,rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments,ring_count=rings,radius=1,location=p)
    o=bpy.context.object;o.scale=Vector(d)/2
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for poly in o.data.polygons:poly.use_smooth=True
    return finish(o,name,mat)

def vent(p,w,h,side="x"):
    x,y,z=p
    box("Recessed ventilation grille",p,(2,w,h) if side=="x" else (w,2,h),"Black",.3)
    for i in range(max(3,int(h/3))):
        q=(x+1.1,y,z-h/2+1+i*3) if side=="x" else (x,y+1.1,z-h/2+1+i*3)
        box("Louver blade",q,(1.5,w-2,.7) if side=="x" else (w-2,1.5,.7),"Steel",.1)

def panel_wall(x,y,z,w,h,side="x",mat="Paint"):
    box("Armor panel",(x,y,z),(2,w,h) if side=="x" else (w,2,h),mat,.3)
    for u in (-1,1):
        for v in (-1,1):
            loc=(x+1.2,y+u*(w/2-2),z+v*(h/2-2)) if side=="x" else (x+u*(w/2-2),y+1.2,z+v*(h/2-2))
            uv_sphere("Panel fastener",loc,(1.4,1.4,1.4),"Steel",8,4)

def foundation(w,d):
    box("Cast concrete foundation",(0,0,5),(w,d,10),"Concrete",2)
    box("Steel foundation rail",(0,0,11),(w-6,d-6,3),"DarkPaint",.6)
    for x in (-w*.40,w*.40):
        for y in (-d*.39,d*.39):
            box("Anchor plate",(x,y,13),(10,10,2),"Steel",.2)
            for s in (-1,1):cyl("Anchor stud",(x+s*3,y,15),.8,3,"Black",8)

def handrail(a,b,z=95):
    a=Vector(a);b=Vector(b);length=(b-a).length
    for i in range(max(2,int(length/22))+1):
        p=a+(b-a)*i/max(2,int(length/22));tube("Railing post",(p.x,p.y,z-20),(p.x,p.y,z),.8,"Steel",sides=8)
    tube("Safety handrail",(a.x,a.y,z),(b.x,b.y,z),1,"Ochre",sides=8)
    tube("Railing midrail",(a.x,a.y,z-10),(b.x,b.y,z-10),.6,"Steel",sides=8)

def stairs(x,y,width,steps=5):
    for i in range(steps):box("Grated stair tread",(x+i*4,y,3+i*3),(5,width,2),"Steel",.15)
    tube("Stair side rail",(x-2,y-width/2-2,18),(x+steps*4,y-width/2-2,18+steps*3),.9,"Ochre")
    tube("Stair side rail",(x-2,y+width/2+2,18),(x+steps*4,y+width/2+2,18+steps*3),.9,"Ochre")

def export(name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in PARTS:o.select_set(True)
    bpy.context.view_layer.objects.active=PARTS[0];bpy.ops.object.join();o=bpy.context.object;o.name=name
    bpy.context.scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    lo=[min(v.co[i] for v in o.data.vertices) for i in range(3)];hi=[max(v.co[i] for v in o.data.vertices) for i in range(3)]
    shift=Vector(((lo[0]+hi[0])/2,(lo[1]+hi[1])/2,lo[2]))
    for v in o.data.vertices:v.co-=shift
    o.data.calc_loop_triangles()
    manifest["meshes"][name]={"fbx":name+".fbx","dimensions":[hi[i]-lo[i] for i in range(3)],"triangles":len(o.data.loop_triangles),"materials":[m.name for m in o.data.materials],"unreal_path":"/Game/Art/"+name}
    bpy.ops.export_scene.fbx(filepath=str(OUT/(name+".fbx")),use_selection=True,object_types={"MESH"},apply_unit_scale=True,axis_forward="-Y",axis_up="Z",bake_anim=False,add_leaf_bones=False,mesh_smooth_type="FACE")
    ASSETS[name]=o;PARTS.clear();return o

# Command center: an armored modular operations building with service decks and communications.
foundation(220,200)
box("Core substructure",(0,0,39),(182,164,51),"DarkPaint",3)
box("Core main pressure hull",(-8,0,87),(152,140,59),"Paint",5)
box("Core roof seam",(-8,0,119),(161,148,5),"Steel",1)
box("Core roof service plinth",(-17,0,129),(130,123,15),"DarkPaint",2)
for y in (-52,-17,18,53):
    panel_wall(70,y,84,32,43)
for x in (-59,-21,17,55):
    for y in (-72,72):panel_wall(x,y,82,34,47,"y")
for y in (-48,0,48):
    box("Armored observation window",(73,y,96),(3,27,12),"Window",1)
    box("Window sill",(76,y,88),(5,31,2),"Steel",.2)
box("Core recessed airlock",(96,0,42),(6,34,48),"Black",1)
for y in (-9,9):box("Core airlock leaf",(100,y,42),(3,16,43),"Paint",.5)
box("Core airlock lamp",(101,0,70),(3,31,2),"Light",.1)
stairs(106,0,37,4)
for y in (-80,80):
    box("Walkway",(-5,y,63),(170,17,4),"Steel",.3)
    handrail((-81,y*1.07),(76,y*1.07),86)
    tube("Coolant feed",(-73,y,18),(-73,y,112),3,"Steel")
    tube("Coolant return",(-66,y,18),(-66,y,112),2,"Rust")
for x in (-53,-13,27):
    box("Roof equipment housing",(x,25,145),(31,46,25),"Paint",1)
    vent((x,49,145),24,19,"y")
cyl("Communications pedestal",(12,-31,154),18,50,"DarkPaint")
uv_sphere("Communications radome",(12,-31,181),(39,39,39),"Paint")
tube("Antenna pole",(-51,-43,140),(-51,-43,204),1.8,"Steel")
for z in (178,190,201):tube("Antenna transverse",(-64,-43,z),(-38,-43,z),.8,"Steel")
cyl("Roof warning beacon",(48,42,150),3,8,"Amber",12)
export("SM_Core")

# Extractor: braced drill rig with hydraulic details, hopper, and grated machinery platform.
foundation(180,155)
for y in (-48,48):
    box("Drill tower beam",(-10,y,91),(13,15,155),"DarkPaint",1)
    tube("Tower diagonal",(-10,y-6,20),(-10,-y+6,155),3,"Steel")
box("Drill tower crown",(-10,0,172),(45,120,15),"Paint",2)
cyl("Drill motor",(-10,0,141),22,44,"DarkPaint")
for a in range(0,360,45):
    q=math.radians(a);box("Motor cooling fin",(-10+math.cos(q)*23,math.sin(q)*23,143),(3,3,30),"Steel",.2)
cyl("Hydraulic drilling rod",(-10,0,87),7,84,"Steel")
tube("Drill cutting body",(-10,0,18),(-10,0,65),3,"Steel",19)
for z in (26,35,44,53,62):
    bpy.ops.mesh.primitive_torus_add(major_segments=24,minor_segments=6,major_radius=8+(z-26)*.25,minor_radius=2,location=(-10,0,z));finish(bpy.context.object,"Drill cutting ring","Steel")
for y in (-67,67):
    box("Hydraulic control block",(-8,y,37),(60,24,38),"Paint",2)
    for x in (-28,-14,0,14):vent((x,y+13,39),8,23,"y")
box("Hopper understructure",(57,0,35),(38,72,41),"DarkPaint",1)
box("Ore hopper",(57,0,60),(45,79,14),"Steel",2)
for y in (-24,-8,8,24):tube("Hopper grating",(34,y,68),(80,y,68),1,"Steel")
tube("Hydraulic line",(-27,-56,51),(-27,-56,160),2,"Rust")
tube("Hydraulic elbow",(-27,-56,160),(-10,-15,160),2,"Rust")
handrail((-50,70),(55,70),102)
export("SM_Extractor")

# Factory: ribbed modular machine hall, service pipes, high exhaust stacks, and loading doors.
foundation(218,184)
box("Factory machine hall",(-4,0,58),(182,150,90),"DarkPaint",3)
for x in (-73,-37,-1,35,71):
    for y in (-76,76):panel_wall(x,y,60,33,83,"y")
for y in (-58,-29,0,29,58):panel_wall(89,y,60,26,82)
box("Loading door inset",(93,0,49),(5,65,65),"Black",.5)
for z in range(23,77,6):box("Loading door shutter",(96,0,z),(2,59,4),"Steel",.2)
box("Loading canopy",(103,0,88),(28,83,5),"DarkPaint",1)
box("Loading light",(110,0,84),(2,62,1.8),"Light",.1)
box("Factory roof",(-4,0,107),(192,160,6),"Steel",1)
for x in (-66,-20,26,67):
    box("Roof raised panel",(x,0,111),(36,145,3),"Paint",.3)
for y in (-38,38):
    cyl("Stack foundation",(-60,y,119),21,18,"DarkPaint")
    cyl("Industrial exhaust",(-60,y,148),14,58,"Steel")
    cyl("Exhaust upper guard",(-60,y,180),17,8,"DarkPaint")
    for z in (129,160):cyl("Exhaust flange",(-60,y,z),16,3,"Rust")
for x in (10,45):
    box("Air handling unit",(x,25,125),(28,67,26),"Paint",1)
    vent((x+15,25,126),56,20)
for z in (28,39,49):tube("External process pipe",(-91,-83,z),(75,-83,z),2.4,"Steel")
for x in (-70,-30,10,50):box("Pipe mounting bracket",(x,-83,38),(3,13,40),"DarkPaint",.2)
stairs(103,0,62,4)
export("SM_Factory")

# Depot: shipping hall, open frame, structural trusses, realistic small cargo modules.
foundation(212,180)
for x in (-85,85):
    for y in (-70,70):box("Depot steel column",(x,y,67),(9,10,110),"DarkPaint",.6)
for y in (-72,72):
    tube("Roof primary girder",(-92,y,122),(92,y,122),5,"Steel")
    for x in (-80,-40,0,40):tube("Roof diagonal truss",(x,y,102),(x+40,y,122),2,"Steel")
box("Warehouse roof",(0,0,127),(206,170,6),"Paint",1)
for x in range(-90,100,15):box("Standing seam roof rib",(x,0,131),(1.5,168,2),"Steel",.1)
box("Depot back wall",(-90,0,63),(6,147,102),"Paint",.5)
for x,y,z in ((-51,-40,38),(-51,17,38),(13,-40,38),(-51,-40,79),(-51,17,79),(16,18,34)):
    box("Cargo container",(x,y,z),(48,44,34),"DarkPaint" if y<0 else "Paint",1)
    for a in (-20,-10,0,10,20):box("Container stiffening rib",(x+a,y-23,z),(1.2,1.6,30),"Steel",.1)
    for side in (-1,1):box("Container corner casting",(x+side*22,y-23,z-15),(4,4,4),"Steel",.1)
box("Loading platform",(89,0,17),(26,95,10),"Steel",.5)
for y in (-51,51):cyl("Loading bollard",(103,y,24),3,28,"Ochre",12)
export("SM_Depot")

# Sensor: bolted steel tower and radar/radio equipment, no cartoon eye form.
foundation(111,105)
for x in (-30,30):
    for y in (-30,30):tube("Sensor tower leg",(x,y,12),(x*.35,y*.35,172),3.5,"Steel")
for z in (36,74,112,150):
    r=31-z*.10
    for y in (-r,r):tube("Sensor lattice diagonal",(-r,y,z),(r,y,z+30),1.7,"Steel")
    for x in (-r,r):tube("Sensor lattice diagonal",(x,-r,z),(x,r,z+30),1.7,"Steel")
box("Sensor service cabinet",(27,0,34),(29,34,43),"Paint",1)
vent((42,0,35),25,29)
cyl("Radar yaw unit",(0,0,172),17,16,"DarkPaint")
box("Phased array backing",(0,0,195),(14,74,50),"Steel",1)
box("Phased array panel",(8,0,195),(3,68,45),"Paint",.7)
for y in range(-28,30,7):
    for z in range(177,217,7):box("Radar array element",(10,y,z),(1.2,5,5),"DarkPaint",.1)
tube("Sensor mast aerial",(-8,0,215),(-8,0,247),1.3,"Steel")
cyl("Sensor navigation light",(-8,0,249),2.2,4,"Amber",12)
export("SM_Sensor")

# Defense platform: armored pedestal, articulated barrel group, cabling, observation optics.
foundation(140,134)
cyl("Turret armored lower ring",(0,0,32),46,39,"DarkPaint")
for a in range(0,360,45):
    t=math.radians(a);panel=box("Turret pedestal armor",(math.cos(t)*44,math.sin(t)*44,33),(27,5,30),"Paint",.6);panel.rotation_euler.z=t+math.pi/2
cyl("Turret bearing",(0,0,55),33,9,"Steel")
box("Turret armored receiver",(-4,0,81),(61,62,44),"Paint",3)
box("Turret rear counterweight",(-35,0,83),(13,70,38),"DarkPaint",1)
for y in (-19,19):
    tube("Turret barrel jacket",(22,y,83),(62,y,83),6.5,"DarkPaint")
    tube("Turret barrel",(57,y,83),(91,y,83),3.6,"Steel")
    tube("Turret muzzle brake",(86,y,83),(99,y,83),5.7,"Black")
    for x in range(29,57,6):tube("Barrel cooling collar",(x,y,83),(x+2,y,83),7.3,"Steel")
vent((-4,33,83),41,25,"y")
box("Targeting optics housing",(18,0,111),(26,28,15),"DarkPaint",1)
for y in (-7,7):uv_sphere("Targeting lens",(32,y,112),(2.5,8,8),"Window",12,6)
export("SM_Turret")

# Leaf meshes use thin curved lobed blades; no sphere/cone proxies form their crowns.
def leaf_mesh(name,items,mat):
    verts=[];faces=[];colors=[]
    for center,length,width,rot,color in items:
        c=Vector(center);offset=len(verts)
        points=((0,0,0),(-width*.48,length*.27,0),(-width*.55,length*.6,.06*length),(0,length,.02*length),(width*.55,length*.6,.06*length),(width*.48,length*.27,0),(0,length*.48,.1*length))
        verts.extend([c+rot@Vector(p) for p in points]);colors.extend([color]*7)
        faces.extend([(offset+6,offset+i,offset+(i+1)%6) for i in range(6)])
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);finish(o,name,mat)
    layer=mesh.color_attributes.new(name="Col",type="FLOAT_COLOR",domain="POINT")
    for i,c in enumerate(colors):layer.data[i].color=c
    return o

def oak(name,seed,height):
    rng=random.Random(seed);trunk=[]
    for i in range(7):trunk.append(Vector((math.sin(i*.7)*height*.018,math.cos(i*.6)*height*.014,i*height*.12)))
    for i in range(6):tube("Oak trunk",trunk[i],trunk[i+1],height*(.037-i*.0045),"Bark",height*(.033-i*.0045),12)
    leaves=[]
    for k in range(24):
        angle=k*2.399+rng.uniform(-.25,.25);h=height*rng.uniform(.37,.78)
        start=Vector((0,0,h));reach=height*rng.uniform(.22,.37)*(1.2-h/height*.55)
        end=Vector((math.cos(angle)*reach,math.sin(angle)*reach,h+height*rng.uniform(.08,.23)))
        mid=start.lerp(end,.55)+Vector((0,0,-height*.025))
        tube("Oak scaffold branch",start,mid,height*.012,"Bark",height*.007,9)
        tube("Oak outer branch",mid,end,height*.007,"Bark",height*.002,8)
        for j in range(5):
            a=angle+(j-2)*.39;tip=end+Vector((math.cos(a)*height*.09,math.sin(a)*height*.09,rng.uniform(-.02,.08)*height))
            tube("Oak twig",end,tip,height*.002,"Bark",.3,6)
            for n in range(25):
                phi=rng.random()*math.tau;z=rng.uniform(-1,1);r=rng.random()**.33*height*.075
                off=Vector((math.cos(phi)*math.sqrt(1-z*z)*r,math.sin(phi)*math.sqrt(1-z*z)*r,z*r*.65))
                rot=Matrix.Rotation(rng.random()*math.tau,3,"Z")@Matrix.Rotation(rng.uniform(-1.1,1.1),3,"X")
                shade=rng.uniform(.7,1.3);color=(.09*shade,.20*shade,.035*shade,1)
                leaves.append((tip+off,rng.uniform(11,18),rng.uniform(7,11),rot,color))
    leaf_mesh("Oak individual lobed leaves",leaves,"Leaf")
    return export(name)

oak("SM_OakA",32,880)
oak("SM_OakB",71,730)

def pine(name,seed,height):
    rng=random.Random(seed);tube("Pine tapering trunk",(0,0,0),(0,0,height),height*.027,"Bark",1.4,12)
    needles=[]
    for level in range(11):
        z=height*(.20+level*.066);reach=height*(.27-level*.021)
        for k in range(7):
            a=k*math.tau/7+level*.67+rng.uniform(-.13,.13)
            end=Vector((math.cos(a)*reach,math.sin(a)*reach,z-height*.025))
            tube("Pine tier branch",(0,0,z+height*.025),end,height*.007,"Bark",.4,7)
            for t in range(5):
                f=(t+1)/6;pos=end*f+Vector((0,0,(z+height*.025)*(1-f)))
                for side in (-1,1):
                    twig=pos+Vector((math.cos(a+side*.75)*reach*.20,math.sin(a+side*.75)*reach*.20,height*.025))
                    tube("Pine branchlet",pos,twig,1.1,"Bark",.2,5)
                    for n in range(4):
                        p=pos.lerp(twig,n/4);rot=Matrix.Rotation(a+side*.8+rng.uniform(-.4,.4),3,"Z")@Matrix.Rotation(rng.uniform(-.7,.7),3,"X")
                        needles.append((p,rng.uniform(15,23),rng.uniform(4,6),rot,(.035,.105+rng.random()*.05,.027,1)))
    leaf_mesh("Pine narrow needle sprays",needles,"Pine");return export(name)

pine("SM_PineA",103,990)
pine("SM_PineB",210,780)

rng=random.Random(91);leaves=[]
for k in range(15):
    a=rng.random()*math.tau;end=Vector((math.cos(a)*rng.uniform(25,60),math.sin(a)*rng.uniform(25,60),rng.uniform(35,90)))
    tube("Shrub woody stem",(0,0,0),end,2,"Bark",.4,6)
    for n in range(30):
        p=end+Vector((rng.uniform(-23,23),rng.uniform(-23,23),rng.uniform(-16,20)))
        leaves.append((p,rng.uniform(9,16),rng.uniform(6,10),Matrix.Rotation(rng.random()*math.tau,3,"Z")@Matrix.Rotation(rng.uniform(-1,1),3,"X"),(.075,.18+rng.random()*.06,.032,1)))
leaf_mesh("Shrub individual leaves",leaves,"Leaf");export("SM_Shrub")

# Grass blades are bent tapered ribbons, with varied natural lengths and colors.
rng=random.Random(29);verts=[];faces=[];colors=[]
for k in range(48):
    a=rng.random()*math.tau;r=rng.random()**.5*26;length=rng.uniform(15,42);width=rng.uniform(.7,1.8)
    root=Vector((math.cos(a)*r,math.sin(a)*r,0));side=Vector((-math.sin(a),math.cos(a),0))*width
    bend=Vector((math.cos(a)*length*.32,math.sin(a)*length*.32,length));i=len(verts)
    verts.extend((root-side,root+side,root+bend*.52-side*.6,root+bend*.52+side*.6,root+bend))
    faces.extend(((i,i+1,i+3,i+2),(i+2,i+3,i+4)))
    tone=rng.uniform(.6,1.25);colors.extend([(.15*tone,.22*tone,.043*tone,1)]*5)
mesh=bpy.data.meshes.new("Bent grass blades");mesh.from_pydata(verts,[],faces);mesh.update()
o=bpy.data.objects.new("Bent grass blades",mesh);bpy.context.collection.objects.link(o);finish(o,o.name,"Grass")
cl=mesh.color_attributes.new(name="Col",type="FLOAT_COLOR",domain="POINT")
for i,c in enumerate(colors):cl.data[i].color=c
export("SM_Grass")

for name,seed,size in (("SM_RockA",3,(142,108,85)),("SM_RockB",7,(185,126,110))):
    rng=random.Random(seed);bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3,radius=1)
    o=bpy.context.object
    for v in o.data.vertices:
        p=v.co;noise=1+rng.uniform(-.12,.12)+math.sin(p.x*6+p.y*2)*.08
        p.x*=size[0]*.5*noise;p.y*=size[1]*.5*noise;p.z*=size[2]*.5*noise
        if p.z < -size[2]*.3:p.z=-size[2]*.3+(p.z+size[2]*.3)*.17
    for poly in o.data.polygons:poly.use_smooth=True
    finish(o,"Weathered irregular stone","Rock")
    bpy.ops.object.select_all(action="DESELECT");o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.mode_set(mode="EDIT");bpy.ops.mesh.select_all(action="SELECT");bpy.ops.uv.smart_project(island_margin=.02);bpy.ops.object.mode_set(mode="OBJECT")
    export(name)

(OUT/"realistic_manifest.json").write_text(json.dumps(manifest,indent=2),encoding="utf-8")

# Review scene: detailed colony at a temperate forest edge with actual ground albedo.
for o in ASSETS.values():o.hide_render=True
placements={"SM_Core":(-210,-90,0),"SM_Factory":(60,30,0),"SM_Depot":(310,80,0),"SM_Extractor":(-360,240,0),"SM_Sensor":(10,280,0),"SM_Turret":(260,-200,0),"SM_OakA":(-610,330,0),"SM_OakB":(450,390,0),"SM_PineA":(-410,650,0),"SM_PineB":(70,650,0),"SM_Shrub":(-40,-260,0),"SM_RockA":(-350,-230,0),"SM_RockB":(360,-310,0)}
for name,pos in placements.items():ASSETS[name].location=pos;ASSETS[name].hide_render=False
for i in range(140):
    rng=random.Random(i+500);o=ASSETS["SM_Grass"].copy();o.data=ASSETS["SM_Grass"].data;bpy.context.collection.objects.link(o)
    o.location=(rng.uniform(-700,700),rng.uniform(-430,600),0);o.rotation_euler.z=rng.random()*math.tau;o.hide_render=False
bpy.ops.mesh.primitive_plane_add(size=6000,location=(0,0,-.2));ground=bpy.context.object
ground_mat=bpy.data.materials.new("Preview photographic grass");ground_mat.use_nodes=True;n=ground_mat.node_tree.nodes;l=ground_mat.node_tree.links;p=n.get("Principled BSDF")
tex=n.new("ShaderNodeTexImage");tex.image=bpy.data.images.load(str(ART/"Textures/PolyHaven/grass_ground_diff_2k.jpg"));coord=n.new("ShaderNodeTexCoord");mapping=n.new("ShaderNodeVectorMath");mapping.operation="SCALE";mapping.inputs[3].default_value=14
l.new(coord.outputs["UV"],mapping.inputs[0]);l.new(mapping.outputs[0],tex.inputs[0]);l.new(tex.outputs["Color"],p.inputs["Base Color"]);p.inputs["Roughness"].default_value=.93;ground.data.materials.append(ground_mat)
world=bpy.data.worlds.new("Earth daylight");world.use_nodes=True;bpy.context.scene.world=world
sky=world.node_tree.nodes.new("ShaderNodeTexSky");sky.sky_type="NISHITA";sky.sun_elevation=.6;sky.sun_rotation=2.4;sky.air_density=1.0
world.node_tree.links.new(sky.outputs[0],world.node_tree.nodes["Background"].inputs[0]);world.node_tree.nodes["Background"].inputs[1].default_value=.25
bpy.ops.object.light_add(type="SUN",location=(0,0,1500));bpy.context.object.data.energy=2.2;bpy.context.object.rotation_euler=(.6,-.4,-.5);bpy.context.object.data.angle=.06
bpy.ops.object.camera_add(location=(1250,-1560,1160));cam=bpy.context.object;cam.rotation_euler=(Vector((0,100,160))-cam.location).to_track_quat("-Z","Y").to_euler();cam.data.type="ORTHO";cam.data.ortho_scale=1900;cam.data.clip_end=12000
scene=bpy.context.scene;scene.camera=cam;scene.render.engine="CYCLES";scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1800;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.view_settings.view_transform="AgX";scene.view_settings.exposure=-1.5
scene.render.filepath=str(ART/"Previews/realistic_forest_colony.png");bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(ART/"Source/Seige_Realistic_Assets.blend"),compress=True);bpy.ops.render.render(write_still=True)
# Architecture close-up uses a second camera framing without changing the retained source layout.
cam.location=(380,-560,450);cam.rotation_euler=(Vector((-140,-30,85))-cam.location).to_track_quat("-Z","Y").to_euler();cam.data.ortho_scale=610
scene.render.filepath=str(ART/"Previews/industrial_core_detail.png");bpy.ops.render.render(write_still=True)
print("SEIGE_REALISTIC_ASSETS_CREATED "+json.dumps({k:v["dimensions"] for k,v in manifest["meshes"].items()}))

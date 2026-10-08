"""Original upright command spacecraft, expansion campus and hovering worker.

Run in Blender background. Uses only the project's original industrial palette.
The two weapon banks are hull sockets; actual equipped guns render from the loadout.
"""
from pathlib import Path
import bpy, math, json
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[1]
# Reuse the tested original mesh/export helpers without building the older assets.
helper=(ROOT/'Tools/create_construction_assets.py').read_text()
exec(compile(helper.split('# Compact vertical-lift shuttle:')[0],str(ROOT/'Tools/create_construction_assets.py'),'exec'))
OUT=ART/'OrbitalV09/Exports';SOURCE=ART/'OrbitalV09/Source'
OUT.mkdir(parents=True,exist_ok=True);SOURCE.mkdir(parents=True,exist_ok=True)

def ring(name,z,r,h,mat='Steel',top=None,sides=48):
    return tube(name,(0,0,z),(0,0,z+h),r,mat,r2=top,sides=sides)

def ellipsoid(name,p,d,mat):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=12,location=p)
    o=bpy.context.object;o.dimensions=d;bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for f in o.data.polygons:f.use_smooth=True
    return finish(o,name,mat)

# A tail-sitting rocket, with an intact pressure hull and a ground-level cargo hatch.
# Blender +X remains Unreal +X; the recessed hatch is the physical workforce origin.
ring('Reactor nozzle shielding',90,255,210,'Carbon',210)
ring('Fusion engine bell',35,175,270,'Steel',110)
ring('Engine throat',30,137,10,'Carbon')
ring('Lower service skirt',170,300,350,'Slate',278)
ring('Cargo pressure barrel',460,278,965,'Ceramic')
ring('Upper avionics collar',1425,278,78,'Slate')
ring('Command and guidance section',1503,278,290,'Ceramic',205)
ring('Smooth ogive lower',1793,205,150,'Ceramic',118)
ring('Smooth ogive nose',1943,118,145,'Ceramic',9)
for z in (520,820,1135,1390):ring('Circumferential panel seam',z,280,5,'Steel')
for a in range(0,360,30):
    t=math.radians(a)
    beam('Longitudinal thermal seam',(math.cos(t)*278,math.sin(t)*278,560),(math.cos(t)*278,math.sin(t)*278,1380),3,'Slate')
    for z in (620,1250):
        tube('RCS actuator housing',(math.cos(t)*265,math.sin(t)*265,z),(math.cos(t)*292,math.sin(t)*292,z),17,'Steel',r2=20)
for a in (45,135,225,315):
    t=math.radians(a);c,s=math.cos(t),math.sin(t)
    beam('Landing outrigger main',(c*220,s*220,700),(c*455,s*455,95),48,'Slate')
    beam('Landing shock cylinder',(c*268,s*268,490),(c*412,s*412,125),23,'Steel')
    box('Landing foot',(c*445,s*445,28),(165,165,56),'Carbon',17,rotation=t)
    box('Landing ceramic fairing',(c*282,s*282,690),(110,72,240),'Ceramic',18,rotation=t)
    box('Navigation beacon',(c*310,s*310,807),(22,22,16),'Light',4)
for sign in (-1,1):
    # Two large hull banks, facing east. 1L in the port bank; 2M+8S in starboard.
    box('Large weapon socket armored sponson',(172,sign*267,1120),(244,166,222),'Slate',18)
    box('Large hardpoint service cover',(299,sign*267,1120),(10,147,192),'Steel',3)
    for z in (1050,1190):box('Bank status lamp',(307,sign*267,z),(4,108,5),'Light',1)
for y in (-120,0,120):
    box('Command observation glazing',(237,y,1605),(16,91,90),'Glass',9)
box('Cargo hatch recessed frame',(284,0,155),(28,190,310),'Slate',8)
box('Cargo hatch dark interior',(302,0,147),(8,157,282),'Carbon',3)
for y in (-90,90):box('Hatch guide',(310,y,150),(18,14,295),'Steel',2)
box('Hatch status light',(313,0,320),(8,125,8),'Light',1)
box('Deployed cargo threshold',(352,0,3),(116,185,6),'Steel',3)
for x in range(309,402,20):box('Threshold traction rib',(x,0,7),(4,165,2),'Yellow',.4)
for y in (-185,185):
    box('Radiator housing',(-218,y,1050),(68,91,480),'Slate',8)
    for z in range(840,1270,28):box('Radiator fin',(-258,y,z),(26,94,5),'Steel',.5)
export('SM_Shuttle','Original vertical orbital command shuttle: upright pressure hull, ogive nose, four landing outriggers, fusion engine, east cargo hatch and two large modular weapon banks.')

# Additional levels surround the ship, preserving the original landing point.
# The ship is a separate actor of constant scale; the central well stays open.
for sign in (-1,1):
    for axis in (0,1):
        x,y=(sign*780,0) if axis==0 else (0,sign*780)
        w,d=(410,1400) if axis==0 else (1400,410)
        box('Campus strip foundation',(x,y,20),(w+60,d+60,40),'Concrete',9)
        box('Expansion service wing',(x,y,200),(w,d,340),'Slate',14)
        box('Roof cassette',(x,y,382),(w+15,d+15,25),'Ceramic',8)
        for k in (-1,0,1):
            xx,yy=(x,k*400) if axis==0 else (k*400,y)
            box('External equipment cell',(xx,yy,480),(220,180,166),'Ceramic',8)
            tube('Cooling fan',(xx,yy,563),(xx,yy,575),65,'Carbon',sides=24)
            for a in range(0,360,60):
                t=math.radians(a);beam('Fan grille',(xx,yy,579),(xx+math.cos(t)*60,yy+math.sin(t)*60,579),3,'Steel')
        for k in range(-550,551,110):
            xx,yy=(x+w*.5+2,k) if axis==0 else (k,y-d*.5-2)
            box('Facade glazing',(xx,yy,249),(8,78,93) if axis==0 else (78,8,93),'Glass',3)
box('East access bridge',(720,0,46),(600,170,20),'Steel',3)
for y in (-96,96):beam('Bridge handrail',(425,y,130),(990,y,130),6,'Yellow')
export('SM_Core','Original four-wing command expansion campus surrounding an open central upright-shuttle well. The original spacecraft remains independently rendered at its original size and location.')

# One legless 80 kg worker. Rounded shell, expressive visor, tool arms and four lift pods.
ellipsoid('Ceramic pressure shell',(0,0,47),(66,55,79),'Ceramic')
ellipsoid('Visor recess',(28,0,64),(17,43,28),'Carbon')
for y in (-11,11):ellipsoid('Optical eye',(36,y,66),(4,9,7),'Light')
box('Lower cargo socket',(28,0,33),(16,37,21),'Slate',5)
for y in (-30,30):
    ellipsoid('Shoulder joint',(0,y,46),(18,18,20),'Slate')
    beam('Manipulator forearm',(0,y,45),(22,y*1.13,28),9,'Ceramic')
    box('Manipulator tool',(26,y*1.13,27),(14,12,9),'Steel',2)
    for sign in (-1,1):
        tube('Low-hover duct',(sign*19,y*.7,12),(sign*19,y*.7,22),12,'Slate',sides=20)
        tube('Hover emitter',(sign*19,y*.7,9),(sign*19,y*.7,13),8,'Light',sides=16)
for z in (33,39,45,51):box('Rear heat exchanger',(-30,0,z),(5,26,2),'Steel',.5)
box('Maintenance hatch',(-27,0,66),(8,25,19),'Slate',3)
export('SM_Robot','Original legless low-hover worker, rounded ceramic shell, twin cyan optical eyes, articulated tool arms, payload socket and four contained lift emitters. Represents one persistent 80 kg worker.')

for o in ASSETS.values():o.location=(0,0,0)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'Seige_Orbital_Workforce.blend'),compress=True)
manifest={'units':'centimeters','license':'Original project work','generator':'Tools/create_orbital_assets_v09.py','meshes':records,'hatch':{'direction_unreal':[1,0],'x_centimeters':302,'runtime_half_footprint_fraction':.71177,'grounded':True},'shuttle_weapon_banks':2}
(OUT/'orbital_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('SEIGE_ORBITAL_ASSETS_COMPLETE',flush=True)

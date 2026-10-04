"""Import original industrial/temperate assets and local CC0 PBR surfaces into Unreal.

Terrain UV0 is supplied by the runtime. Vertex R is dirt and G is rock; the rest is grass.
All texture references are local packaged assets. No network requests occur in this script.
"""
from pathlib import Path
import json
import unreal

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/"Art"; OUT=ART/"RealisticExports"; DEST="/Game/Art"
DATA=json.loads((OUT/"realistic_manifest.json").read_text(encoding="utf-8"))
TOOLS=unreal.AssetToolsHelpers.get_asset_tools(); ED=unreal.EditorAssetLibrary; ME=unreal.MaterialEditingLibrary
ED.make_directory(DEST);ED.make_directory(DEST+"/Textures")

def asset(name,kind,factory):
    path=DEST+"/"+name
    if ED.does_asset_exist(path):return ED.load_asset(path)
    result=TOOLS.create_asset(name,DEST,kind,factory)
    if not result:raise RuntimeError("Failed creating "+path)
    return result

def node(mat,kind,x=0,y=0,**props):
    n=ME.create_material_expression(mat,kind,x,y)
    for k,v in props.items():n.set_editor_property(k,v)
    return n

def wire(a,b,pin,out=""):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError("Could not connect "+str(a)+" to "+pin)

def output(n,matprop,pin=""):
    if not ME.connect_material_property(n,pin,matprop):raise RuntimeError("Could not connect material output "+str(matprop))

def scalar(mat,name,value,x=-800,y=0):
    return node(mat,unreal.MaterialExpressionScalarParameter,x,y,parameter_name=name,default_value=value)

def vector(mat,name,value,x=-800,y=0):
    return node(mat,unreal.MaterialExpressionVectorParameter,x,y,parameter_name=name,default_value=unreal.LinearColor(*value))

def binary(mat,kind,a,b,ap="",bp="",x=0,y=0):
    n=node(mat,kind,x,y);wire(a,n,"A",ap);wire(b,n,"B",bp);return n

def multiply(mat,a,b,ap="",bp="",x=0,y=0):return binary(mat,unreal.MaterialExpressionMultiply,a,b,ap,bp,x,y)

def new_material(name):
    m=asset(name,unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_instanced_static_meshes",True)
    ME.delete_all_material_expressions(m);return m

def save_material(m):
    ME.layout_material_expressions(m);ME.recompile_material(m);ED.save_loaded_asset(m,only_if_is_dirty=False)

# Twelve photographic maps: three terrain surfaces and bark, each color/normal/roughness.
surface_ids={"Grass":"grass_ground","Dirt":"brown_mud_dry","Rock":"rock_boulder_dry","Bark":"bark_brown_02"}
suffixes={"Color":"diff","Normal":"nor_dx","Roughness":"rough"}
tasks=[]
for surface,source in surface_ids.items():
    for channel,suffix in suffixes.items():
        path=ART/"Textures/PolyHaven"/(source+"_"+suffix+"_2k.jpg")
        if not path.exists():raise RuntimeError("Missing local PBR map "+str(path))
        task=unreal.AssetImportTask();task.filename=str(path);task.destination_path=DEST+"/Textures"
        task.destination_name="T_"+surface+"_"+channel;task.automated=True;task.replace_existing=True;task.save=True;tasks.append(task)
TOOLS.import_asset_tasks(tasks)
TEXTURES={}
for surface in surface_ids:
    for channel in suffixes:
        key=surface+channel;t=ED.load_asset(DEST+"/Textures/T_"+surface+"_"+channel)
        if not t:raise RuntimeError("Texture import failed "+key)
        t.set_editor_property("srgb",channel=="Color")
        if channel=="Normal":
            t.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("flip_green_channel",False)
        else:t.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_DEFAULT)
        ED.save_loaded_asset(t,only_if_is_dirty=False);TEXTURES[key]=t

def sample(mat,key,uv):
    n=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name=key,texture=TEXTURES[key])
    sampler=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if key.endswith("Normal") else (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if key.endswith("Color") else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    n.set_editor_property("sampler_type",sampler);wire(uv,n,"UVs");return n

# Terrain: a weighted PBR blend with macro color variation anchored in world space.
terrain=new_material("M_Terrain")
uv=node(terrain,unreal.MaterialExpressionTextureCoordinate)
uv=multiply(terrain,uv,scalar(terrain,"TextureScale",3.5))
vc=node(terrain,unreal.MaterialExpressionVertexColor)
sum_rg=binary(terrain,unreal.MaterialExpressionAdd,vc,vc,"R","G")
sat=node(terrain,unreal.MaterialExpressionSaturate);wire(sum_rg,sat,"")
grass_weight=node(terrain,unreal.MaterialExpressionOneMinus);wire(sat,grass_weight,"")

def terrain_channel(channel):
    samples={s:sample(terrain,s+channel,uv) for s in ("Grass","Dirt","Rock")}
    pin="R" if channel=="Roughness" else ""
    grass=multiply(terrain,samples["Grass"],grass_weight,pin)
    dirt=multiply(terrain,samples["Dirt"],vc,pin,"R")
    rock=multiply(terrain,samples["Rock"],vc,pin,"G")
    return binary(terrain,unreal.MaterialExpressionAdd,binary(terrain,unreal.MaterialExpressionAdd,grass,dirt),rock)

base=terrain_channel("Color")
position=node(terrain,unreal.MaterialExpressionWorldPosition)
noise_position=multiply(terrain,position,scalar(terrain,"MacroScale",.0008))
noise=node(terrain,unreal.MaterialExpressionNoise,scale=1.0,quality=1,levels=2,output_min=.73,output_max=1.08)
wire(noise_position,noise,"")
base=multiply(terrain,base,noise)
base=multiply(terrain,base,vector(terrain,"TerrainTint",(1,1,1,1)))
output(base,unreal.MaterialProperty.MP_BASE_COLOR)
normal=node(terrain,unreal.MaterialExpressionNormalize);wire(terrain_channel("Normal"),normal,"")
output(normal,unreal.MaterialProperty.MP_NORMAL)
output(terrain_channel("Roughness"),unreal.MaterialProperty.MP_ROUGHNESS)
save_material(terrain)

# Rough metal/concrete surfaces with understated procedural weathering.
industrial=new_material("M_Industrial")
tint=vector(industrial,"Tint",(.5,.53,.5,1));rough=scalar(industrial,"Roughness",.6);metal=scalar(industrial,"Metallic",.4)
pos=node(industrial,unreal.MaterialExpressionWorldPosition);p=multiply(industrial,pos,scalar(industrial,"WeatherScale",.12))
n=node(industrial,unreal.MaterialExpressionNoise,scale=1.0,quality=1,levels=2,output_min=.72,output_max=1.02);wire(p,n,"")
output(multiply(industrial,tint,n),unreal.MaterialProperty.MP_BASE_COLOR)
output(rough,unreal.MaterialProperty.MP_ROUGHNESS);output(metal,unreal.MaterialProperty.MP_METALLIC)
output(multiply(industrial,tint,scalar(industrial,"Emission",0)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
save_material(industrial)

# Geometry leaves carry varied vertex colors. Two-sided foliage gives backlit leaf edges.
foliage=new_material("M_Foliage")
foliage.set_editor_property("two_sided",True)
foliage.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
leaf_vc=node(foliage,unreal.MaterialExpressionVertexColor);leaf_tint=vector(foliage,"Tint",(1,1,1,1))
leaf_color=multiply(foliage,leaf_vc,leaf_tint)
output(leaf_color,unreal.MaterialProperty.MP_BASE_COLOR)
output(multiply(foliage,leaf_color,scalar(foliage,"Transmission",.65)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
output(scalar(foliage,"Roughness",.83),unreal.MaterialProperty.MP_ROUGHNESS)
save_material(foliage)

surface_materials={}
for surface in ("Bark","Rock"):
    m=new_material("M_"+surface+"PBR")
    coords=node(m,unreal.MaterialExpressionTextureCoordinate);coords=multiply(m,coords,scalar(m,"TextureScale",1))
    output(multiply(m,sample(m,surface+"Color",coords),vector(m,"Tint",(1,1,1,1))),unreal.MaterialProperty.MP_BASE_COLOR)
    output(sample(m,surface+"Normal",coords),unreal.MaterialProperty.MP_NORMAL)
    output(sample(m,surface+"Roughness",coords),unreal.MaterialProperty.MP_ROUGHNESS,"R")
    save_material(m);surface_materials[surface]=m

instances={}
for slot,settings in DATA["palette"].items():
    kind=slot.removeprefix("RM_");instance=asset("MI_Real_"+kind,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    parent=foliage if settings["foliage"] else surface_materials.get(kind,industrial)
    ME.set_material_instance_parent(instance,parent)
    color=(1,1,1,1) if settings["foliage"] or kind in surface_materials else settings["color"]
    ME.set_material_instance_vector_parameter_value(instance,"Tint",unreal.LinearColor(*color))
    if parent==industrial:
        ME.set_material_instance_scalar_parameter_value(instance,"Roughness",settings["roughness"])
        ME.set_material_instance_scalar_parameter_value(instance,"Metallic",settings["metallic"])
        ME.set_material_instance_scalar_parameter_value(instance,"Emission",settings["emission"])
    ME.update_material_instance(instance);ED.save_loaded_asset(instance,only_if_is_dirty=False);instances[slot]=instance

tasks=[]
for name,record in DATA["meshes"].items():
    task=unreal.AssetImportTask();task.filename=str(OUT/record["fbx"]);task.destination_path=DEST;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
    options=unreal.FbxImportUI();options.import_mesh=True;options.import_as_skeletal=False;options.import_materials=False;options.import_textures=False
    options.automated_import_should_detect_type=False;options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    sm=options.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=True;sm.generate_lightmap_u_vs=True
    sm.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
    sm.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    task.options=options;tasks.append(task)
TOOLS.import_asset_tasks(tasks)
report={"materials":["M_Terrain","M_Industrial","M_Foliage","M_BarkPBR","M_RockPBR"],"textures":list(TEXTURES),"meshes":{}}
# The Python commandlet does not register this lazily loaded editor subsystem.
# Its mesh-editing methods are stateless and also work on the class default object.
mesh_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
for name,record in DATA["meshes"].items():
    mesh=ED.load_asset(DEST+"/"+name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError("Missing mesh "+name)
    slots=mesh.get_editor_property("static_materials")
    if len(slots)!=len(record["materials"]):raise RuntimeError("Unexpected material count "+name)
    for index,slot in enumerate(slots):
        key=str(slot.get_editor_property("imported_material_slot_name"))
        if key not in instances:key=record["materials"][index]
        mesh.set_material(index,instances[key])
    if name.startswith(("SM_Oak","SM_Pine")):
        targets=((1.0,1.0),(.45,.45),(.16,.18),(.04,.06))
    elif name in ("SM_Shrub","SM_Grass"):
        targets=((1.0,1.0),(.45,.25),(.15,.07))
    else:
        targets=((1.0,1.0),(.5,.3),(.2,.1))
    reduction=unreal.StaticMeshReductionOptions()
    reduction.set_editor_property("auto_compute_lod_screen_size",False)
    lods=[]
    for fraction,screen in targets:
        setting=unreal.StaticMeshReductionSettings()
        setting.set_editor_property("percent_triangles",fraction)
        setting.set_editor_property("screen_size",screen)
        lods.append(setting)
    reduction.set_editor_property("reduction_settings",lods)
    result=mesh_editor.set_lods(mesh,reduction)
    if result!=len(targets):raise RuntimeError("LOD generation failed for "+name+": "+str(result))
    ED.save_loaded_asset(mesh,only_if_is_dirty=False)
    box=mesh.get_bounding_box();lo,hi=box.min,box.max;dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.2:raise RuntimeError("Ground pivot mismatch "+name)
    if any(abs(a-b)>max(.3,b*.01) for a,b in zip(dims,record["dimensions"])):raise RuntimeError("Mesh scale mismatch "+name)
    report["meshes"][name]={"dimensions_cm":dims,"ground_z":lo.z,"material_slots":len(slots),"triangles_source":record["triangles"],"lod_count":mesh_editor.get_lod_count(mesh),"lod_vertices":[mesh_editor.get_number_verts(mesh,i) for i in range(len(targets))],"lod_screen_sizes":list(mesh_editor.get_lod_screen_sizes(mesh))}
(ART/"realistic_import_report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
# Authored leaf coverage takes precedence over generic reduction for trees.
if (OUT/"foliage_lods.json").exists():
    import runpy
    runpy.run_path(str(ROOT/"Tools/import_foliage_lods.py"),run_name="__main__")
    report=json.loads((ART/"realistic_import_report.json").read_text(encoding="utf-8"))
unreal.log("SEIGE_REALISTIC_IMPORT_SUCCESS "+json.dumps(report))

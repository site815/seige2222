"""Targeted importer for the six original full-scale industrial buildings.

Run only after create_industry_assets.py. Does not modify foliage, terrain, gameplay,
robots, bugs, or maps. Replaces existing /Game/Art/SM_* building references in place.
Use -IndustryCoreOnly for the roof refinement without reimporting other meshes/materials.
"""
from pathlib import Path
import json, unreal

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/"Art";OUT=ART/"IndustryExports"
DATA=json.loads((OUT/"industry_manifest.json").read_text(encoding="utf-8"))
DEST="/Game/Art/Industry";MESH_DEST="/Game/Art"
CORE_ONLY="-IndustryCoreOnly" in unreal.SystemLibrary.get_command_line()
mesh_records={k:v for k,v in DATA["meshes"].items() if not CORE_ONLY or k=="SM_Core"}
ED=unreal.EditorAssetLibrary;TOOLS=unreal.AssetToolsHelpers.get_asset_tools();ME=unreal.MaterialEditingLibrary
ED.make_directory(DEST);ED.make_directory(DEST+"/Textures")

def asset(name,cls,factory):
    path=DEST+"/"+name
    return ED.load_asset(path) if ED.does_asset_exist(path) else TOOLS.create_asset(name,DEST,cls,factory)
def node(mat,cls,**props):
    n=ME.create_material_expression(mat,cls)
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def wire(a,b,pin,out=""):
    if not ME.connect_material_expressions(a,out,b,pin):raise RuntimeError("Could not connect "+pin)
def output(a,prop,pin=""):
    if not ME.connect_material_property(a,pin,prop):raise RuntimeError("Could not connect output "+str(prop))

if CORE_ONLY:
    instances={slot:ED.load_asset(DEST+"/MI_Industry_"+slot.removeprefix("IM_")) for slot in DATA["palette"]}
    if any(value is None for value in instances.values()):raise RuntimeError("Core-only import needs the existing industrial materials")
else:
    tasks=[]
    for surface in DATA["textures"]:
        for channel in ("Color","Roughness","Normal"):
            name="T_Industry_"+surface+"_"+channel;path=ART/"Textures/Industry"/(name+".png")
            if not path.exists():raise RuntimeError("Missing authored PBR map "+str(path))
            t=unreal.AssetImportTask();t.filename=str(path);t.destination_path=DEST+"/Textures";t.destination_name=name
            t.automated=True;t.replace_existing=True;t.save=True;tasks.append(t)
    TOOLS.import_asset_tasks(tasks)
    textures={}
    for surface in DATA["textures"]:
        textures[surface]={}
        for channel in ("Color","Roughness","Normal"):
            t=ED.load_asset(DEST+"/Textures/T_Industry_"+surface+"_"+channel)
            if not t:raise RuntimeError("Texture import failed: "+surface+channel)
            t.set_editor_property("srgb",channel=="Color")
            if channel=="Normal":
                t.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP)
                t.set_editor_property("flip_green_channel",True) # Blender/OpenGL-authored +Y maps.
            else:t.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_DEFAULT)
            ED.save_loaded_asset(t,only_if_is_dirty=False);textures[surface][channel]=t

    master=asset("M_IndustrySurface",unreal.Material,unreal.MaterialFactoryNew())
    master.set_editor_property("used_with_instanced_static_meshes",True);ME.delete_all_material_expressions(master)
    samples={}
    for channel in ("Color","Roughness","Normal"):
        t=node(master,unreal.MaterialExpressionTextureSampleParameter2D,parameter_name=channel,texture=textures["Paint"][channel])
        t.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if channel=="Normal" else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if channel=="Color" else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        samples[channel]=t
    tint=node(master,unreal.MaterialExpressionVectorParameter,parameter_name="Tint",default_value=unreal.LinearColor(.6,.64,.63,1))
    color=node(master,unreal.MaterialExpressionMultiply);wire(tint,color,"A");wire(samples["Color"],color,"B")
    rough=node(master,unreal.MaterialExpressionScalarParameter,parameter_name="RoughnessScale",default_value=1.0)
    mult=node(master,unreal.MaterialExpressionMultiply);wire(rough,mult,"A");wire(samples["Roughness"],mult,"B","R")
    metal=node(master,unreal.MaterialExpressionScalarParameter,parameter_name="Metallic",default_value=.4);output(metal,unreal.MaterialProperty.MP_METALLIC)
    normal_strength=node(master,unreal.MaterialExpressionScalarParameter,parameter_name="NormalStrength",default_value=.35)
    flat_normal=node(master,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(0,0,1,1))
    normal_mix=node(master,unreal.MaterialExpressionLinearInterpolate);wire(flat_normal,normal_mix,"A");wire(samples["Normal"],normal_mix,"B");wire(normal_strength,normal_mix,"Alpha")
    normal=node(master,unreal.MaterialExpressionNormalize);wire(normal_mix,normal,"");output(normal,unreal.MaterialProperty.MP_NORMAL)
    emission=node(master,unreal.MaterialExpressionScalarParameter,parameter_name="Emission",default_value=0)
    # v0.9.2 art pass 4: one HLSL block adds per-building tint variation (hash
    # of the object pivot), soil grime over the lowest metre and a half above
    # the pivot (every kit mesh has its pivot on the ground), seasonal snow on
    # upward faces from MPC_Weather.SnowCoverage, and lit windows / dimmed
    # daytime lamps from MPC_Weather.Night. Instances choose with parameters.
    weather=ED.load_asset("/Game/Art/WeatherV09/MPC_Weather")
    if not weather:raise RuntimeError("MPC_Weather is missing; run import_weather_v09.py first")
    parameters=list(weather.get_editor_property("scalar_parameters"))
    if not any(str(q.get_editor_property("parameter_name"))=="Night" for q in parameters):
        q=unreal.CollectionScalarParameter();q.set_editor_property("parameter_name","Night");q.set_editor_property("default_value",0);parameters.append(q)
        weather.set_editor_property("scalar_parameters",parameters);ED.save_loaded_asset(weather,False)
    snow=node(master,unreal.MaterialExpressionCollectionParameter,collection=weather,parameter_name="SnowCoverage")
    night=node(master,unreal.MaterialExpressionCollectionParameter,collection=weather,parameter_name="Night")
    params={name:node(master,unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=value) for name,value in
            (("TintVariation",.06),("GrimeStrength",.45),("GrimeHeight",160.),("SnowAmount",1.),("WindowGlow",0.),("LampDayScale",1.))}
    # Large-world positions stay in material nodes (LWC-aware); the HLSL blocks
    # only receive a float seed in [0,1) and the height above the pivot.
    world=node(master,unreal.MaterialExpressionWorldPosition);pivot=node(master,unreal.MaterialExpressionObjectPositionWS);up=node(master,unreal.MaterialExpressionVertexNormalWS)
    scaled=node(master,unreal.MaterialExpressionMultiply,const_b=.00123);wire(pivot,scaled,"A")
    seed=node(master,unreal.MaterialExpressionFrac);wire(scaled,seed,"")
    above=node(master,unreal.MaterialExpressionSubtract);wire(world,above,"A");wire(pivot,above,"B")
    height=node(master,unreal.MaterialExpressionComponentMask,r=False,g=False,b=True,a=False);wire(above,height,"")
    def custom(code,output_type,inputs):
        c=node(master,unreal.MaterialExpressionCustom);c.set_editor_property("code",code);c.set_editor_property("output_type",output_type)
        pins=[]
        for name in inputs:
            i=unreal.CustomInput();i.set_editor_property("input_name",name);pins.append(i)
        c.set_editor_property("inputs",pins);return c
    surface=custom("""
float2 k = Seed.xy * 100.0;
float r1 = frac(sin(dot(k, float2(12.9898, 78.233))) * 43758.5453);
float r2 = frac(sin(dot(k, float2(39.3468, 11.1350))) * 24634.6345);
float3 c = Base * (1.0 + Variation * (r1 * 2.0 - 1.0));
c *= lerp(float3(1.0 + Variation * 0.6, 1.0, 1.0 - Variation * 0.6), float3(1.0 - Variation * 0.6, 1.0, 1.0 + Variation * 0.6), r2);
float g = saturate(1.0 - Height / max(GrimeHeight, 1.0)); g = g * g * GrimeStrength * (0.65 + 0.35 * Base.g / max(max(Base.r, Base.b), 0.05));
c = lerp(c, c * float3(0.52, 0.47, 0.40), saturate(g));
float s = saturate((Up.z - 0.55) * 3.0) * saturate(Snow) * SnowAmount;
return lerp(c, float3(0.84, 0.87, 0.90), s);
""",unreal.CustomMaterialOutputType.CMOT_FLOAT3,["Base","Seed","Height","Up","Variation","GrimeStrength","GrimeHeight","Snow","SnowAmount"])
    for pin,source in (("Base",color),("Seed",seed),("Height",height),("Up",up),("Variation",params["TintVariation"]),("GrimeStrength",params["GrimeStrength"]),("GrimeHeight",params["GrimeHeight"]),("Snow",snow),("SnowAmount",params["SnowAmount"])):wire(source,surface,pin)
    output(surface,unreal.MaterialProperty.MP_BASE_COLOR)
    roughness=custom("""
float g = saturate(1.0 - Height / max(GrimeHeight, 1.0)); g = g * g * GrimeStrength;
float s = saturate((Up.z - 0.55) * 3.0) * saturate(Snow) * SnowAmount;
return clamp(lerp(R + g * 0.25, 0.62, s), 0.08, 0.98);
""",unreal.CustomMaterialOutputType.CMOT_FLOAT1,["R","Height","Up","GrimeStrength","GrimeHeight","Snow","SnowAmount"])
    for pin,source in (("R",mult),("Height",height),("Up",up),("GrimeStrength",params["GrimeStrength"]),("GrimeHeight",params["GrimeHeight"]),("Snow",snow),("SnowAmount",params["SnowAmount"])):wire(source,roughness,pin)
    output(roughness,unreal.MaterialProperty.MP_ROUGHNESS)
    glow=custom("""
float2 k = Seed.xy * 100.0 + float2(3.1, 7.7);
float lit = step(0.3, frac(sin(dot(k, float2(27.619, 57.583))) * 15731.743));
float3 lamp = Tint * Emission * lerp(LampDayScale, 1.0, saturate(Night));
float3 windows = float3(1.0, 0.72, 0.42) * WindowGlow * saturate(Night) * lit;
return lamp + windows;
""",unreal.CustomMaterialOutputType.CMOT_FLOAT3,["Tint","Emission","LampDayScale","Night","WindowGlow","Seed"])
    for pin,source in (("Tint",tint),("Emission",emission),("LampDayScale",params["LampDayScale"]),("Night",night),("WindowGlow",params["WindowGlow"]),("Seed",seed)):wire(source,glow,pin)
    output(glow,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    ME.layout_material_expressions(master);ME.recompile_material(master);ED.save_loaded_asset(master,only_if_is_dirty=False)
    instances={}
    for slot,p in DATA["palette"].items():
        mi=asset("MI_Industry_"+slot.removeprefix("IM_"),unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        ME.set_material_instance_parent(mi,master)
        ME.set_material_instance_vector_parameter_value(mi,"Tint",unreal.LinearColor(*p["color"]))
        ME.set_material_instance_scalar_parameter_value(mi,"Metallic",p["metallic"])
        ME.set_material_instance_scalar_parameter_value(mi,"RoughnessScale",p["roughness"]/.55)
        ME.set_material_instance_scalar_parameter_value(mi,"Emission",p["emission"])
        if "normal_strength" in p:ME.set_material_instance_scalar_parameter_value(mi,"NormalStrength",p["normal_strength"]) # v0.9.2 texture pass: authored seams and bolts
        # Art pass 4 instance choices: glass panes light up at night, lamps dim by
        # day, emissive slots keep their exact colour (no variation, grime or snow).
        for name,value in (p.get("material_pass") or {}).items():ME.set_material_instance_scalar_parameter_value(mi,name,value)
        for channel,t in textures[p["surface"] or "Paint"].items():ME.set_material_instance_texture_parameter_value(mi,channel,t)
        ME.update_material_instance(mi);ED.save_loaded_asset(mi,only_if_is_dirty=False);instances[slot]=mi

tasks=[]
for name,record in mesh_records.items():
    t=unreal.AssetImportTask();t.filename=str(OUT/record["fbx"]);t.destination_path=MESH_DEST;t.destination_name=name
    t.automated=True;t.replace_existing=True;t.replace_existing_settings=True;t.save=True
    opts=unreal.FbxImportUI();opts.import_mesh=True;opts.import_as_skeletal=False;opts.import_materials=False;opts.import_textures=False
    opts.automated_import_should_detect_type=False;opts.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    sm=opts.static_mesh_import_data;sm.combine_meshes=True;sm.auto_generate_collision=False;sm.generate_lightmap_u_vs=True
    sm.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS;sm.convert_scene=True;sm.convert_scene_unit=True
    t.options=opts;tasks.append(t)
MATERIALS_ONLY="-IndustryMaterialsOnly" in unreal.SystemLibrary.get_command_line()
if not MATERIALS_ONLY:TOOLS.import_asset_tasks(tasks)
editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
report=json.loads((ART/"industry_import_report.json").read_text(encoding="utf-8")) if CORE_ONLY else {"source":"Original Blender-authored industrial architecture and procedural PBR surfaces","meshes":{},"material_master":DEST+"/M_IndustrySurface","textures":9}
# A materials-only refresh (v0.9.2 texture pass) only rebuilds the master and the
# MI_Industry_* instances every building mesh already references; it neither
# rebuilds LODs nor rewrites the mesh import report.
if MATERIALS_ONLY:
    unreal.log("SEIGE_INDUSTRY_IMPORT_SUCCESS "+json.dumps({"materials_only":True,"instances":sorted(instances)}))
    mesh_records={}
for name,record in mesh_records.items():
    mesh=ED.load_asset(MESH_DEST+"/"+name)
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError("Missing imported building "+name)
    slots=mesh.get_editor_property("static_materials")
    imported_names=[str(s.get_editor_property("imported_material_slot_name")) for s in slots]
    if not set(record["materials"]).issubset(imported_names):raise RuntimeError("Missing new material slots "+name+": "+str(imported_names))
    for i,slot in enumerate(slots):
        key=str(slot.get_editor_property("imported_material_slot_name"))
        # Reimport preserves previous unused slots. Assign by authored name, never by
        # array position, then validate every active LOD0 section uses the new set.
        if key not in instances:continue
        mesh.set_material(i,instances[key])
    active_slots=[editor.get_lod_material_slot(mesh,0,i) for i in range(mesh.get_num_sections(0))]
    if any(index<0 or index>=len(slots) or imported_names[index] not in instances for index in active_slots):raise RuntimeError("Old/unknown material on active geometry "+name+": "+str([(index,imported_names[index]) for index in active_slots]))
    reduction=unreal.StaticMeshReductionOptions();reduction.set_editor_property("auto_compute_lod_screen_size",False)
    levels=[]
    for triangles,screen in ((1,1),(.48,.26),(.16,.08)):
        setting=unreal.StaticMeshReductionSettings();setting.set_editor_property("percent_triangles",triangles);setting.set_editor_property("screen_size",screen);levels.append(setting)
    reduction.set_editor_property("reduction_settings",levels)
    if editor.set_lods(mesh,reduction)!=3:raise RuntimeError("Failed building LODs for "+name)
    ED.save_loaded_asset(mesh,only_if_is_dirty=False)
    bounds=mesh.get_bounding_box();lo,hi=bounds.min,bounds.max;dims=[hi.x-lo.x,hi.y-lo.y,hi.z-lo.z]
    if abs(lo.z)>.5 or any(abs(a-b)>max(.5,b*.01) for a,b in zip(dims,record["dimensions"])):raise RuntimeError("Physical scale/ground pivot mismatch "+name+": "+str(dims))
    report["meshes"][name]={"dimensions_cm":dims,"ground_z":lo.z,"source_triangles":record["triangles"],"lod_count":editor.get_lod_count(mesh),"material_slots":len(slots),"active_material_slots":[imported_names[index] for index in active_slots]}
if not MATERIALS_ONLY:
    (ART/"industry_import_report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    unreal.log("SEIGE_INDUSTRY_IMPORT_SUCCESS "+json.dumps(report))

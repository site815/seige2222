"""Import the original Rex mesh, skeletal loops and fur materials into Nature-independent assets.

Run only in the parent-coordinated Unreal Editor commandlet. No reference photos
are imported. Existing unrelated Content assets and player settings are untouched.
"""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/CompanionDog';DEST='/Game/Art/CompanionDog'
DATA=json.loads((ART/'dog_manifest.json').read_text())
ED=u.EditorAssetLibrary;AT=u.AssetToolsHelpers.get_asset_tools();ME=u.MaterialEditingLibrary
SME=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem) or u.get_default_object(u.SkeletalMeshEditorSubsystem)
ED.make_directory(DEST);ED.make_directory(DEST+'/Textures')
u.SystemLibrary.execute_console_command(None,'Interchange.FeatureFlags.Import.FBX 0')

def node(m,kind,**props):
    result=ME.create_material_expression(m,kind)
    for key,value in props.items():result.set_editor_property(key,value)
    return result
def link(a,b,pin,output=''):
    if not ME.connect_material_expressions(a,output,b,pin):raise RuntimeError('Dog material link failed: '+pin)
def output(n,prop,pin=''):
    if not ME.connect_material_property(n,pin,prop):raise RuntimeError('Dog material output failed: '+str(prop))

textures={}
for name,normal in [('T_Dog_FurNormal',True),('T_Dog_FurDetail',False),('T_Dog_FurAlpha',False),('T_Dog_CoatDetail',False)]:
    task=u.AssetImportTask();task.filename=str(ART/'Textures'/(name+'.png'));task.destination_path=DEST+'/Textures';task.destination_name=name
    task.automated=True;task.replace_existing=True;task.save=True;AT.import_asset_tasks([task])
    tex=ED.load_asset(DEST+'/Textures/'+name)
    if not isinstance(tex,u.Texture2D):raise RuntimeError('Missing original dog texture '+name)
    tex.set_editor_property('srgb',False)
    tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP if normal else u.TextureCompressionSettings.TC_MASKS)
    ED.save_loaded_asset(tex,False);textures[name]=tex

materials={}
for slot,settings in DATA['materials'].items():
    name='M_'+slot.removeprefix('DM_');path=DEST+'/'+name
    m=ED.load_asset(path) if ED.does_asset_exist(path) else AT.create_asset(name,DEST,u.Material,u.MaterialFactoryNew())
    ME.delete_all_material_expressions(m);m.set_editor_property('used_with_skeletal_mesh',True)
    if slot in ('DM_Coat','DM_FurCard'):
        vc=node(m,u.MaterialExpressionVertexColor)
        # FBX is exported with sRGB vertex colors. Legacy skeletal import stores
        # normalized bytes verbatim; decode those values before material lighting.
        decode=node(m,u.MaterialExpressionCustom,
            code='return lerp(Color/12.92,pow((Color+.055)/1.055,2.4),step(.04045,Color));',
            output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,description='Decode authored FBX sRGB coat colors')
        item=u.CustomInput();item.set_editor_property('input_name','Color');decode.set_editor_property('inputs',[item]);link(vc,decode,'Color')
        detail=node(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='FurDetail',texture=textures['T_Dog_CoatDetail'],sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        strength=node(m,u.MaterialExpressionLinearInterpolate,const_a=1,const_alpha=.85);link(detail,strength,'B')
        base=node(m,u.MaterialExpressionMultiply);link(decode,base,'A');link(strength,base,'B');output(base,u.MaterialProperty.MP_BASE_COLOR)
        texture=node(m,u.MaterialExpressionTextureObjectParameter,parameter_name='CoatRelief',texture=textures['T_Dog_CoatDetail'],sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
        uv=node(m,u.MaterialExpressionTextureCoordinate)
        normal=node(m,u.MaterialExpressionCustom,code='uint W,H; CoatTexture.GetDimensions(W,H); float2 S=1.0/float2(W,H); float X=Texture2DSample(CoatTexture,CoatTextureSampler,UV+float2(S.x,0)).r-Texture2DSample(CoatTexture,CoatTextureSampler,UV-float2(S.x,0)).r; float Y=Texture2DSample(CoatTexture,CoatTextureSampler,UV+float2(0,S.y)).r-Texture2DSample(CoatTexture,CoatTextureSampler,UV-float2(0,S.y)).r; return normalize(float3(-X*3,-Y*3,1));',output_type=u.CustomMaterialOutputType.CMOT_FLOAT3,description='Fine tangent-space coat relief')
        inputs=[]
        for name in ['CoatTexture','UV']:
            item=u.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
        normal.set_editor_property('inputs',inputs);link(texture,normal,'CoatTexture');link(uv,normal,'UV');output(normal,u.MaterialProperty.MP_NORMAL)
        if slot=='DM_FurCard':
            m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED);m.set_editor_property('opacity_mask_clip_value',.33);m.set_editor_property('two_sided',True)
            alpha=node(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='FurAlpha',texture=textures['T_Dog_FurAlpha'],sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS)
            output(alpha,u.MaterialProperty.MP_OPACITY_MASK,'R')
    else:
        color=node(m,u.MaterialExpressionVectorParameter,parameter_name='Color',default_value=u.LinearColor(*settings['color']))
        output(color,u.MaterialProperty.MP_BASE_COLOR)
    for parameter,value,prop in [('Roughness',settings['roughness'],u.MaterialProperty.MP_ROUGHNESS),('Metallic',settings['metallic'],u.MaterialProperty.MP_METALLIC),('Specular',.3 if slot in ('DM_Eye','DM_Nose') else .18,u.MaterialProperty.MP_SPECULAR)]:
        n=node(m,u.MaterialExpressionScalarParameter,parameter_name=parameter,default_value=value);output(n,prop)
    ME.layout_material_expressions(m)
    if ME.recompile_material(m):raise RuntimeError('Dog material compile failed '+name)
    ED.save_loaded_asset(m,False);materials[slot]=m

def task_for(filename,name):
    task=u.AssetImportTask();task.filename=str(ART/'Exports'/filename);task.destination_path=DEST;task.destination_name=name
    task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
    return task

options=u.FbxImportUI();options.automated_import_should_detect_type=False
options.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
options.import_mesh=True;options.import_as_skeletal=True;options.import_materials=False;options.import_textures=False;options.import_animations=False
options.create_physics_asset=False
data=options.skeletal_mesh_import_data
# These import-data UPROPERTYs are editor-only in UE 5.8. In particular,
# bImportMorphTargets/bUseT0AsRefPose do not have BlueprintReadWrite and thus
# have no direct Python attribute; the reflected editor-property API is needed.
for key,value in {
    'convert_scene':True,'convert_scene_unit':True,
    'normal_import_method':u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS,
    'vertex_color_import_option':u.VertexColorImportOption.REPLACE,
    'import_morph_targets':False,'use_t0_as_ref_pose':False,
}.items():data.set_editor_property(key,value)
mesh_path=DEST+'/SK_CompanionDog'
if ED.does_asset_exist(mesh_path):
    previous=ED.load_asset(mesh_path);stored=previous.get_editor_property('asset_import_data')
    if isinstance(stored,u.FbxSkeletalMeshImportData):stored.set_editor_property('vertex_color_import_option',u.VertexColorImportOption.REPLACE)
task=task_for('SK_CompanionDog.fbx','SK_CompanionDog');task.options=options;AT.import_asset_tasks([task])
mesh=ED.load_asset(mesh_path)
if not isinstance(mesh,u.SkeletalMesh):raise RuntimeError('Skeletal dog mesh was not imported')
if not mesh.has_vertex_colors():raise RuntimeError('Dog coat vertex colors missing')
skeleton=mesh.get_editor_property('skeleton')
if not isinstance(skeleton,u.Skeleton):raise RuntimeError('Dog skeleton missing')
slots=list(mesh.get_editor_property('materials'));seen=[]
for slot in slots:
    name=str(slot.get_editor_property('imported_material_slot_name'))
    if name not in materials:raise RuntimeError('Unknown dog material slot '+name)
    slot.set_editor_property('material_interface',materials[name]);seen.append(name)
required=set(DATA.get('used_materials',materials))
if required-set(seen):raise RuntimeError('Incomplete dog material assignments: '+str(sorted(required-set(seen))))
mesh.set_editor_property('materials',slots)
if not SME.regenerate_lod(mesh,3,False,False):raise RuntimeError('Dog skeletal LOD generation failed')
for lod in range(SME.get_lod_count(mesh)):
    for section in range(SME.get_num_sections(mesh,lod)):
        slot=SME.get_lod_material_slot(mesh,lod,section)
        if 0<=slot<len(seen) and seen[slot]=='DM_FurCard':SME.set_section_cast_shadow(mesh,lod,section,False)
bounds=mesh.get_imported_bounds();extent=bounds.box_extent;dimensions=[extent.x*2,extent.y*2,extent.z*2]
expected=[b-a for a,b in zip(DATA['reference_pose_bounds_cm']['min'],DATA['reference_pose_bounds_cm']['max'])]
if any(abs(a-b)>2 for a,b in zip(dimensions,expected)):raise RuntimeError('Dog unit/axis bounds mismatch: '+str(dimensions)+' expected '+str(expected))
found=set();pending=['root']
while pending:
    name=pending.pop()
    if name in found:continue
    found.add(name);pending.extend(str(x) for x in SME.get_bone_children(mesh,name))
if set(DATA['bones'])-found:raise RuntimeError('Dog rig bones missing: '+str(sorted(set(DATA['bones'])-found)))
ED.save_loaded_asset(mesh,False);ED.save_loaded_asset(skeleton,False)

animations={}
for name,definition in DATA['animations'].items():
    options=u.FbxImportUI();options.automated_import_should_detect_type=False
    options.mesh_type_to_import=u.FBXImportType.FBXIT_ANIMATION;options.import_mesh=False;options.import_as_skeletal=True
    options.import_animations=True;options.import_materials=False;options.import_textures=False;options.skeleton=skeleton
    options.override_animation_name=name
    data=options.anim_sequence_import_data
    # Animation import settings likewise expose editor properties, not Python
    # descriptors (FbxAnimSequenceImportData.h in the installed 5.8 engine).
    for key,value in {
        'convert_scene':True,'convert_scene_unit':True,
        'animation_length':u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
        'import_bone_tracks':True,'use_default_sample_rate':False,
        'custom_sample_rate':30,'remove_redundant_keys':False,
    }.items():data.set_editor_property(key,value)
    task=task_for(name+'.fbx',name);task.options=options;AT.import_asset_tasks([task])
    clip=ED.load_asset(DEST+'/'+name)
    if not isinstance(clip,u.AnimSequence):raise RuntimeError('Dog animation was not imported: '+name)
    if clip.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Dog clip skeleton mismatch')
    length=clip.get_play_length()
    if abs(length-definition['duration_seconds'])>.04:raise RuntimeError('Dog animation duration mismatch: '+name+' '+str(length))
    clip.set_editor_property('enable_root_motion',False);ED.save_loaded_asset(clip,False)
    animations[name]={'path':clip.get_path_name(),'seconds':length,'root_motion':False}

report={'mesh':mesh.get_path_name(),'skeleton':skeleton.get_path_name(),'animations':animations,
    'lod_count':SME.get_lod_count(mesh),'lod_vertices':[SME.get_num_verts(mesh,i) for i in range(SME.get_lod_count(mesh))],
    'material_slots':seen,'vertex_colors':mesh.has_vertex_colors(),'vertex_color_encoding':'FBX sRGB decoded by material',
    'source_triangles':DATA['triangles'],'source_bones':DATA['bones'],'source_reference_bounds_cm':DATA['reference_pose_bounds_cm'],
    'imported_dimensions_cm':dimensions,'verified_bones':sorted(found),'fur_card_shadow_casting':False,
    'runtime_review_pending':True,'reference_photographs_imported':False}
(ART/'import_report.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('SEIGE_COMPANION_DOG_IMPORTED '+json.dumps(report))

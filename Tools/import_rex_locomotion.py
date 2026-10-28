"""Import only Rex's locomotion clips and an idle/walk/run blend space.

Run with UnrealEditor-Cmd -run=pythonscript. This deliberately does not reimport
the skeletal mesh, photographic materials, textures, or reference skeleton.
"""
from pathlib import Path
import hashlib
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
DEST = '/Game/Art/CompanionDog'
ED = u.EditorAssetLibrary
AT = u.AssetToolsHelpers.get_asset_tools()
manifest = json.loads((ART / 'dog_manifest.json').read_text())
runtime = ROOT / 'Content/Art/CompanionDog'
retained = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in runtime.rglob('*.uasset')
            if not path.name.startswith(('A_Dog', 'BS_Dog'))}
skeleton = ED.load_asset(DEST+'/SK_CompanionDog_Skeleton')
if not isinstance(skeleton, u.Skeleton):
    raise RuntimeError('Rex reference skeleton is missing')
u.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
clips = {}
for name in ('A_DogWalk', 'A_DogRun'):
    definition = manifest['animations'][name]
    options = u.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.skeleton = skeleton
    options.override_animation_name = name
    data = options.anim_sequence_import_data
    for key, value in dict(convert_scene=True, convert_scene_unit=True,
            animation_length=u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME,
            import_bone_tracks=True, use_default_sample_rate=False,
            custom_sample_rate=definition['fps'], remove_redundant_keys=False).items():
        data.set_editor_property(key, value)
    task = u.AssetImportTask()
    task.filename = str(ART/'Exports'/(name+'.fbx'))
    task.destination_path = DEST
    task.destination_name = name
    task.options = options
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    AT.import_asset_tasks([task])
    clip = ED.load_asset(DEST+'/'+name)
    if not isinstance(clip, u.AnimSequence) or clip.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('Missing or incompatible Rex animation '+name)
    if abs(clip.get_play_length()-definition['duration_seconds']) > .005:
        raise RuntimeError('Unexpected Rex animation duration '+name)
    clip.set_editor_property('enable_root_motion', False)
    ED.save_loaded_asset(clip, False)
    clips[name] = dict(path=clip.get_path_name(), duration_seconds=clip.get_play_length(),
                       stride_meters=definition['stride_meters'])

path = DEST+'/BS_DogLocomotion'
blend = ED.load_asset(path) if ED.does_asset_exist(path) else None
if blend is None:
    factory = u.BlendSpaceFactoryNew()
    factory.set_editor_property('target_skeleton', skeleton)
    factory.set_editor_property('preview_skeletal_mesh', ED.load_asset(DEST+'/SK_CompanionDog'))
    blend = AT.create_asset('BS_DogLocomotion', DEST, u.BlendSpace1D, factory)
if not isinstance(blend, u.BlendSpace1D):
    raise RuntimeError('Could not create Rex locomotion blend space')
parameters = list(blend.get_editor_property('blend_parameters'))
parameters[0].set_editor_property('display_name', 'Speed (m/s)')
parameters[0].set_editor_property('min', 0.0)
# Blend axes are float32 but sample locations use FVector doubles. Keep a
# little headroom rather than put the run sample exactly on a rounded bound.
parameters[0].set_editor_property('max', 4.0)
parameters[0].set_editor_property('grid_num', 14)
parameters[0].set_editor_property('snap_to_grid', False)
blend.set_editor_property('blend_parameters', parameters)
blend.set_editor_property('interpolate_using_grid', False)
# Runtime acceleration already supplies the smoothly changing physical speed;
# a second blend-space time filter would disconnect paws from world motion.
blend.set_editor_property('target_weight_interpolation_speed_per_sec', 0.0)
samples = []
for name, speed in (('A_DogIdle', 0), ('A_DogWalk', 5/3.6), ('A_DogRun', 14/3.6)):
    sample = u.BlendSample()
    sample.set_editor_property('animation', ED.load_asset(DEST+'/'+name))
    sample.set_editor_property('sample_value', u.Vector(speed, 0, 0))
    sample.set_editor_property('rate_scale', 1.0)
    samples.append(sample)
blend.set_editor_property('sample_data', samples)
# Python property assignment does not invoke UBlendSpace::ResampleData, and
# the runtime cache types are not Python-exposed. This Editor-only command
# invokes the native rebuild and evaluates six actual runtime blend inputs.
editor_world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(editor_world, 'Seige.RebuildRexBlendSpace')
ED.save_loaded_asset(blend, False)
for path, digest in retained.items():
    if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest:
        raise RuntimeError('Animation-only import modified retained asset '+path)
report = dict(clips=clips, blend_space=blend.get_path_name(),
              blend_speed_samples_m_s=[0, 5/3.6, 14/3.6],
              blend_axis_max_m_s=4.0, native_cache_rebuild_command='Seige.RebuildRexBlendSpace',
              retained_model_materials_skeleton_unchanged=True)
(ART/'locomotion_import_report.json').write_text(json.dumps(report, indent=2)+'\n')
manifest['locomotion_revision']['import_pending'] = False
manifest['locomotion_revision']['unreal_import_report'] = 'locomotion_import_report.json'
(ART/'dog_manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
u.log('SEIGE_REX_LOCOMOTION_IMPORTED '+json.dumps(report))

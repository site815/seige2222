"""Import original PCM effects; run as an Unreal Python commandlet."""
from pathlib import Path
import hashlib, json
import unreal as u
ROOT=Path(__file__).resolve().parents[1];records=json.loads((ROOT/'Art/Audio/manifest.json').read_text())
tools=u.AssetToolsHelpers.get_asset_tools();editor=u.EditorAssetLibrary
editor.make_directory('/Game/Art/Audio')
imported={};evidence={}
for name,record in records.items():
    source=ROOT/'Art/Audio'/record['file']
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    if digest!=record['sha256']:raise RuntimeError('Source checksum mismatch: '+name)
    task=u.AssetImportTask();task.filename=str(source);task.destination_path='/Game/Art/Audio'
    task.destination_name='S_'+name;task.automated=True;task.replace_existing=True;task.replace_existing_settings=True;task.save=True
    tools.import_asset_tasks([task]);sound=editor.load_asset('/Game/Art/Audio/S_'+name)
    if not isinstance(sound,u.SoundWave):raise RuntimeError('Sound effect import failed: '+name)
    sound.set_editor_property('looping',False)
    sound.set_editor_property('volume',1.0)
    sound.set_editor_property('pitch',1.0)
    editor.save_loaded_asset(sound,False);imported[name]=sound.get_path_name()
    evidence[name]={'asset':sound.get_path_name(),'source_sha256':digest,'generator_revision':record.get('generator_revision','legacy')}
(ROOT/'Art/Audio/import_report.json').write_text(json.dumps(imported,indent=2)+'\n')
(ROOT/'Art/Audio/import_source_hashes.json').write_text(json.dumps(evidence,indent=2)+'\n')
u.log('ORIGINAL_AUDIO_IMPORTED '+str(len(imported)))

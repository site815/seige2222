"""Import original PCM effects; run as an Unreal Python commandlet."""
from pathlib import Path
import json
import unreal as u
ROOT=Path(__file__).resolve().parents[1];records=json.loads((ROOT/'Art/Audio/manifest.json').read_text())
tools=u.AssetToolsHelpers.get_asset_tools();editor=u.EditorAssetLibrary
editor.make_directory('/Game/Art/Audio')
imported={}
for name,record in records.items():
    task=u.AssetImportTask();task.filename=str(ROOT/'Art/Audio'/record['file']);task.destination_path='/Game/Art/Audio'
    task.destination_name='S_'+name;task.automated=True;task.replace_existing=True;task.save=True
    tools.import_asset_tasks([task]);sound=editor.load_asset('/Game/Art/Audio/S_'+name)
    if not isinstance(sound,u.SoundWave):raise RuntimeError('Sound effect import failed: '+name)
    sound.set_editor_property('looping',False)
    editor.save_loaded_asset(sound,False);imported[name]=sound.get_path_name()
(ROOT/'Art/Audio/import_report.json').write_text(json.dumps(imported,indent=2)+'\n')
u.log('ORIGINAL_AUDIO_IMPORTED '+str(len(imported)))

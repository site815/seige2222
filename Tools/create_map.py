import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level('/Game/Maps/Colony', False)
assert levels.save_current_level()
unreal.log('SEIGE_MAP_CREATED')

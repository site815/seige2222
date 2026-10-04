"""Finalize HISM shader usage for cooking; only edits the five art master materials."""
import unreal

for name in ("M_Terrain", "M_Industrial", "M_Foliage", "M_BarkPBR", "M_RockPBR"):
    material = unreal.EditorAssetLibrary.load_asset("/Game/Art/" + name)
    if not material:
        raise RuntimeError("Missing material " + name)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    if not material.get_editor_property("used_with_instanced_static_meshes"):
        raise RuntimeError("HISM material usage was not saved: " + name)
unreal.log("SEIGE_HISM_MATERIAL_USAGE_READY")

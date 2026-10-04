"""Import the original Blender assets into Unreal and restore their authored palette.

Run through UnrealEditor-Cmd with -run=pythonscript -script=Tools/import_assets.py.
Requires PythonScriptPlugin and EditorScriptingUtilities. Does not create a level.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
EXPORT = ROOT / "Art" / "Exports"
MANIFEST = json.loads((EXPORT / "asset_manifest.json").read_text(encoding="utf-8"))
DEST = "/Game/Art"
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDITOR = unreal.EditorAssetLibrary
EDIT_MATERIAL = unreal.MaterialEditingLibrary


def load_or_create(name, asset_class, factory):
    asset_path = DEST + "/" + name
    asset = EDITOR.load_asset(asset_path) if EDITOR.does_asset_exist(asset_path) else None
    if asset:
        if not isinstance(asset, asset_class):
            raise RuntimeError("Unexpected asset type at " + DEST + "/" + name)
        return asset
    asset = TOOLS.create_asset(name, DEST, asset_class, factory)
    if not asset:
        raise RuntimeError("Failed creating " + name)
    return asset


def material_parameter(material, expression_class, name, value, x, y):
    expr = EDIT_MATERIAL.create_material_expression(material, expression_class, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", value)
    return expr


EDITOR.make_directory(DEST)
master = load_or_create("M_Colony", unreal.Material, unreal.MaterialFactoryNew())
EDIT_MATERIAL.delete_all_material_expressions(master)
tint = material_parameter(master, unreal.MaterialExpressionVectorParameter, "Tint", unreal.LinearColor(.79, .88, .87, 1), -500, 0)
roughness = material_parameter(master, unreal.MaterialExpressionScalarParameter, "Roughness", .65, -500, 180)
metallic = material_parameter(master, unreal.MaterialExpressionScalarParameter, "Metallic", .16, -500, 280)
emission = material_parameter(master, unreal.MaterialExpressionScalarParameter, "Emission", 0.0, -500, 390)
multiply = EDIT_MATERIAL.create_material_expression(master, unreal.MaterialExpressionMultiply, -230, 340)
EDIT_MATERIAL.connect_material_expressions(tint, "", multiply, "A")
EDIT_MATERIAL.connect_material_expressions(emission, "", multiply, "B")
EDIT_MATERIAL.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
EDIT_MATERIAL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
EDIT_MATERIAL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
EDIT_MATERIAL.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
EDIT_MATERIAL.recompile_material(master)
EDITOR.save_loaded_asset(master, only_if_is_dirty=False)

palette_assets = {}
for slot_name, settings in MANIFEST["palette"].items():
    name = "MI_" + slot_name.removeprefix("MAT_")
    instance = load_or_create(name, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    EDIT_MATERIAL.set_material_instance_parent(instance, master)
    EDIT_MATERIAL.set_material_instance_vector_parameter_value(instance, "Tint", unreal.LinearColor(*settings["color"]))
    EDIT_MATERIAL.set_material_instance_scalar_parameter_value(instance, "Roughness", .65)
    EDIT_MATERIAL.set_material_instance_scalar_parameter_value(instance, "Metallic", .16)
    EDIT_MATERIAL.set_material_instance_scalar_parameter_value(instance, "Emission", settings["emission"])
    EDIT_MATERIAL.update_material_instance(instance)
    EDITOR.save_loaded_asset(instance, only_if_is_dirty=False)
    palette_assets[slot_name] = instance

tasks = []
for name, record in MANIFEST["meshes"].items():
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(EXPORT / record["fbx"]))
    task.set_editor_property("destination_path", DEST)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", True)
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    static_options = options.get_editor_property("static_mesh_import_data")
    static_options.set_editor_property("combine_meshes", True)
    static_options.set_editor_property("auto_generate_collision", True)
    static_options.set_editor_property("generate_lightmap_u_vs", True)
    static_options.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    static_options.set_editor_property("convert_scene", True)
    static_options.set_editor_property("convert_scene_unit", True)
    static_options.set_editor_property("import_uniform_scale", 1.0)
    task.set_editor_property("options", options)
    tasks.append(task)

TOOLS.import_asset_tasks(tasks)
report = {"master_material": DEST + "/M_Colony", "meshes": {}}
for name, record in MANIFEST["meshes"].items():
    mesh = EDITOR.load_asset(DEST + "/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Static mesh was not imported: " + name)
    slots = mesh.get_editor_property("static_materials")
    assigned = []
    for index, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("imported_material_slot_name"))
        if slot_name not in palette_assets:
            slot_name = str(slot.get_editor_property("material_slot_name"))
        if slot_name not in palette_assets:
            if index >= len(record["materials"]):
                raise RuntimeError("Unrecognized material slot on " + name + ": " + slot_name)
            slot_name = record["materials"][index]
        mesh.set_material(index, palette_assets[slot_name])
        assigned.append(slot_name)
    if len(slots) != len(record["materials"]):
        raise RuntimeError("Material slot count changed for " + name)
    EDITOR.save_loaded_asset(mesh, only_if_is_dirty=False)
    bounds = mesh.get_bounding_box()
    lower, upper = bounds.min, bounds.max
    dimensions = [upper.x-lower.x, upper.y-lower.y, upper.z-lower.z]
    expected = record["dimensions"]
    # Export/import must not silently rescale centimeters to meters or lose the ground pivot.
    if any(abs(a-b) > max(.1, b * .01) for a, b in zip(sorted(dimensions), sorted(expected))):
        raise RuntimeError("Unexpected mesh dimensions for " + name + ": " + str(dimensions))
    if abs(lower.z) > .1:
        raise RuntimeError("Ground pivot offset on " + name + ": " + str(lower.z))
    report["meshes"][name] = {"path": mesh.get_path_name(), "dimensions_cm": dimensions, "ground_z": lower.z, "material_slots": assigned}

(ROOT / "Art" / "import_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("SEIGE_ART_IMPORT_SUCCESS " + json.dumps(report))

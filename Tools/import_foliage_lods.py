"""Replace destructive automatic tree reduction with authored canopy LODs."""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "Art"
OUT = ART / "RealisticExports"
DATA = json.loads((OUT / "foliage_lods.json").read_text(encoding="utf-8"))
ED = unreal.EditorAssetLibrary
mesh_editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncStaticMeshCompilation 0")
unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncStaticMeshCompilationFinishAll")
report = {}
for name, lods in DATA.items():
    mesh = ED.load_asset("/Game/Art/" + name)
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("Missing source mesh " + name)
    original_materials = list(mesh.get_editor_property("static_materials"))
    if not mesh_editor.remove_lods(mesh):
        raise RuntimeError("Could not clear automatic tree LODs " + name)
    for lod in lods:
        index = lod["lod"]
        if mesh_editor.import_lod(mesh, index, str(OUT / lod["fbx"])) != index:
            raise RuntimeError("Custom LOD import failed " + name + ": " + str(index))
    # Import reuses the original Bark and Leaf/Pine material slots.
    slots = mesh.get_editor_property("static_materials")
    if len(slots) != len(original_materials):
        raise RuntimeError("Custom LOD changed material slot count " + name)
    for index, slot in enumerate(original_materials):
        mesh.set_material(index, slot.material_interface)
    if mesh_editor.get_lod_count(mesh) != 4:
        raise RuntimeError("Expected four complete LODs " + name)
    vertices = [mesh_editor.get_number_verts(mesh, i) for i in range(4)]
    if any(v <= 0 for v in vertices):
        raise RuntimeError("An imported LOD is empty " + name)
    # Import/material changes can leave asynchronous render-data builds queued.
    # Resolve those before setting the final source-model and render thresholds.
    expected_sizes = [1.0, .45, .18, .06]
    if not mesh_editor.set_lod_screen_sizes(mesh, expected_sizes):
        raise RuntimeError("Could not apply tree LOD thresholds " + name)
    build_settings = mesh_editor.get_lod_build_settings(mesh, 0)
    mesh_editor.set_lod_build_settings(mesh, 0, build_settings)
    unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncStaticMeshCompilationFinishAll")
    vertices = [mesh_editor.get_number_verts(mesh, i) for i in range(4)]
    ED.save_loaded_asset(mesh, only_if_is_dirty=False)
    screens = list(mesh_editor.get_lod_screen_sizes(mesh))
    report[name] = {"lod_vertices": vertices, "lod_screen_sizes": screens,
        "requested_lod_screen_sizes": expected_sizes, "requires_fresh_load_verification": True,
        "custom_lods": lods, "material_slots": len(slots)}
(ART / "foliage_lod_import_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
general_path = ART / "realistic_import_report.json"
if general_path.exists():
    general = json.loads(general_path.read_text(encoding="utf-8"))
    for name, record in report.items():
        general["meshes"][name].update(record)
    general_path.write_text(json.dumps(general, indent=2), encoding="utf-8")
unreal.log("SEIGE_CANOPY_LOD_IMPORT_SUCCESS " + json.dumps(report))

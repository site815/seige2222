"""Verify saved custom tree assets in a fresh Unreal editor commandlet.

Interchange's in-process render data can report zero screen sizes immediately
after import even when source-model thresholds persist correctly on disk.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "Art"
source = json.loads((ART / "RealisticExports/foliage_lods.json").read_text(encoding="utf-8"))
e = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.get_default_object(unreal.StaticMeshEditorSubsystem)
report = {}
for name, lods in source.items():
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/Art/" + name)
    if not mesh or e.get_lod_count(mesh) != 4:
        raise RuntimeError("Tree does not have four saved LODs: " + name)
    screens = list(e.get_lod_screen_sizes(mesh))
    if any(abs(a-b) > .001 for a,b in zip(screens, [1,.45,.18,.06])):
        raise RuntimeError("Saved tree thresholds are incorrect: " + name + str(screens))
    vertices = [e.get_number_verts(mesh, i) for i in range(4)]
    if any(n <= 0 for n in vertices):
        raise RuntimeError("Empty saved tree LOD: " + name)
    materials = list(mesh.get_editor_property("static_materials"))
    if len(materials) != 2 or any(not slot.material_interface for slot in materials):
        raise RuntimeError("Missing tree materials: " + name)
    report[name] = {"lod_vertices": vertices, "lod_screen_sizes": screens, "custom_lods": lods,
        "material_slots": len(materials), "verified_from_fresh_load": True}
(ART / "foliage_lod_import_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
path = ART / "realistic_import_report.json"
general = json.loads(path.read_text(encoding="utf-8"))
for name, result in report.items():
    general["meshes"][name].update(result)
path.write_text(json.dumps(general, indent=2), encoding="utf-8")
unreal.log("SEIGE_CANOPY_LODS_VERIFIED " + json.dumps(report))

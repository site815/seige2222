"""Author canopy-preserving tree LODs from the original Blender geometry.

Generic quadric reduction removes disconnected leaves before woody branches.
These LODs simplify wood separately and retain distributed, enlarged leaf blades.
"""
from pathlib import Path
import bpy, json, math
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "Art"
OUT = ART / "RealisticExports"
bpy.ops.wm.open_mainfile(filepath=str(ART / "Source/Seige_Realistic_Assets.blend"))
bpy.context.preferences.filepaths.save_version = 0
names = ("SM_OakA", "SM_OakB", "SM_PineA", "SM_PineB")
report = {}
created = []

for name in names:
    source = bpy.data.objects[name]
    mesh = source.data
    colors = mesh.color_attributes.get("Col")
    if not colors or colors.domain != "POINT":
        raise RuntimeError("Missing per-point leaf colors: " + name)
    leaf_slots = {i for i, m in enumerate(mesh.materials) if m.name in ("RM_Leaf", "RM_Pine")}
    wood_polys = [p for p in mesh.polygons if p.material_index not in leaf_slots]
    leaf_polys = [p for p in mesh.polygons if p.material_index in leaf_slots]
    # Each authored curved leaf is one disconnected seven-vertex, six-face fan.
    fans = {}
    for p in leaf_polys:
        fans.setdefault(p.vertices[0], []).append(p)
    if any(len(faces) != 6 for faces in fans.values()):
        raise RuntimeError("Unexpected source leaf topology: " + name)
    report[name] = []

    for level, stride, leaf_scale, wood_ratio in ((1, 2, 1.70, .45), (2, 3, 2.10, .22), (3, 6, 2.85, .10)):
        old_ids = sorted({v for p in wood_polys for v in p.vertices})
        lookup = {old: new for new, old in enumerate(old_ids)}
        wood_mesh = bpy.data.meshes.new(name + "_WoodLOD" + str(level))
        wood_mesh.from_pydata([mesh.vertices[i].co for i in old_ids], [], [[lookup[i] for i in p.vertices] for p in wood_polys])
        wood_mesh.update()
        for m in mesh.materials:
            wood_mesh.materials.append(m)
        for p, original in zip(wood_mesh.polygons, wood_polys):
            p.material_index = original.material_index
            p.use_smooth = original.use_smooth
        wood = bpy.data.objects.new("LOD%02d_%s" % (level, name), wood_mesh)
        bpy.context.collection.objects.link(wood)
        bpy.ops.object.select_all(action="DESELECT")
        wood.select_set(True)
        bpy.context.view_layer.objects.active = wood
        dec = wood.modifiers.new("Reduce wood only", "DECIMATE")
        dec.ratio = wood_ratio
        bpy.ops.object.modifier_apply(modifier=dec.name)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(island_margin=.015)
        bpy.ops.object.mode_set(mode="OBJECT")

        vertices, faces, rgba, indices = [], [], [], []
        for leaf_index, (center_id, polygons) in enumerate(sorted(fans.items())):
            if leaf_index % stride:
                continue
            # Original fan center is vertex6; its boundary points are vertices0..5.
            first = center_id - 6
            ids = [first, first + 2, first + 3, first + 4]
            points = [mesh.vertices[i].co.copy() for i in ids]
            center = sum(points, Vector()) / 4
            points = [center + (p - center) * leaf_scale for p in points]
            offset = len(vertices)
            vertices.extend(points)
            faces.extend(((offset, offset + 1, offset + 2), (offset, offset + 2, offset + 3)))
            rgba.extend([tuple(colors.data[center_id].color)] * 4)
            indices.extend([polygons[0].material_index] * 2)
        leaf_mesh = bpy.data.meshes.new(name + "_CanopyLOD" + str(level))
        leaf_mesh.from_pydata(vertices, [], faces)
        leaf_mesh.update()
        for m in mesh.materials:
            leaf_mesh.materials.append(m)
        for p, slot in zip(leaf_mesh.polygons, indices):
            p.material_index = slot
        cl = leaf_mesh.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="POINT")
        for item, color in zip(cl.data, rgba):
            item.color = color
        leaf = bpy.data.objects.new(name + "_CanopyLOD" + str(level), leaf_mesh)
        bpy.context.collection.objects.link(leaf)
        leaf.select_set(True)
        bpy.context.view_layer.objects.active = wood
        bpy.ops.object.join()
        # Explicitly preserve the active color layer for FBX interoperability.
        wood.data.color_attributes.active_color = wood.data.color_attributes["Col"]
        wood.data.calc_loop_triangles()
        file = OUT / (name + "_LOD" + str(level) + ".fbx")
        bpy.ops.export_scene.fbx(filepath=str(file), use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
            axis_forward="-Y", axis_up="Z", bake_anim=False, add_leaf_bones=False, mesh_smooth_type="FACE",
            colors_type="SRGB", prioritize_active_color=True)
        report[name].append({"lod": level, "fbx": file.name, "triangles": len(wood.data.loop_triangles),
            "leaves": len(vertices) // 4, "leaf_scale": leaf_scale, "wood_fraction": wood_ratio})
        created.append((name, level, wood))

(OUT / "foliage_lods.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
for ob in bpy.context.scene.objects:
    if ob.type == "MESH":
        ob.hide_render = True
for name, level, ob in created:
    ob.location = ((names.index(name) - 1.5) * 1150, (level - 2) * 1300, 0)
    ob.hide_render = False
scene = bpy.context.scene
camera = scene.camera
camera.location = (3700, -5700, 4700)
camera.rotation_euler = (Vector((0, 0, 300)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.ortho_scale = 5500
scene.render.resolution_x = 1700
scene.render.resolution_y = 1300
scene.cycles.samples = 24
scene.render.filepath = str(ART / "Previews/foliage_lod_canopies.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ART / "Source/Seige_Foliage_LODs.blend"), compress=True)
bpy.ops.render.render(write_still=True)
print("SEIGE_CANOPY_LODS_CREATED " + json.dumps(report))

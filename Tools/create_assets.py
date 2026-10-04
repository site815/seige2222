"""Create original SEIGE prototype art with Blender, then export FBX and a palette manifest.

Run: blender --background --factory-startup --python Tools/create_assets.py
Geometry is authored in centimeters, with ground pivots and +X as the front.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
bpy.context.preferences.filepaths.save_version = 0
ART = ROOT / "Art"
EXPORT = ART / "Exports"
EXPORT.mkdir(parents=True, exist_ok=True)
(ART / "Source").mkdir(exist_ok=True)
(ART / "Previews").mkdir(exist_ok=True)

PALETTE = {
    "Pearl": ((0.79, 0.88, 0.87, 1), 0.0),
    "Ivory": ((0.94, 0.92, 0.78, 1), 0.0),
    "Teal": ((0.018, 0.42, 0.39, 1), 0.0),
    "DeepTeal": ((0.018, 0.063, 0.085, 1), 0.0),
    "Coral": ((0.95, 0.19, 0.09, 1), 0.0),
    "Gold": ((0.95, 0.61, 0.08, 1), 0.0),
    "Violet": ((0.35, 0.12, 0.63, 1), 0.0),
    "Lens": ((0.06, 0.88, 0.94, 1), 1.6),
    "Signal": ((1.0, 0.37, 0.13, 1), 1.2),
    "Steel": ((0.19, 0.27, 0.30, 1), 0.0),
    "Chitin": ((0.085, 0.013, 0.022, 1), 0.0),
    "RedChitin": ((0.31, 0.034, 0.035, 1), 0.0),
    "Bone": ((0.54, 0.43, 0.25, 1), 0.0),
    "BugEye": ((0.85, 0.12, 0.015, 1), 1.2),
}
MATERIALS = {}
for key, (color, emission) in PALETTE.items():
    mat = bpy.data.materials.new("MAT_" + key)
    mat.diffuse_color = color
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = color
    shader.inputs["Roughness"].default_value = 0.65
    shader.inputs["Metallic"].default_value = 0.16
    shader.inputs["Emission Color"].default_value = color
    shader.inputs["Emission Strength"].default_value = emission
    MATERIALS[key] = mat

bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.scene.unit_settings.scale_length = 0.01
PARTS = []
ASSETS = []
MANIFEST = {"units": "centimeters", "forward": "+X", "palette": {}, "meshes": {}}
for key, (color, emission) in PALETTE.items():
    MANIFEST["palette"]["MAT_" + key] = {"color": color, "emission": emission}


def finish_part(obj, name, material):
    obj.name = name
    obj.data.materials.append(MATERIALS[material])
    PARTS.append(obj)
    return obj


def cube(name, loc, dims, material="Pearl", bevel=3):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj = bpy.context.object
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        modifier = obj.modifiers.new("Soft manufactured edges", "BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        normals = obj.modifiers.new("Corner normals", "WEIGHTED_NORMAL")
        bpy.ops.object.modifier_apply(modifier=normals.name)
    return finish_part(obj, name, material)


def sphere(name, loc, dims, material="Pearl", segments=20, rings=12):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, radius=1, location=loc)
    obj = bpy.context.object
    obj.scale = Vector(dims) / 2
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return finish_part(obj, name, material)


def cylinder(name, loc, radius, depth, material="Pearl", vertices=24):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=loc)
    return finish_part(bpy.context.object, name, material)


def cone(name, loc, radius1, radius2, depth, material="Pearl", vertices=20):
    bpy.ops.mesh.primitive_cone_add(vertices=vertices, radius1=radius1, radius2=radius2, depth=depth, location=loc)
    return finish_part(bpy.context.object, name, material)


def rod(name, start, end, radius, material="Steel", end_radius=None):
    start, end = Vector(start), Vector(end)
    delta = end - start
    obj = cone(name, (start + end) / 2, radius, radius if end_radius is None else end_radius, delta.length, material, 12)
    obj.rotation_euler = delta.to_track_quat("Z", "Y").to_euler()
    return obj


def ring(name, loc, major, minor, material="Teal"):
    bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=8, location=loc, major_radius=major, minor_radius=minor)
    return finish_part(bpy.context.object, name, material)


def export_asset(name):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in PARTS:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = PARTS[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.name = name
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    # Every export has its own origin at ground level, irrespective of preview layout.
    vertices = [v.co for v in obj.data.vertices]
    lower = [min(v[i] for v in vertices) for i in range(3)]
    upper = [max(v[i] for v in vertices) for i in range(3)]
    shift = Vector(((lower[0] + upper[0]) / 2, (lower[1] + upper[1]) / 2, lower[2]))
    for vertex in obj.data.vertices:
        vertex.co -= shift
    vertices = [v.co for v in obj.data.vertices]
    lower = [min(v[i] for v in vertices) for i in range(3)]
    upper = [max(v[i] for v in vertices) for i in range(3)]
    obj.data.calc_loop_triangles()
    MANIFEST["meshes"][name] = {
        "fbx": name + ".fbx",
        "bounds_min": lower,
        "bounds_max": upper,
        "dimensions": [upper[i] - lower[i] for i in range(3)],
        "triangles": len(obj.data.loop_triangles),
        "materials": [m.name for m in obj.data.materials],
        "unreal_path": "/Game/Art/" + name,
    }
    bpy.ops.export_scene.fbx(
        filepath=str(EXPORT / (name + ".fbx")),
        use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE", axis_forward="-Y", axis_up="Z",
        add_leaf_bones=False, bake_anim=False, use_mesh_modifiers=True,
        mesh_smooth_type="FACE", path_mode="AUTO",
    )
    ASSETS.append(obj)
    PARTS.clear()
    return obj


# Command core: a welcoming four-pod hub, teal orbital collar, and luminous beacon.
cylinder("Core landing skirt", (0, 0, 7), 105, 14, "DeepTeal", 12)
cylinder("Core foundation", (0, 0, 18), 91, 15, "Teal", 12)
cube("Core living hull", (0, 0, 68), (143, 137, 90), "Pearl", 19)
for side in (-1, 1):
    cube("Core side pod", (-5, side * 75, 55), (104, 37, 60), "Ivory", 15)
    cube("Core pod accent", (-5, side * 92, 65), (55, 3, 10), "Coral", 1)
cube("Core visor", (73, 0, 74), (4, 83, 25), "DeepTeal", 6)
for side in (-1, 1):
    cube("Core friendly status window", (76, side * 25, 77), (3, 17, 9), "Lens", 3)
cube("Core door", (74, 0, 39), (4, 31, 34), "Teal", 5)
cube("Core step", (87, 0, 9), (52, 50, 12), "Pearl", 4)
cylinder("Core roof", (0, 0, 117), 77, 16, "Pearl")
ring("Core luminous roof band", (0, 0, 118), 71, 4, "Lens")
cone("Core beacon pedestal", (0, 0, 144), 34, 23, 40, "Teal")
ring("Core gold crown", (0, 0, 160), 27, 5, "Gold")
sphere("Core beacon dome", (0, 0, 175), (44, 44, 34), "Lens")
for y in (-48, 48):
    cube("Roof equipment", (-35, y, 137), (31, 25, 18), "Coral", 5)
export_asset("SM_Core")

# Extractor: twin gantries frame a readable suspended drill and a discharge bin.
cube("Extractor base", (0, 0, 8), (161, 137, 16), "DeepTeal", 7)
for y in (-48, 48):
    cube("Extractor gantry foot", (0, y, 30), (74, 30, 43), "Pearl", 8)
    cube("Extractor gantry leg", (0, y, 80), (25, 22, 103), "Gold", 6)
cube("Extractor crossbeam", (0, 0, 133), (58, 126, 26), "Pearl", 9)
cylinder("Extractor motor", (0, 0, 113), 25, 34, "Teal")
cylinder("Extractor shaft", (0, 0, 69), 12, 56, "Steel")
cone("Extractor drill", (0, 0, 32), 3, 26, 48, "Steel")
for z in (28, 40, 52):
    ring("Drill cutting collar", (0, 0, z), (z + 8) * .34, 3, "Gold")
cube("Extractor delivery bin", (58, 0, 35), (34, 67, 35), "Teal", 6)
cube("Extractor bin rim", (58, 0, 53), (36, 69, 7), "Ivory", 2)
cube("Extractor display", (31, 0, 135), (3, 40, 13), "DeepTeal", 2)
cube("Extractor signal", (34, 0, 135), (2, 20, 4), "Lens", 1)
sphere("Extractor warning", (-16, 0, 153), (13, 13, 16), "Signal")
export_asset("SM_Extractor")

# Factory: compact rounded workshop with twin ceramic stacks and an assembly hatch.
cube("Factory base", (0, 0, 8), (203, 168, 16), "DeepTeal", 8)
cube("Factory main hull", (-14, 0, 56), (154, 134, 88), "Pearl", 17)
cube("Factory roof accent", (-14, 0, 104), (144, 122, 18), "Coral", 8)
cube("Factory assembly hatch", (65, 0, 51), (8, 70, 54), "DeepTeal", 8)
cube("Factory hatch light", (71, 0, 75), (3, 52, 5), "Lens", 1)
cube("Factory conveyor", (92, 0, 20), (32, 75, 17), "Steel", 3)
for y in (-44, 44):
    cube("Factory side tank", (-17, y * 1.65, 49), (83, 21, 53), "Teal", 8)
for y in (-31, 31):
    cylinder("Factory chimney", (-49, y, 136), 18, 62, "Ivory")
    ring("Factory chimney stripe", (-49, y, 149), 18, 3, "Coral")
    cylinder("Factory chimney cap", (-49, y, 169), 21, 8, "DeepTeal")
for x in (-23, 0, 23):
    cube("Factory roof vents", (x, 0, 118), (8, 56, 7), "DeepTeal", 2)
export_asset("SM_Factory")

# Depot: open loading portal, distinct stacked color-coded storage pods.
cube("Depot base", (0, 0, 8), (182, 161, 16), "DeepTeal", 8)
cube("Depot rear spine", (-59, 0, 58), (28, 132, 93), "Pearl", 7)
for y in (-58, 58):
    cube("Depot port post", (47, y, 58), (24, 23, 99), "Pearl", 6)
cube("Depot floating roof", (-4, 0, 112), (169, 149, 24), "Pearl", 11)
cube("Depot header", (82, 0, 110), (4, 92, 12), "Teal", 2)
for x, y, z, mat in ((-27, -29, 38, "Coral"), (-27, 29, 38, "Teal"), (28, -29, 38, "Gold"), (-27, -29, 80, "Teal"), (-27, 29, 80, "Coral")):
    cube("Depot cargo pod", (x, y, z), (43, 45, 36), mat, 5)
    cube("Depot pod band", (x + 22, y, z), (2, 25, 6), "Ivory", 1)
cube("Depot docking pad", (73, 0, 15), (29, 74, 9), "Gold", 3)
export_asset("SM_Depot")

# Sensor: lightweight quadrupod mast with an oversized friendly orbital eye.
cylinder("Sensor base", (0, 0, 7), 56, 14, "DeepTeal", 8)
for angle in (45, 135, 225, 315):
    a = math.radians(angle)
    rod("Sensor support", (math.cos(a)*45, math.sin(a)*45, 11), (0, 0, 79), 7, "Pearl")
cylinder("Sensor mast", (0, 0, 91), 12, 115, "Teal")
ring("Sensor mast collar", (0, 0, 99), 18, 4, "Gold")
sphere("Sensor head", (0, 0, 155), (58, 70, 43), "Pearl")
sphere("Sensor lens surround", (27, 0, 156), (11, 50, 30), "DeepTeal")
sphere("Sensor lens", (33, 0, 156), (8, 35, 20), "Lens")
rod("Sensor aerial", (-12, 0, 170), (-18, 0, 193), 2.4, "Steel")
sphere("Sensor aerial signal", (-18, 0, 195), (8, 8, 8), "Signal")
export_asset("SM_Sensor")

# Turret: small protected twin emitter platform, visually defensive rather than humanoid.
cylinder("Turret footing", (0, 0, 8), 62, 16, "DeepTeal", 12)
cone("Turret pedestal", (0, 0, 33), 47, 30, 42, "Pearl")
ring("Turret yaw band", (0, 0, 57), 33, 5, "Teal")
cube("Turret body", (-3, 0, 81), (66, 74, 48), "Pearl", 11)
cube("Turret shield", (-21, 0, 100), (26, 78, 22), "Coral", 7)
for y in (-20, 20):
    rod("Turret barrel", (25, y, 84), (71, y, 84), 8, "Steel")
    rod("Turret muzzle", (68, y, 84), (79, y, 84), 10, "Teal")
    sphere("Turret muzzle glow", (80, y, 84), (3, 10, 10), "Lens")
sphere("Turret targeting eye", (33, 0, 101), (6, 13, 9), "Lens")
export_asset("SM_Turret")

# Robot: original pebble-shaped worker with a coral backpack and a binocular dot face.
for y in (-9, 9):
    cube("Robot shoe", (1, y, 4), (22, 11, 8), "DeepTeal", 3)
    cylinder("Robot leg", (0, y, 11), 4, 9, "Teal", 12)
cube("Robot body", (0, 0, 24), (23, 26, 22), "Pearl", 8)
cube("Robot backpack", (-13, 0, 26), (9, 20, 18), "Coral", 4)
cube("Robot head", (1, 0, 42), (24, 31, 19), "Ivory", 7)
cube("Robot face", (13, 0, 42), (3, 25, 12), "DeepTeal", 4)
for y in (-6, 6):
    sphere("Robot eye", (15, y, 44), (2.5, 4.5, 5.2), "Lens", 12, 8)
    rod("Robot upper arm", (0, y * 2.5, 30), (2, y * 3, 21), 3.5, "Teal")
    sphere("Robot mitten", (4, y * 3, 18), (10, 8, 10), "Pearl", 16, 10)
cube("Robot smile", (15, 0, 38.8), (1, 6, 1.4), "Lens", .4)
rod("Robot aerial", (-4, 0, 50), (-7, 0, 55), 1.2, "Steel")
sphere("Robot aerial light", (-7, 0, 56), (4, 4, 4), "Coral", 12, 8)
export_asset("SM_Robot")

# Bug: original six-legged armored alien, hooked mandibles and layered organic plates.
sphere("Bug abdomen", (-18, 0, 34), (60, 43, 36), "Chitin")
sphere("Bug thorax", (11, 0, 32), (40, 36, 30), "RedChitin")
sphere("Bug head", (37, 0, 26), (33, 31, 24), "Chitin")
for x in (-39, -26, -13, 0):
    shell = sphere("Bug overlapping carapace", (x, 0, 45), (24, 43 - abs(x) * .12, 15), "RedChitin", 16, 10)
    rod("Bug dorsal spine", (x, 0, 48), (x - 9, 0, 60), 4, "Bone", .1)
for side in (-1, 1):
    for i, x in enumerate((-20, 3, 24)):
        knee = (x - 13 + i * 5, side * 38, 25)
        tip = (x - 19 + i * 5, side * (58 - i * 4), 1.5)
        rod("Bug upper leg", (x, side * 14, 32), knee, 5.5, "Chitin", 3.5)
        sphere("Bug leg joint", knee, (9, 9, 9), "RedChitin", 12, 8)
        rod("Bug hooked leg", knee, tip, 3.5, "Bone", .6)
    for x, y, z in ((43, 11, 31), (48, 7, 32), (38, 14, 33)):
        sphere("Bug compound eye", (x, side*y, z), (5, 4, 4), "BugEye", 12, 8)
    rod("Bug mandible base", (47, side*11, 22), (64, side*17, 15), 4.5, "Bone", 2.7)
    rod("Bug mandible hook", (64, side*17, 15), (63, side*5, 17), 2.8, "Bone", .2)
    rod("Bug antenna", (40, side*9, 37), (53, side*20, 47), 1.4, "Chitin", .5)
export_asset("SM_Bug")

(EXPORT / "asset_manifest.json").write_text(json.dumps(MANIFEST, indent=2), encoding="utf-8")

# A retained source scene and contact sheet make all exports reviewable together.
positions = ((-360, 130), (-100, 130), (160, 130), (420, 130), (-320, -130), (-80, -130), (170, -130), (400, -130))
for obj, (x, y) in zip(ASSETS, positions):
    obj.location = (x, y, 0)

bpy.ops.mesh.primitive_plane_add(size=2200, location=(0, 0, -.7))
floor = bpy.context.object
floor.name = "Preview ground — not exported"
floor.data.materials.append(MATERIALS["DeepTeal"])
for location, energy, size in (((50, -250, 850), 3500000, 700), ((-650, 150, 500), 1700000, 550), ((550, 500, 650), 2400000, 600)):
    bpy.ops.object.light_add(type="AREA", location=location)
    light = bpy.context.object
    light.data.energy = energy
    light.data.shape = "DISK"
    light.data.size = size
    light.rotation_euler = (Vector((0, 0, 35)) - light.location).to_track_quat("-Z", "Y").to_euler()
bpy.ops.object.camera_add(location=(910, -1270, 1050))
camera = bpy.context.object
camera.rotation_euler = (Vector((20, 0, 45)) - camera.location).to_track_quat("-Z", "Y").to_euler()
camera.data.type = "ORTHO"
camera.data.ortho_scale = 1180
camera.data.clip_end = 10000
scene = bpy.context.scene
scene.camera = camera
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.world.color = (0.25, 0.25, 0.25)
scene.render.resolution_x = 1600
scene.render.resolution_y = 1000
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = "AgX"
scene.render.image_settings.file_format = "PNG"
scene.render.filepath = str(ART / "Previews" / "asset_contact_sheet.png")
bpy.ops.wm.save_as_mainfile(filepath=str(ART / "Source" / "Seige_Original_Assets.blend"))
bpy.ops.render.render(write_still=True)
for asset_name, filename, scale in (("SM_Robot", "robot_detail.png", 105), ("SM_Bug", "bug_detail.png", 180)):
    detail_obj = next(obj for obj in ASSETS if obj.name == asset_name)
    center = detail_obj.location + Vector((0, 0, 28))
    camera.location = center + Vector((140, -170, 110))
    camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.ortho_scale = scale
    scene.render.resolution_x = 900
    scene.render.resolution_y = 900
    scene.render.filepath = str(ART / "Previews" / filename)
    bpy.ops.render.render(write_still=True)
print("SEIGE_ASSETS_CREATED " + json.dumps({name: entry["dimensions"] for name, entry in MANIFEST["meshes"].items()}))

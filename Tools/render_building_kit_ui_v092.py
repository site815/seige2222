"""Bake HUD portraits and review sheets for the v0.9.2 building kit from its saved Blender source.

    blender -b -P Tools/render_building_kit_ui_v092.py [-- --preview] [-- --only=solarArray,hangar]

Portraits use the same orthographic studio recipe as Tools/render_orbital_ui_v09.py so the
new cards match the existing ones: 384x384 RGBA, Cycles 20 samples, AgX, +0.2 EV.
--preview additionally renders 900x600 review images on a ground plane to Art/BuildingKitV092/Previews.
"""
from pathlib import Path
import bpy, json, math, sys
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]; ART = ROOT / 'Art'; KIT = ART / 'BuildingKitV092'
OUT = KIT / 'UI'; PREV = KIT / 'Previews'; OUT.mkdir(parents=True, exist_ok=True); PREV.mkdir(parents=True, exist_ok=True)
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else sys.argv[1:]
ONLY = next((a.split('=', 1)[1].split(',') for a in ARGS if a.startswith('--only=')), None)
PREVIEW = '--preview' in ARGS
bpy.ops.wm.open_mainfile(filepath=str(KIT / 'Source/BuildingKitV092.blend'))
manifest = json.loads((KIT / 'kit_manifest.json').read_text(encoding='utf-8'))
assets = {o.name: o for o in bpy.data.objects if o.type == 'MESH' and o.name in manifest['meshes']}
for o in assets.values(): o.hide_render = True
scene = bpy.context.scene; scene.render.engine = 'CYCLES'; scene.cycles.device = 'CPU'; scene.cycles.samples = 20; scene.cycles.use_denoising = True
scene.render.film_transparent = True; scene.render.image_settings.file_format = 'PNG'; scene.render.image_settings.color_mode = 'RGBA'
scene.render.resolution_x = 384; scene.render.resolution_y = 384; scene.render.resolution_percentage = 100
scene.world = bpy.data.worlds.new('Kit portrait neutral studio'); scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.48, .58, .7, 1); scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
scene.view_settings.view_transform = 'AgX'; scene.view_settings.exposure = .2
# The studio lights turn with the kit's +90 degree export yaw so each portrait keeps
# the light-to-building relation of the original orbital card recipe.
TURN = math.pi / 2
bpy.ops.object.light_add(type='SUN', location=(0, 0, 4000)); key = bpy.context.object; key.rotation_euler = (.35, -.4, -.65 + TURN); key.data.energy = 3; key.data.angle = .12
bpy.ops.object.light_add(type='SUN', location=(0, 0, 3000)); fill = bpy.context.object; fill.rotation_euler = (-.5, .65, 2.4 + TURN); fill.data.energy = .8; fill.data.angle = .35
bpy.ops.object.camera_add(); camera = bpy.context.object; camera.data.type = 'ORTHO'; camera.data.clip_end = 100000; scene.camera = camera
ground = None
if PREVIEW:
    bpy.ops.mesh.primitive_plane_add(size=12000); ground = bpy.context.object; ground.name = 'PreviewGround'
    gm = bpy.data.materials.new('PreviewGround'); gm.use_nodes = True; gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.16, .2, .09, 1); ground.data.materials.append(gm); ground.hide_render = True
report = {}
_shuttle = []


def shuttle_parts():
    """The retained level-1 shuttle, which stands in the open well of every
    command-campus level: imported once from the orbital set, scaled to its
    2880 cm landing size and centred on the origin, so the campus portraits
    show the building as the game draws it."""
    if not _shuttle:
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(ART / 'OrbitalV09/Exports/SM_Shuttle.fbx'))
        parts = [o for o in bpy.data.objects if o not in before]
        roots = [o for o in parts if o.parent is None]
        def bounds():
            bpy.context.view_layer.update()
            pts = [o.matrix_world @ Vector(c) for o in parts if o.type == 'MESH' for c in o.bound_box]
            return Vector([min(p[i] for p in pts) for i in range(3)]), Vector([max(p[i] for p in pts) for i in range(3)])
        lo, hi = bounds(); k = 2880 / max(hi.x - lo.x, hi.y - lo.y)
        for o in roots: o.scale = [v * k for v in o.scale]; o.location = [v * k for v in o.location]
        lo, hi = bounds()
        for o in roots: o.location -= Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
        for o in parts: o.hide_render = True
        _shuttle.extend(o for o in parts if o.type == 'MESH')
    return _shuttle


for name, obj in assets.items():
    key_name = name.removeprefix('SM_')
    if ONLY and key_name[0].lower() + key_name[1:] not in ONLY:
        continue
    obj.hide_render = False; saved = obj.location.copy(); obj.location = (0, 0, 0)
    extras = shuttle_parts() if key_name.startswith('CommandCampus') else []
    for o in extras: o.hide_render = False
    bpy.context.view_layer.update()
    corners = [o.matrix_world @ Vector(v) for o in [obj] + extras for v in o.bound_box]
    low = Vector(tuple(min(v[i] for v in corners) for i in range(3))); high = Vector(tuple(max(v[i] for v in corners) for i in range(3)))
    center = (low + high) / 2; extent = (high - low).length
    # The saved meshes are already turned to their export orientation (front
    # toward +X here, Unreal +X in game). Same front-and-right-side framing as
    # the default game camera (yaw 135 looks at Unreal +X and -Y = Blender +Y).
    camera.location = center + Vector((1.5, 1.1, 1.05)) * extent
    camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler(); camera.data.ortho_scale = extent * .91
    scene.render.filepath = str(OUT / ('T_Building_' + key_name + '.png')); bpy.ops.render.render(write_still=True)
    report[key_name] = {'source_mesh': name, 'source_blend': 'Art/BuildingKitV092/Source/BuildingKitV092.blend', 'texture': '/Game/Art/Interface/T_Building_' + key_name, 'size': 384}
    if PREVIEW:
        ground.hide_render = False; scene.render.film_transparent = False; scene.render.resolution_x = 900; scene.render.resolution_y = 600
        camera.data.ortho_scale = extent * 1.05; camera.location = center + Vector((1.45, 1.25, .9)) * extent
        camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(PREV / (key_name + '.png')); bpy.ops.render.render(write_still=True)
        ground.hide_render = True; scene.render.film_transparent = True; scene.render.resolution_x = 384; scene.render.resolution_y = 384
    obj.hide_render = True; obj.location = saved
    for o in extras: o.hide_render = True
existing = json.loads((OUT / 'portraits.json').read_text(encoding='utf-8')) if (OUT / 'portraits.json').exists() else {}
existing.update(report)
(OUT / 'portraits.json').write_text(json.dumps(existing, indent=2) + '\n', encoding='utf-8')
print('KIT_PORTRAITS_COMPLETE ' + json.dumps(sorted(report)), flush=True)

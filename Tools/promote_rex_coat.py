"""Promote a reviewed coat bake, asserting that anatomy and weights did not change."""
from pathlib import Path
import bpy, hashlib, json, shutil, struct
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
STAGE = ROOT / 'Saved/rex-coat-polish'
def geometry_hash(dog):
    digest = hashlib.sha256()
    for vertex in dog.data.vertices:
        digest.update(struct.pack('<3f', *vertex.co))
        for group in vertex.groups:
            digest.update(struct.pack('<If', group.group, group.weight))
    for poly in dog.data.polygons:
        digest.update(struct.pack('<' + 'I' * len(poly.vertices), *poly.vertices))
    for loop in dog.data.uv_layers['UV0'].data:
        digest.update(struct.pack('<2f', *loop.uv))
    return digest.hexdigest()
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
before = geometry_hash(bpy.data.objects['SK_CompanionDog'])
bpy.ops.wm.open_mainfile(filepath=str(STAGE / 'Coat-candidate.blend'))
dog = bpy.data.objects['SK_CompanionDog']
assert geometry_hash(dog) == before, 'Coat pass changed geometry, UVs or weights'
bpy.context.preferences.filepaths.save_version = 0
mat = dog.data.materials[0]
old = bpy.data.materials.get('DM_Coat')
if old and old != mat:
    old.name = 'Pre-polish coat'
mat.name = 'DM_Coat'
for filename in ('T_Dog_Albedo.png', 'T_Dog_CoatNormal.png'):
    shutil.copy2(STAGE / filename, ART / 'Textures' / filename)
for node in mat.node_tree.nodes:
    if node.type == 'TEX_IMAGE' and node.image:
        filename = 'T_Dog_CoatNormal.png' if node.image.colorspace_settings.name == 'Non-Color' else 'T_Dog_Albedo.png'
        node.image.filepath = str(ART / 'Textures' / filename)
        node.image.pack()
for action in bpy.data.actions:
    if action.name in ('A_DogIdle', 'A_DogWalk'):
        action.use_fake_user = True
bpy.data.orphans_purge(do_recursive=True)
bpy.ops.wm.save_as_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
manifest_path = ART / 'dog_manifest.json'
manifest = json.loads(manifest_path.read_text())
manifest['textures']['T_Dog_Albedo'] = '4096 sRGB photo coat; retained facial mapping plus directional multi-scale photographic body/leg/chest fur'
manifest['textures']['T_Dog_CoatNormal'] = '2048 tangent-space normal baked from matching photographic fur on UV0; OpenGL source, green flipped in Unreal'
manifest['coat_polish'] = {'geometry_uv_weights_sha256': before, 'anatomy_unchanged': True,
    'pipeline': ['Tools/prepare_rex_coat_tiles.py', 'Tools/polish_rex_coat.py', 'Tools/promote_rex_coat.py'],
    'normal_strength': .65, 'fur_flow': 'Downward chest/legs; along flanks, blended non-mirrored photographic samples',
    'preview_uses_runtime_baked_normal': True}
manifest['unreal_import_pending'] = True
manifest['runtime_review_pending'] = True
manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
(ART / 'coat_polish_validation.json').write_text(json.dumps(manifest['coat_polish'], indent=2) + '\n')
scene = bpy.context.scene
rig = bpy.data.objects['CompanionDogRig']
scene.cycles.samples = 24
scene.render.resolution_x = 1400
scene.render.resolution_y = 1050
scene.render.resolution_percentage = 100
for name, pos, aim, lens in [('front', (300, 0, 85), (30, 0, 47), 64),
    ('profile', (3, -280, 85), (-3, 0, 43), 50), ('right', (3, 280, 85), (-3, 0, 43), 50),
    ('rear', (-265, 25, 88), (-10, 0, 40), 55), ('three_quarter', (205, -240, 120), (4, 0, 44), 55),
    ('face', (175, 0, 78), (47, 0, 66), 110)]:
    scene.camera.location = pos
    scene.camera.rotation_euler = (Vector(aim) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera.data.lens = lens
    scene.render.filepath = str(ART / 'Previews' / ('dog_' + name + '.png'))
    bpy.ops.render.render(write_still=True)
rig.animation_data.action = bpy.data.actions['A_DogWalk']
for frame in (4, 7, 10):
    scene.frame_set(frame)
    scene.camera.location = (250, -65, 105)
    scene.camera.rotation_euler = (Vector((15, 0, 43)) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera.data.lens = 58
    scene.render.filepath = str(ART / 'Previews' / ('dog_walk_' + str(frame) + '.png'))
    bpy.ops.render.render(write_still=True)
print('REX_COAT_PROMOTED_ANATOMY_UNCHANGED', before, flush=True)

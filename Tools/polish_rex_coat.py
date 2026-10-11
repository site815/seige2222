"""Bake photographic fur clumps and matching tangent normals without editing anatomy.

Candidate output stays under Saved/rex-coat-polish until reviewed. The final
source geometry, skeleton, skin weights, animations and facial projection remain
unchanged. Run after the revision-8 exporter, before the Unreal importer.
"""
from pathlib import Path
import bpy, bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/CompanionDog'
OUT = ROOT / 'Saved/rex-coat-polish'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ART / 'Source/CompanionDog.blend'))
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 16
scene.cycles.use_denoising = True
dog = bpy.data.objects['SK_CompanionDog']
rig = bpy.data.objects['CompanionDogRig']
rig.data.pose_position = 'REST'
coat = dog.copy()
coat.data = dog.data.copy()
bpy.context.collection.objects.link(coat)
coat.name = 'Coat bake only'
coat.parent = None
for modifier in list(coat.modifiers):
    coat.modifiers.remove(modifier)
bm = bmesh.new()
bm.from_mesh(coat.data)
bmesh.ops.delete(bm, geom=[p for p in bm.faces if p.material_index != 0], context='FACES')
bm.to_mesh(coat.data)
bm.free()
dog.hide_render = True
mat = bpy.data.materials.new('Photographic fur bake')
mat.use_nodes = True
nodes = mat.node_tree.nodes
links = mat.node_tree.links
nodes.clear()
def node(kind):
    return nodes.new(kind)
def link(a, op, b, ip):
    links.new(a.outputs[op], b.inputs[ip])
out = node('ShaderNodeOutputMaterial')
em = node('ShaderNodeEmission')
link(em, 0, out, 'Surface')
uv = node('ShaderNodeUVMap')
uv.uv_map = 'UV0'
old = node('ShaderNodeTexImage')
old.image = bpy.data.images.load(str(ART / 'Textures/T_Dog_Albedo.png'), check_existing=False)
link(uv, 0, old, 'Vector')
geo = node('ShaderNodeNewGeometry')
scale = node('ShaderNodeVectorMath')
scale.operation = 'MULTIPLY'
scale.inputs[1].default_value = (1/10, 1/10, 1/12)
link(geo, 'Position', scale, 0)
patches = {}
for name in ('body', 'leg', 'chest'):
    tex = node('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(ART / 'Source/PhotoPatches' / ('coat-' + name + '.png')), check_existing=False)
    tex.projection = 'BOX'
    tex.projection_blend = .25
    link(scale, 0, tex, 'Vector')
    # A second non-integral sample suppresses repeated clump bands without
    # reflecting the crop into rosettes. Both samples retain hair direction.
    scaled2 = node('ShaderNodeVectorMath')
    scaled2.operation = 'MULTIPLY_ADD'
    scaled2.inputs[1].default_value = (1.43, 1.43, 1.43)
    scaled2.inputs[2].default_value = (.371, .613, .237)
    link(scale, 0, scaled2, 0)
    tex2 = node('ShaderNodeTexImage')
    tex2.image = tex.image
    tex2.projection = 'BOX'
    tex2.projection_blend = .25
    link(scaled2, 0, tex2, 'Vector')
    variation = node('ShaderNodeMixRGB')
    variation.inputs[0].default_value = .45
    link(tex, 'Color', variation, 1)
    link(tex2, 'Color', variation, 2)
    patches[name] = variation
vc = node('ShaderNodeVertexColor')
vc.layer_name = 'PhotoRegion'
sep = node('ShaderNodeSeparateColor')
link(vc, 'Color', sep, 0)
chest = node('ShaderNodeMixRGB')
link(sep, 'Blue', chest, 0)
link(patches['body'], 'Color', chest, 1)
link(patches['chest'], 'Color', chest, 2)
legs = node('ShaderNodeMixRGB')
link(sep, 'Green', legs, 0)
link(chest, 0, legs, 1)
link(patches['leg'], 'Color', legs, 2)
face = node('ShaderNodeMixRGB')
link(sep, 'Red', face, 0)
link(legs, 0, face, 1)
link(old, 'Color', face, 2)
link(face, 0, em, 'Color')
coat.data.materials.clear()
coat.data.materials.append(mat)
for poly in coat.data.polygons:
    poly.material_index = 0
bpy.ops.object.select_all(action='DESELECT')
coat.select_set(True)
bpy.context.view_layer.objects.active = coat
coat.data.uv_layers.active = coat.data.uv_layers['UV0']
coat.data.uv_layers['UV0'].active_render = True
scene.render.bake.use_selected_to_active = False
scene.render.bake.margin = 16
albedo = bpy.data.images.new('Detailed photo coat', width=4096, height=4096, alpha=False)
albedo.colorspace_settings.name = 'sRGB'
target = node('ShaderNodeTexImage')
target.image = albedo
nodes.active = target
bpy.ops.object.bake(type='EMIT')
albedo.filepath_raw = str(OUT / 'T_Dog_Albedo.png')
albedo.file_format = 'PNG'
albedo.save()
# The normal map is baked in the actual final UV orientation. It follows both
# the anatomical fur flow and the photographic clumps, without UV6x mismatch.
bs = node('ShaderNodeBsdfPrincipled')
link(face, 0, bs, 'Base Color')
bs.inputs['Roughness'].default_value = .8
bs.inputs['Specular IOR Level'].default_value = .2
height = node('ShaderNodeRGBToBW')
link(legs, 0, height, 0)
inv = node('ShaderNodeMath')
inv.operation = 'SUBTRACT'
inv.inputs[0].default_value = 1
link(sep, 'Red', inv, 1)
strength = node('ShaderNodeMath')
strength.operation = 'MULTIPLY'
strength.inputs[1].default_value = .75
link(inv, 0, strength, 0)
bump = node('ShaderNodeBump')
bump.inputs['Distance'].default_value = .22
link(height, 0, bump, 'Height')
link(strength, 0, bump, 'Strength')
link(bump, 0, bs, 'Normal')
link(bs, 0, out, 'Surface')
normal = bpy.data.images.new('Photographic tangent fur', width=2048, height=2048, alpha=False)
normal.colorspace_settings.name = 'Non-Color'
target.image = normal
nodes.active = target
scene.render.bake.normal_space = 'TANGENT'
bpy.ops.object.bake(type='NORMAL')
normal.filepath_raw = str(OUT / 'T_Dog_CoatNormal.png')
normal.file_format = 'PNG'
normal.save()
# Preview the actual baked maps rather than an unexported procedural bump.
preview = bpy.data.materials.new('DM_Coat_PhotoPolish')
preview.use_nodes = True
pn = preview.node_tree.nodes
pl = preview.node_tree.links
pb = pn.get('Principled BSDF')
pb.inputs['Roughness'].default_value = .8
pb.inputs['Specular IOR Level'].default_value = .2
puv = pn.new('ShaderNodeUVMap')
puv.uv_map = 'UV0'
pt = pn.new('ShaderNodeTexImage')
pt.image = albedo
pl.new(puv.outputs[0], pt.inputs['Vector'])
pl.new(pt.outputs['Color'], pb.inputs['Base Color'])
nt = pn.new('ShaderNodeTexImage')
nt.image = normal
pl.new(puv.outputs[0], nt.inputs['Vector'])
nm = pn.new('ShaderNodeNormalMap')
nm.uv_map = 'UV0'
nm.inputs['Strength'].default_value = .65
pl.new(nt.outputs['Color'], nm.inputs['Color'])
pl.new(nm.outputs[0], pb.inputs['Normal'])
dog.data.materials[0] = preview
dog.hide_render = False
bpy.data.objects.remove(coat, do_unlink=True)
rig.data.pose_position = 'POSE'
rig.animation_data.action = bpy.data.actions['A_DogIdle']
scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'Coat-candidate.blend'))
scene.render.resolution_x = 1400
scene.render.resolution_y = 1050
scene.render.resolution_percentage = 100
for name, pos, aim, lens in [('front', (300, 0, 85), (30, 0, 47), 64),
                             ('side', (3, -280, 85), (-3, 0, 43), 50),
                             ('three_quarter', (205, -240, 120), (4, 0, 44), 55)]:
    scene.camera.location = pos
    scene.camera.rotation_euler = (Vector(aim) - scene.camera.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera.data.lens = lens
    scene.render.filepath = str(OUT / ('coat-' + name + '.png'))
    bpy.ops.render.render(write_still=True)
print('REX_PHOTOGRAPHIC_COAT_BAKED', flush=True)

"""Matched Blender distance-shape comparison, not an Unreal performance test."""
from pathlib import Path
import bpy, math, random
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'Art/EnvironmentV08/Canopies/Previews'
for revision, folder, name in (
    ('v07', 'Art/EnvironmentV07/Source', 'SM_BroadleafProxy'),
    ('v08', 'Art/EnvironmentV08/Canopies/Source', 'SM_BroadleafSpraysV08')):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(ROOT/folder/(name+'.blend')), link=False) as (_, data):
        data.objects = [name]
    tree = data.objects[0]
    bpy.context.scene.collection.objects.link(tree)
    tree.hide_render = False
    tree.hide_set(False)
    mat = bpy.data.materials.new('EqualMeanAlbedoShapeReview')
    mat.use_nodes = True
    shader = mat.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (.045,.080,.022,1)
    shader.inputs['Roughness'].default_value = .90
    shader.inputs['Specular IOR Level'].default_value = .10
    for i, old in enumerate(tree.data.materials):
        if old.name.startswith(('M_CanopyProxy','M_BroadleafSprays')):
            tree.data.materials[i] = mat
    rng = random.Random(8222)
    for y in range(3):
        for x in range(4):
            obj = tree if (x,y)==(0,0) else tree.copy()
            if obj != tree:
                bpy.context.scene.collection.objects.link(obj)
            obj.location = ((x-1.5)*1650+rng.uniform(-180,180),
                            (y-1)*1550+rng.uniform(-180,180),0)
            obj.rotation_euler.z = rng.uniform(0,math.tau)
            scale = rng.uniform(.85,1.1)
            obj.scale = (scale,scale,scale)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = .01
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    world = bpy.data.worlds.new('ReviewWorld')
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs[0].default_value = (.24,.28,.32,1)
    world.node_tree.nodes['Background'].inputs[1].default_value = .7
    scene.world = world
    bpy.ops.object.light_add(type='SUN')
    bpy.context.object.rotation_euler = (.6,-.4,-.55)
    bpy.context.object.data.energy = 2.7
    bpy.context.object.data.angle = .12
    bpy.ops.object.camera_add(location=(8500,-11500,11000))
    cam = bpy.context.object
    cam.rotation_euler = (Vector((0,0,600))-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = 11000
    cam.data.clip_end = 100000
    scene.camera = cam
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Medium High Contrast'
    scene.render.resolution_x = 1200
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = str(OUT/('distance_shape_'+revision+'.png'))
    bpy.ops.render.render(write_still=True)

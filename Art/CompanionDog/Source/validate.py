"""Offline Blender FBX round-trip checks; does not launch or modify Unreal."""
from pathlib import Path
import bpy, json, math, hashlib

root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'dog_manifest.json').read_text())
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(root/'Exports/SK_CompanionDog.fbx'))
mesh=next(o for o in bpy.data.objects if o.type=='MESH')
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
expected=set(manifest['bones']);actual=set(rig.data.bones.keys())
assert actual==expected, {'missing_bones':sorted(expected-actual),'extra_bones':sorted(actual-expected)}
invalid=[]
for v in mesh.data.vertices:
    total=sum(g.weight for g in v.groups)
    if not all(math.isfinite(x) for x in v.co) or not math.isfinite(total) or abs(total-1)>.001 or len(v.groups)>4:
        invalid.append(v.index)
assert not invalid, {'invalid_vertices':len(invalid)}
used=sorted({mesh.data.materials[p.material_index].name for p in mesh.data.polygons})
assert set(used)==set(manifest['used_materials']), {'roundtrip':used,'expected':manifest['used_materials']}
assert 'DM_Iris' in used
assert mesh.data.color_attributes, 'Missing coat vertex colors'
for name,sha in manifest['exports'].items():
    assert hashlib.sha256((root/'Exports'/name).read_bytes()).hexdigest()==sha,name
mesh.data.calc_loop_triangles()
report={'fbx_roundtrip_mesh':mesh.name,'bones':len(actual),'bone_names_match':True,
    'unweighted_or_invalid_vertices':len(invalid),'max_influences':max(len(v.groups) for v in mesh.data.vertices),
    'vertex_colors':len(mesh.data.color_attributes),'used_materials':used,'export_hashes_match':True,
    'clip_files_present':True,'animations':{}}
for name,definition in manifest['animations'].items():
    previous=set(bpy.data.objects);bpy.context.scene.render.fps=30
    bpy.ops.import_scene.fbx(filepath=str(root/'Exports'/(name+'.fbx')))
    animated=next(o for o in bpy.data.objects if o not in previous and o.type=='ARMATURE')
    assert set(animated.data.bones.keys())==expected,name
    action=animated.animation_data.action;start,end=action.frame_range;duration=(end-start)/30
    assert abs(duration-definition['duration_seconds'])<.0001,(name,duration)
    bpy.context.scene.frame_set(round(start));first={b.name:b.matrix.copy() for b in animated.pose.bones}
    bpy.context.scene.frame_set(round(end))
    error=max(abs(b.matrix[i][j]-first[b.name][i][j]) for b in animated.pose.bones for i in range(4) for j in range(4))
    assert error<.02,(name,error)
    report['animations'][name]={'duration_seconds':duration,'loop_matrix_max_error':error,'bones':len(animated.pose.bones)}
report['walk_authored_ik_max_error_cm']=manifest['animations']['A_DogWalk']['max_ik_target_error_cm']
report['source_triangles']=manifest['triangles']
report['source_cosmetic_revision']=manifest['cosmetic_revision']
report['unreal_import_verified']=False
(root/'source_validation.json').write_text(json.dumps(report,indent=2)+'\n')
print('DOG_COMPLETE_SOURCE_VALIDATION '+json.dumps(report),flush=True)

"""Read-only Blender/NumPy diagnostic; no mesh or material assets are changed."""
from pathlib import Path
import bpy,json,numpy as np
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/EnvironmentV04'
data=json.loads((ART/'sources.json').read_text())
report={}
for source_id in ('grass_medium_02','grass_bermuda_01'):
    bpy.ops.wm.open_mainfile(filepath=str(ROOT/data['assets'][source_id]['original_source']['local_path']))
    image=bpy.data.images.load(str(ROOT/data['assets'][source_id]['maps']['surface']['alpha']['local_path']),check_existing=True)
    image.colorspace_settings.name='Non-Color';w,h=image.size[:]
    pixels=np.empty(w*h*4,np.float32);image.pixels.foreach_get(pixels);alpha=pixels.reshape(h,w,4)[:,:,0]
    objects=([bpy.data.objects['grass_medium_02_'+letter] for letter in 'bcde'] if source_id=='grass_medium_02' else
        [o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('grass_bermuda_01_') and any(c.name=='grass_bermuda_01_static' for c in o.users_collection) and any(k in o.name for k in ('medium_','small_','seedling_'))])
    entries=[]
    for o in objects:
        mesh=o.data;mesh.calc_loop_triangles();layer=mesh.uv_layers.active
        uv=np.array([d.uv[:] for d in layer.data]);tri_uv=np.array([[uv[i] for i in t.loops] for t in mesh.loop_triangles])
        # Diagnostic samples only: not a proof that any entire triangle is opaque.
        bary=np.array(((1,0,0),(0,1,0),(0,0,1),(.5,.5,0),(.5,0,.5),(0,.5,.5),(1/3,1/3,1/3)))
        sample=np.einsum('bc,tcd->tbd',bary,tri_uv)
        indices=np.floor((sample%1)*[w,h]).astype(int)
        values=alpha[indices[:,:,1],indices[:,:,0]]
        entries.append({'object':o.name,'uv_layer':layer.name,'uv_min':uv.min(0).tolist(),'uv_max':uv.max(0).tolist(),
            'triangles':len(tri_uv),'all_7_samples_opaque_fraction':float((values.min(1)>.99).mean()),
            'all_7_samples_below_clip_fraction':float((values.max(1)<.33).mean()),'mean_alpha_samples':float(values.mean())})
    report[source_id]=entries
(ROOT/'Art/EnvironmentV05/source_uv_coverage.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report),flush=True)

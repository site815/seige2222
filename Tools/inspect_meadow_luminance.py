"""Blender batch measurement of the byte source used for linear UE ground shading."""
from pathlib import Path
import bpy,numpy as np,json
ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'Art/Textures/PolyHaven/leafy_grass_diff_2k.jpg'
im=bpy.data.images.load(str(path));values=np.empty(len(im.pixels),np.float32);im.pixels.foreach_get(values)
rgb=values.reshape(im.size[1],im.size[0],4)[:,:,:3]
# Byte image pixels expose normalized encoded RGB; Unreal's sRGB texture samples
# decode these before any material luminance computation.
linear=np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
luma=linear@np.array([.3,.59,.11],np.float32)
record={'source':path.relative_to(ROOT).as_posix(),'is_float':im.is_float,'rgb_center':rgb[1023,1024].tolist(),
    'linear_luminance_mean':float(luma.mean()),'linear_luminance_std':float(luma.std()),
    'percentiles':{str(p):float(np.percentile(luma,p)) for p in (1,10,25,50,75,90,99)},
    'prior_mapping_lower_clamp_fraction':float((luma<(.22-.14/1.2)).mean()),
    'luminance_weights':[.3,.59,.11]}
record['representative_filtered_levels']={}
for size in (128,64,32,16):
    block=luma.shape[0]//size
    filtered=luma.reshape(size,block,size,block).mean(axis=(1,3))
    record['representative_filtered_levels'][str(size)]={'std':float(filtered.std()),'p10':float(np.percentile(filtered,10)),'p50':float(np.percentile(filtered,50)),'p90':float(np.percentile(filtered,90))}
(ROOT/'Art/EnvironmentV04/terrain_luminance_analysis.json').write_text(json.dumps(record,indent=2));print(json.dumps(record),flush=True)

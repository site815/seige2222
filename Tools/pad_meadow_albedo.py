"""Blender batch: extend photographic atlas RGB beyond its unchanged cutout mask.

Texture-build padding prevents black atlas gutters entering grass colour mips.
The downloaded photograph and alpha remain untouched; this is a CC0 derivative.
"""
from pathlib import Path
import bpy, numpy as np, json, hashlib
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'Art/EnvironmentV04'
SOURCE=globals().get('SOURCE_ASSET','grass_medium_02')
folder=ART/'Textures'/SOURCE
source=folder/(SOURCE+'_diff_2k.jpg');alpha=folder/(SOURCE+'_alpha_2k.png')
def pixels(path):
    im=bpy.data.images.load(str(path),check_existing=False)
    values=np.empty(len(im.pixels),np.float32);im.pixels.foreach_get(values)
    return im,values.reshape(im.size[1],im.size[0],4)
im,rgba=pixels(source);maskim,coverage=pixels(alpha)
known=coverage[:,:,0]>.95;original=known.copy();colour=rgba[:,:,:3].copy()
mean=np.median(colour[known],axis=0)
# Pad 24 texels from solid photograph pixels. Remaining unused background gets
# the photographed median, so even very distant colour mips have no black seam.
for iteration in range(24):
    totals=np.zeros_like(colour);weights=np.zeros(known.shape,np.float32)
    for axis,amount in ((0,-1),(0,1),(1,-1),(1,1)):
        k=np.roll(known,amount,axis);c=np.roll(colour,amount,axis)
        if axis==0:k[0 if amount==1 else -1,:]=False
        else:k[:,0 if amount==1 else -1]=False
        totals+=c*k[:,:,None];weights+=k
    new=(~known)&(weights>0)
    colour[new]=totals[new]/weights[new,None];known|=new
colour[~known]=mean;rgba[:,:,:3]=colour;rgba[:,:,3]=1
out=bpy.data.images.new('Grass Medium 02 mip-safe colour',width=im.size[0],height=im.size[1],alpha=False)
out.colorspace_settings.name='sRGB';out.pixels.foreach_set(rgba.ravel())
destination=folder/(SOURCE+'_diff_padded_2k.png')
out.filepath_raw=str(destination);out.file_format='PNG';out.save()
report={'source':source.relative_to(ROOT).as_posix(),'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'alpha_source':alpha.relative_to(ROOT).as_posix(),'derivative':destination.relative_to(ROOT).as_posix(),
    'sha256':hashlib.sha256(destination.read_bytes()).hexdigest(),'bytes':destination.stat().st_size,
    'process':'24-texel RGB dilation from alpha > .95, remaining invisible atlas background filled with source median; alpha unchanged',
    'source_solid_fraction':float(original.mean()),'license':'CC0-1.0','original_source_unchanged':True}
(ART/('meadow_atlas_padding.json' if SOURCE=='grass_medium_02' else 'bermuda_atlas_padding.json')).write_text(json.dumps(report,indent=2))
print('MEADOW_ATLAS_PADDING_COMPLETE '+json.dumps(report),flush=True)
if SOURCE=='grass_medium_02':
    exec(compile(Path(__file__).read_text(),__file__,'exec'),{'__file__':__file__,'__name__':'__main__','SOURCE_ASSET':'grass_bermuda_01'})

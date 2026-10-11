"""Prepare photographic fur from retained private-reference crop derivatives. Requires Pillow and numpy."""
from pathlib import Path
from PIL import Image,ImageFilter
import numpy as np,shutil
root=Path(__file__).resolve().parents[1];out=root/'Art/CompanionDog/Source/PhotoPatches'
for n,target in [('body',[171,145,112]),('leg',[207,192,169]),('chest',[193,168,133])]:
 src=out/f'raw-{n}.png'
 im=Image.open(src).convert('RGB');a=np.asarray(im,dtype=np.float32);blur=np.asarray(im.filter(ImageFilter.GaussianBlur(38)),dtype=np.float32)
 ratio=1+1.15*(a/np.maximum(blur,1)-1);ratio=np.clip(ratio,.62,1.4);b=np.clip(ratio*np.array(target),0,255)
 # Crossfade only the narrow repeat boundaries, without reflected rosette motifs.
 h,w=b.shape[:2];nedge=max(4,int(min(h,w)*.035))
 for i in range(nedge):
  f=(1-i/nedge)*.5;left=b[:,i].copy();right=b[:,-1-i].copy();b[:,i]=left*(1-f)+right*f;b[:,-1-i]=right*(1-f)+left*f
 for i in range(nedge):
  f=(1-i/nedge)*.5;top=b[i].copy();bottom=b[-1-i].copy();b[i]=top*(1-f)+bottom*f;b[-1-i]=bottom*(1-f)+top*f
 tile=Image.fromarray(b.astype(np.uint8)).resize((1024,1024),Image.Resampling.LANCZOS)
 if n in ('leg','chest'):tile=tile.transpose(Image.Transpose.ROTATE_90)
 tile.save(out/f'coat-{n}.png')
 print(n,np.asarray(b).mean((0,1)),np.asarray(b).std((0,1)))

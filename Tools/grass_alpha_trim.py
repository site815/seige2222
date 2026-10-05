"""Conservative CPU-only UV alpha trimming for Blender source grass templates.

No density reduction: all nonzero source alpha and a filter margin are retained.
Candidates require rendered mip/coverage validation before replacing game assets.
"""
import bpy
import numpy as np


def convex_hull(points):
    points = sorted(set(map(tuple, points)))
    if len(points) < 3:
        return np.array(points, dtype=np.float64)
    def cross(o, a, b):
        return (a[0]-o[0])*(b[1]-o[1])-(a[1]-o[1])*(b[0]-o[0])
    low, high = [], []
    for p in points:
        while len(low)>1 and cross(low[-2],low[-1],p)<=0: low.pop()
        low.append(p)
    for p in reversed(points):
        while len(high)>1 and cross(high[-2],high[-1],p)<=0: high.pop()
        high.append(p)
    return np.array(low[:-1]+high[:-1], dtype=np.float64)


class AlphaTrimmer:
    def __init__(self, path, margin=8):
        image=bpy.data.images.load(str(path),check_existing=True)
        image.colorspace_settings.name='Non-Color'
        self.width,self.height=image.size[:]
        pixels=np.empty(self.width*self.height*4,np.float32)
        image.pixels.foreach_get(pixels)
        self.support=pixels.reshape(self.height,self.width,4)[:,:,0]>1/255
        self.scale=np.array((self.width,self.height),dtype=np.float64)
        self.margin=margin
        self.cache={}

    def polygon(self, uv):
        key=tuple(np.round(uv.ravel(),7))
        if key in self.cache:return self.cache[key]
        original=np.eye(3,dtype=np.float64)
        points=uv*self.scale
        # Repeat/wrapped UV islands need a tile-aware filter; preserve them here.
        if (points<0).any() or (points>self.scale).any():
            self.cache[key]=original;return original
        lo=np.maximum(0,np.floor(points.min(axis=0)-self.margin-1)).astype(int)
        hi=np.minimum(self.scale,np.ceil(points.max(axis=0)+self.margin+1)).astype(int)
        mask=self.support[lo[1]:hi[1],lo[0]:hi[0]].copy()
        # Exclude neighboring atlas islands outside this triangle's filtered
        # footprint; an axis-aligned UV box alone can include unrelated leaves.
        yy,xx=np.ogrid[lo[1]:hi[1],lo[0]:hi[0]]
        winding=np.sign(np.cross(points[1]-points[0],points[2]-points[0]))
        for i,a in enumerate(points):
            edge=points[(i+1)%3]-a
            distance=winding*(edge[0]*(yy+.5-a[1])-edge[1]*(xx+.5-a[0]))
            mask&=distance>=-(self.margin+1)*np.linalg.norm(edge)
        rows=np.flatnonzero(mask.any(axis=1)) if mask.size else []
        if len(rows)==0:
            self.cache[key]=[];return []
        # Row extrema yield the same convex hull as every occupied texel, with
        # substantially less CPU work. Expand by the full filter margin plus
        # half a texel so no source sample's pixel area is clipped.
        strip=mask[rows]
        left=strip.argmax(axis=1)+lo[0]+.5
        right=mask.shape[1]-1-strip[:,::-1].argmax(axis=1)+lo[0]+.5
        ys=rows+lo[1]+.5
        hull=convex_hull(np.concatenate((np.stack((left,ys),1),np.stack((right,ys),1))))
        pad=self.margin+.5
        hull=convex_hull([p+d for p in hull for d in ((-pad,-pad),(pad,-pad),(pad,pad),(-pad,pad))])
        polygon=[p for p in original]
        for i,a in enumerate(hull):
            b=hull[(i+1)%len(hull)];edge=b-a
            def distance(bary):
                q=bary@points-a
                return edge[0]*q[1]-edge[1]*q[0]
            clipped=[]
            for j,current in enumerate(polygon):
                previous=polygon[j-1];dc,dp=distance(current),distance(previous)
                inside_c,inside_p=dc>=-1.e-7,dp>=-1.e-7
                if inside_c!=inside_p:
                    clipped.append(previous+(current-previous)*(dp/(dp-dc)))
                if inside_c:clipped.append(current)
            polygon=clipped
            if len(polygon)<3:polygon=[];break
        # Too many tiny triangles can erase the raster benefit. Preserve the
        # original face when conservative clipping becomes geometrically costly.
        if len(polygon)>5:polygon=original
        if len(polygon)>=3:
            p=np.array(polygon)@points
            area=abs(np.sum(p[:,0]*np.roll(p[:,1],-1)-p[:,1]*np.roll(p[:,0],-1)))
            old=abs(np.cross(points[1]-points[0],points[2]-points[0]))
            if old<1.e-10 or area/old>.90 or (len(polygon)>3 and area/old>.70):polygon=original
        self.cache[key]=polygon
        return polygon

    def trim(self, obj):
        mesh=obj.data;mesh.calc_loop_triangles()
        coords=np.array([v.co[:] for v in mesh.vertices],dtype=np.float64)
        uv=np.array([v.uv[:] for v in mesh.uv_layers.active.data],dtype=np.float64)
        normals=np.array([n.vector[:] for n in mesh.corner_normals],dtype=np.float64)
        low,high=coords.min(axis=0),coords.max(axis=0)
        boundary=np.any(np.isclose(coords,low,rtol=0,atol=1.e-7)|np.isclose(coords,high,rtol=0,atol=1.e-7),axis=1)
        # Each tuft is rotated around Z during assembly. Preserve its horizontal
        # convex-hull vertices too, so any such rotation retains exact bounds.
        horizontal_hull=set(map(tuple,convex_hull(coords[:,:2])))
        boundary|=np.array([tuple(p[:2]) in horizontal_hull for p in coords])
        vertices=[];faces=[];loops_uv=[];loops_normal=[];materials=[];smooth=[];lookup={}
        before_area=after_area=0.;removed=clipped=protected=0
        for tri in mesh.loop_triangles:
            ids=np.array(tri.vertices);loopids=np.array(tri.loops)
            p=coords[ids];tex=uv[loopids];norm=normals[loopids]
            area=float(np.linalg.norm(np.cross(p[1]-p[0],p[2]-p[0]))*.5);before_area+=area
            if boundary[ids].any():
                polygon=np.eye(3);protected+=1
            else:polygon=self.polygon(tex)
            if len(polygon)<3:removed+=1;continue
            if len(polygon)!=3 or not np.allclose(polygon,np.eye(3),atol=1.e-7):clipped+=1
            for i in range(1,len(polygon)-1):
                bary=np.array((polygon[0],polygon[i],polygon[i+1]))
                xyz=bary@p;newuv=bary@tex;newnorm=bary@norm
                nextarea=float(np.linalg.norm(np.cross(xyz[1]-xyz[0],xyz[2]-xyz[0]))*.5)
                if nextarea<1.e-14:continue
                after_area+=nextarea;face=[]
                for vertex in xyz:
                    key=tuple(np.round(vertex,9))
                    if key not in lookup:lookup[key]=len(vertices);vertices.append(vertex)
                    face.append(lookup[key])
                faces.append(face);loops_uv.extend(newuv)
                newnorm/=np.maximum(np.linalg.norm(newnorm,axis=1,keepdims=True),1.e-12)
                loops_normal.extend(newnorm);materials.append(tri.material_index);smooth.append(mesh.polygons[tri.polygon_index].use_smooth)
        result=bpy.data.meshes.new(mesh.name+'_AlphaTrimCandidate')
        result.from_pydata(vertices,[],faces);result.update()
        for material in mesh.materials:result.materials.append(material)
        layer=result.uv_layers.new(name='UV0');layer.data.foreach_set('uv',np.asarray(loops_uv,np.float32).ravel())
        result.polygons.foreach_set('material_index',np.asarray(materials,np.int32))
        result.polygons.foreach_set('use_smooth',np.asarray(smooth,np.bool_))
        result.normals_split_custom_set(loops_normal)
        newcoords=np.array([v.co[:] for v in result.vertices])
        if not np.allclose(newcoords.min(axis=0),low,atol=1.e-7,rtol=0) or not np.allclose(newcoords.max(axis=0),high,atol=1.e-7,rtol=0):
            raise RuntimeError('Alpha trim changed source bounds: '+obj.name)
        record={'object':obj.name,'triangles_before':len(mesh.loop_triangles),'triangles_after':len(faces),
            'fully_transparent_triangles_removed':removed,'triangles_clipped':clipped,'bound_extrema_triangles_preserved':protected,
            'surface_area_before':before_area,'surface_area_after':after_area,'surface_area_reduction_percent':100*(1-after_area/before_area),
            'bounds_unchanged':True,'texture_margin_texels':self.margin,'alpha_nonzero_threshold':1/255}
        obj.data=result
        return record

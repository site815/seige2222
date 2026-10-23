// The same physical geography contract used by native routing and terrain.
export function validateEnvironment(e) {
  const fail=m=>{throw new Error(`Environment: ${m}`);};
  const n=(v,name,lo,hi)=>{if(!Number.isFinite(v)||v<lo||v>hi)fail(`${name} outside ${lo}..${hi}`);};
  if(!e||![1,2].includes(e.schema_version)||typeof e.enabled!=='boolean')fail('schema or enabled flag');
  for(const [key,lo,hi]of [['river_half_width',20,2000],['river_depth',1,200],['bank_width',50,3000],['bank_height',0,200]])n(e[key],key,lo,hi);
  if(e.schema_version===2){
    for(const [key,lo,hi]of [['valley_width',1000,18000],['base_elevation',-1500,1500],['north_slope',0,.02],['macro_amplitude',0,1000]])n(e[key],key,lo,hi);
    if(e.valley_width<=e.bank_width)fail('valley must extend beyond bank');
    if(!Array.isArray(e.additional_rivers)||e.additional_rivers.length>4)fail('invalid additional drainage rivers');
  }
  const rivers=[e.river,...(e.schema_version===2?e.additional_rivers:[])];
  for(const river of rivers){
    if(!Array.isArray(river)||river.length<2||river.length>64)fail('river requires 2..64 points');
    for(const [i,p]of river.entries()){
      n(p.x,'river x',-100000,100000);n(p.y,'river y',-100000,100000);n(p.height,'river height',-2000,2000);
      if(i&&(p.height>river[i-1].height||Math.hypot(p.x-river[i-1].x,p.y-river[i-1].y)<100))fail('river must descend through distinct points');
    }
  }
  if(!Array.isArray(e.lakes)||e.lakes.length>8||!Array.isArray(e.cliffs)||e.cliffs.length>16)fail('invalid landform arrays');
  const distance=(p,a,b)=>{const dx=b.x-a.x,dy=b.y-a.y,t=Math.max(0,Math.min(1,((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy)));return Math.hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);};
  for(const l of e.lakes){
    n(l.x,'lake x',-90000,90000);n(l.y,'lake y',-90000,90000);n(l.radius_x,'lake radius x',500,12000);n(l.radius_y,'lake radius y',500,12000);n(l.height,'lake height',-2000,2000);n(l.depth,'lake depth',1,500);
    if(e.schema_version===2){n(l.shore_variation,'lake shore variation',0,.3);n(l.shore_phase,'lake shore phase',-10,10);}
    if(!rivers.some(river=>river.some((p,i)=>i&&Math.abs(p.height-l.height)<.001&&Math.abs(river[i-1].height-l.height)<.001&&distance({x:l.x/l.radius_x,y:l.y/l.radius_y},{x:p.x/l.radius_x,y:p.y/l.radius_y},{x:river[i-1].x/l.radius_x,y:river[i-1].y/l.radius_y})<1-(l.shore_variation??0))))fail('lake must connect to a level river reach');
  }
  if(e.schema_version===2){
    if(!Array.isArray(e.fords)||e.fords.length>32)fail('invalid natural ford array');
    for(const f of e.fords){
      n(f.x,'ford x',-90000,90000);n(f.y,'ford y',-90000,90000);n(f.radius,'ford radius',500,2500);n(f.blend,'ford bank blend',200,1500);n(f.depth,'ford depth',.5,4);
      if(f.radius<e.river_half_width*2||!rivers.some(r=>r.some((p,i)=>i&&distance(f,r[i-1],p)<e.river_half_width*.2)))fail('ford must span a river');
      if(e.lakes.some(l=>Math.hypot((f.x-l.x)/l.radius_x,(f.y-l.y)/l.radius_y)<1+(f.radius+f.blend)/Math.min(l.radius_x,l.radius_y)+l.shore_variation))fail('ford must avoid lake basins');
    }
  }
  for(const c of e.cliffs){
    n(c.x,'cliff x',-90000,90000);n(c.y,'cliff y',-90000,90000);n(c.radius_x,'cliff radius x',500,15000);n(c.radius_y,'cliff radius y',500,15000);n(c.height,'cliff height',0,1200);n(c.edge_ratio,'cliff edge',.03,.6);
    if(e.schema_version===2){n(c.angle_degrees,'cliff orientation',-360,360);n(c.irregularity,'cliff irregularity',0,.3);}
  }
  return true;
}

// The same physical geography contract used by native routing and terrain.
export function validateEnvironment(e) {
  const fail=m=>{throw new Error(`Environment: ${m}`);};
  const n=(v,name,lo,hi)=>{if(!Number.isFinite(v)||v<lo||v>hi)fail(`${name} outside ${lo}..${hi}`);};
  if(!e||e.schema_version!==1||typeof e.enabled!=='boolean')fail('schema or enabled flag');
  for(const [key,lo,hi]of [['river_half_width',20,2000],['river_depth',1,200],['bank_width',50,3000],['bank_height',0,200]])n(e[key],key,lo,hi);
  if(!Array.isArray(e.river)||e.river.length<2||e.river.length>64)fail('river requires 2..64 points');
  for(const [i,p]of e.river.entries()){
    n(p.x,'river x',-100000,100000);n(p.y,'river y',-100000,100000);n(p.height,'river height',-2000,2000);
    if(i&&(p.height>e.river[i-1].height||Math.hypot(p.x-e.river[i-1].x,p.y-e.river[i-1].y)<100))fail('river must descend through distinct points');
  }
  if(!Array.isArray(e.lakes)||e.lakes.length>8||!Array.isArray(e.cliffs)||e.cliffs.length>16)fail('invalid landform arrays');
  const distance=(p,a,b)=>{const dx=b.x-a.x,dy=b.y-a.y,t=Math.max(0,Math.min(1,((p.x-a.x)*dx+(p.y-a.y)*dy)/(dx*dx+dy*dy)));return Math.hypot(p.x-a.x-t*dx,p.y-a.y-t*dy);};
  for(const l of e.lakes){
    n(l.x,'lake x',-90000,90000);n(l.y,'lake y',-90000,90000);n(l.radius_x,'lake radius x',500,12000);n(l.radius_y,'lake radius y',500,12000);n(l.height,'lake height',-2000,2000);n(l.depth,'lake depth',1,500);
    if(!e.river.some((p,i)=>i&&Math.abs(p.height-l.height)<.001&&Math.abs(e.river[i-1].height-l.height)<.001&&distance({x:l.x/l.radius_x,y:l.y/l.radius_y},{x:p.x/l.radius_x,y:p.y/l.radius_y},{x:e.river[i-1].x/l.radius_x,y:e.river[i-1].y/l.radius_y})<1))fail('lake must connect to a level river reach');
  }
  for(const c of e.cliffs){n(c.x,'cliff x',-90000,90000);n(c.y,'cliff y',-90000,90000);n(c.radius_x,'cliff radius x',500,15000);n(c.radius_y,'cliff radius y',500,15000);n(c.height,'cliff height',0,1200);n(c.edge_ratio,'cliff edge',.03,.6);}
  return true;
}

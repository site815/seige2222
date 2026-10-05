import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

export function validateCombatRules(directory) {
  const read=name=>JSON.parse(fs.readFileSync(path.join(directory,`${name}.json`),'utf8'));
  const errors=[];const check=(ok,message)=>{if(!ok)errors.push(message);};
  const resources=new Map(read('resources').resources.map(r=>[r.id,r]));
  const buildings=new Set(read('buildings').buildings.map(b=>b.id));
  const combat=read('combat'), wd=read('weapons'), cd=read('chassis'), fixedStep=read('policies').policies.fixed_step_seconds;
  for(const [name,doc] of [['combat',combat],['weapons',wd],['chassis',cd]])check(doc.schema_version===1,`${name}: schema_version must be 1`);
  const number=(o,key,min=0,max=1e12)=>check(Number.isFinite(o[key])&&o[key]>=min&&o[key]<=max,`${o.id??'combat'}: invalid ${key}`);
  const amounts=(o,key)=>{check(o[key]&&typeof o[key]==='object'&&!Array.isArray(o[key]),`${o.id}: ${key} must be an object`);for(const [id,n] of Object.entries(o[key]??{}))check(resources.has(id)&&Number.isFinite(n)&&n>=0&&(!resources.get(id)?.discrete||Number.isInteger(n)),`${o.id}: invalid cost ${id}`);};
  const weapons=new Map();
  check(wd.hardpoint_sizes&&Object.keys(wd.hardpoint_sizes).length===3,'three named hardpoint sizes required');
  for(const size of ['small','medium','large']){
    const h=wd.hardpoint_sizes?.[size]??{};
    for(const key of ['width_meters','height_meters','length_meters'])number(h,key,.001,1000);
    number(h,'mass_limit_kg',.001,1e9);check(Number.isInteger(h.mount_points)&&h.mount_points>=1&&h.mount_points<=256,`${size}: invalid hardpoint points`);
    const base=wd.hardpoint_sizes?.small??{},area=h.width_meters*h.height_meters*base.mount_points,pointArea=base.width_meters*base.height_meters*h.mount_points;
    check(Number.isFinite(area)&&Number.isFinite(pointArea)&&Math.abs(area-pointArea)<=1e-6*Math.max(area,pointArea),`${size}: hardpoint points must be proportional to frontal area`);
  }
  for(const family of ['energy','kinetic','missile','plasma'])for(const key of ['shield_multiplier','armor_multiplier'])number(combat.damage_profiles?.[family]??{},key,.05,10);
  for(const w of wd.weapons??[]){check(typeof w.id==='string'&&w.id.length>0&&!weapons.has(w.id),`duplicate/empty weapon ${w.id}`);weapons.set(w.id,w);const i=['small','medium','large'].indexOf(w.size);check(i>=0&&w.mount_points===wd.hardpoint_sizes?.[w.size]?.mount_points,`${w.id}: mount points must match its declared size`);check(['energy','kinetic','missile','plasma'].includes(w.family),`${w.id}: family`);number(w,'mass_kg',.001,wd.hardpoint_sizes?.[w.size]?.mass_limit_kg??0);number(w,'damage',.001,1e6);number(w,'reload_seconds',fixedStep,600);number(w,'range_meters',1,3000);number(w,'speed_m_s',0,10000);number(w,'accuracy_degrees',0,45);number(w,'splash_meters',0,100);number(w,'energy_kwh',0,1000);number(w,'ammo_per_shot',0,1000);number(w,'shield_multiplier',.05,10);number(w,'armor_multiplier',.05,10);number(w,'homing_degrees_s',0,720);amounts(w,'cost');check(w.ammo===''?w.ammo_per_shot===0:resources.has(w.ammo)&&w.ammo_per_shot>0,`${w.id}: ammunition reference`);check(w.family==='energy'?w.speed_m_s===0&&w.ammo==='':w.speed_m_s>0,`${w.id}: projectile type`);}
  const fits=(label,loadout,points,mass)=>{check(Array.isArray(loadout)&&loadout.length<=128,`${label}: invalid loadout`);let p=0,m=0;for(const id of loadout??[]){const w=weapons.get(id);check(!!w&&id!==combat.bug_weapon,`${label}: invalid weapon ${id}`);p+=w?.mount_points??0;m+=w?.mass_kg??0;}check(p<=points&&m<=mass,`${label}: area/mass overload`);};
  const chassis=new Map(),combinations=new Set();check(cd.chassis?.length===12,'exactly twelve chassis required');
  for(const c of cd.chassis??[]){const i=['small','medium','large','behemoth'].indexOf(c.size);check(typeof c.id==='string'&&!chassis.has(c.id),`duplicate chassis ${c.id}`);chassis.set(c.id,c);check(['wheeled','tracked','mech'].includes(c.family)&&i>=0&&!combinations.has(c.family+c.size),`${c.id}: family/size coverage`);combinations.add(c.family+c.size);check(Number.isInteger(c.capacity_points)&&c.capacity_points>=1&&c.capacity_points<=65536&&c.tier===i+1,`${c.id}: invalid capacity points or size tier`);for(const key of ['mass_kg','max_weapon_mass_kg','cargo_mass_kg','storage_litres','health','battery_kwh','speed_kmh','radius_meters','sensor_meters','max_grade','rough_speed_multiplier','travel_kwh_km','build_seconds','build_kwh'])number(c,key,.000001,1e7);number(c,'armor');number(c,'shield');number(c,'road_damage_hp_m',0,1000);check(Number.isInteger(c.mount_points)&&c.mount_points>=1&&c.mount_points<=128,`${c.id}: mount points`);check(c.family!=='wheeled'||(c.legs===0&&(i===0||c.wheels===[4,6,8,12][i])),`${c.id}: wheels`);check(c.family!=='mech'||(c.wheels===0&&(i<2||c.legs===[2,2,4,8][i])),`${c.id}: legs`);amounts(c,'cost');check(Object.entries(c.cost??{}).reduce((s,[id,n])=>s+n*(resources.get(id)?.unit_mass_kg??0),0)+1e-8>=c.mass_kg,`${c.id}: fabricated mass exceeds materials`);}
  check(Number.isInteger(combat.fleet_capacity)&&combat.fleet_capacity>=1&&combat.fleet_capacity<=65536,'fleet capacity must be a positive supported integer');
  for(const [key,min,max] of [['maximum_vehicles',1,256],['maximum_projectiles',1,10000],['maximum_queue',1,128],['service_meters',1,100],['charge_kw',.01,10000],['ammo_buffer_shots',1,1000],['enemy_radius_meters',.1,20],['shield_regen_hp_s',0,10000],['shield_regen_kwh_hp',.000001,100],['shield_delay_seconds',0,3600],['formation_spacing_meters',.1,100],['rough_grade',.001,1],['road_speed_multiplier',1,4],['bug_range_meters',1,1000],['bug_fire_interval',.1,600],['bug_ranged_fraction',0,1]])number(combat,key,min,max);
  for(const key of ['maximum_vehicles','maximum_projectiles','maximum_queue'])check(Number.isInteger(combat[key]),`${key}: integer required`);
  check(weapons.has(combat.bug_weapon),'unknown bug weapon');
  number(combat,'loot_range_meters',1,100);number(combat,'loot_kg_s',.01,10000);number(combat,'refit_energy_fraction',.001,1);
  check(Array.isArray(combat.loot_priority)&&combat.loot_priority.length===3&&new Set(combat.loot_priority).size===3&&combat.loot_priority.every(x=>['tier_desc','owned_quantity_asc','resource_id_asc'].includes(x)),'loot_priority must contain the three unique ordered criteria');
  for(const [id,p] of Object.entries(combat.building_platforms??{})){check(buildings.has(id),`unknown combat platform ${id}`);check(Number.isInteger(p.mount_points)&&p.mount_points>=0&&p.mount_points<=256,`${id}: mount points`);for(const key of ['max_weapon_mass_kg','shield_hp','armor_hp'])number(p,key,0,1e7);fits(id,p.loadout,p.mount_points,p.max_weapon_mass_kg);}
  for(const [id,n] of Object.entries(combat.core_fleet_limits??{}))check(buildings.has(id)&&Number.isInteger(n)&&n>=1&&n<=3,`${id}: fleet limit`);
  for(const [id,f] of Object.entries(combat.factories??{}))check(buildings.has(id)&&['all','wheeled','tracked','mech'].includes(f.family)&&Number.isInteger(f.maximum_tier)&&f.maximum_tier>=1&&f.maximum_tier<=4&&Number.isFinite(f.speed_multiplier)&&f.speed_multiplier>0&&f.speed_multiplier<=100,`${id}: factory capability`);
  let points=0;for(const v of combat.initial_vehicles??[]){const c=chassis.get(v.chassis);check(!!c,`unknown initial chassis ${v.chassis}`);if(c){fits(v.chassis,v.loadout,c.mount_points,c.max_weapon_mass_kg);check(Number.isFinite(v.battery_kwh)&&v.battery_kwh>=0&&v.battery_kwh<=c.battery_kwh,'initial battery');points+=c.capacity_points;}}
  check(points<=combat.fleet_capacity,'initial fleet overloaded');
  return errors;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const directory=path.resolve(process.argv[2]??'Rules');const errors=validateCombatRules(directory);
  if(errors.length){console.error(errors.join('\n'));process.exitCode=1;}else console.log('Combat configuration valid: 12 chassis, weapon area/mass, ammunition, platforms and fleet limits.');
}

import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
export function validateTransportVehicles(directory) {
  const read=n=>JSON.parse(fs.readFileSync(path.join(directory,`${n}.json`),'utf8'));
  const errors=[],check=(b,s)=>{if(!b)errors.push(s);},d=read('transport_vehicles'),m=read('mobility');
  const resources=new Map(read('resources').resources.map(r=>[r.id,r])),buildings=new Map(read('buildings').buildings.map(b=>[b.id,b]));
  check(d.schema_version===1&&m.schema_version===1,'Transport and mobility schemas must be 1');
  check(d.initial_count===1&&d.shuttle_vehicle_capacity===1,'Exactly one starter transport and one shuttle vehicle bay required');
  check(d.cargo_mass_kg===2000&&d.cargo_litres===3000,'Transport payload is 2000 kg / 3000 L');
  for(const k of ['body_mass_kg','health','battery_kwh','initial_battery_kwh','travel_kwh_per_km','charging_kw','handling_seconds_per_drone_load','build_seconds','build_energy_kwh','service_meters'])check(Number.isFinite(d[k])&&d[k]>0&&d[k]<=1e9,`Invalid transport ${k}`);
  check(d.initial_battery_kwh<=d.battery_kwh,'Initial battery exceeds capacity');
  check(Number.isFinite(d.minimum_departure_charge_fraction)&&d.minimum_departure_charge_fraction>=.01&&d.minimum_departure_charge_fraction<=1,'Invalid departure charge reserve');
  check(Number.isInteger(d.maximum_queue)&&d.maximum_queue>=1&&d.maximum_queue<=128,'Invalid transport queue cap');
  check(Array.isArray(d.body_size_meters)&&d.body_size_meters.length===3&&d.body_size_meters.every(n=>Number.isFinite(n)&&n>0&&n<=100),'Invalid body size');
  let mass=0;for(const [id,n] of Object.entries(d.build_cost??{})){const r=resources.get(id);check(r&&!r.discrete&&Number.isFinite(n)&&n>0,`Invalid transport material ${id}`);mass+=n*(r?.unit_mass_kg??0);}
  check(Math.abs(mass-d.body_mass_kg)<1e-6,'Transport body and material bill mass differ');
  check(Object.keys(d.factories??{}).length>0,'Transport needs a factory');
  for(const [id,speed] of Object.entries(d.factories??{}))check(buildings.get(id)?.role==='vehicle_factory'&&Number.isFinite(speed)&&speed>0&&speed<=10,`Invalid transport factory ${id}`);
  const profiles=['drone','transport','wheeled','tracked','mech'];check(Object.keys(m.profiles??{}).length===profiles.length,'Exactly five mobility profiles required');
  for(const id of profiles){const p=m.profiles?.[id]??{};for(const k of ['body_radius_meters','access_meters','speed_kmh','maximum_slope','rough_slope','rough_speed_multiplier','sample_meters'])check(Number.isFinite(p[k])&&p[k]>0,`${id}: invalid ${k}`);check(Number.isFinite(p.clearance_meters)&&p.clearance_meters>=0,`${id}: invalid clearance`);check(p.rough_slope<=p.maximum_slope&&p.rough_speed_multiplier<=1,`${id}: invalid rough-terrain policy`);for(const k of ['allow_easy','allow_rough','allow_water','allow_road'])check(typeof p[k]==='boolean',`${id}: missing ${k}`);}
  return errors;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const errors=validateTransportVehicles(path.resolve(process.argv[2]??'Rules'));
  if(errors.length){console.error(errors.join('\n'));process.exitCode=1;}else console.log('Autonomous transport, mass/volume, factory and five-family mobility rules valid.');
}

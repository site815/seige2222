import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {validateCombatRules} from './validate_combat.mjs';

const root=fileURLToPath(new URL('../',import.meta.url)),source=path.join(root,'Rules');
const directory=path.join(root,'Saved','Automation','CombatJSBudgetRules');
fs.mkdirSync(directory,{recursive:true});
for(const file of fs.readdirSync(source))if(file.endsWith('.json'))fs.copyFileSync(path.join(source,file),path.join(directory,file));
const read=name=>JSON.parse(fs.readFileSync(path.join(directory,`${name}.json`),'utf8'));
const write=(name,value)=>fs.writeFileSync(path.join(directory,`${name}.json`),JSON.stringify(value));
const weapons=read('weapons'),combat=read('combat'),chassis=read('chassis');
assert.equal(combat.fleet_capacity,50);
for(const [i,size] of ['small','medium','large'].entries()){
  const h=weapons.hardpoint_sizes[size];
  assert.equal(h.width_meters,.5*2**i);assert.equal(h.height_meters,h.width_meters);
  assert.equal(h.length_meters,2*2**i);assert.equal(h.mass_limit_kg,500*8**i);assert.equal(h.mount_points,4**i);
  h.width_meters*=1.2;h.height_meters*=1.2;h.length_meters*=1.25;h.mass_limit_kg*=1.2;h.mount_points*=2;
}
for(const w of weapons.weapons){w.mount_points*=2;if(w.id==='laser_small'){w.mass_kg=550;w.cost.alloy=545;}}
for(const c of chassis.chassis){assert.equal(c.capacity_points,2**(c.tier-1));c.capacity_points*=20;c.mount_points*=2;c.max_weapon_mass_kg*=2;}
combat.fleet_capacity=60;
for(const p of Object.values(combat.building_platforms)){p.mount_points*=2;p.max_weapon_mass_kg*=2;}
write('weapons',weapons);write('chassis',chassis);write('combat',combat);
assert.deepEqual(validateCombatRules(directory),[],'consistent edited dimensions, mass, point units and fleet budgets must load');
const reject=(name,change,pattern)=>{const doc=structuredClone({weapons,combat,chassis}[name]);change(doc);write(name,doc);assert.ok(validateCombatRules(directory).some(error=>pattern.test(error)),pattern);write(name,{weapons,combat,chassis}[name]);};
reject('weapons',d=>d.hardpoint_sizes.medium.mount_points=9,/frontal area/);
reject('weapons',d=>d.weapons.find(w=>w.id==='laser_small').mass_kg=601,/mass_kg/);
reject('weapons',d=>d.hardpoint_sizes.small.width_meters=0,/width_meters/);
reject('combat',d=>d.fleet_capacity=0,/fleet capacity/);
reject('chassis',d=>d.chassis[0].capacity_points=1.5,/capacity points/);
console.log('Combat editable-budget validation: authored defaults, consistent edited catalog, 5 invalid cases passed.');

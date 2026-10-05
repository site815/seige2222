// Idempotent catalog migration for tiered facilities, replication and stored workers.
// All numeric balance below is provisional; runtime reads the resulting external JSON.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
const read=f=>JSON.parse(fs.readFileSync(path.join(root,f),'utf8').replace(/^\uFEFF/,''));
const write=(f,v)=>fs.writeFileSync(path.join(root,f),JSON.stringify(v,null,2)+'\n');
const clone=v=>structuredClone(v);
const resources=read('Rules/resources.json');
for(const r of resources.resources)r.discrete=r.id==='stored_workers';
const putResource=r=>{const i=resources.resources.findIndex(v=>v.id===r.id);if(i<0)resources.resources.push(r);else resources.resources[i]=r;};
for(const [id,name,tier,unit,mass,litres,color,discrete] of [
 ['shells','Kinetic ammunition',2,'kg',1,.2,[.58,.48,.32],false],
 ['missiles','Missile ammunition',2,'kg',1,1,[.6,.65,.68],false],
 ['stored_workers','Inactive workers',2,'workers',1,50,[.64,.72,.81],true],
])putResource({id,name,class:'manufactured',unit,unit_mass_kg:mass,litres_per_unit:litres,tier,color,stockpile_visual:'crates',discrete});
resources.quantity_note='Raw/product quantities are kg or L. Stored workers are indivisible counts with external mass and berth volume. The current 1kg worker material bill and 50 L occupied berth are provisional accounting values, not finished robot engineering. Grid energy and credits remain separate ledgers.';
write('Rules/resources.json',resources);

const recipes=read('Rules/recipes.json');
for(const r of recipes.recipes)r.worker_output=r.id==='assemble_robot'?1:0;
for(const r of [
 {id:'make_shells',seconds:6,inputs:{alloy:4,components:1,fuel:1.25},outputs:{shells:6},energy_kwh:.6,worker_output:0},
 {id:'make_missiles',seconds:12,inputs:{alloy:4,conductors:1,circuits:1,fuel:5},outputs:{missiles:10},energy_kwh:1.2,worker_output:0},
]){const i=recipes.recipes.findIndex(v=>v.id===r.id);if(i<0)recipes.recipes.push(r);else recipes.recipes[i]=r;}
recipes.balance_status='Provisional material-balanced gameplay transactions. Worker_output creates counted stored_workers using the same explicit material/energy ledger; it is not free population. Core replication and dedicated factory multipliers are building data. Combat chassis assembly costs are in the combat catalogs.';
write('Rules/recipes.json',recipes);

const buildings=read('Rules/buildings.json'),by=new Map(buildings.buildings.map(v=>[v.id,v]));
const depot=clone(by.get('depot')),factory=clone(by.get('alloy_refinery')),turret=clone(by.get('turret'));
for(const b of by.values()){
 b.level=/_3$/.test(b.id)?3:/_2$/.test(b.id)?2:1;
 b.family=b.id.replace(/_[23]$/,'');
 b.allowed_recipes=[];b.recipe_time_multiplier=1;b.recipe_energy_multiplier=1;b.recipe_input_multiplier=1;
 b.workforce_mode=b.role==='core'?'proportional':'full_staff';
 b.stores_inactive_workers=['core','service','storage','worker_factory'].includes(b.role);
}
const put=(id,base,fields)=>{const value={...clone(base),id,...fields};by.set(id,value);return value;};
const tierFamily=(id,base,make)=>{
 for(let level=1;level<=3;level++){
  const key=level===1?id:`${id}_${level}`;
  put(key,base,{family:id,level,next_upgrade:level<3?`${id}_${level+1}`:'',upgrade_cost:level<3?{alloy:24*level,circuits:6*level,components:3*level}:{},...make(level)});
 }
};
const core=clone(by.get('command_core'));
tierFamily('command_core',core,level=>({
 visual:level===1?'shuttle':'core',workforce_mode:'proportional',
 name:level===1?'Orbital command shuttle · Level 1':`Command center · Level ${level}`,
 description:'Parked orbital shuttle and expandable command center: fusion power, storage, slow universal replication and four large laser hardpoints. Replication requires actual materials and energy.',
 footprint:240*level,reserved_footprint:720,jobs:4+2*level,health:1800*2**(level-1),storage_capacity:[40000,60000,80000][level-1],robot_support_capacity:[8,24,48][level-1],
 construction_seconds:[900,3600,7200][level-1],construction_workers:[6,6,8][level-1],
 upgrade_cost:level<3?{alloy:80*level,circuits:20*level,components:12*level,fusion_reactors:4*level}:{},
 allowed_recipes:['assemble_robot',...recipes.recipes.filter(r=>r.id!=='assemble_robot').map(r=>r.id)],
 recipe_time_multiplier:10,recipe_energy_multiplier:4,recipe_input_multiplier:1,stores_inactive_workers:true,
}));
tierFamily('solar_array',clone(by.get('solar_array')),level=>({name:`Solar array · Level ${level}`,description:'Same-footprint solar installation; upgraded panels increase passive output on the connected road grid.',health:250*level,construction_seconds:600*level,footprint:210,reserved_footprint:210}));
for(const [id,label] of [['vehicle_factory','Wheeled vehicle factory'],['tank_factory','Tracked vehicle factory'],['mech_factory','Mech factory']]){
 const base={...clone(factory),role:'vehicle_factory',category:'defense',recipe:'',jobs:2,cost:{alloy:35,circuits:10,components:5},footprint:260,reserved_footprint:260,storage_capacity:1200,allowed_recipes:[],stores_inactive_workers:false};
 tierFamily(id,base,level=>({name:`${label} · Level ${level}`,description:'Builds physically paid configurable combat chassis. Higher levels support larger chassis and faster assembly; fleet and loadout rules remain external.',health:450*level,jobs:1+level,storage_capacity:[2000,6000,40000][level-1],construction_seconds:1800*level}));
}
put('worker_factory',factory,{family:'worker_factory',level:1,name:'Worker factory',category:'logistics',role:'worker_factory',recipe:'',cost:{alloy:20,circuits:6,components:3},jobs:2,health:300,footprint:210,reserved_footprint:210,storage_capacity:300,construction_seconds:1200,construction_workers:2,allowed_recipes:['assemble_robot'],recipe_time_multiplier:.5,recipe_energy_multiplier:.5,recipe_input_multiplier:1,stores_inactive_workers:true,next_upgrade:'',upgrade_cost:{},description:'Rapid, energy-efficient worker assembly from real materials. Inactive workers occupy physical storage and can be reactivated, recycled or traded.'});
put('ammunition_works',factory,{family:'ammunition_works',level:1,name:'Ammunition works',recipe:'make_shells',allowed_recipes:['make_shells','make_missiles'],cost:{alloy:24,circuits:7,components:3},storage_capacity:240,description:'Selectable kinetic or missile ammunition batches consume physical inputs and transaction energy.',next_upgrade:'',upgrade_cost:{}});
for(const [id,name,weapon] of [['turret','Laser tower','Laser'],['kinetic_tower','Kinetic tower','Kinetic cannon'],['missile_tower','Missile tower','Missile launcher'],['plasma_tower','Plasma tower','Plasma projector']]){
 tierFamily(id,turret,level=>({name:`${name} · Level ${level}`,description:'Automatic defense on one fixed-size plot; weapon size and layered defenses are defined by the combat catalog. Staffing, power and ammunition where required must be available.',weapon_name:weapon,storage_capacity:[200,500,1600][level-1],health:300*level,footprint:turret.footprint,reserved_footprint:turret.footprint,jobs:2,construction_seconds:900*level}));
}
const wall={...clone(depot),role:'wall',visual:'wall',category:'defense',jobs:0,footprint:50,reserved_footprint:50,storage_capacity:100,recipe:'',allowed_recipes:[],stores_inactive_workers:false,cost:{alloy:6},weapon_name:'',damage_per_shot:0,reload_seconds:0,attack_range:0};
tierFamily('wall_segment',wall,level=>({name:`Wall segment · Level ${level}`,description:'A structural 6 m wall segment with a player-selected inside and outside. Remains a physical obstruction without operating power.',health:600*level,cost:{alloy:6*level},construction_seconds:120*level,construction_workers:2,upgrade_cost:level<3?{alloy:6*(level+1)}:{}}));
const menu=buildings.build_menu.filter(id=>!/_2$|_3$/.test(id)&&!id.startsWith('wall_segment'));
for(const id of ['worker_factory','ammunition_works','vehicle_factory','tank_factory','mech_factory','kinetic_tower','missile_tower','plasma_tower'])if(!menu.includes(id))menu.push(id);
for(const b of by.values()){
 b.allowed_recipes??=[];b.recipe_time_multiplier??=1;b.recipe_energy_multiplier??=1;b.recipe_input_multiplier??=1;
 b.stores_inactive_workers??=['core','service','storage','worker_factory'].includes(b.role);
}
write('Rules/buildings.json',{...buildings,balance_status:'Provisional costs, durations, staffing, capacities and health. Confirmed tier geometry and hardpoint constraints are validated separately. Upgrade bills are incremental and stored on the source level.',buildings:[...by.values()],build_menu:menu});

const energy=read('Rules/energy.json');
const baseEnergy=id=>clone(energy.energy.buildings[id]??energy.energy.buildings.depot);
for(const b of by.values())if(!energy.energy.buildings[b.id])energy.energy.buildings[b.id]=baseEnergy(b.family==='command_core'?'command_core':b.family==='solar_array'?'solar_array':'depot');
for(const b of by.values()){
 const e=energy.energy.buildings[b.id];
 if(b.role==='core')Object.assign(e,{generation_kw:30*2**(b.level-1),battery_capacity_kwh:30*2**(b.level-1),initial_battery_kwh:b.level===1?20:0,idle_kw:2*b.level,self_start:true,requires_road_grid:false});
 else if(b.family==='solar_array')Object.assign(e,{generation_kw:25*2**(b.level-1),idle_kw:0,self_start:true,requires_road_grid:true});
 else if(['vehicle_factory','worker_factory'].includes(b.role)||b.id==='ammunition_works')Object.assign(e,{generation_kw:0,idle_kw:b.role==='vehicle_factory'?3*b.level:2,self_start:false,requires_road_grid:true});
 else if(b.role==='wall')Object.assign(e,{generation_kw:0,idle_kw:.02*b.level,self_start:false,requires_road_grid:true});
 else if(b.role==='defense')Object.assign(e,{generation_kw:0,idle_kw:1*b.level,self_start:false,requires_road_grid:true});
}
write('Rules/energy.json',energy);
const policies=read('Rules/policies.json');
Object.assign(policies.policies,{surplus_policy:'store_inactive',inactive_worker_resource:'stored_workers',inactive_worker_litres:50,worker_store_seconds:5,worker_reactivate_seconds:5,worker_disassemble_seconds:10,worker_disassembly_kwh:1,worker_disassembly_outputs:{components:.5,circuits:.5},auto_disassemble_parts_shortage:true,auto_disassemble_storage_full:true,default_inactive_worker_target:0,default_port_worker_reserve_target:0});
write('Rules/policies.json',policies);
const trade=read('Rules/trade.json');
for(const id of ['shells','missiles'])trade.trade.prices[id]={buy_credits:.001,sell_credits:.0005};
trade.trade.prices.stored_workers={buy_credits:.0015,sell_credits:.0007};
write('Rules/trade.json',trade);
const transport=read('Rules/transport.json');Object.assign(transport.transport,{repair_resource:'alloy',repair_service_range_meters:12,repair_health_per_second:20,repair_health_per_unit:100,repair_energy_kwh_per_health:.0001,repair_buffer_units:2});for(const t of transport.transport.tiers)t.health_per_meter={road:100,road_rail:150,road_rail_vacuum:200}[t.id];write('Rules/transport.json',transport);

const ui=read('Interface/ui.json');
const addEntry=(group,definition,shortcut)=>{const g=ui.build_groups.find(v=>v.id===group);if(!g.entries.some(v=>v.definition===definition))g.entries.push({definition,shortcut});};
addEntry('production','ammunition_works','M');addEntry('logistics','worker_factory','F');addEntry('logistics','wall','W');
for(const [id,key] of [['kinetic_tower','K'],['missile_tower','M'],['plasma_tower','P'],['vehicle_factory','V'],['tank_factory','G'],['mech_factory','H']])addEntry('defense',id,key);
ui.build_groups.find(g=>g.id==='defense').description='Automatic laser, kinetic, missile and plasma defenses; configurable wheeled, tracked and mech production.';
ui.build_groups.find(g=>g.id==='logistics').description='Road grids, cargo, workers and trade. Wall: click joints, E flips inside/outside, Enter commits, Backspace undoes; Delete removes a selected segment.';
write('Interface/ui.json',ui);
const ai=read('AIFILES/colony_ai.json');if(!ai.build_targets.some(v=>v.definition==='worker_factory')){const at=ai.build_targets.findIndex(v=>v.definition==='trading_port');ai.build_targets.splice(at+1,0,{definition:'worker_factory',count:1});}write('AIFILES/colony_ai.json',ai);
console.log(JSON.stringify({resources:resources.resources.length,recipes:recipes.recipes.length,buildings:by.size,blueprints:menu.length,groups:ui.build_groups.map(g=>({id:g.id,entries:g.entries.length}))},null,2));

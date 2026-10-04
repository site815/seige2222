#!/usr/bin/env node
// Run with: node Tools/validate_rules.mjs [Rules-directory] [--self-test]
// Validation is independent of Unreal; it never alters runtime rules.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const defaultRules = fileURLToPath(new URL('../Rules/', import.meta.url));
const names = ['resources', 'recipes', 'buildings', 'policies', 'scenario'];
const positive = ['fixed_step_seconds','dispatch_interval','courier_speed','courier_capacity','courier_min_batch','delivery_buffer_cycles','repair_health_per_unit','robot_retire_seconds','upkeep_interval','wave_interval','spawn_radius','roam_interval','enemy_health','enemy_speed','extractor_snap_distance'];
const nonnegative = ['repair_buffer_units','repair_health_per_second','minimum_build_spacing','population_buffer_robots','upkeep_per_robot','upkeep_buffer_intervals','upkeep_shortage_efficiency','objective_produced_amount','objective_survival_seconds','wave_first_time','wave_per_building','wave_per_population','wave_escalation_per_wave','roam_first_time','enemy_attack_range','enemy_damage_per_second','enemy_courier_attack_range','shuttle_capacity'];
const integerPolicies = ['max_couriers','minimum_population','objective_building_count','wave_base_count','wave_max_count','roam_count','event_history_limit','placement_requires_visibility'];
const selectors = {
  population_policy:['fill_open_jobs'], workforce_mode:['full_staff','proportional'], surplus_policy:['retire_without_refund'],
  logistics_policy:['local_delivery'], enemy_target_policy:['nearest_building'], extraction_limit_policy:['one_extractor_per_node'],
  repair_policy:['local_materials'], objective_policy:['survive_and_manufacture'], shuttle_policy:['preloaded_cargo_only'],
  storage_policy:['overflow_only'], rule_time_basis:['simulation_seconds'],
};
export function readRules(directory) {
  return Object.fromEntries(names.map(n => [n, JSON.parse(fs.readFileSync(path.join(directory, `${n}.json`), 'utf8').replace(/^\uFEFF/, ''))]));
}
export function validateRules(d) {
  const fail = text => { throw new Error(text); };
  const object = (v,n) => { if (!v || typeof v !== 'object' || Array.isArray(v)) fail(`${n} must be an object`); return v; };
  const array = (v,n) => { if (!Array.isArray(v)) fail(`${n} must be an array`); return v; };
  const str = (v,n,empty=false) => { if (typeof v !== 'string' || (!empty && !v.length)) fail(`${n} must be a ${empty?'':'nonempty '}string`); return v; };
  const num = (v,n,min=0,integer=false,exclusive=false) => { if (!Number.isFinite(v) || v < min || (exclusive && v === min) || (integer && (!Number.isSafeInteger(v) || v > 2147483647))) fail(`${n} has an invalid number`); return v; };
  const position = (v,n) => { array(v,n); if (v.length !== 2 || v.some(x => !Number.isFinite(x))) fail(`${n} requires two finite coordinates`); };
  const color = (v,n) => { array(v,n); if (v.length !== 3 || v.some(x => !Number.isFinite(x) || x<0 || x>1)) fail(`${n} requires three color channels in [0,1]`); };
  const sum = v => Object.values(v).reduce((a,b)=>a+b,0);
  const catalog = (rows,n) => { array(rows,n); const result = new Map(); for (const row of rows) { object(row,n); str(row.id,`${n}.id`); if(result.has(row.id)) fail(`Duplicate ${n} ID ${row.id}`); result.set(row.id,row); } return result; };
  for (const n of names) { object(d[n],n); str(d[n].version,`${n}.version`); if(d[n].version !== d.resources.version) fail('Rule file versions differ'); }
  const resources = catalog(d.resources.resources,'resources');
  if (!resources.size) fail('Resource catalog cannot be empty');
  for (const r of resources.values()) { str(r.name,`${r.id}.name`); num(r.tier,`${r.id}.tier`,0,true); color(r.color,`${r.id}.color`); }
  const amounts = (v,n) => { object(v,n); for(const [id,a] of Object.entries(v)) { if(!resources.has(id)) fail(`${n} references unknown resource ${id}`); num(a,`${n}.${id}`); } };
  const recipes = catalog(d.recipes.recipes,'recipes');
  for (const r of recipes.values()) { num(r.seconds,`${r.id}.seconds`,0,false,true); amounts(r.inputs,`${r.id}.inputs`); amounts(r.outputs,`${r.id}.outputs`); if(sum(r.inputs)<=0) fail(`${r.id} needs positive inputs`); }
  const buildings = catalog(d.buildings.buildings,'buildings');
  const roles = ['core','extractor','processor','storage','sensor','defense'];
  for (const b of buildings.values()) {
    for(const k of ['name','category','role','description','visual']) str(b[k],`${b.id}.${k}`);
    for(const k of ['recipe','extract_resource']) str(b[k],`${b.id}.${k}`,true);
    if(!roles.includes(b.role)) fail(`Unsupported building role ${b.role}`);
    for(const k of ['health','footprint','storage_capacity']) num(b[k],`${b.id}.${k}`,0,false,true);
    for(const k of ['sensor_range','attack_range','damage_per_second','extract_rate']) num(b[k],`${b.id}.${k}`);
    num(b.jobs,`${b.id}.jobs`,0,true); color(b.color,`${b.id}.color`); amounts(b.cost,`${b.id}.cost`);
    if(b.recipe && !recipes.has(b.recipe)) fail(`${b.id} references unknown recipe`);
    if(b.extract_resource && !resources.has(b.extract_resource)) fail(`${b.id} references unknown extraction resource`);
    if((b.role==='extractor') !== !!(b.extract_resource && b.extract_rate>0) || (b.role==='processor') !== !!b.recipe) fail(`${b.id} role and capability disagree`);
    if(b.recipe) { const r=recipes.get(b.recipe); if(sum(r.inputs)>b.storage_capacity || sum(r.outputs)>b.storage_capacity || sum(r.outputs)<=0) fail(`${b.id} recipe does not fit storage`); }
  }
  const menu = array(d.buildings.build_menu,'build_menu');
  if(new Set(menu).size !== menu.length || menu.some(id=>!buildings.has(id) || buildings.get(id).role==='core')) fail('Build menu has duplicate, missing or core entries');
  if([...buildings.values()].filter(b=>b.role==='core').length !== 1) fail('Exactly one core definition is required');
  const p=object(d.policies.policies,'policies'); const s=object(d.scenario.scenario,'scenario');
  for(const k of positive) num(p[k],k,0,false,true);
  for(const k of nonnegative) num(p[k],k);
  for(const k of integerPolicies) num(p[k],k,0,true);
  if(p.max_couriers<1 || p.event_history_limit<1 || p.wave_max_count<p.wave_base_count || p.placement_requires_visibility>1 || p.upkeep_shortage_efficiency>1 || p.courier_min_batch>p.courier_capacity || p.fixed_step_seconds>p.dispatch_interval) fail('Inconsistent policy ranges');
  for(const [k,allowed] of Object.entries(selectors)) if(!allowed.includes(p[k])) fail(`Unsupported ${k}: ${p[k]}`);
  for(const k of ['upkeep_resource','repair_resource','objective_resource']) if(!resources.has(p[k])) fail(`Unknown resource in ${k}`);
  if(!recipes.has(p.population_recipe) || Object.keys(recipes.get(p.population_recipe).outputs).length) fail('Population recipe requires no physical outputs');
  if(!buildings.has(p.objective_building)) fail('Objective building does not exist');
  amounts(p.core_reserves,'core_reserves');
  str(s.title,'scenario.title'); num(s.world_half_size,'world_half_size',0,false,true); num(s.random_seed,'random_seed',0,true); num(s.starting_population,'starting_population',0,true);
  if(!buildings.has(s.core_definition) || buildings.get(s.core_definition).role!=='core') fail('Scenario core is invalid');
  const core=buildings.get(s.core_definition);
  position(s.core_position,'core_position'); amounts(s.starting_inventory,'starting_inventory'); amounts(s.starting_shuttle_cargo,'starting_shuttle_cargo');
  if(p.spawn_radius>s.world_half_size || s.core_position.some(x=>Math.abs(x)+core.footprint>s.world_half_size) || s.starting_population<p.minimum_population || sum(s.starting_inventory)>core.storage_capacity || sum(s.starting_shuttle_cargo)>p.shuttle_capacity) fail('Scenario spawn, capacity or population bounds invalid');
  for(const [i,n] of array(s.deposits,'deposits').entries()) { object(n,`deposits[${i}]`); if(!resources.has(n.resource)) fail('Unknown deposit resource'); position(n.position,'deposit.position'); if(n.position.some(x=>Math.abs(x)>s.world_half_size)) fail('Deposit outside sector'); }
  // Reachability ignores initial stock: recurring robot inputs, upkeep and repairs must be renewable.
  const available=new Set();
  for(const n of s.deposits) if(menu.some(id=>buildings.get(id).extract_resource===n.resource)) available.add(n.resource);
  const usableRecipes=[...buildings.values()].filter(b=>menu.includes(b.id) && b.recipe).map(b=>recipes.get(b.recipe));
  for(let old=-1;old!==available.size;) { old=available.size; for(const r of usableRecipes) if(Object.keys(r.inputs).every(id=>available.has(id))) for(const id of Object.keys(r.outputs)) available.add(id); }
  for(const id of [...Object.keys(recipes.get(p.population_recipe).inputs),p.upkeep_resource,p.repair_resource,p.objective_resource]) if(!available.has(id)) fail(`No renewable recipe path to ${id}`);
  // Finite bootstrap supplies must cover each selected building once, plus another population's robots.
  const bootstrap={};
  for(const id of menu) for(const [r,n] of Object.entries(buildings.get(id).cost)) bootstrap[r]=(bootstrap[r]??0)+n;
  const newJobs=[...buildings.values()].filter(b=>menu.includes(b.id)).reduce((n,b)=>n+b.jobs,0);
  for(const [r,n] of Object.entries(recipes.get(p.population_recipe).inputs)) bootstrap[r]=(bootstrap[r]??0)+n*newJobs;
  for(const [r,n] of Object.entries(bootstrap)) if((s.starting_inventory[r]??0)<n) fail(`Starter stock cannot bootstrap the selected building set and workers: ${r} needs ${n}`);
  // Buffer allocations cannot create an unavoidable storage deadlock.
  for(const b of buildings.values()) {
    let reserve=p.repair_buffer_units;
    if(b.recipe) reserve+=sum(recipes.get(b.recipe).inputs)*p.delivery_buffer_cycles;
    if(b.role==='core') reserve+=sum(p.core_reserves)+sum(recipes.get(p.population_recipe).inputs)*p.population_buffer_robots+s.starting_population*p.upkeep_per_robot*p.upkeep_buffer_intervals;
    if(reserve>b.storage_capacity) fail(`${b.id} storage is smaller than its demand buffers`);
  }
  return {version:d.resources.version,resources:resources.size,recipes:recipes.size,buildings:buildings.size,renewable:[...available].sort(),bootstrap};
}
function selfTest(source) {
  const cases=[
    ['missing numeric policy', d=>delete d.policies.policies.courier_speed],
    ['negative transport capacity', d=>d.policies.policies.courier_capacity=-1],
    ['fractional job count',d=>d.buildings.buildings[1].jobs=.5],
    ['unknown recipe input',d=>d.recipes.recipes[0].inputs.unknown=1],
    ['duplicate resource ID',d=>d.resources.resources.push({...d.resources.resources[0]})],
    ['unsupported policy',d=>d.policies.policies.population_policy='manual_targets'],
    ['inconsistent versions',d=>d.scenario.version='incompatible'],
    ['multiple cores',d=>d.buildings.buildings.push({...d.buildings.buildings[0],id:'extra_core'})],
    ['unreachable recurring input',d=>d.scenario.scenario.deposits=d.scenario.scenario.deposits.filter(n=>n.resource!=='carbon')],
    ['insufficient startup stock',d=>d.scenario.scenario.starting_inventory={}],
    ['overfilled shuttle',d=>d.scenario.scenario.starting_shuttle_cargo={components:9999}],
    ['out-of-bounds deposit',d=>d.scenario.scenario.deposits[0].position=[999999,0]],
    ['invalid color',d=>d.resources.resources[0].color=[2,0,0]],
    ['zero recipe duration',d=>d.recipes.recipes[0].seconds=0],
    ['storage deadlock',d=>d.buildings.buildings[1].storage_capacity=1],
  ];
  for(const [name,mutate] of cases) { const d=structuredClone(source); mutate(d); let rejected=false; try {validateRules(d);} catch {rejected=true;} if(!rejected) throw new Error(`Negative test was accepted: ${name}`); }
  return cases.length;
}
if (process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  try {
    const dir=process.argv.slice(2).find(x=>!x.startsWith('--')) ?? defaultRules;
    const d=readRules(path.resolve(dir)); const result=validateRules(d);
    console.log(`Rules valid: ${result.version}; ${result.resources} items, ${result.recipes} recipes, ${result.buildings} buildings.`);
    console.log(`Renewable paths verified for ${result.renewable.length} items; startup building/worker affordability verified.`);
    if(process.argv.includes('--self-test')) console.log(`${selfTest(d)} negative validation cases passed.`);
  } catch(error) { console.error(`RULE VALIDATION FAILED: ${error.message}`); process.exitCode=1; }
}

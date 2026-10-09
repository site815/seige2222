#!/usr/bin/env node
// Run with: node Tools/validate_rules.mjs [Rules-directory] [--self-test]
// Validation is independent of Unreal; it never alters runtime rules.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { validateCombatRules } from './validate_combat.mjs';
import { validateEnvironment } from './validate_environment.mjs';

const defaultRules = fileURLToPath(new URL('../Rules/', import.meta.url));
const names = ['resources', 'recipes', 'buildings', 'policies', 'scenario', 'transport', 'energy', 'trade', 'companions', 'walls', 'calendar', 'workers'];
const positive = ['fixed_step_seconds','dispatch_interval','courier_capacity','courier_min_batch','delivery_buffer_cycles','repair_health_per_unit','robot_retire_seconds','upkeep_interval','wave_interval','spawn_radius','roam_interval','enemy_health','enemy_speed','extractor_snap_distance'];
const nonnegative = ['repair_buffer_units','repair_health_per_second','minimum_build_spacing','population_buffer_robots','upkeep_per_robot','upkeep_buffer_intervals','upkeep_shortage_efficiency','objective_produced_amount','objective_survival_seconds','wave_first_time','wave_per_building','wave_per_population','wave_escalation_per_wave','roam_first_time','enemy_attack_range','enemy_damage_per_second','enemy_courier_attack_range','shuttle_capacity'];
const integerPolicies = ['max_couriers','minimum_population','objective_building_count','wave_base_count','wave_max_count','roam_count','event_history_limit','placement_requires_visibility'];
const deliveryClasses = ['fuel','maintenance','repair','defense','construction','production','trade','reserve','storage'];
const selectors = {
  population_policy:['fill_open_jobs'], workforce_mode:['full_staff','proportional'], surplus_policy:['store_inactive'],
  logistics_policy:['local_delivery'], enemy_target_policy:['nearest_building'], extraction_limit_policy:['one_extractor_per_node'],
  repair_policy:['local_materials'], objective_policy:['survive_and_manufacture'], shuttle_policy:['preloaded_cargo_only'],
  storage_policy:['overflow_only'], rule_time_basis:['simulation_seconds'],
  construction_policy:['phased_physical_delivery'], robot_support_policy:['local_capacity_and_maintenance'],
  construction_source_policy:['surplus_then_largest_load'],
};
export function readRules(directory) {
  return {ruleDirectory:directory,environment:JSON.parse(fs.readFileSync(path.join(directory,'environment.json'),'utf8').replace(/^\uFEFF/,'')),...Object.fromEntries(names.map(n => [n, JSON.parse(fs.readFileSync(path.join(directory, `${n}.json`), 'utf8').replace(/^\uFEFF/, ''))]))};
}
export function validateRules(d) {
  validateEnvironment(d.environment);
  const fail = text => { throw new Error(text); };
  if(!d.ruleDirectory)fail('Rules directory is required for independent combat catalogs');
  const combatErrors=validateCombatRules(d.ruleDirectory);if(combatErrors.length)fail(`Combat catalogs: ${combatErrors.join('; ')}`);
  const object = (v,n) => { if (!v || typeof v !== 'object' || Array.isArray(v)) fail(`${n} must be an object`); return v; };
  const array = (v,n) => { if (!Array.isArray(v)) fail(`${n} must be an array`); return v; };
  const str = (v,n,empty=false) => { if (typeof v !== 'string' || (!empty && !v.length)) fail(`${n} must be a ${empty?'':'nonempty '}string`); return v; };
  const num = (v,n,min=0,integer=false,exclusive=false) => { if (!Number.isFinite(v) || v < min || (exclusive && v === min) || (integer && (!Number.isSafeInteger(v) || v > 2147483647))) fail(`${n} has an invalid number`); return v; };
  const position = (v,n) => { array(v,n); if (v.length !== 2 || v.some(x => !Number.isFinite(x))) fail(`${n} requires two finite coordinates`); };
  const color = (v,n) => { array(v,n); if (v.length !== 3 || v.some(x => !Number.isFinite(x) || x<0 || x>1)) fail(`${n} requires three color channels in [0,1]`); };
  const sum = v => Object.values(v).reduce((a,b)=>a+b,0);
  const catalog = (rows,n) => { array(rows,n); const result = new Map(); for (const row of rows) { object(row,n); str(row.id,`${n}.id`); if(result.has(row.id)) fail(`Duplicate ${n} ID ${row.id}`); result.set(row.id,row); } return result; };
  for (const n of names) { object(d[n],n); str(d[n].version,`${n}.version`); if(d[n].version !== d.resources.version) fail('Rule file versions differ'); }
  const calendar=object(d.calendar.calendar,'calendar');
  for(const key of ['daylight_seconds','night_seconds']){num(calendar[key],`calendar.${key}`,1);if(calendar[key]>86400||Math.abs(calendar[key]*1e6-Math.round(calendar[key]*1e6))>.001)fail('Calendar duration is outside supported microsecond bounds');}
  num(calendar.days_per_season,'calendar.days_per_season',1,true);if(calendar.days_per_season>366)fail('Calendar season exceeds366 days');
  num(calendar.initial_elapsed_seconds,'calendar.initial_elapsed_seconds');if(calendar.initial_elapsed_seconds>9007199254||Math.abs(calendar.initial_elapsed_seconds*1e6-Math.round(calendar.initial_elapsed_seconds*1e6))>.001)fail('Calendar start is not a safe microsecond time');
  if(JSON.stringify(calendar.seasons)!==JSON.stringify(['spring','summer','autumn','winter'])||calendar.solar_curve!=='daylight_half_sine')fail('Unsupported season order or solar curve');
  const workers=object(d.workers,'workers');if(workers.scheduler_policy!=='finite_shared_pool')fail('Worker scheduler must use the finite shared pool');
  const workerBounds={body_radius_meters:[.1,5],haul_mass_kg:[.1,10000],haul_volume_litres:[.1,10000],logistics_workers:[1,1000,true],logistics_facilities_per_worker:[1,1000,true],logistics_max_workers:[1,1000,true],core_minimum_operators:[1,1000,true],loading_seconds:[.01,3600],unloading_seconds:[.01,3600],route_retry_seconds:[.01,3600],deployment_ground_seconds:[.01,3600],deployment_hatch_seconds:[.01,3600],hatch_exit_spacing_seconds:[.01,3600],core_hatch_fraction:[.01,1],workstation_spacing_meters:[.1,10],idle_return_seconds:[.01,3600]};
  for(const [key,[min,max,integer]]of Object.entries(workerBounds)){num(workers[key],`workers.${key}`,min,!!integer);if(workers[key]>max)fail(`workers.${key} exceeds supported bound`);}
  if(!['fixed','completed_facilities'].includes(workers.logistics_scaling_policy))fail('Unsupported logistics scaling policy');
  if(workers.logistics_max_workers<workers.logistics_workers)fail('Logistics maximum cannot be below its base worker count');
  const resources = catalog(d.resources.resources,'resources');
  if (!resources.size) fail('Resource catalog cannot be empty');
  const dog=object(d.companions.dog,'companions.dog');
  for(const key of ['name','food_resource','mesh','walk_animation','idle_animation'])str(dog[key],`dog.${key}`);
  if(!resources.has(dog.food_resource)||resources.get(dog.food_resource).unit!=='kg')fail('Companion food must be a kilogram cargo resource');
  const dogRanges={initial_count:[0,1,true],seed:[0,2147483647,true],walk_kmh:[.1,20],walk_cycle_meters:[.05,5],roam_radius_meters:[1,1000],food_per_meal_kg:[.001,100],meal_interval_seconds:[1,86400],fed_duration_seconds:[1,172800],feed_radius_meters:[1,1000],morale_bonus:[0,.5],morale_radius_meters:[1,1000],rest_seconds:[0,300],eye_height_cm:[20,150],view_fov:[45,110],look_sensitivity:[.01,1],body_radius_meters:[.05,1],maximum_slope_grade:[.1,2]};
  for(const [key,[min,max,integer]] of Object.entries(dogRanges)){num(dog[key],`dog.${key}`,min,!!integer);if(dog[key]>max)fail(`dog.${key} exceeds supported bound`);}
  for(const key of ['mesh','walk_animation','idle_animation'])if(!dog[key].startsWith('/Game/Art/'))fail(`dog.${key} must refer to a cooked game asset`);
  for (const r of resources.values()) { str(r.name,`${r.id}.name`); num(r.tier,`${r.id}.tier`,0,true); color(r.color,`${r.id}.color`); if(!['bulk','ingots','crates'].includes(r.stockpile_visual)) fail(`${r.id}: invalid stockpile_visual`); if(!['standard','rare','manufactured'].includes(r.class)||!['kg','L','workers'].includes(r.unit))fail(`${r.id}: invalid resource class or unit`);num(r.unit_mass_kg,`${r.id}.unit_mass_kg`,0,false,true);num(r.litres_per_unit,`${r.id}.litres_per_unit`,0,false,true);if(r.unit==='kg'&&r.unit_mass_kg!==1)fail(`${r.id}: kilogram quantities must weigh one kilogram per unit`);if(typeof r.discrete!=='boolean'||r.discrete!==(r.unit==='workers')||(r.unit==='workers'&&(r.id!=='stored_workers'||r.class!=='manufactured')))fail(`${r.id}: invalid discrete cargo metadata`); }
  const expectedRaw={water:'standard',iron_ore:'standard',silica:'standard',carbon:'standard',copper_ore:'rare',radioactive_ore:'rare',crystalline:'rare',hydrocarbons:'rare'};
  for(const [id,classification] of Object.entries(expectedRaw))if(resources.get(id)?.class!==classification)fail(`Confirmed raw resource pool is missing or misclassified: ${id}`);
  if([...resources.values()].filter(r=>r.class!=='manufactured').length!==8)fail('Current raw pool must contain exactly four standard and four rare types');
  if(resources.has('energy')||resources.has('credits'))fail('Grid energy and the external-trade account are not cargo resources');
  const volume=v=>Object.entries(v).reduce((total,[id,n])=>total+n*resources.get(id).litres_per_unit,0);
  const mass=v=>Object.entries(v).reduce((total,[id,n])=>total+n*resources.get(id).unit_mass_kg,0);
  const amounts = (v,n) => { object(v,n); for(const [id,a] of Object.entries(v)) { if(!resources.has(id)) fail(`${n} references unknown resource ${id}`); num(a,`${n}.${id}`,0,resources.get(id).discrete); } };
  const recipes = catalog(d.recipes.recipes,'recipes');
  const workerCargo=resources.get('stored_workers');if(!workerCargo||!workerCargo.discrete)fail('Inactive workers require discrete physical cargo');
  for (const r of recipes.values()) { num(r.seconds,`${r.id}.seconds`,0,false,true); num(r.energy_kwh,`${r.id}.energy_kwh`);num(r.worker_output,`${r.id}.worker_output`,0,true);amounts(r.inputs,`${r.id}.inputs`); amounts(r.outputs,`${r.id}.outputs`); if(sum(r.inputs)<=0||sum(r.outputs)+r.worker_output<=0) fail(`${r.id} needs positive inputs and outputs`);if(r.inputs.stored_workers||r.outputs.stored_workers||(r.worker_output>0&&sum(r.outputs)>0))fail(`${r.id}: worker assembly must use worker_output exclusively`);if(Math.abs(mass(r.inputs)-mass(r.outputs)-r.worker_output*workerCargo.unit_mass_kg)>1e-6)fail(`${r.id}: physical recipe mass does not balance`); }
  const buildings = catalog(d.buildings.buildings,'buildings');
  const roles = ['core','extractor','processor','storage','sensor','defense','service','generator','battery','trade','worker_factory','vehicle_factory','wall'];
  const logisticsExclusions=array(workers.logistics_excluded_roles,'workers.logistics_excluded_roles');
  if(new Set(logisticsExclusions).size!==logisticsExclusions.length||logisticsExclusions.some(role=>!roles.includes(role)))fail('Logistics exclusions require unique known building roles');
  for (const b of buildings.values()) {
    for(const k of ['name','role','description','visual']) str(b[k],`${b.id}.${k}`);
    if('category' in b)fail(`${b.id}: category is retired; build-menu groups come from Interface/ui.json`);
    str(b.recipe,`${b.id}.recipe`,true);
    amounts(b.extraction_rates,`${b.id}.extraction_rates`);
    if('extract_resource' in b || 'extract_rate' in b)fail(`${b.id}: use deposit-selected extraction_rates`);
    for(const [id,rate] of Object.entries(b.extraction_rates))if(rate<=0||resources.get(id).class==='manufactured'||resources.get(id).discrete)fail(`${b.id}: mine rates require positive raw-resource outputs`);
    if(!roles.includes(b.role)) fail(`Unsupported building role ${b.role}`);
    if(!['full_staff','proportional'].includes(b.workforce_mode))fail(`${b.id}: invalid workforce policy`);
    str(b.family,`${b.id}.family`);num(b.level,`${b.id}.level`,1,true);if(b.level>3)fail(`${b.id}: building level exceeds three`);
    array(b.allowed_recipes,`${b.id}.allowed_recipes`);if(new Set(b.allowed_recipes).size!==b.allowed_recipes.length||b.allowed_recipes.some(id=>!recipes.has(id)))fail(`${b.id}: invalid selectable recipes`);
    for(const key of ['recipe_time_multiplier','recipe_energy_multiplier','recipe_input_multiplier'])num(b[key],`${b.id}.${key}`,0,false,true);
    if(typeof b.stores_inactive_workers!=='boolean')fail(`${b.id}: missing inactive-worker storage selector`);
    for(const k of ['health','footprint','storage_capacity','construction_seconds']) num(b[k],`${b.id}.${k}`,0,false,true);
    num(b.reserved_footprint,`${b.id}.reserved_footprint`,b.footprint); position(b.access_port,`${b.id}.access_port`); if(Math.max(...b.access_port.map(Math.abs))!==1) fail(`${b.id}: access port must lie on reserved edge`); if(typeof b.deployment_defense!=='boolean'||(b.deployment_defense&&b.role!=='core'))fail(`${b.id}: invalid deployment defense`);
    num(b.construction_workers,`${b.id}.construction_workers`,1,true);num(b.robot_support_capacity,`${b.id}.robot_support_capacity`,0,true);
    num(b.staffing_priority,`${b.id}.staffing_priority`,0,true);
    if(!['indoor','outdoor'].includes(b.inventory_presentation)||!['extraction','assembly','handling','inspection','service'].includes(b.worker_activity)) fail(`${b.id}: invalid presentation metadata`);
    for(const k of ['sensor_range','attack_range','damage_per_shot','reload_seconds','power_usage_kw','power_generation_kw']) num(b[k],`${b.id}.${k}`);
    str(b.weapon_name,`${b.id}.weapon_name`,true);
    const armed=b.damage_per_shot>0;
    if('damage_per_second' in b || (armed && (!b.weapon_name || b.reload_seconds<=0 || b.attack_range<=0)) || (!armed && (b.weapon_name || b.reload_seconds!==0 || b.attack_range!==0))) fail(`${b.id} weapon fields disagree; DPS is derived from shot damage and reload`);
    if(armed && !Number.isFinite(b.damage_per_shot/b.reload_seconds)) fail(`${b.id} derived weapon DPS is not finite`);
    if(b.power_usage_kw!==0 || b.power_generation_kw!==0) fail(`${b.id}: energy.json owns power values; legacy fields must be zero`);
    str(b.next_upgrade,`${b.id}.next_upgrade`,true);amounts(b.upgrade_cost,`${b.id}.upgrade_cost`);
    num(b.jobs,`${b.id}.jobs`,0,true); color(b.color,`${b.id}.color`); amounts(b.cost,`${b.id}.cost`);
    if([...Object.keys(b.cost),...Object.keys(b.upgrade_cost)].some(id=>resources.get(id).discrete))fail(`${b.id}: phased construction cannot consume fractional pieces of discrete cargo`);
    if(volume(b.cost)>b.storage_capacity || (!['core','service'].includes(b.role)&&b.robot_support_capacity>0)) fail(`${b.id} invalid construction storage or support role`);
    if(b.recipe && !recipes.has(b.recipe)) fail(`${b.id} references unknown recipe`);
    if(b.extract_resource && !resources.has(b.extract_resource)) fail(`${b.id} references unknown extraction resource`);
    if((b.role==='extractor') !== !!Object.keys(b.extraction_rates).length || (b.role==='processor') !== !!b.recipe) fail(`${b.id} role and capability disagree`);
    for(const id of new Set([...b.allowed_recipes,...(b.recipe?[b.recipe]:[])])){const r=recipes.get(id);if(volume(r.inputs)*b.recipe_input_multiplier>b.storage_capacity||volume(r.outputs)+r.worker_output*workerCargo.litres_per_unit>b.storage_capacity)fail(`${b.id} recipe does not fit storage`);if(r.worker_output>0&&!b.stores_inactive_workers)fail(`${b.id}: worker production needs physical berth storage`);}
    if(b.stores_inactive_workers&&b.storage_capacity<workerCargo.litres_per_unit)fail(`${b.id}: no room for one inactive worker`);
    if(b.role==='worker_factory'&&(!b.allowed_recipes.length||b.allowed_recipes.some(id=>recipes.get(id).worker_output<=0)))fail(`${b.id}: worker factory needs worker recipes`);
    if(b.role==='core'&&(b.allowed_recipes.length!==recipes.size||b.allowed_recipes[0]!==d.policies.policies.population_recipe))fail(`${b.id}: universal core must offer every recipe, workers first`);
  }
  // Upgraded levels carry the cumulative installed bill: previous level cost
  // plus its upgrade_cost, so documentation, review placement and losses agree.
  for(const b of buildings.values())if(b.next_upgrade&&buildings.has(b.next_upgrade)){
    const target=buildings.get(b.next_upgrade),ids=new Set([...Object.keys(b.cost),...Object.keys(b.upgrade_cost),...Object.keys(target.cost)]);
    for(const id of ids)if(Math.abs((target.cost[id]??0)-(b.cost[id]??0)-(b.upgrade_cost[id]??0))>1e-6)fail(`${target.id}: cumulative cost of ${id} must equal ${b.id} cost plus upgrade_cost`);
  }
  const familyLevels=new Set();
  for(const b of buildings.values()){const base=buildings.get(b.family),target=buildings.get(b.next_upgrade);if(!base||base.family!==b.family||base.level!==1||base.role!==b.role||familyLevels.has(`${b.family}:${b.level}`))fail(`${b.id}: invalid or duplicate family level`);familyLevels.add(`${b.family}:${b.level}`);if(b.role!=='core'&&b.footprint!==base.footprint)fail(`${b.id}: upgraded building must keep its footprint`);if(b.next_upgrade&&(!target||target.role!==b.role||target.family!==b.family||target.level!==b.level+1||target.reserved_footprint!==b.reserved_footprint||sum(b.upgrade_cost)<=0))fail(`${b.id}: invalid in-place upgrade`);const visited=new Set();let current=b;while(current?.next_upgrade){if(visited.has(current.id))fail('Cyclic building upgrades');visited.add(current.id);current=buildings.get(current.next_upgrade);}}
  const energy=object(d.energy.energy,'energy');if(energy.dispatch_policy!=='pending_defense_before_optional_transactions')fail('Unsupported energy dispatch policy');num(energy.worker_kw,'energy.worker_kw');num(energy.connection_tolerance_meters,'energy.connection_tolerance_meters');num(energy.minimum_operating_fraction,'energy.minimum_operating_fraction',0,false,true);if(energy.minimum_operating_fraction>1)fail('Energy operation fraction >1');object(energy.buildings,'energy.buildings');
  if(Object.keys(energy.buildings).length!==buildings.size)fail('Every building requires one energy definition');
  for(const [id,e] of Object.entries(energy.buildings)){if(!buildings.has(id))fail(`Unknown power definition ${id}`);object(e,id);if(!['constant','solar'].includes(e.generation_source)||(e.generation_source==='solar'&&(e.generation_kw<=0||e.fuel_resource)))fail('Invalid energy generation source');for(const k of ['generation_kw','battery_capacity_kwh','initial_battery_kwh','idle_kw','fuel_units_per_kwh','fuel_buffer_seconds'])num(e[k],`${id}.${k}`);num(e.priority,`${id}.priority`,0,true);str(e.fuel_resource,`${id}.fuel_resource`,true);if(typeof e.self_start!=='boolean'||typeof e.requires_road_grid!=='boolean')fail('Energy selectors must be boolean');if(e.initial_battery_kwh>e.battery_capacity_kwh||(e.initial_battery_kwh>0&&buildings.get(id).role!=='core')||(e.fuel_resource&&(!resources.has(e.fuel_resource)||e.generation_kw<=0||e.fuel_units_per_kwh<=0))||(!e.fuel_resource&&e.fuel_units_per_kwh!==0))fail('Invalid fuel generation or starting battery');}
  const trade=object(d.trade.trade,'trade');object(trade.prices,'trade.prices');object(trade.ports,'trade.ports');if(Object.keys(trade.prices).length!==resources.size||Object.keys(trade.ports).length!==3)fail('Trade must cover all cargo and three port levels');
  for(const [id,p] of Object.entries(trade.prices)){if(!resources.has(id))fail('Unknown trade cargo');num(p.buy_credits,'buy price',0,false,true);num(p.sell_credits,'sell price',0,false,true);if(p.sell_credits>p.buy_credits)fail('Export/import arbitrage');}
  const levels=new Set();for(const [id,p] of Object.entries(trade.ports)){if(buildings.get(id)?.role!=='trade')fail('Invalid trade port');num(p.level,'port.level',1,true);if(p.level>3||levels.has(p.level))fail('Port levels must be unique 1-3');levels.add(p.level);num(p.capacity_kg,'port.capacity_kg',0,false,true);num(p.shipment_seconds,'port.shipment_seconds',0,false,true);num(p.energy_kwh,'port.energy_kwh');}
  const menu = array(d.buildings.build_menu,'build_menu');
  if(new Set(menu).size !== menu.length || menu.some(id=>!buildings.has(id) || ['core','wall'].includes(buildings.get(id).role)||buildings.get(id).level!==1)) fail('Build menu has duplicate, missing, upgrade-only or custom-tool entries');
  const cores=[...buildings.values()].filter(b=>b.role==='core'),baseCore=cores.find(b=>b.level===1);if(cores.length!==3||!baseCore||cores.some(b=>b.family!==baseCore.id||b.footprint!==baseCore.footprint*b.level||b.reserved_footprint!==baseCore.footprint*3))fail('Core requires three width levels and the largest reserved plot at every level');
  const walls=object(d.walls.walls,'walls');array(walls.level_definitions,'wall level definitions');array(walls.height_meters,'wall heights');
  if(walls.level_definitions.length!==3||new Set(walls.level_definitions).size!==3||walls.height_meters.length!==3)fail('Walls require three distinct building levels and heights');
  for(const [i,id] of walls.level_definitions.entries()){if(buildings.get(id)?.role!=='wall'||buildings.get(id)?.level!==i+1)fail('Wall levels must reference ordered wall definitions');num(walls.height_meters[i],'wall height',0,false,true);}
  for(const k of ['segment_length_meters','minimum_edge_meters','width_meters','access_clearance_meters','joint_pick_radius_meters'])num(walls[k],`walls.${k}`,0,false,true);
  for(const k of ['maximum_joints','maximum_segments'])num(walls[k],`walls.${k}`,2,true);
  if(walls.minimum_edge_meters>walls.segment_length_meters||walls.maximum_joints>4096||walls.maximum_segments>4096)fail('Invalid wall construction limits');
  const t=object(d.transport.transport,'transport');
  for(const k of ['meters_per_world_unit','worker_walk_kmh','access_clearance','path_clearance','snap_distance','minimum_segment_meters','maximum_segment_meters'])num(t[k],`transport.${k}`,0,false,true);
  if(!resources.has(t.repair_resource)||resources.get(t.repair_resource).discrete)fail('Road repairs require divisible physical material');
  for(const k of ['repair_service_range_meters','repair_health_per_second','repair_health_per_unit','repair_energy_kwh_per_health'])num(t[k],`transport.${k}`,0,false,true);
  num(t.repair_buffer_units,'transport.repair_buffer_units',0,false,true);
  num(t.construction_staffing_priority,'transport.construction_staffing_priority',0,true);
  num(t.unpowered_speed_multiplier,'transport.unpowered_speed_multiplier',1);
  num(t.max_segments,'transport.max_segments',1,true); if(t.max_segments>512||t.maximum_segment_meters<t.minimum_segment_meters||t.access_clearance<=t.path_clearance)fail('Invalid transport limits');
  const tiers=catalog(t.tiers,'transport.tiers');
  for(const tier of tiers.values())if(Object.keys(tier.cost_per_100_meters??{}).some(id=>resources.get(id)?.discrete))fail('Length-based road costs cannot contain indivisible cargo');
  for(const tier of tiers.values()){str(tier.name,'tier.name');num(tier.health_per_meter,'tier.health_per_meter',0,false,true);num(tier.idle_kw_per_100_meters,'tier.idle_kw_per_100_meters');if(!['road','rail','vacuum'].includes(tier.visual))fail('Unknown transport visual');str(tier.next_tier,'tier.next_tier',true);num(tier.speed_multiplier,'tier.speed_multiplier',1);num(tier.width_meters,'tier.width_meters',0,false,true);num(tier.construction_seconds_per_100_meters,'tier.construction_seconds_per_100_meters',0,false,true);num(tier.minimum_construction_seconds,'tier.minimum_construction_seconds',0,false,true);num(tier.construction_workers,'tier.construction_workers',1,true);amounts(tier.cost_per_100_meters,'tier.cost');if(sum(tier.cost_per_100_meters)<=0)fail('Road cost cannot be empty');}
  const seenTiers=new Set();let tierId=t.initial_tier;while(tierId){if(seenTiers.has(tierId)||!tiers.has(tierId))fail('Invalid/cyclic transport tier chain');seenTiers.add(tierId);const tier=tiers.get(tierId),next=tiers.get(tier.next_tier);if(tier.next_tier&&(!next||next.speed_multiplier<=tier.speed_multiplier||next.width_meters<tier.width_meters))fail('Road upgrades must retain width and increase speed');tierId=tier.next_tier;}if(seenTiers.size!==tiers.size)fail('Unreachable transport tier');
  const p=object(d.policies.policies,'policies'); const s=object(d.scenario.scenario,'scenario');
  let end=0;for(const stage of array(p.construction_stages,'construction_stages')){str(stage.name,'construction stage name');num(stage.end,'construction stage end',end,false,true);if(stage.end>1)fail('Construction stage beyond completion');end=stage.end;}if(end!==1)fail('Construction stages must finish at one');
  for(const k of positive) num(p[k],k,0,false,true);
  for(const k of nonnegative) num(p[k],k);
  for(const k of integerPolicies) num(p[k],k,0,true);
  const deliveryOrder=array(p.delivery_priority_order,'delivery_priority_order');
  if(deliveryOrder.length!==deliveryClasses.length || new Set(deliveryOrder).size!==deliveryClasses.length || deliveryOrder.some(id=>!deliveryClasses.includes(id))) fail('delivery_priority_order must contain every supported delivery class exactly once');
  num(p.delivery_refill_trigger_fraction,'delivery_refill_trigger_fraction',0,false,true);
  num(p.delivery_raw_input_buffer_loads,'delivery_raw_input_buffer_loads');
  if(p.delivery_refill_trigger_fraction>1)fail('delivery_refill_trigger_fraction must be in (0,1]');
  if(p.max_couriers<1 || p.event_history_limit<1 || p.wave_max_count<p.wave_base_count || p.placement_requires_visibility>1 || p.upkeep_shortage_efficiency>1 || p.courier_min_batch>p.courier_capacity || p.fixed_step_seconds>p.dispatch_interval) fail('Inconsistent policy ranges');
  for(const b of buildings.values()) if(b.damage_per_shot>0 && b.reload_seconds<p.fixed_step_seconds) fail(`${b.id}: reload cannot be shorter than fixed_step_seconds`);
  for(const [k,allowed] of Object.entries(selectors)) if(!allowed.includes(p[k])) fail(`Unsupported ${k}: ${p[k]}`);
  for(const k of ['upkeep_resource','repair_resource','objective_resource']) if(!resources.has(p[k])) fail(`Unknown resource in ${k}`);
  if(!recipes.has(p.population_recipe)||recipes.get(p.population_recipe).worker_output!==1||Object.keys(recipes.get(p.population_recipe).outputs).length) fail('Population recipe requires exactly one worker output');
  if(p.inactive_worker_resource!==workerCargo.id||p.inactive_worker_litres!==workerCargo.litres_per_unit)fail('Worker berth volume must match physical worker cargo');
  for(const k of ['worker_store_seconds','worker_reactivate_seconds','worker_disassemble_seconds'])num(p[k],k,0,false,true);
  num(p.worker_disassembly_kwh,'worker_disassembly_kwh',0,false,true);
  for(const k of ['auto_disassemble_parts_shortage','auto_disassemble_storage_full'])if(typeof p[k]!=='boolean')fail(`${k} must be boolean`);
  for(const k of ['default_inactive_worker_target','default_port_worker_reserve_target'])num(p[k],k,0,true);
  amounts(p.worker_disassembly_outputs,'worker_disassembly_outputs');if(p.worker_disassembly_outputs.stored_workers||sum(p.worker_disassembly_outputs)<=0||mass(p.worker_disassembly_outputs)>workerCargo.unit_mass_kg+1e-6)fail('Worker disassembly must return parts without creating mass');
  if(!buildings.has(p.objective_building)) fail('Objective building does not exist');
  amounts(p.core_reserves,'core_reserves');
  str(s.title,'scenario.title'); num(s.world_half_size,'world_half_size',0,false,true); num(s.random_seed,'random_seed',0,true); num(s.starting_population,'starting_population',0,true);
  if(!buildings.has(s.core_definition) || buildings.get(s.core_definition).role!=='core') fail('Scenario core is invalid');
  const core=buildings.get(s.core_definition);
  if(!menu.some(id=>{const b=buildings.get(id);return b.robot_support_capacity>b.jobs&&core.robot_support_capacity>=core.jobs+Math.max(b.construction_workers,b.jobs);}))fail('Starter support must leave builders and staff for a support expansion');
  position(s.core_position,'core_position'); amounts(s.starting_inventory,'starting_inventory'); amounts(s.starting_shuttle_cargo,'starting_shuttle_cargo');
  amounts(s.starting_deployment_materials,'starting_deployment_materials');
  if(Object.entries(core.cost).some(([id,n])=>(s.starting_deployment_materials[id]??0)!==n) || sum(core.cost)!==sum(s.starting_deployment_materials) || s.starting_population<core.construction_workers || core.robot_support_capacity<s.starting_population) fail('Landing shuttle needs the exact deployment kit and enough supported builders');
  if(volume(s.starting_inventory)+volume(s.starting_deployment_materials)>core.storage_capacity)fail('Shuttle carried stock and deployment kit exceed core capacity in litres');
  if(p.spawn_radius>s.world_half_size || s.core_position.some(x=>Math.abs(x)+core.reserved_footprint>s.world_half_size) || s.starting_population<p.minimum_population || volume(s.starting_inventory)>core.storage_capacity || mass(s.starting_shuttle_cargo)>p.shuttle_capacity) fail('Scenario spawn, capacity or population bounds invalid');
  if(s.starting_credits!==0)fail('A new landing starts with zero Galactic credits');
  const generation=object(s.resource_generation,'resource_generation');
  if(generation.standard_count!==3||generation.rare_count!==2||'deposits' in s)fail('Use seeded 3+2 distinct deposits');
  num(generation.inner_area_fraction,'inner_area_fraction',0,false,true);num(generation.minimum_separation_half_size_fraction,'minimum_separation_half_size_fraction',0,false,true);
  if(generation.inner_area_fraction>1||generation.minimum_separation_half_size_fraction>=Math.sqrt(generation.inner_area_fraction))fail('Resource placement area or separation is outside supported bounds');
  // A region exports a local raw and imports missing raw types through its paid trading port.
  // This checks catalogue reachability; native AI tests verify physical bootstrap and trading.
  const available=new Set(Object.keys(expectedRaw));
  for(const id of available)if(!menu.some(b=>(buildings.get(b).extraction_rates[id]??0)>0))fail(`No buildable extractor for ${id}`);
  const usableRecipes=[...new Set([...buildings.values()].filter(b=>menu.includes(b.id)||b.role==='core').flatMap(b=>[...b.allowed_recipes,...(b.recipe?[b.recipe]:[])]))].map(id=>recipes.get(id));
  for(let old=-1;old!==available.size;) { old=available.size; for(const r of usableRecipes) if(Object.keys(r.inputs).every(id=>available.has(id))){for(const id of Object.keys(r.outputs)) available.add(id);if(r.worker_output>0)available.add(workerCargo.id);} }
  for(const id of [...Object.keys(recipes.get(p.population_recipe).inputs),p.upkeep_resource,p.repair_resource,p.objective_resource]) if(!available.has(id)) fail(`No renewable recipe path to ${id}`);
  // Finite bootstrap stock covers an energy/trade route and one local extractor, not every factory.
  const bootstrap={};
  const start=object(s.bootstrap,'bootstrap');const starters=array(start.building_definitions,'bootstrap.building_definitions');
  if(!starters.includes('solar_array')||!starters.includes('trading_port')||!starters.includes('robot_service_bay')||new Set(starters).size!==starters.length)fail('Bootstrap must include solar generation, level-1 trade and worker support');
  num(start.road_length_meters,'bootstrap.road_length_meters',100);if(start.extractor_count!==1)fail('Bootstrap must cover one actual local extractor');
  for(const id of starters){if(!menu.includes(id))fail(`Missing bootstrap blueprint ${id}`);for(const [r,n] of Object.entries(buildings.get(id).cost))bootstrap[r]=(bootstrap[r]??0)+n;}
  const extractorMaximum={};for(const b of buildings.values())if(b.role==='extractor')for(const [r,n]of Object.entries(b.cost))extractorMaximum[r]=Math.max(extractorMaximum[r]??0,n);
  for(const [r,n]of Object.entries(extractorMaximum))bootstrap[r]=(bootstrap[r]??0)+n;
  for(const [r,n]of Object.entries(tiers.get(t.initial_tier).cost_per_100_meters))bootstrap[r]=(bootstrap[r]??0)+n*start.road_length_meters/100;
  const newJobs=starters.reduce((n,id)=>n+buildings.get(id).jobs,0)+Math.max(...[...buildings.values()].filter(b=>b.role==='extractor').map(b=>b.jobs))+2;
  for(const [r,n] of Object.entries(recipes.get(p.population_recipe).inputs)) bootstrap[r]=(bootstrap[r]??0)+n*newJobs;
  // Each operating structure retains its local repair buffer; these units cannot fund sites.
  bootstrap[p.repair_resource]=(bootstrap[p.repair_resource]??0)+p.repair_buffer_units*(starters.length+2);
  for(const [r,n] of Object.entries(bootstrap)) if((s.starting_inventory[r]??0)<n) fail(`Starter stock cannot bootstrap road, solar, trade, local extraction and workers: ${r} needs ${n}`);
  // Cross-module storage: a factory must hold its largest permitted paid bill, not
  // just its own construction kit. Enumerate legal mount/mass combinations so
  // a refit and an ammunition buffer cannot silently strand an upgraded tower.
  const combat=JSON.parse(fs.readFileSync(path.join(d.ruleDirectory,'combat.json'),'utf8'));
  const hulls=JSON.parse(fs.readFileSync(path.join(d.ruleDirectory,'chassis.json'),'utf8')).chassis;
  const weapons=JSON.parse(fs.readFileSync(path.join(d.ruleDirectory,'weapons.json'),'utf8')).weapons.filter(w=>w.family!=='alien');
  const armamentVolume=(mountPoints,massLimit)=>{
    const byPoints=Array.from({length:mountPoints+1},()=>new Map());byPoints[0].set(0,0);let maximum=0;
    for(let points=0;points<=mountPoints;points++)for(const [weight,occupied] of byPoints[points]){
      maximum=Math.max(maximum,occupied);
      for(const w of weapons){const nextPoints=points+w.mount_points,nextWeight=weight+w.mass_kg;
        if(nextPoints>mountPoints||nextWeight>massLimit)continue;
        const nextVolume=occupied+volume(w.cost)+(w.ammo?w.ammo_per_shot*combat.ammo_buffer_shots*resources.get(w.ammo).litres_per_unit:0);
        byPoints[nextPoints].set(nextWeight,Math.max(byPoints[nextPoints].get(nextWeight)??0,nextVolume));
      }
    }return maximum;
  };
  const combatStorage=new Map();
  for(const [id,capability] of Object.entries(combat.factories)){
    let required=0;for(const h of hulls)if(h.tier<=capability.maximum_tier&&(capability.family==='all'||h.family===capability.family))required=Math.max(required,volume(h.cost)+armamentVolume(h.mount_points,h.max_weapon_mass_kg));
    combatStorage.set(id,required);
  }
  for(const [id,platform]of Object.entries(combat.building_platforms))combatStorage.set(id,Math.max(combatStorage.get(id)??0,armamentVolume(platform.mount_points,platform.max_weapon_mass_kg)));
  // Buffer allocations cannot create an unavoidable storage deadlock.
  for(const b of buildings.values()) {
    let reserve=p.repair_buffer_units*resources.get(p.repair_resource).litres_per_unit;
    if(b.recipe)for(const [id,quantity]of Object.entries(recipes.get(b.recipe).inputs)){
      const resource=resources.get(id),cycles=quantity*b.recipe_input_multiplier*p.delivery_buffer_cycles;
      const raw=!resource.discrete&&['standard','rare'].includes(resource.class);
      const target=raw?Math.max(cycles,p.delivery_raw_input_buffer_loads*Math.min(workers.haul_mass_kg/resource.unit_mass_kg,workers.haul_volume_litres/resource.litres_per_unit)):cycles;
      reserve+=target*resource.litres_per_unit;
    }
    if(b.role==='core') reserve+=volume(p.core_reserves)+volume(recipes.get(p.population_recipe).inputs)*p.population_buffer_robots;
    reserve+=b.robot_support_capacity*p.upkeep_per_robot*p.upkeep_buffer_intervals*resources.get(p.upkeep_resource).litres_per_unit;
    if(reserve+(combatStorage.get(b.id)??0)>b.storage_capacity) fail(`${b.id} storage is smaller than its permitted fabrication/refit bill and demand buffers`);
  }
  return {version:d.resources.version,resources:resources.size,recipes:recipes.size,buildings:buildings.size,renewable:[...available].sort(),bootstrap};
}
function selfTest(source) {
  // The order and refill threshold are external policy choices, not fixed
  // authored values. A complete reordering and both valid bounds remain usable.
  for(const fraction of [.0001,1]){const d=structuredClone(source);d.policies.policies.delivery_priority_order.reverse();d.policies.policies.delivery_refill_trigger_fraction=fraction;validateRules(d);}
  for(const loads of [0,.5,2]){const d=structuredClone(source);d.policies.policies.delivery_raw_input_buffer_loads=loads;validateRules(d);}
  for(const policy of ['fixed','completed_facilities']){const d=structuredClone(source);d.workers.logistics_scaling_policy=policy;d.workers.logistics_facilities_per_worker=1000;d.workers.logistics_max_workers=d.workers.logistics_workers;d.workers.logistics_excluded_roles=[];validateRules(d);}
  const cases=[
    ['missing raw input buffer loads',d=>delete d.policies.policies.delivery_raw_input_buffer_loads],
    ['negative raw input buffer loads',d=>d.policies.policies.delivery_raw_input_buffer_loads=-1],
    ['nonnumeric raw input buffer loads',d=>d.policies.policies.delivery_raw_input_buffer_loads='2'],
    ['null raw input buffer loads',d=>d.policies.policies.delivery_raw_input_buffer_loads=null],
    ['nonfinite raw input buffer loads',d=>d.policies.policies.delivery_raw_input_buffer_loads=Infinity],
    ['NaN raw input buffer loads',d=>d.policies.policies.delivery_raw_input_buffer_loads=NaN],
    ['raw input buffers exceed physical storage',d=>d.policies.policies.delivery_raw_input_buffer_loads=10000],
    ['missing logistics scaling policy',d=>delete d.workers.logistics_scaling_policy],
    ['unknown logistics scaling policy',d=>d.workers.logistics_scaling_policy='free_couriers'],
    ['missing logistics divisor',d=>delete d.workers.logistics_facilities_per_worker],
    ['zero logistics divisor',d=>d.workers.logistics_facilities_per_worker=0],
    ['fractional logistics divisor',d=>d.workers.logistics_facilities_per_worker=1.5],
    ['excessive logistics divisor',d=>d.workers.logistics_facilities_per_worker=1001],
    ['missing logistics maximum',d=>delete d.workers.logistics_max_workers],
    ['fractional logistics maximum',d=>d.workers.logistics_max_workers=1.5],
    ['excessive logistics maximum',d=>d.workers.logistics_max_workers=1001],
    ['logistics maximum below base',d=>{d.workers.logistics_workers=2;d.workers.logistics_max_workers=1;}],
    ['missing logistics exclusions',d=>delete d.workers.logistics_excluded_roles],
    ['nonarray logistics exclusions',d=>d.workers.logistics_excluded_roles='core'],
    ['duplicate logistics exclusions',d=>d.workers.logistics_excluded_roles=['core','core']],
    ['unknown logistics exclusion',d=>d.workers.logistics_excluded_roles=['free_workers']],
    ['missing delivery priorities',d=>delete d.policies.policies.delivery_priority_order],
    ['unknown construction source policy',d=>d.policies.policies.construction_source_policy='oldest_buffer_first'],
    ['missing construction source policy',d=>delete d.policies.policies.construction_source_policy],
    ['nonarray delivery priorities',d=>d.policies.policies.delivery_priority_order='fuel'],
    ['incomplete delivery priorities',d=>d.policies.policies.delivery_priority_order.pop()],
    ['duplicate delivery priority',d=>d.policies.policies.delivery_priority_order[1]=d.policies.policies.delivery_priority_order[0]],
    ['unknown delivery priority',d=>d.policies.policies.delivery_priority_order[0]='free_cargo'],
    ['nonstring delivery priority',d=>d.policies.policies.delivery_priority_order[0]=null],
    ['missing delivery refill trigger',d=>delete d.policies.policies.delivery_refill_trigger_fraction],
    ['zero delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction=0],
    ['negative delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction=-.1],
    ['excess delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction=1.01],
    ['nonnumeric delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction='0.5'],
    ['nonfinite delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction=Infinity],
    ['NaN delivery refill trigger',d=>d.policies.policies.delivery_refill_trigger_fraction=NaN],
    ['calendar negative phase',d=>d.calendar.calendar.initial_elapsed_seconds=-1],
    ['calendar season order',d=>d.calendar.calendar.seasons.reverse()],
    ['calendar fractional microsecond',d=>d.calendar.calendar.daylight_seconds=1800.0000001],
    ['calendar invalid solar curve',d=>d.calendar.calendar.solar_curve='constant'],
    ['worker haul invalid volume',d=>d.workers.haul_volume_litres=0],
    ['worker fractional essential operators',d=>d.workers.core_minimum_operators=1.5],
    ['energy undefined solar selector',d=>d.energy.energy.buildings.solar_array.generation_source='magic'],
    ['energy missing dispatch policy',d=>delete d.energy.energy.dispatch_policy],
    ['energy unknown dispatch policy',d=>d.energy.energy.dispatch_policy='unlimited_defense'],
    ['fractional worker stock',d=>d.scenario.scenario.starting_inventory.stored_workers=.5],
    ['worker cargo not discrete',d=>d.resources.resources.find(r=>r.id==='stored_workers').discrete=false],
    ['bulk cargo marked discrete',d=>d.resources.resources[0].discrete=true],
    ['fractional worker output',d=>d.recipes.recipes.find(r=>r.id==='assemble_robot').worker_output=.5],
    ['worker assembly creates mass',d=>d.resources.resources.find(r=>r.id==='stored_workers').unit_mass_kg=10],
    ['worker storage metadata mismatch',d=>d.policies.policies.inactive_worker_litres=49],
    ['worker disassembly free energy',d=>d.policies.policies.worker_disassembly_kwh=0],
    ['largest factory bill cannot fit',d=>d.buildings.buildings.find(b=>b.id==='vehicle_factory_3').storage_capacity=3600],
    ['universal core chassis cannot fit',d=>d.buildings.buildings.find(b=>b.id==='command_core').storage_capacity=1400],
    ['large tower refit cannot fit',d=>d.buildings.buildings.find(b=>b.id==='turret_3').storage_capacity=70],
    ['road repair has no material',d=>d.transport.transport.repair_resource='missing'],
    ['road repair has no energy',d=>d.transport.transport.repair_energy_kwh_per_health=0],
    ['worker disassembly creates matter',d=>d.policies.policies.worker_disassembly_outputs.components=100],
    ['fractional reserve target',d=>d.policies.policies.default_inactive_worker_target=.5],
    ['unknown selectable output',d=>d.buildings.buildings[0].allowed_recipes.push('missing')],
    ['core omits universal output',d=>d.buildings.buildings[0].allowed_recipes.pop()],
    ['instant factory multiplier',d=>d.buildings.buildings.find(b=>b.id==='worker_factory').recipe_time_multiplier=0],
    ['worker factory no berth',d=>d.buildings.buildings.find(b=>b.id==='worker_factory').stores_inactive_workers=false],
    ['wrong upgrade family',d=>d.buildings.buildings.find(b=>b.id==='solar_array_2').family='command_core'],
    ['upgraded fixed footprint grows',d=>d.buildings.buildings.find(b=>b.id==='solar_array_2').footprint+=1],
    ['core level width wrong',d=>d.buildings.buildings.find(b=>b.id==='command_core_2').footprint=240],
    ['zero road durability',d=>d.transport.transport.tiers[0].health_per_meter=0],
    ['unknown per-building workforce mode',d=>d.buildings.buildings[0].workforce_mode='free_workers'],
    ['worker body in phased construction',d=>d.buildings.buildings[1].cost.stored_workers=1],
    ['worker body in length-based road cost',d=>d.transport.transport.tiers[0].cost_per_100_meters.stored_workers=1],
    ['upgrade blueprint exposed',d=>d.buildings.build_menu.push('solar_array_2')],
    ['invalid wall family',d=>d.walls.walls.level_definitions[0]='solar_array'],
    ['zero wall length',d=>d.walls.walls.segment_length_meters=0],
    ['wall level height missing',d=>d.walls.walls.height_meters.pop()],
    ['missing companion food',d=>d.companions.dog.food_resource='not_a_resource'],
    ['invalid companion walking speed',d=>d.companions.dog.walk_kmh=0],
    ['unbounded companion morale',d=>d.companions.dog.morale_bonus=10],
    ['invalid first-person camera',d=>d.companions.dog.eye_height_cm=-1],
    ['invalid companion count',d=>d.companions.dog.initial_count=.5],
    ['missing energy definition',d=>delete d.energy.energy.buildings.command_core],
    ['battery creates free charge',d=>d.energy.energy.buildings.solar_array.initial_battery_kwh=1],
    ['trade arbitrage',d=>d.trade.trade.prices.water.sell_credits=999],
    ['missing road energy',d=>delete d.transport.transport.tiers[0].idle_kw_per_100_meters],
    ['missing walking speed', d=>delete d.transport.transport.worker_walk_kmh],
    ['invalid core reserved footprint',d=>d.buildings.buildings[0].reserved_footprint=1],
    ['invalid port',d=>d.buildings.buildings[0].access_port=[0,0]],
    ['cyclic roads',d=>d.transport.transport.tiers[0].next_tier=d.transport.transport.initial_tier],
    ['invalid stages',d=>d.policies.policies.construction_stages[0].end=2],
    ['missing extraction rates',d=>delete d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates],
    ['empty mine coverage',d=>d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates={}],
    ['zero extraction rate',d=>d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates.water=0],
    ['unknown extracted resource',d=>d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates.unknown=1],
    ['manufactured extraction',d=>d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates.components=1],
    ['nonmine extraction',d=>d.buildings.buildings[0].extraction_rates.water=1],
    ['missing raw mine coverage',d=>delete d.buildings.buildings.find(b=>b.role==='extractor').extraction_rates.water],
    ['negative transport capacity', d=>d.policies.policies.courier_capacity=-1],
    ['fractional job count',d=>d.buildings.buildings[1].jobs=.5],
    ['unknown recipe input',d=>d.recipes.recipes[0].inputs.unknown=1],
    ['duplicate resource ID',d=>d.resources.resources.push({...d.resources.resources[0]})],
    ['unsupported policy',d=>d.policies.policies.population_policy='manual_targets'],
    ['inconsistent versions',d=>d.scenario.version='incompatible'],
    ['multiple cores',d=>d.buildings.buildings.push({...d.buildings.buildings[0],id:'extra_core'})],
    ['missing raw family',d=>d.resources.resources=d.resources.resources.filter(n=>n.id!=='carbon')],
    ['insufficient startup stock',d=>d.scenario.scenario.starting_inventory={}],
    ['overfilled shuttle',d=>d.scenario.scenario.starting_shuttle_cargo={components:9999}],
    ['incorrect inner area',d=>d.scenario.scenario.resource_generation.inner_area_fraction=1.1],
    ['incorrect deposit count',d=>d.scenario.scenario.resource_generation.standard_count=4],
    ['free starting credits',d=>d.scenario.scenario.starting_credits=10],
    ['invalid bulk volume',d=>d.resources.resources[0].litres_per_unit=0],
    ['invalid mass unit',d=>d.resources.resources.find(r=>r.unit==='kg').unit_mass_kg=2],
    ['unbalanced physical recipe',d=>d.recipes.recipes[0].outputs.alloy+=1],
    ['invalid color',d=>d.resources.resources[0].color=[2,0,0]],
    ['zero recipe duration',d=>d.recipes.recipes[0].seconds=0],
    ['storage deadlock',d=>d.buildings.buildings[1].storage_capacity=1],
    ['missing weapon damage',d=>delete d.buildings.buildings[0].damage_per_shot],
    ['zero armed reload',d=>d.buildings.buildings[0].reload_seconds=0],
    ['unarmed weapon metadata',d=>d.buildings.buildings[1].weapon_name='Phantom weapon'],
    ['faster-than-step reload',d=>d.buildings.buildings[0].reload_seconds=d.policies.policies.fixed_step_seconds/2],
    ['independent DPS override',d=>d.buildings.buildings[0].damage_per_second=999],
    ['unimplemented power demand',d=>d.buildings.buildings[0].power_usage_kw=1],
    ['nonfinite derived DPS',d=>{d.buildings.buildings[0].damage_per_shot=Number.MAX_VALUE;d.buildings.buildings[0].reload_seconds=.1;}],
    ['instant construction',d=>d.buildings.buildings[1].construction_seconds=0],
    ['unknown stockpile geometry',d=>d.resources.resources[0].stockpile_visual='magic'],
    ['unknown inventory location',d=>d.buildings.buildings[0].inventory_presentation='nowhere'],
    ['unknown workforce activity',d=>d.buildings.buildings[0].worker_activity='unbounded_patrol'],
    ['fractional builders',d=>d.buildings.buildings[1].construction_workers=1.5],
    ['negative support',d=>d.buildings.buildings[0].robot_support_capacity=-1],
    ['cumulative bill drift',d=>{d.buildings.buildings.find(b=>b.id==='turret_2').cost.alloy+=1;}],
    ['retired category field',d=>{d.buildings.buildings[0].category='command';}],
    ['missing landing kit',d=>d.scenario.scenario.starting_deployment_materials={}],
    ['unsupported starter crew',d=>d.buildings.buildings[0].robot_support_capacity=1],
    ['unsupported construction policy',d=>d.policies.policies.construction_policy='instant'],
    ['missing staffing priority',d=>delete d.buildings.buildings[1].staffing_priority],
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

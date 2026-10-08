import assert from 'node:assert/strict';
import {readConfiguration, validateConfiguration} from './validate_configuration.mjs';

// Exercise the real configuration validator without mutating live definitions.
const data = readConfiguration();
for (const policy of ['independent_tactics_trade_construction', 'shared_action_budget']) {
  const candidate = structuredClone(data);
  candidate.ai.decision_scheduling_policy = policy;
  assert.doesNotThrow(() => validateConfiguration(candidate), policy);
}
for (const policy of [undefined, null, 1, 'unlimited_free_actions']) {
  const candidate = structuredClone(data);
  if (policy === undefined) delete candidate.ai.decision_scheduling_policy;
  else candidate.ai.decision_scheduling_policy = policy;
  assert.throws(() => validateConfiguration(candidate), /decision_scheduling_policy/);
}
for (const placementIndex of [undefined, -1, .5, 4097]) {
  const candidate = structuredClone(data);
  if (placementIndex === undefined) delete candidate.ai.build_targets[0].placement_index;
  else candidate.ai.build_targets[0].placement_index = placementIndex;
  assert.throws(() => validateConfiguration(candidate), /placement_index/);
}
for(const [key,values] of Object.entries({export_policy:[undefined,null,'sell_everything'],bulk_input_policy:[undefined,null,'unlimited_stock'],fuel_import_buffer_cycles:[undefined,0,101,'4'],fuel_import_refill_fraction:[undefined,0,1.01,'0.5'],core_replication_policy:[undefined,'free_goods'],core_replication_recipes:[undefined,[],['unknown'],['assemble_robot'],['smelt_alloy','smelt_alloy']]})) {
  for(const value of values){const candidate=structuredClone(data);if(value===undefined)delete candidate.ai.economy[key];else candidate.ai.economy[key]=value;assert.throws(()=>validateConfiguration(candidate),new RegExp(key));}
}
for(const fraction of [.001,1]){const candidate=structuredClone(data);candidate.ai.economy.fuel_import_refill_fraction=fraction;assert.doesNotThrow(()=>validateConfiguration(candidate));}
for(const policy of ['remaining_output_bill','recipe_buffers']){const candidate=structuredClone(data);candidate.ai.economy.bulk_input_policy=policy;assert.doesNotThrow(()=>validateConfiguration(candidate));}
for(const policy of ['surplus_shipment_value','local_raw_only']){const candidate=structuredClone(data);candidate.ai.economy.export_policy=policy;assert.doesNotThrow(()=>validateConfiguration(candidate));}
for(const [key,value] of [['defense_coverage_policy',undefined],['defense_coverage_policy','ignore_walls'],['coverage_samples',0],['coverage_samples',8.5],['coverage_probe_distance_meters',-1],['coverage_excluded_roles',['unknown']]]) {
  const candidate=structuredClone(data);if(value===undefined)delete candidate.ai.placement[key];else candidate.ai.placement[key]=value;
  assert.throws(()=>validateConfiguration(candidate),new RegExp(key));
}
for(const policy of ['prefer_covered_approaches','first_legal']){const candidate=structuredClone(data);candidate.ai.placement.defense_coverage_policy=policy;assert.doesNotThrow(()=>validateConfiguration(candidate));}
const invalidEstablished = [
  d => { delete d.ai.developed_initialization; },
  d => { d.ai.developed_initialization = 'free_live_builds'; },
  d => { d.developed.kind = 'unbounded_grants'; },
  d => { d.developed.age_seconds = 0; },
  d => { d.developed.credits = -1; },
  d => { d.developed.idle_workers = 0; },
  d => { d.developed.layout_rotations = 5; },
  d => { d.developed.road_tier = 'unknown'; },
  d => { d.developed.equipment = 'unlimited'; },
  d => { d.developed.fleet = 'spawn_on_demand'; },
  d => { d.developed.buildings = []; },
  d => { d.developed.buildings[0].definition = 'turret'; },
  d => { d.developed.buildings[1].definition = 'command_core'; },
  d => { d.developed.buildings[1].definition = 'unknown'; },
  d => { d.developed.buildings[1].anchor = 'deposit'; },
  d => { d.developed.buildings[0].offset_meters = [1,0]; },
  d => { d.developed.buildings[1].offset_meters = [0,0]; },
  d => { d.developed.buildings[0].operators = 7; },
  d => { d.developed.buildings[0].battery_kwh = 99999; },
  d => { d.developed.buildings[1].recipe = 'make_circuits'; },
  d => { d.developed.buildings[0].inventory.alloy = 1e9; },
  d => { d.developed.buildings[0].inventory.stored_workers = .5; },
  d => { d.developed.buildings[1].inventory.stored_workers = 1; },
  d => { d.developed.buildings[0].inventory.unknown_resource = 1; },
];
for (const edit of invalidEstablished) { const candidate=structuredClone(data); edit(candidate); assert.throws(()=>validateConfiguration(candidate), /[Ee]stablished|developed_initialization|Core and bound deposit/); }
for (const mode of ['established_manifest','simulated_history']) { const candidate=structuredClone(data);candidate.ai.developed_initialization=mode;assert.doesNotThrow(()=>validateConfiguration(candidate)); }
console.log('AI configuration validation: economy, placement and established manifest passed (59 negative cases).');

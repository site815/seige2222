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
console.log('AI configuration validation: 2 supported schedules, 4 invalid schedules and 4 invalid placement indices passed.');

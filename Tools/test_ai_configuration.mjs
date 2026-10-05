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
console.log('AI scheduling validation: 2 supported policies, 4 invalid cases passed.');

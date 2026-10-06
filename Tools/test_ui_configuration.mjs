import assert from 'node:assert/strict';
import {readConfiguration, validateConfiguration} from './validate_configuration.mjs';

// Exercise the actual complete validator. Each mutation is isolated in memory;
// runtime definitions and the user's saved games are never modified.
const data = readConfiguration();
assert.doesNotThrow(() => validateConfiguration(data), 'current interface must validate');
assert.deepEqual(data.ui.resource_groups.map(group => group.id), ['credits', 'energy', 'raw', 'basic', 'advanced']);
const mines = data.rules.buildings.buildings.filter(building => building.role === 'extractor');
assert.equal(mines.length, 1, 'one Extraction Mine definition replaces the resource-specific catalog');
assert.equal(data.ui.build_groups.flatMap(group => group.entries).filter(entry => entry.definition === mines[0].id).length, 1,
  'the mine has exactly one normal construction-menu entry');

const group = (candidate, id) => candidate.ui.resource_groups.find(value => value.id === id);
const cases = [
  ['missing resource group', candidate => candidate.ui.resource_groups.pop(), /resource_groups must be an array/],
  ['duplicate raw material', candidate => {
    const raw = group(candidate, 'raw');
    raw.entries[1] = structuredClone(raw.entries[0]);
  }, /Duplicate UI grouped resource/],
  ['unknown material', candidate => { group(candidate, 'raw').entries[0].resource = 'not_a_resource'; }, /unknown resource/],
  ['omitted advanced product', candidate => group(candidate, 'advanced').entries.pop(), /resource groups omit/],
  ['manufactured product in raw group', candidate => {
    const raw = group(candidate, 'raw'), basic = group(candidate, 'basic');
    [raw.entries[0], basic.entries[0]] = [basic.entries[0], raw.entries[0]];
  }, /belongs to basic/],
  ['physical material in credits group', candidate => {
    group(candidate, 'credits').entries.push(structuredClone(group(candidate, 'raw').entries[0]));
  }, /resource group credits entries must be an array/],
  ['inactive workers mixed into materials', candidate => {
    group(candidate, 'advanced').entries.push({resource: candidate.rules.policies.policies.inactive_worker_resource, label: 'Workers'});
  }, /Inactive workers belong to workforce controls/],
];
for (const [name, mutate, diagnostic] of cases) {
  const candidate = structuredClone(data);
  mutate(candidate);
  assert.throws(() => validateConfiguration(candidate), diagnostic, name);
}
console.log('UI configuration validation: five ordered resource groups, one mine entry and 7 invalid cases passed.');

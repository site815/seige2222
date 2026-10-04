#!/usr/bin/env node
// Validates the complete editable definition set without loading Unreal or changing files.
// Usage: node Tools/validate_configuration.mjs [project-directory]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { readRules, validateRules } from './validate_rules.mjs';

const defaultRoot = fileURLToPath(new URL('../', import.meta.url));
export function readConfiguration(root = defaultRoot) {
  const read = relative => JSON.parse(fs.readFileSync(path.join(root, relative), 'utf8').replace(/^\uFEFF/, ''));
  return {
    rules: readRules(path.join(root, 'Rules')),
    ai: read('AIFILES/colony_ai.json'),
    developed: read('AIFILES/developed_start.json'),
    ui: read('Interface/ui.json'),
  };
}

export function validateConfiguration(data) {
  const rules = validateRules(data.rules);
  const fail = message => { throw new Error(message); };
  const object = (value, label) => {
    if (!value || typeof value !== 'object' || Array.isArray(value)) fail(`${label} must be an object`);
    return value;
  };
  const array = (value, label, minimum = 0, maximum = Infinity) => {
    if (!Array.isArray(value) || value.length < minimum || value.length > maximum) fail(`${label} must be an array with ${minimum}..${maximum} entries`);
    return value;
  };
  const text = (value, label) => {
    if (typeof value !== 'string' || !value.trim()) fail(`${label} must be a nonempty string`);
    return value;
  };
  const number = (value, label, minimum, maximum, integer = false) => {
    if (!Number.isFinite(value) || value < minimum || value > maximum || (integer && !Number.isSafeInteger(value))) fail(`${label} must be ${integer ? 'an integer' : 'a number'} in ${minimum}..${maximum}`);
    return value;
  };
  const version = (value, label) => { object(value, label); if (value.version !== 1) fail(`${label}.version must be 1`); };
  const unique = (seen, value, label) => { if (seen.has(value)) fail(`Duplicate ${label}: ${value}`); seen.add(value); };
  const buildings = new Map(data.rules.buildings.buildings.map(b => [b.id, b]));
  const resources = new Set(data.rules.resources.resources.map(r => r.id));
  const menu = new Set(data.rules.buildings.build_menu);
  const scenario = data.rules.scenario.scenario;
  const policies = data.rules.policies.policies;
  const buildable = (id, label) => {
    text(id, label);
    if (!menu.has(id)) fail(`${label} references unknown or unbuildable definition: ${id}`);
    return buildings.get(id);
  };

  const { ai, developed, ui } = data;
  version(ai, 'AIFILES/colony_ai.json');
  version(developed, 'AIFILES/developed_start.json');
  version(ui, 'Interface/ui.json');
  number(ai.decision_interval_seconds, 'AI decision_interval_seconds', policies.fixed_step_seconds, 3600);
  number(ai.max_actions_per_decision, 'AI max_actions_per_decision', 1, 16, true);
  number(ai.max_sensors, 'AI max_sensors', 1, 128, true);
  number(ai.developed_setup_action_limit, 'AI developed_setup_action_limit', 1, 512, true);
  if (buildable(ai.sensor_definition, 'AI sensor_definition').sensor_range <= 0) fail('AI sensor_definition must provide sensor coverage');
  const placement = object(ai.placement, 'AI placement');
  const extent = scenario.world_half_size;
  number(placement.ring_start, 'AI placement.ring_start', 1e-8, extent * 2);
  number(placement.ring_step, 'AI placement.ring_step', 1e-8, extent * 2);
  number(placement.ring_limit, 'AI placement.ring_limit', placement.ring_start, extent * 2);
  number(placement.angles, 'AI placement.angles', 4, 128, true);
  number(placement.node_clearance, 'AI placement.node_clearance', 0, extent);
  number(placement.sensor_overlap, 'AI placement.sensor_overlap', 1e-8, 1);
  number(placement.defense_distance, 'AI placement.defense_distance', 0, extent);
  if (((placement.ring_limit - placement.ring_start) / placement.ring_step + 1) * placement.angles > 4096) fail('AI placement search exceeds its supported candidate limit');
  const targets = array(ai.build_targets, 'AI build_targets', 1, 128);
  const finalCounts = new Map();
  for (const [index, target] of targets.entries()) {
    object(target, `AI build_targets[${index}]`);
    buildable(target.definition, `AI build_targets[${index}].definition`);
    number(target.count, `AI build_targets[${index}].count`, 1, 128, true);
    if ((finalCounts.get(target.definition) ?? 0) >= target.count) fail(`Repeated AI target counts must increase: ${target.definition}`);
    finalCounts.set(target.definition, target.count);
  }
  number(developed.population, 'Developed population', scenario.starting_population, 2147483647, true);
  object(developed.inventory, 'Developed inventory');
  let stock = 0;
  for (const [id, amount] of Object.entries(developed.inventory)) {
    if (!resources.has(id)) fail(`Developed inventory references unknown resource: ${id}`);
    stock += number(amount, `Developed inventory.${id}`, 0, Number.MAX_VALUE);
  }
  if (stock > buildings.get(scenario.core_definition).storage_capacity) fail('Developed inventory exceeds core capacity');
  // Necessary setup bounds only; geometry, staffing and actual AI behavior require native tests.
  const cost = new Map();
  let setupActions = 0;
  for (const [id, count] of finalCounts) {
    setupActions += count;
    for (const [resource, amount] of Object.entries(buildings.get(id).cost)) cost.set(resource, (cost.get(resource) ?? 0) + amount * count);
  }
  if (setupActions > ai.developed_setup_action_limit) fail('Developed setup action limit cannot establish the requested building targets');
  for (const [id, amount] of cost) if ((developed.inventory[id] ?? 0) < amount) fail(`Developed seed cannot pay target construction costs for ${id}`);

  const shortcut = (value, label) => {
    const key = text(value, label).toUpperCase();
    if (!/^[A-Z]$/.test(key) || key === 'B') fail(`${label} must be one letter A-Z, excluding reserved construction key B`);
    return key;
  };
  const groupIds = new Set(), groupKeys = new Set(), assigned = new Set();
  const groups = array(ui.build_groups, 'UI build_groups', 1);
  for (const [index, group] of groups.entries()) {
    const label = `UI build_groups[${index}]`;
    object(group, label);
    unique(groupIds, text(group.id, `${label}.id`), 'UI group ID');
    text(group.name, `${label}.name`); text(group.description, `${label}.description`);
    unique(groupKeys, shortcut(group.shortcut, `${label}.shortcut`), 'UI category shortcut');
    const entryKeys = new Set();
    for (const [entryIndex, entry] of array(group.entries, `${label}.entries`, 1).entries()) {
      const entryLabel = `${label}.entries[${entryIndex}]`;
      object(entry, entryLabel);
      buildable(entry.definition, `${entryLabel}.definition`);
      unique(assigned, entry.definition, 'UI building entry');
      unique(entryKeys, shortcut(entry.shortcut, `${entryLabel}.shortcut`), `UI shortcut in ${group.id}`);
    }
  }
  for (const id of menu) if (!assigned.has(id)) fail(`Build-menu definition has no UI entry: ${id}`);
  const summaryIds = new Set(), summaryLabels = new Set();
  for (const [index, row] of array(ui.summary_resources, 'UI summary_resources', 1).entries()) {
    object(row, `UI summary_resources[${index}]`);
    text(row.resource, `UI summary_resources[${index}].resource`);
    if (!resources.has(row.resource)) fail(`UI summary references unknown resource: ${row.resource}`);
    unique(summaryIds, row.resource, 'UI summary resource');
    unique(summaryLabels, text(row.label, `UI summary_resources[${index}].label`), 'UI summary label');
  }
  const creditHeadings = new Set();
  for (const [index, row] of array(ui.credits, 'UI credits', 1).entries()) {
    object(row, `UI credits[${index}]`);
    unique(creditHeadings, text(row.heading, `UI credits[${index}].heading`), 'UI credit heading');
    text(row.text, `UI credits[${index}].text`);
  }
  return { ...rules, aiTargets: targets.length, developedStock: stock, uiGroups: groups.length, uiBuildings: assigned.size, summaryResources: summaryIds.size, credits: creditHeadings.size };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const result = validateConfiguration(readConfiguration(path.resolve(process.argv[2] ?? defaultRoot)));
    console.log(`Configuration valid: ${result.version}; ${result.resources} items, ${result.recipes} recipes, ${result.buildings} buildings.`);
    console.log(`AI: ${result.aiTargets} priorities, ${result.developedStock} developed starting items. Interface: ${result.uiGroups} groups, ${result.uiBuildings} buildings, ${result.summaryResources} summary resources, ${result.credits} credits.`);
  } catch (error) {
    console.error(`CONFIGURATION VALIDATION FAILED: ${error.message}`);
    process.exitCode = 1;
  }
}

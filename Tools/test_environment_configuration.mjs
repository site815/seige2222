import fs from 'node:fs';
import assert from 'node:assert/strict';
import {validateEnvironment} from './validate_environment.mjs';
const original=JSON.parse(fs.readFileSync(new URL('../Rules/environment.json',import.meta.url),'utf8'));
assert.equal(validateEnvironment(original),true);
const invalid=[
  e=>e.enabled='yes', e=>e.river=[], e=>e.river[1].height=e.river[0].height+1,
  e=>e.river[1]={...e.river[0]}, e=>e.lakes[0].height+=1,
  e=>e.river_half_width=0, e=>e.lakes[0].radius_x=-1, e=>e.cliffs[0].edge_ratio=0,
  e=>e.valley_width=e.bank_width, e=>e.north_slope=-.1, e=>e.base_elevation=NaN,
  e=>e.additional_rivers[0][1].height=e.additional_rivers[0][0].height+1,
  e=>e.additional_rivers[0]=[], e=>e.lakes[0].shore_variation=1,
  e=>e.cliffs[0].angle_degrees=Infinity, e=>e.cliffs[0].irregularity=-1,
  e=>e.fords[0].depth=50, e=>e.fords[0].radius=20, e=>e.fords[0].x+=10000,
];
for(const mutate of invalid){const copy=structuredClone(original);mutate(copy);assert.throws(()=>validateEnvironment(copy));}
const editable=structuredClone(original);editable.river_half_width+=20;editable.bank_width+=50;assert.equal(validateEnvironment(editable),true);
console.log(`Environment: authored + edited profile valid; ${invalid.length} malformed physical profiles rejected.`);

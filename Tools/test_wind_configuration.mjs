import assert from 'node:assert/strict';
import {readConfiguration, validateConfiguration} from './validate_configuration.mjs';

const data=readConfiguration();
assert.doesNotThrow(()=>validateConfiguration(data));
const invalid=[
 ['wind_cycle_seconds',0],['wind_maximum_playback_rate',0],['wind_maximum_playback_rate',21],['tree_wind_sway_centimeters',31],
 ['leaf_wind_rustle_centimeters',9],['grass_wind_sway_centimeters',11],
 ['tree_wind_distance_meters',401],['wind_fade_start_fraction',1],
];
for(const [key,value] of invalid){
 const candidate=structuredClone(data);candidate.weather[key]=value;
 assert.throws(()=>validateConfiguration(candidate),new RegExp(key));
}
const quiet=structuredClone(data);
quiet.weather.tree_wind_sway_centimeters=0;
quiet.weather.leaf_wind_rustle_centimeters=0;
quiet.weather.grass_wind_sway_centimeters=0;
assert.doesNotThrow(()=>validateConfiguration(quiet));
console.log(`Foliage wind: authored and zero-amplitude profiles valid; ${invalid.length} invalid clock/bounds/range settings rejected.`);

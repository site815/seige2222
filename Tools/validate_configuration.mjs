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
  const availableAssetPackages = new Set();
  const contentDirectory = path.join(root, 'Content');
  const indexAssets = directory => {
    for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
      const filename = path.join(directory, entry.name);
      if (entry.isDirectory()) indexAssets(filename);
      else if (entry.isFile() && entry.name.endsWith('.uasset')) availableAssetPackages.add(`/Game/${path.relative(contentDirectory, filename).split(path.sep).join('/').slice(0, -7)}`);
    }
  };
  indexAssets(contentDirectory);
  return {
    rules: readRules(path.join(root, 'Rules')),
    ai: read('AIFILES/colony_ai.json'),
    developed: read('AIFILES/developed_start.json'),
    ui: read('Interface/ui.json'),
    graphics: read('Graphics/scene.json'),
    weather: read('Graphics/weather.json'),
    buildingVisuals: fs.existsSync(path.join(root, 'Graphics/building_visuals.json')) ? read('Graphics/building_visuals.json') : null,
    kitManifest: fs.existsSync(path.join(root, 'Art/BuildingKitV092/kit_manifest.json')) ? read('Art/BuildingKitV092/kit_manifest.json') : null,
    availableAssetPackages,
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
  const resources = new Map(data.rules.resources.resources.map(r => [r.id,r]));
  const menu = new Set(data.rules.buildings.build_menu);
  const scenario = data.rules.scenario.scenario;
  const policies = data.rules.policies.policies;
  const buildable = (id, label) => {
    text(id, label);
    if (!menu.has(id)) fail(`${label} references unknown or unbuildable definition: ${id}`);
    return buildings.get(id);
  };

  const { ai, developed, ui, graphics } = data;
  version(ai, 'AIFILES/colony_ai.json');
  version(developed, 'AIFILES/developed_start.json');
  version(ui, 'Interface/ui.json');
  if ('frontend' in ui) {
    const frontend = object(ui.frontend, 'UI frontend');
    for (const key of ['title', 'eyebrow', 'tagline']) if (key in frontend) text(frontend[key], `UI frontend ${key}`);
  }
  if ('simulation_speeds' in ui) {
    const speeds = array(ui.simulation_speeds, 'UI simulation_speeds', 3, 3);
    if (new Set(speeds).size !== 3 || !speeds.every(value => [1, 5, 10].includes(value))) fail('UI simulation_speeds must contain 1, 5 and 10 once each');
  }
  const weather=object(data.weather,'Graphics/weather.json');version(weather,'Graphics/weather.json');
  const weatherBounds={night_exposure_offset_ev:[0,4],sun_direction_update_degrees:[.01,2],sun_shadow_update_seconds:[.05,10],night_sky_intensity_fraction:[.05,1],sunrise_sunset_softness:[.01,.5],winter_accumulation_fraction:[.001,.49],winter_melt_fraction:[.001,.49],maximum_snow_coverage:[0,1],snowflake_count:[0,2048,true],snow_radius_meters:[5,100],snow_height_meters:[5,100],snow_fall_meters_per_second:[.1,10],snowflake_size_centimeters:[.1,10]};
  for(const [key,[min,max,integer]]of Object.entries(weatherBounds))number(weather[key],`Weather ${key}`,min,max,!!integer);
  // Optional building art overrides: every key must be a building definition and every kind an imported mesh.
  if (data.buildingVisuals !== null && data.buildingVisuals !== undefined) {
    const visuals = object(data.buildingVisuals, 'Graphics/building_visuals.json'); version(visuals, 'Graphics/building_visuals.json');
    const entries = object(visuals.visuals, 'Graphics/building_visuals.json visuals');
    for (const [id, kind] of Object.entries(entries)) {
      if (!buildings.has(id)) fail(`building_visuals references an unknown building: ${id}`);
      text(kind, `building_visuals.${id}`); if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(kind)) fail(`building_visuals.${id} kind must be alphanumeric`);
      const mesh = `/Game/Art/SM_${kind[0].toUpperCase()}${kind.slice(1)}`;
      if (!data.availableAssetPackages.has(mesh)) fail(`building_visuals.${id} references a missing Content mesh: ${mesh}`);
    }
    // Optional docking points: [x, y, z, yaw] per mesh kind, in the mesh's own
    // centimetres; a kit mesh's points must match its generated manifest.
    if (visuals.berths !== undefined) {
      const berths = object(visuals.berths, 'Graphics/building_visuals.json berths');
      const kinds = new Set(Object.values(entries).concat([...buildings.values()].map(b => b.visual)));
      for (const [kind, points] of Object.entries(berths)) {
        if (!kinds.has(kind)) fail(`building_visuals.berths names a kind no building uses: ${kind}`);
        const list = array(points, `building_visuals.berths.${kind}`, 1, 16);
        list.forEach((point, index) => {
          const values = array(point, `building_visuals.berths.${kind}[${index}]`, 4, 4);
          values.forEach(value => { if (typeof value !== 'number' || !Number.isFinite(value)) fail(`building_visuals.berths.${kind}[${index}] must hold numbers`); });
          if (Math.abs(values[0]) > 5000 || Math.abs(values[1]) > 5000 || values[2] < 0 || values[2] > 2000 || Math.abs(values[3]) > 360) fail(`building_visuals.berths.${kind}[${index}] lies outside its mesh range`);
        });
        const generated = data.kitManifest?.meshes?.[`SM_${kind[0].toUpperCase()}${kind.slice(1)}`]?.berths_unreal;
        if (generated && (generated.length !== list.length || generated.some((point, index) => point.some((value, axis) => Math.abs(value - list[index][axis]) > (axis === 3 ? .5 : 1)))))
          fail(`building_visuals.berths.${kind} differs from the generated kit manifest (Art/BuildingKitV092/kit_manifest.json)`);
      }
    }
  }
  for(const key of ['ambient_cubemap','snow_collection','snowflake_material']){text(weather[key],`Weather ${key}`);if(!/^\/Game\/(?:[A-Za-z0-9_]+\/)*[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)?$/.test(weather[key]))fail(`Weather ${key} requires a game asset`);}
  version(graphics, 'Graphics/scene.json');
  number(graphics.world_centimeters_per_unit, 'Graphics world_centimeters_per_unit', 1, 20);
  if(Math.abs(graphics.world_centimeters_per_unit/100-data.rules.transport.transport.meters_per_world_unit)>1e-9)fail('Graphics and transport physical scales must agree');
  number(graphics.nanite_max_pixels_per_edge, 'Graphics nanite_max_pixels_per_edge', .5, 8);
  number(graphics.nanite_survey_pixels_per_edge, 'Graphics nanite_survey_pixels_per_edge', .5, 8);
  number(graphics.nanite_survey_start_zoom, 'Graphics nanite_survey_start_zoom', 5000, 60000);
  number(graphics.nanite_survey_end_zoom, 'Graphics nanite_survey_end_zoom', 10000, 200000);
  number(graphics.camera_fov, 'Graphics camera_fov', 35, 80);
  number(graphics.camera_pitch, 'Graphics camera_pitch', 5, 85);
  number(graphics.minimum_camera_pitch, 'Graphics minimum_camera_pitch', 5, 25);
  number(graphics.maximum_camera_pitch, 'Graphics maximum_camera_pitch', 60, 85);
  if (graphics.camera_pitch < graphics.minimum_camera_pitch || graphics.camera_pitch > graphics.maximum_camera_pitch) fail('Graphics default camera pitch lies outside orbit limits');
  number(graphics.camera_ground_clearance_cm, 'Graphics camera_ground_clearance_cm', 100, 500);
  number(graphics.sun_intensity, 'Graphics sun_intensity', .1, 20);
  number(graphics.sky_intensity, 'Graphics sky_intensity', .1, 5);
  number(graphics.cloud_shadow_strength, 'Graphics cloud_shadow_strength', 0, 1);
  number(graphics.sun_source_angle, 'Graphics sun_source_angle', .1, 5);
  number(graphics.sun_elevation_degrees, 'Graphics sun_elevation_degrees', 15, 80);
  number(graphics.exposure_bias, 'Graphics exposure_bias', -2, 2);
  number(graphics.color_saturation, 'Graphics color_saturation', .5, 1.5);
  number(graphics.ambient_occlusion_intensity, 'Graphics ambient_occlusion_intensity', 0, 1);
  number(graphics.cloud_shadow_resolution_scale, 'Graphics cloud_shadow_resolution_scale', .25, 2);
  if (typeof graphics.sky_realtime_capture !== 'boolean') fail('Graphics sky_realtime_capture must be boolean');
  number(graphics.fog_density, 'Graphics fog_density', 0, .01);
  number(graphics.fog_start_distance_m, 'Graphics fog_start_distance_m', 0, 10000);
  number(graphics.atmosphere_mie_scale, 'Graphics atmosphere_mie_scale', 0, 2);
  number(graphics.atmosphere_aerial_perspective_scale, 'Graphics atmosphere_aerial_perspective_scale', 0, 3);
  number(graphics.bloom_intensity, 'Graphics bloom_intensity', 0, 1);
  number(graphics.color_contrast, 'Graphics color_contrast', .5, 1.5);
  number(graphics.vignette_intensity, 'Graphics vignette_intensity', 0, 1);
  number(graphics.ambient_occlusion_radius_cm, 'Graphics ambient_occlusion_radius_cm', 20, 500);
  number(graphics.sun_color_temperature_kelvin, 'Graphics sun_color_temperature_kelvin', 2000, 12000);
  number(graphics.fog_height_falloff, 'Graphics fog_height_falloff', .01, 2);
  number(graphics.fog_inscattering_luminance, 'Graphics fog_inscattering_luminance', 0, 5);
  number(graphics.fog_max_opacity, 'Graphics fog_max_opacity', 0, 1);
  number(graphics.sky_lower_hemisphere_luminance, 'Graphics sky_lower_hemisphere_luminance', 0, 1);
  if (typeof graphics.grass_far_proxy !== 'boolean') fail('Graphics grass_far_proxy must be boolean');
  number(graphics.camera_yaw, 'Graphics camera_yaw', -360, 360);
  number(graphics.default_zoom, 'Graphics default_zoom', 900, 20000);
  number(graphics.minimum_zoom, 'Graphics minimum_zoom', 60, 2000);
  number(graphics.maximum_zoom, 'Graphics maximum_zoom', 180000, 720000);
  number(graphics.camera_zoom_response, 'Graphics camera_zoom_response', 1, 30);
  if (![512, 1024].includes(graphics.detailed_terrain_resolution)) fail('Graphics detailed_terrain_resolution must be 512 or 1024');
  number(graphics.rolling_terrain_wavelength, 'Graphics rolling_terrain_wavelength', 1000, 10000);
  number(graphics.rolling_terrain_amplitude, 'Graphics rolling_terrain_amplitude', 0, 1000);
  number(graphics.micro_terrain_wavelength, 'Graphics micro_terrain_wavelength', 100, 1000);
  number(graphics.micro_terrain_amplitude, 'Graphics micro_terrain_amplitude', 0, 50);
  number(graphics.core_pad_inner_ratio, 'Graphics core_pad_inner_ratio', 1, 2);
  number(graphics.core_pad_outer_ratio, 'Graphics core_pad_outer_ratio', 1.1, 4);
  if (graphics.core_pad_outer_ratio <= graphics.core_pad_inner_ratio) fail('Graphics pad outer ratio must exceed inner ratio');
  number(graphics.ridge_center_x, 'Graphics ridge_center_x', -90000, 90000);
  number(graphics.ridge_center_y, 'Graphics ridge_center_y', -90000, 90000);
  number(graphics.ridge_angle_degrees, 'Graphics ridge_angle_degrees', -360, 360);
  number(graphics.ridge_width, 'Graphics ridge_width', 500, 10000);
  number(graphics.ridge_length, 'Graphics ridge_length', 1000, 30000);
  number(graphics.ridge_height, 'Graphics ridge_height', 0, 1500);
  if (1500 + graphics.rolling_terrain_amplitude + graphics.micro_terrain_amplitude + graphics.ridge_height >= 6000) fail('Graphics relief exceeds ground-trace bounds');
  if (graphics.minimum_zoom > graphics.default_zoom) fail('Graphics minimum_zoom exceeds default_zoom');
  if (graphics.minimum_zoom >= graphics.default_zoom * .45) fail('Graphics minimum_zoom must lie below the close-view transition');
  number(graphics.forest_candidates, 'Graphics forest_candidates', 1000, 200000, true);
  number(graphics.near_forest_candidates, 'Graphics near_forest_candidates', 100, 30000, true);
  number(graphics.grass_shadow_distance_m, 'Graphics grass_shadow_distance_m', 0, 500);
  number(graphics.grass_wind_distance_m, 'Graphics grass_wind_distance_m', 0, 150);
  number(graphics.grass_detail_distance_m, 'Graphics grass_detail_distance_m', 10, 150);
  number(graphics.grass_lod_transition_m, 'Graphics grass_lod_transition_m', 10, 200);
  number(graphics.grass_stream_radius_m, 'Graphics grass_stream_radius_m', 150, 1200);
  number(graphics.grass_stream_budget_ms, 'Graphics grass_stream_budget_ms', .5, 8);
  number(graphics.grass_stream_cells_per_frame, 'Graphics grass_stream_cells_per_frame', 1, 32, true);
  number(graphics.forest_detail_distance_m, 'Graphics forest_detail_distance_m', 75, 1000);
  number(graphics.forest_lod_transition_m, 'Graphics forest_lod_transition_m', 20, 500);
  if (graphics.grass_stream_radius_m <= graphics.grass_detail_distance_m + graphics.grass_lod_transition_m) fail('Grass proxy streaming must extend beyond the detail transition');
  number(graphics.grass_programmable_distance_m, 'Graphics grass_programmable_distance_m', 0, 900);
  number(graphics.forest_programmable_distance_m, 'Graphics forest_programmable_distance_m', 0, 2000);
  if(typeof graphics.grass_distance_field_lighting !== 'boolean') fail('Graphics grass_distance_field_lighting must be boolean');
  number(graphics.neighboring_forest_candidates_per_sector, 'Graphics neighboring_forest_candidates_per_sector', 0, 12000, true);
  if(typeof graphics.neighboring_forest_shadow !== 'boolean') fail('Graphics neighboring_forest_shadow must be boolean');
  number(graphics.region_map_zoom, 'Graphics region_map_zoom', 60000, 240000);
  number(graphics.region_map_transition_width, 'Graphics region_map_transition_width', 5000, 60000);
  if (graphics.region_map_zoom - graphics.region_map_transition_width / 2 < 60000 || graphics.region_map_zoom + graphics.region_map_transition_width / 2 >= graphics.maximum_zoom) fail('Graphics region transition must follow the sector overview and end before maximum zoom');
  if (graphics.nanite_survey_pixels_per_edge < graphics.nanite_max_pixels_per_edge || graphics.nanite_survey_start_zoom < graphics.default_zoom || graphics.nanite_survey_end_zoom <= graphics.nanite_survey_start_zoom || graphics.nanite_survey_end_zoom > graphics.region_map_zoom - graphics.region_map_transition_width / 2) fail('Invalid Nanite survey transition');
  const medium = object(graphics.medium_profile, 'Graphics medium_profile');
  for (const key of ['native_antialiasing', 'upscaling_antialiasing']) {
    if (!['taa', 'tsr'].includes(medium[key])) fail(`Invalid Medium ${key}`);
  }
  const quality = object(medium.quality_groups, 'Medium quality_groups');
  const mediumGroups = ['ViewDistance', 'AntiAliasing', 'Shadow', 'GlobalIllumination', 'Reflection', 'PostProcess', 'Texture', 'Effects', 'Foliage', 'Shading', 'Landscape'];
  if (Object.keys(quality).length !== mediumGroups.length) fail('Invalid Medium quality groups');
  for (const key of mediumGroups) number(quality[key], `Medium quality ${key}`, 0, 3, true);
  const rendering = object(medium.render_settings, 'Medium render_settings');
  const ranges = [['r.TSR.History.ScreenPercentage', 100, 200, false], ['r.TSR.ThinGeometryDetection', 0, 1, true], ['r.TSR.ThinGeometryDetection.Coverage.ShadingRange', 0, 3, true], ['r.TSR.Velocity.WeightClampingSampleCount', 1, 8, false], ['r.Tonemapper.Sharpen', 0, 1, false], ['r.MaxAnisotropy', 4, 16, true], ['r.TemporalAA.Quality', 1, 2, true], ['r.TemporalAAFilterSize', .5, 1, false], ['r.TemporalAACurrentFrameWeight', .04, .2, false], ['r.Shadow.Virtual.SMRT.RayCountDirectional', 1, 8, true], ['r.Shadow.Virtual.SMRT.SamplesPerRayDirectional', 1, 8, true],
    ['r.Shadow.Virtual.ResolutionLodBiasDirectional', -2, 3, false], ['r.Shadow.Virtual.ResolutionLodBiasDirectionalMoving', -2, 3, false], ['r.VolumetricCloud.ViewRaySampleMaxCount', 32, 2048, true], ['r.VolumetricCloud.ShadowMap.RaySampleMaxCount', 8, 512, true], ['r.VolumetricCloud.ReflectionRaySampleMaxCount', 8, 512, true], ['r.HZBOcclusion', 0, 1, true]]; // keep identical to SeigeCamera.cpp
  if (Object.keys(rendering).length !== ranges.length) fail('Invalid Medium rendering settings');
  for (const [key, min, max, integer] of ranges) number(rendering[key], `Medium rendering ${key}`, min, max, integer);
  number(graphics.orbit_yaw_degrees_per_pixel, 'Graphics orbit_yaw_degrees_per_pixel', .05, 2);
  number(graphics.orbit_pitch_degrees_per_pixel, 'Graphics orbit_pitch_degrees_per_pixel', .05, 2);
  number(graphics.ground_cover_candidates, 'Graphics ground_cover_candidates', 10000, 400000, true);
  number(graphics.grass_scale_min, 'Graphics grass_scale_min', .1, 3);
  number(graphics.grass_scale_max, 'Graphics grass_scale_max', .1, 3);
  if (graphics.grass_scale_min > graphics.grass_scale_max) fail('Graphics grass scale range is reversed');
  const natureAssets = object(graphics.nature_assets, 'Graphics nature_assets');
  const natureRoles = ['OakA', 'OakB', 'PineA', 'PineB', 'Shrub', 'Grass', 'GrassB', 'Wildflowers', 'RockA', 'RockB'];
  for (const role of natureRoles) text(natureAssets[role], `Graphics nature_assets.${role}`);
  if (!(data.availableAssetPackages instanceof Set)) fail('Configuration is missing its project asset index; load it with readConfiguration');
  for (const [role, asset] of Object.entries({...natureAssets, terrain_material: graphics.terrain_material, cloud_material: graphics.cloud_material, grass_proxy_asset: graphics.grass_proxy_asset, broadleaf_proxy_asset: graphics.broadleaf_proxy_asset, conifer_proxy_asset: graphics.conifer_proxy_asset})) {
    text(asset, `Graphics nature_assets.${role}`);
    // This bundled Unreal material is explicitly included by the cooker; custom
    // replacements must be project assets so missing packages are caught here.
    if (role === 'cloud_material' && asset === '/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst') continue;
    if (!/^\/Game\/(?:[A-Za-z0-9_]+\/)*[A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)?$/.test(asset)) fail(`Graphics nature_assets.${role} must be a /Game asset path`);
    const [packageName, objectName] = asset.split('.');
    if (objectName && objectName !== packageName.split('/').at(-1)) fail(`Graphics nature_assets.${role} has a mismatched object name`);
    if (!data.availableAssetPackages.has(packageName)) fail(`Graphics nature_assets.${role} references a missing Content asset: ${packageName}`);
  }
  number(ai.decision_interval_seconds, 'AI decision_interval_seconds', policies.fixed_step_seconds, 3600);
  number(ai.max_actions_per_decision, 'AI max_actions_per_decision', 1, 16, true);
  if (!['independent_tactics_trade_construction', 'shared_action_budget'].includes(ai.decision_scheduling_policy)) fail('Unsupported AI decision_scheduling_policy');
  const guardService=object(ai.guard_service,'AI guard_service');
  number(guardService.recharge_below_fraction,'AI guard_service.recharge_below_fraction',1e-8,1);
  number(guardService.resume_above_fraction,'AI guard_service.resume_above_fraction',guardService.recharge_below_fraction,1);
  if(guardService.resume_above_fraction<=guardService.recharge_below_fraction)fail('AI guard service resume threshold must exceed recharge threshold');
  number(ai.max_sensors, 'AI max_sensors', 1, 128, true);
  number(ai.developed_setup_action_limit, 'AI developed_setup_action_limit', 1, 512, true);
  if (ai.target_policy !== 'complete_and_staff_in_order') fail('Unsupported AI target_policy');
  if (ai.support_recovery_policy !== 'restore_capacity_before_expansion') fail('Unsupported AI support_recovery_policy');
  const economy=object(ai.economy,'AI economy');
  if(buildable(economy.solar_definition,'AI solar_definition').role!=='generator'||buildable(economy.trade_definition,'AI trade_definition').role!=='trade')fail('AI bootstrap needs a generator and trading port');
  number(economy.export_batch,'AI export_batch',1e-8,10000);number(economy.import_batch,'AI import_batch',1e-8,10000);
  number(economy.recipe_input_buffer_cycles,'AI recipe_input_buffer_cycles',1,100);number(economy.credit_buffer_batches,'AI credit_buffer_batches',0,100);
  number(economy.fuel_import_buffer_cycles,'AI fuel_import_buffer_cycles',1,100);number(economy.fuel_import_refill_fraction,'AI fuel_import_refill_fraction',1e-8,1);
  if(!['remaining_output_bill','recipe_buffers'].includes(economy.bulk_input_policy))fail('Unsupported AI bulk_input_policy');
  if(!['surplus_shipment_value','local_raw_only'].includes(economy.export_policy))fail('Unsupported AI export_policy');
  if(economy.core_replication_policy!=='funded_shortage_first')fail('Unsupported AI core_replication_policy');
  const replicationRecipes=array(economy.core_replication_recipes,'AI core_replication_recipes',1,128), replicationSeen=new Set();
  for(const id of replicationRecipes){const recipe=data.rules.recipes.recipes.find(r=>r.id===id);if(replicationSeen.has(id)||!recipe||recipe.worker_output>0||!Object.keys(recipe.outputs).length||!buildings.get(scenario.core_definition).allowed_recipes.includes(id))fail(`Invalid AI core_replication_recipes entry: ${id}`);replicationSeen.add(id);}
  for(const[id,n]of Object.entries(object(economy.reserve_targets,'AI reserve_targets'))){if(!resources.has(id))fail(`Unknown AI reserve target ${id}`);number(n,`AI reserve_targets.${id}`,0,Number.MAX_VALUE);}
  number(ai.developed_setup_seconds, 'AI developed_setup_seconds', ai.decision_interval_seconds, 172800);
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
  if (!['prefer_covered_approaches','first_legal'].includes(placement.defense_coverage_policy)) fail('Unsupported AI placement.defense_coverage_policy');
  number(placement.coverage_samples, 'AI placement.coverage_samples', 8, 64, true);
  number(placement.coverage_probe_distance_meters, 'AI placement.coverage_probe_distance_meters', 1e-8, 100);
  const coverageExcluded = new Set(), knownBuildingRoles = new Set([...buildings.values()].map(building => building.role));
  for (const role of array(placement.coverage_excluded_roles, 'AI placement.coverage_excluded_roles', 1)) {
    if (!knownBuildingRoles.has(role)) fail('Unknown AI placement.coverage_excluded_roles entry');
    unique(coverageExcluded, role, 'AI coverage excluded role');
  }
  if (((placement.ring_limit - placement.ring_start) / placement.ring_step + 1) * placement.angles > 4096) fail('AI placement search exceeds its supported candidate limit');
  const targets = array(ai.build_targets, 'AI build_targets', 1, 128);
  const finalCounts = new Map();
  for (const [index, target] of targets.entries()) {
    object(target, `AI build_targets[${index}]`);
    buildable(target.definition, `AI build_targets[${index}].definition`);
    number(target.count, `AI build_targets[${index}].count`, 1, 128, true);
    number(target.placement_index, `AI build_targets[${index}].placement_index`, 0, 4096, true);
    if (target.optional !== undefined && typeof target.optional !== 'boolean') fail(`AI build_targets[${index}].optional must be a boolean`);
    if ((finalCounts.get(target.definition) ?? 0) >= target.count) fail(`Repeated AI target counts must increase: ${target.definition}`);
    finalCounts.set(target.definition, target.count);
  }
  const upgrades = object(ai.upgrades, 'AI upgrades'), upgradeFamilies = new Set();
  number(upgrades.max_concurrent, 'AI upgrades.max_concurrent', 0, 8, true);
  number(upgrades.defense_quiet_radius, 'AI upgrades.defense_quiet_radius', 0, extent * 2);
  for (const family of array(upgrades.families, 'AI upgrades.families', 0, 32)) {
    if (![...buildings.values()].some(b => b.family === family && b.next_upgrade && menu.has(b.id))) fail(`AI upgrades.families names no upgradable buildable family: ${family}`);
    unique(upgradeFamilies, family, 'AI upgrade family');
  }
  if (!['established_manifest','simulated_history'].includes(ai.developed_initialization)) fail('Invalid AI developed_initialization');
  if (developed.kind !== 'established_colony' || developed.equipment !== 'definition_defaults' || developed.fleet !== 'scenario_guard_manifest') fail('Invalid established kind/equipment/fleet');
  if (developed.road_tier !== data.rules.transport.transport.initial_tier) fail('Invalid established road_tier');
  number(developed.age_seconds, 'Established age_seconds', 60, 31536000);
  number(developed.credits, 'Established credits', 0, 1000000);
  number(developed.idle_workers, 'Established idle_workers', 0, 1000, true);
  number(developed.layout_rotations, 'Established layout_rotations', 1, 4, true);
  const established = array(developed.buildings, 'Established buildings', 1, 128);
  let stock = 0, operators = developed.idle_workers, capacity = 0, mines = 0;
  const fixedPlots = [];
  for (const [i, row] of established.entries()) {
    object(row, `Established building ${i}`);
    const def = buildings.get(row.definition);
    // Rows after the core are buildable blueprints or upgraded levels of one.
    let root = def;
    while (root) { const parent = [...buildings.values()].find(b => b.next_upgrade === root.id); if (!parent) break; root = parent; }
    if (!def || (i === 0 ? row.definition !== scenario.core_definition : !menu.has(root.id) || def.role === 'core')) fail('Established buildings must begin with one valid command core');
    if (!['core','deposit'].includes(row.anchor) || (def.role === 'extractor') !== (row.anchor === 'deposit')) fail('Invalid established anchor');
    if (row.anchor === 'deposit') mines++;
    array(row.offset_meters, 'Established offset_meters', 2, 2).forEach(v => number(v, 'Established offset_meters', -1800, 1800));
    if ((i === 0 || row.anchor === 'deposit') && row.offset_meters.some(v => v !== 0)) fail('Core and bound deposit must have zero offset_meters');
    number(row.operators, 'Established operators', def.jobs, def.jobs, true); operators += row.operators; capacity += def.robot_support_capacity;
    number(row.battery_kwh, 'Established battery_kwh', 0, data.rules.energy.energy.buildings[row.definition].battery_capacity_kwh);
    if (typeof row.recipe !== 'string' || (row.recipe && row.recipe !== def.recipe && !def.allowed_recipes.includes(row.recipe))) fail('Invalid established recipe');
    object(row.inventory, 'Established inventory'); let volume = 0;
    for (const [id, amount] of Object.entries(row.inventory)) {
      const resource = resources.get(id); if (!resource) fail('Unknown established inventory resource');
      number(amount, 'Established inventory amount', 0, 1e9, resource.discrete);
      if (id === 'stored_workers' && !def.stores_inactive_workers) fail('Established inventory stores workers in an incompatible building');
      volume += amount * resource.litres_per_unit;
    }
    if (volume > def.storage_capacity + 1e-8) fail('Established inventory exceeds capacity'); stock += volume;
    if (row.anchor === 'core') {
      const radius = def.reserved_footprint * data.rules.transport.transport.meters_per_world_unit;
      for (const other of fixedPlots) if (Math.abs(row.offset_meters[0]-other.x) < radius+other.radius+policies.minimum_build_spacing*data.rules.transport.transport.meters_per_world_unit && Math.abs(row.offset_meters[1]-other.y) < radius+other.radius+policies.minimum_build_spacing*data.rules.transport.transport.meters_per_world_unit) fail('Established reserved plots overlap');
      fixedPlots.push({x:row.offset_meters[0], y:row.offset_meters[1], radius});
    }
  }
  if (mines !== 1 || operators > capacity) fail('Established manifest needs one bound mine and supported worker capacity');
  const workerRules = data.rules.workers;
  const eligible = established.filter(row => !workerRules.logistics_excluded_roles.includes(buildings.get(row.definition).role)).length;
  const logistics = Math.min(workerRules.logistics_max_workers, workerRules.logistics_workers + (workerRules.logistics_scaling_policy === 'completed_facilities' ? Math.floor(eligible / workerRules.logistics_facilities_per_worker) : 0));
  if (developed.idle_workers < logistics) fail('Established idle_workers must cover actual logistics jobs');

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
      if(!['road','upgrade_road','wall'].includes(entry.definition)) buildable(entry.definition, `${entryLabel}.definition`);
      unique(assigned, entry.definition, 'UI building entry');
      unique(entryKeys, shortcut(entry.shortcut, `${entryLabel}.shortcut`), `UI shortcut in ${group.id}`);
    }
  }
  for (const id of menu) if (!assigned.has(id)) fail(`Build-menu definition has no UI entry: ${id}`);
  const summaryIds = new Set();
  const expectedGroups = ['credits','energy','raw','basic','advanced'];
  for (const [index, group] of array(ui.resource_groups, 'UI resource_groups', 5, 5).entries()) {
    object(group, `UI resource_groups[${index}]`);
    if (group.id !== expectedGroups[index]) fail('UI resource groups must be Credits, Energy, Raw, Basic, Advanced in that order');
    text(group.label, `UI resource group ${group.id} label`);
    const entries = array(group.entries, `UI resource group ${group.id} entries`, index < 2 ? 0 : 1, index < 2 ? 0 : 8);
    for (const row of entries) {
      object(row, `UI resource group ${group.id} entry`);
      const resource = resources.get(text(row.resource, 'UI resource id'));
      if (!resource) fail(`UI summary references unknown resource: ${row.resource}`);
      if (row.resource === policies.inactive_worker_resource) fail('Inactive workers belong to workforce controls, not material groups');
      const expected = resource.class === 'manufactured' ? (resource.tier <= 1 ? 'basic' : 'advanced') : 'raw';
      if (group.id !== expected) fail(`UI resource ${row.resource} belongs to ${expected}`);
      unique(summaryIds, row.resource, 'UI grouped resource');
      text(row.label, `UI resource ${row.resource} label`);
    }
  }
  for (const id of resources.keys()) if (id !== policies.inactive_worker_resource && !summaryIds.has(id)) fail(`UI resource groups omit ${id}`);
  const creditHeadings = new Set();
  for (const [index, row] of array(ui.credits, 'UI credits', 1).entries()) {
    object(row, `UI credits[${index}]`);
    unique(creditHeadings, text(row.heading, `UI credits[${index}].heading`), 'UI credit heading');
    text(row.text, `UI credits[${index}].text`);
  }
  return { ...rules, aiTargets: targets.length, developedStock: stock, developedBuildings: established.length, developedActiveWorkers: operators, developedStoredWorkers: established.reduce((sum,row)=>sum+(row.inventory.stored_workers??0),0), uiGroups: groups.length, uiBuildings: assigned.size, summaryResources: summaryIds.size, credits: creditHeadings.size, natureRoles: natureRoles.length, renderScale: graphics.world_centimeters_per_unit };
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const result = validateConfiguration(readConfiguration(path.resolve(process.argv[2] ?? defaultRoot)));
    console.log(`Configuration valid: ${result.version}; ${result.resources} items, ${result.recipes} recipes, ${result.buildings} buildings.`);
    console.log(`AI: ${result.aiTargets} priorities, ${result.developedBuildings} established buildings, ${result.developedActiveWorkers} active + ${result.developedStoredWorkers} stored workers. Interface: ${result.uiGroups} groups, ${result.uiBuildings} building/tool entries, ${result.summaryResources} summary resources, ${result.credits} credits.`);
    console.log(`Graphics: ${result.natureRoles} nature roles resolve to Content assets; ${result.renderScale} rendered centimeters per simulation unit.`);
  } catch (error) {
    console.error(`CONFIGURATION VALIDATION FAILED: ${error.message}`);
    process.exitCode = 1;
  }
}

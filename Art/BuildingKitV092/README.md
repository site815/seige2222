# Building kit v0.9.2

Distinct silhouettes for the building families that previously shared one factory hall (23 of 28 blueprints) or the warehouse (solar arrays, battery bank, trading ports), plus the service bay, sensor mast and extraction rig. Thirty-five original meshes, generated procedurally in Blender 4.5 on the project's industrial palette; no marketplace models, no photographs.

| Mesh | Buildings | Reads as |
| --- | --- | --- |
| SM_SolarArray / 2 / 3 | solar_array L1–3 | Fixed A-frame field → single-axis trackers → dual-axis masts, inverter kiosk |
| SM_BatteryBank | battery_bank | Twelve container modules under busbar gantries, conversion hall, grid transformer |
| SM_TradingPort / 2 / 3 | trading_port L1–3 | Lit landing pad, control tower, cargo gantry, growing container stacks |
| SM_Refinery | alloy_refinery | Smelting hall, twin blast furnaces, tall stacks, inclined ore conveyor, slag pit |
| SM_FuelRefinery | fuel_refinery, biofuel_refinery | Bunded tank farm, distillation column, condenser, flare stack, pipe racks |
| SM_Greenhouse | food_producer | Four glazed gable bays, processing block, irrigation tanks |
| SM_Works | conductor, substrate, circuit, component, battery works | White clean hall, window band, skylights, chilled-water plant, gas cabinet |
| SM_ChipWorks | ai_chip_works | Two-storey cleanroom block, roof plenum, exhaust ducts, scrubber stack, nitrogen tank |
| SM_FusionWorks | fusion_reactor_works | Containment dome on a drum, turbine hall, twin cooling towers, switchyard |
| SM_AmmunitionWorks | ammunition_works | Three bermed magazines behind blast walls, lightning masts, filling hall |
| SM_FuelGenerator | fuel_generator | Twin containerised gen-sets, silenced stacks, bunded day tank, switchgear |
| SM_Hangar / Tracked / Mech | vehicle, tank and mech factories L1–3 | Barrel-roofed assembly hangars; ramp for tracked, erection gantry for mechs |
| SM_TowerLaser | turret L1–3 | Octagonal mast with heat-sink fins and capacitor rings under a round deck (2026-10-09) |
| SM_TowerKinetic | kinetic_tower L1–3 | Squat armoured bunker with sloped plates and a shell hoist under a square deck |
| SM_TowerMissile | missile_tower L1–3 | Raised launcher platform over a reload magazine, reload crane, fire-control cabin |
| SM_TowerPlasma | plasma_tower L1–3 | Stacked copper induction coils on a containment column with cooling vanes |
| SM_DepotYard | depot | Barrel-roofed warehouse, three marked loading docks, dispatch office, gantry-served container yard |
| SM_WorkerFactory | worker_factory | Dark assembly hall with a glazed front, roof clerestories, receiving dock and a test track |
| SM_ServiceBay | robot_service_bay | Three open charging berths (contact plates, pedestals, charge leads, diagnostic arms) under a canopy that covers only the pedestals, plant hall with control gallery and roof chillers, coolant plant, parts cage (2026-10-09) |
| SM_SensorMast | sensor | Tapered lattice mast with phased-array faces, a rotating search radar and beacon whip; equipment shelter, UPS cabinet, cable tray, fenced pad (2026-10-09) |
| SM_ExtractionRig | extraction_mine | Derrick on a substructure over the wellhead, top drive, pipe rack, process tanks, conveyor to an ore hopper over a loading bay, operator cabin, power skid (2026-10-09) |
| SM_Tower*2 / *3 | turret, kinetic, missile, plasma L2–3 | Level growth around the unchanged 250 cm deck: deck rails, radar mast, second locker, barrier line (L2); armoured deck skirt, floodlight masts, generator pack, surveillance mast (L3) |

The tower bases are authored at the 1080 cm tower plot with their weapon deck at **250 cm**, the fixed height at which `SeigeCombatVisuals.cpp` places the procedural mounts, so every level's mounts sit on the deck. The 2026-10-09 contrast pass gave the halls dark roof membranes, dark aprons with lane markings in front of doors and light hangar walls under dark barrel roofs, so buildings separate from the ground and from each other at colony zoom.

Every buildable blueprint except the procedural wall segments now uses a kit mesh; the command core and its shuttle keep the v0.9 orbital meshes. See `docs/TODO.md`.

**Orientation (2026-10-09).** Every kit mesh is authored with its front (doors, docks, berths) toward −Y and turned +90° about Z at export, so in Unreal the front faces **+X** — the side where every building's access port and road arrive (`access_port: [1, 0]` for all 52 definitions) and one of the two faces the default camera (yaw 135°) sees. Before this the fronts faced Unreal +Y, away from the camera: Blender's FBX export and Unreal's import keep X and mirror Y, so authored (x, y) lands at Unreal (−y, −x). The portraits are rendered from the same turned meshes with the studio lights turned with them.

**Docked workers.** `SM_ServiceBay` records its three berth positions in `kit_manifest.json` (`berths_unreal`: x, y, z, yaw in the Unreal mesh frame); `Graphics/building_visuals.json` carries the same points under `berths` (the validator checks they agree) and `ASeigeGameMode::SyncServiceVisuals` draws each real stored worker body held by that bay on a berth, one body per berth.

## Pipeline

- [`Tools/create_building_kit_v092.py`](../../Tools/create_building_kit_v092.py) — Blender batch. Centimetres, Z up, authored front −Y and exported with a +90° turn (front to Unreal +X), pivot at the horizontal bounding-box centre on the ground, box-projected UVs (one tile per 160 cm), `IM_*` slots from `Art/IndustryExports/industry_manifest.json`. Writes `Exports/*.fbx` (ignored; regenerated), `kit_manifest.json` (dimensions, triangles, SHA-256, building families) and `Source/BuildingKitV092.blend` (ignored). `-- --only=a,b` builds a subset.
- [`Tools/render_building_kit_ui_v092.py`](../../Tools/render_building_kit_ui_v092.py) — portraits `UI/T_Building_<Kind>.png` (384², same orthographic studio as the orbital set) and, with `--preview`, review renders. `Previews/kit_contact_sheet.jpg` is the retained sheet.
- [`Tools/import_building_kit_v092.py`](../../Tools/import_building_kit_v092.py) — Unreal commandlet: `/Game/Art/SM_<Kind>` with the existing `MI_Industry_*` instances (the construction reveal copies their parameters), three generated LODs (100 / 45 / 14 % triangles at screen sizes 1 / .25 / .08), ground-pivot and bounds checks, portraits to `/Game/Art/Interface/`. Writes `import_report.json`.
- [`Graphics/building_visuals.json`](../../Graphics/building_visuals.json) — building id → kind. Loaded by `ASeigeGameMode::LoadBuildingVisuals` (optional file; invalid content blocks play like the other graphics settings) and used by the world, the placement ghost and the HUD portraits. `Rules/buildings.json` is untouched, so the save fingerprint is unchanged. `Tools/validate_configuration.mjs` checks every id and that every `SM_<Kind>` package exists.

The world scales each mesh uniformly so its larger horizontal extent equals the building's footprint (`SeigeGameMode.cpp` `Visual()`), so the kit is authored at plot size: 2460 cm for the 25.2 m logistics class, 2060 cm for the 20.7 m processor class, 3100 cm for the 31.2 m vehicle factories.

## Review

`-GraphicsBenchmark -BenchmarkView=showcase -BenchmarkShowcase` places one completed, unpaid instance of every ordinary blueprint in a 6-column grid east of the core and frames it (`-BenchmarkZoom`, `-BenchmarkPitch`, `-BenchmarkYaw` as for the rex view; `-BenchmarkBuildMenu=production|logistics|defense|extraction` opens the palette in the capture). Captures: `Saved/Screenshots/Benchmark/kit-showcase*`, `kit-cards-*`. Triangle counts are 8.5–41 k per mesh at LOD0 (`kit_manifest.json`); the showcase view with all 28 buildings ran at 111–122 FPS at 2560×1080 in the Development editor build.

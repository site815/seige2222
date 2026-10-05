# seige2222 — Graphics and Interface Milestone 0.4

[Design index](README.md) · [Status definitions](README.md#design-status)

**Status: v0.4.0 Windows Shipping verified locally, 2026-10-05.** The native suite passed **27 tests: 26 clean and one with editor background HTTP warnings**, with zero failures or unrun tests. Both final editor and Shipping routes completed **53 stages with zero failures**; the package exited with code 0. These results establish the tested paths, not exhaustive user acceptance or Manor Lords visual parity.

## Current direction and reference

The user prioritized a realistic natural background inspired by Manor Lords, deferring further building art. Resources appear as an overlay with alerts beneath; construction uses a **floating bottom overlay**, superseding the earlier top-only layout. Surrounding sectors use a less detailed map, while the focused area receives detailed terrain. Middle-button orbit should respond naturally, and every building must expose relevant resources, power, damage, reload, and DPS, including zero/unarmed values.

On **2026-10-05**, the installed Manor Lords game's latest Autosave was loaded, paused for wide landscape, close-ground, and map-zoom inspection, then exited without saving. The resulting lessons were to use actual terrain relief, continuous varied grass, coherent soil/vegetation transitions, readable texture at ordinary orbit distance, and useful shadow/cloud depth. These guide original implementation rather than establish copied assets or a matching renderer.

The user rejected an earlier flat-ground/texture pass. Its 41-stage action route passed, but weapons/unarmed/regional-AI captures had timing errors. Later 53-stage routes corrected capture timing and drove further terrain/material iteration. Functional success did not make the earlier art acceptable.

## Current implementation

| Area | Implemented behavior | Boundary |
| --- | --- | --- |
| Camera | Captured middle-button orbit with editable sensitivity; 8–80° stored pitch, minimum zoom 120 logical units, and 160 cm ground clearance. Close zoom lowers the view while preserving the chosen orbit angle. | Native checks cover close view, restored angle, and downhill/uphill/horizontal terrain hits; this is not exhaustive human usability testing. |
| Region and terrain | Cartographic 3×3 survey; focused 1024-subdivision surface at approximately 3.52 m spacing, rolling/ridge relief, compact level foundations, and natural unbuilt deposits. Meshes and picking share cached triangles. | Native checks include incremental sector-edge seams and hidden-neighbor terrain. Focus does not transfer ownership or create cross-sector travel. |
| Ground cover | Two mixed grass meshes each contain 77,572 source triangles, including 920 low Bermuda tufts beneath taller swards, with unchanged bounds and runtime scale 1.0–1.3. Original wildflowers, 500–900 m culling, 9×9 streamed cells, and 350,000 candidates support continuous cover. | Approximately 1.31 meadow patches/m² is a tuning target, not a performance measurement. Scaled mesh bounds determine foundation clearance. |
| Materials and light | Imported alpha-edge color padding and grass transmission calibration; close photographic detail plus an 8 m tonal/normal/roughness layer; cloud-influenced lighting. | The medium layer uses measured luminance center 0.25, contrast 4, and a 0.86–1.14 tonal clamp. It does not displace geometry. |
| Interface and information | Resource overlay/alerts, floating bottom construction, region focus, and shared Overview, Weapons, Power, Production, Resources, and Maintenance rows. | The final route exercises intended armed/unarmed/zero-power views and ownership-safe commands. Future fleet controls and full robot needs remain outside this slice. |

[Graphics definitions](../../Graphics/scene.json) retain six rendered centimeters per simulation unit, giving 3.6 km sectors and a 10.8 km neighborhood without changing logical balance. Asset evidence: [grass import](../../Art/EnvironmentV04/meadow_import_report.json), [grass calibration](../../Art/EnvironmentV04/meadow_calibration_report.json), and [terrain material](../../Art/EnvironmentV04/terrain_import_report.json).

## Truthful statistics and authority

Rules `prototype-4.0` declare weapon name, damage per shot, reload seconds, and range; nominal DPS is damage divided by reload. Combat executes those shots. Efficiency slows reload progress, idle time holds one ready shot, and saves preserve reload/shot state. Core/turret nominal DPS is retained, but shot timing and overkill differ from the earlier continuous model. Unarmed definitions explicitly show zero damage, reload, DPS, and range.

All current buildings show **0 kW consumption and generation**, with an explicit explanation that no separate power grid exists. Validators reject nonzero power until that mechanism is implemented. Shared information also reports costs, health, jobs, efficiency, scaled ranges, local/required zero-stock inputs, incoming cargo, recipe quantities/timing, extraction, repair/upkeep, and relevant core facts. These values do not settle future energy or robot-needs design.

**Start a new scenario:** changed `prototype-4.0` rule fingerprints intentionally reject earlier-rule saves. Outer scenario format 2 does not override simulation compatibility. See [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md).

Human play controls only the home colony; observer mode may inspect AI colonies without issuing construction commands. Neighbor entity IDs must not operate home buildings. Hidden live data must not leak through dossiers, models, ground pads, or foliage clearings. Viewing the map does not add trade, remote extraction, fleets, raiding, networking, or shared inventories.

**Retained placement limitation:** Circular simulation spacing can permit diagonal placements whose square visible foundation corners overlap. Building-art/footprint alignment remains deferred; flat-pad and seam tests do not resolve that mismatch.

## Evidence and remaining work

| Check | Result | Evidence |
| --- | --- | --- |
| Native suite | **27 passed: 26 clean + 1 with warnings; 0 failed, 0 unrun** | `Saved/Automation/v04-verified/index.json` |
| Warning origin | Background editor request/retry to `google.com/generate_204` failed during a passing test | `Saved/native-v04-verified.log`; no gameplay assertion failure. The packaged network observation below is separate from this editor request. |
| Final rendered route | **53 stages, 0 failures** | `Saved/render-v04-delivery.log`, `Saved/PresentationSmoke.json` |
| Packaged observer FPS snapshot | **48.83 FPS** | Instantaneous Shipping report value, not an average or benchmark. |
| Shipping package/interaction | **Exit 0; 53 stages, 0 failures** | `Saved/package-v04.log`, `Saved/packaged-v0.4.0-UiSmoke-verification.json` |
| Packaged network observation | **45 samples, each 0 TCP / 0 UDP endpoints** | Bounded process-tree endpoint observation, not packet capture or proof about all runs. |
| Loose definitions | **10 files match source hashes** | `Saved/staged-v04-files.json` |

Close and middle-distance grass is materially fuller, with real hills. Distant ground remains smooth/olive and the forest remains uniform; the result is still below the Manor Lords reference. Retained **Shipping captures**: [colony](../../Art/Previews/v04_gameplay.png), [meadow](../../Art/Previews/v04_meadow.png), [terrain](../../Art/Previews/v04_terrain.png), [regions](../../Art/Previews/v04_regions.png), and [weapons](../../Art/Previews/v04_weapons.png).

Observed editor play/middle/ground/hills snapshots were approximately **47/34/33/48 FPS**, compared with earlier **51/45/44/50** readings. They illustrate the denser cover's observed cost, not controlled benchmark averages or a minimum-performance guarantee. The [development report](../DEVELOPMENT_REPORT.md) records those limits and the verified package checks.

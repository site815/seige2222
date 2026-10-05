# seige2222 — Historical development records, v0.2–v0.7

These archived records describe their named releases only. Statements about launchers, save formats, missing features and current art apply at that release date; they do not describe the active v0.8 candidate. See [current development and verification](DEVELOPMENT_REPORT.md).

## Historical v0.7.0 scenery, performance and scenario controls

The v0.7 source adds independent **Background bugs** and **Periodic attacks** switches to scenario setup. Both default on and apply to the player and every AI colony before developed-start preparation. Saves retain the switches; legacy v0.6 snapshots without them load as on/on. The unchanged `prototype-6.0` Rules fingerprint preserves v0.6 compatibility. Partial, invalid or inconsistent flags reject the entire load without replacing the current scenario.

The environment uses brighter sunlight and sky fill, more colorful photographic terrain, simpler distant foliage and the provider's authored lower-detail nearby broadleaf tree. All nine sectors retain the same deterministic tree placements across focus changes. Grass streams under a soft per-frame budget in spatially grouped instances; this removes the global batch updates that made rotation particularly expensive in the first candidate. Native rendering uses TAA; reduced rendering percentages retain TSR. The user-facing quality choice remains **Medium**, with render resolution separate.

These changes reduce scenery disappearance and camera cost; they do not establish zero popping or Manor Lords quality. Visible detail changes and softness remain possible. Switching the detailed sector still rebuilds terrain/forest and refills nearby grass, so a transition hitch remains outside the fixed-focus orbit measurements. See the [v0.7 graphics record](game-design/GRAPHICS_PERFORMANCE_0_7.md) and [asset provenance](../Art/EnvironmentV07/README.md).

**v0.7.0 is built and verified in `Builds/v0.7.0/Windows`; `Play-seige2222.cmd` selects it.** Compatible v0.6 saves remain supported.

| v0.7 check | Result |
| --- | --- |
| Native automation | **38 clean passes; zero warnings, failures or unrun tests** |
| Definitions | Configuration validation and **32 negative Rules cases** passed; all nine staged JSON hashes match source |
| Shipping package and interaction route | **Build exit 0; 79 stages, zero failures, game exit 0**; 874 courier-motion frames between fixed simulation ticks |
| Offline observation | **86 process-tree samples, zero TCP/UDP endpoints**; bounded endpoint observation, not packet capture |
| Actual display route | **Four states, zero failures**: 3840×1600 borderless, 1280×720 windowed, 75% rendering, restored native; AA methods 2/2/4/2 confirm TAA/TSR selection |

The [verification record](verification/v0.7.0.json) retains executable and external-definition hashes. [Scenario controls](../Art/Previews/v07_scenario.png), [main menu](../Art/Previews/v07_main.png) and [display results](../Art/EnvironmentV07/display-shipping.json) come from the Shipping package. Full generated logs and binaries remain local. Interaction-route FPS is not a benchmark.

Final Shipping measurements on the RTX 4070 Ti SUPER / Ryzen 7 9800X3D, at 100% rendering:

| View | v0.6 native static FPS | v0.7 native static FPS | v0.7 native orbit FPS | v0.7 1600×900 static FPS |
| --- | ---: | ---: | ---: | ---: |
| Colony | 26.96 | 46.23 | 46.85 | 153.74 |
| Meadow | 20.11 | 31.84 | 42.19 | 103.30 |
| Ground | 20.25 | 30.35 | 32.95 | 102.86 |
| Hills | 23.78 | 36.81 | 36.40 | 94.18 |
| Boundary | 35.06 | 43.98 | 41.49 | 107.25 |

Native means **3840×1600**. Each view waits for initial scenery, settles for four seconds and samples at least five seconds. Orbit adds one complete yaw turn with modest pitch variation, including ongoing camera-driven streaming; it does not cross sectors. The simulation is paused with empty neighbors. These are one-run mean FPS values, not minimum FPS or busy-colony stress results. The old static route began warmup directly after synchronous setup. The full v0.7 comparison changes foliage, shaders, lighting and quality settings together, so it is not an equal-quality or single-optimization claim.

[Native static](../Art/EnvironmentV07/benchmark-shipping-native.json), [native orbit](../Art/EnvironmentV07/benchmark-shipping-orbit-native.json), and [1600×900 static](../Art/EnvironmentV07/benchmark-shipping-medium.json) reports retain frame-time distributions, hardware and settings. Native orbit p95 ranges from 24.33 to 37.27 ms. **Native ultrawide still does not sustain 60 FPS.** The 1600×900 boundary view regressed from v0.6's 145.45 to 107.25 FPS; denser neighboring scenery and other concurrent changes mean gains are not universal. [Colony](../Art/Previews/v07_colony.png) and [ground](../Art/Previews/v07_ground.png) show the actual Shipping rendering. Distant silhouettes, aerial ground variation and temporal foliage quality still fall short of the reference target.

## Historical v0.6.0 local delivery verified

**The v0.6.0 Windows Shipping package is built and verified in `Builds/v0.6.0/Windows`; `Play-seige2222.cmd` selects it. Start a new scenario using `prototype-6.0`.** All 34 native tests passed cleanly. The packaged interaction route passed 79 stages with zero failures and exit 0; the separate actual-display route passed four states with zero failures. This is a functional release, with native-resolution performance and reference-quality visuals still unfinished.

The camera now surveys the complete home sector before the grid map blends in. Wheel zoom and map-entry/sector-selection zoom ease over rendered frames. Native borderless is the default display mode; window sizes and independent 50–100% rendering resolution are selectable, with one **Medium** quality profile. Windows high-DPI support prevents the 3840×1600 monitor being treated as a 1920×800 virtual desktop. There is no exclusive-fullscreen mode. A direct HUD **Menu**, Escape and F10 open the paused game menu; Resume restores the preceding pause state. Runtime font rendering and a cleaner main-menu composition replace enlarged bitmap text. Local speeds are **1×, 5×, 10×**, cycled with +/−; Space toggles pause.

The graphics pass reduces atmospheric haze and bloom, restores anisotropic grass color filtering, and tunes temporal reconstruction and Nanite detail while retaining foliage density. Source assets and licenses remain documented. Static review shows clearer terrain and text, but does not establish elimination of moving grass shimmer or Manor Lords quality parity. See [Graphics Performance 0.6](game-design/GRAPHICS_PERFORMANCE_0_6.md) for Epic references, local engine-source findings and measured tradeoffs.

Couriers show their actual cargo and visible exterior loading stops. Representative workers fetch tools and work around buildings when staffing, supplies and operating conditions allow it. Outdoor inventory uses raw piles, ingots and crates; indoor inventory stays inside. Construction retains real reservation and physical delivery before worker assembly; delivered visual stacks shrink as material is installed. Courier/bug movement and construction interpolate every rendered frame between authoritative 20 Hz simulation ticks. This does not change transport speed, production rates, AI rules or building costs, and it does not introduce independent pathfinding for every robot.

| v0.6 check | Recorded result | Evidence and boundary |
| --- | --- | --- |
| Full native automation | **34 clean passed; 0 warnings, failed or unrun** | `Saved/Automation/v06-final/index.json`; includes camera survey/easing, menu pause/save, 1/5/10 shortcuts, interpolation by entity ID, exterior work/stock placement and operating gates. |
| External definitions | Configuration and **32 negative Rules cases** passed | Rules, AI, Interface and Graphics validation. All nine packaged JSON files match source SHA-256 hashes. |
| Shipping package and interaction | **Exit 0; 79 stages, 0 failures** | `Saved/package-v06.log`, `Saved/packaged-v0.6.0-UiSmoke-verification.json`; representative 1× courier movement changed on 831 frames between fixed ticks. This is functional evidence, not a human motion-quality or FPS guarantee. |
| Packaged network observation | **84 samples; all 0 TCP/UDP endpoints** | Launched game process tree during the interaction route; bounded endpoint observation, not packet capture or every possible run. |
| Actual display changes | **4 states, 0 failures** | [Retained Shipping report](../Art/EnvironmentV06/display-shipping.json): native 3840×1600 borderless, 1280×720 windowed, windowed at 75% rendering, then restored native/100%; Medium retained. No saved display preferences overwritten. |
| Runtime visual review | Menu, settings, survey, construction, stockyards and grass inspected | [Main menu](../Art/Previews/v06_main.png), [settings](../Art/Previews/v06_settings.png), [survey](../Art/Previews/v06_sector_survey.png), [construction](../Art/Previews/v06_construction.png), [stockyards](../Art/Previews/v06_stockyards.png), [ground](../Art/Previews/v06_ground.png). Captures are from the 1600×900 interaction route; display assertions come from the separate actual-window test. |

Final Shipping timings on the RTX 4070 Ti SUPER / Ryzen 7 9800X3D, the same Medium profile at 100% rendering, one run per resolution:

| View | 3840×1600 mean FPS | 1600×900 mean FPS |
| --- | ---: | ---: |
| Colony | 26.96 | 107.17 |
| Meadow | 20.11 | 80.06 |
| Ground | 20.25 | 79.55 |
| Hills | 23.78 | 80.89 |
| Boundary | 35.06 | 145.45 |

Each view has at least four seconds of warmup followed by five seconds of complete frame intervals in a paused fresh colony with empty neighbors. These are fixed-view graphics measurements, not minimum FPS or a busy-colony simulation benchmark. [Native report](../Art/EnvironmentV06/benchmark-shipping-native.json) and [1600×900 report](../Art/EnvironmentV06/benchmark-shipping-medium.json) retain settings, mean frame time and p95. Lower render resolution is available without changing HUD resolution; **native ultrawide remains slow**. The earlier editor profile experiments observed 25–40% gains at native resolution, but changed multiple settings and do not establish equal-quality gains against v0.5.

The [v0.6 verification record](verification/v0.6.0.json) retains executable/definition hashes and test counts. Full generated logs and local binaries remain outside Git. No separate Shipping save/load roundtrip or occupied-service-bay acceptance test is claimed; native tests cover persistence. Exterior worker/tool and material counts are representative, and crowded stockyards are not a global collision/pathfinding system. Construction refunds, full morale/revolt, fleets, cross-colony logistics and persistent multiplayer remain later work.

## Historical v0.5.0 local delivery verified

**The v0.5.0 Windows Shipping package was built and verified in `Builds/v0.5.0/Windows`; the launcher selected it for that release. All 29 native tests passed: 28 clean successes, one success with editor background HTTP warnings, zero failed or unrun. The packaged interaction route passed all 63 stages with zero failures and exit 0.** Local verification does not claim final visual acceptance or reference-game parity. Historical v0.4 evidence follows below.

Construction now reserves uncommitted core stock at order time without consuming it immediately. Tagged couriers deliver the complete bill to a site's separate inventory, then assigned robot builders perform timed work. Completion consumes those materials into the structure. Until then, the site does not produce, repair, sense or fire. Site progress, deliveries in transit, pauses and maintenance state survive save/load; invalid snapshots are validated separately before replacing live state.

The initial core is a real six-second deployment using six starting robots and a separate shuttle-carried kit. Its operating supplies and preloaded escape cargo are distinct from that kit. The original shuttle descends during deployment and remains docked afterward. On core loss or ejection, only cargo already aboard is retained; cross-server relocation remains outside this slice. The placement ghost, scaffold, delivered stacks, aggregate builders and progressive building reveal visualize simulation state without creating a unit-micromanagement system.

A staffed robot service bay supplies sixteen support berths in addition to the core's eight. It has one operating job, requires physical construction, and consumes maintenance components from its own delivered stock. Robot manufacture follows open jobs, assembly supplies and available support capacity; unsupported or poorly maintained population lowers efficiency. The external staffing order is core, service, defense, sensor, then other industry/storage, applied to construction and operation. Power remains explicitly zero kW: charging is an abstract service-capacity model, not a simulated electrical grid or per-robot battery system.

AI follows the same construction and support rules. Its data-defined plan completes and staffs earlier targets before expanding. Developed starts actually run a finite supplied colony through normal delivery, workforce, maintenance, production and threats rather than granting completed structures. Rules are now `prototype-5.0`, simulation saves use format 2, and exact Rules/AI fingerprints reject older saves. **Start a new v0.5 scenario.**

The graphics source adds the same terrain material to coarse neighboring grids and sparse deterministic background woodland, preserving continuous scenery without exposing hidden colony structures. Focused terrain, real relief and nearby grass density remain. Grass shadow distance, indirect-lighting contribution and Nanite geometry target are externally configurable. The forest-floor material fades high-frequency detail with distance. The final matched Shipping comparison at Epic quality and native 1600×900 measured 5.14–15.10% higher mean FPS with the selected Nanite geometry target: 38.80–70.32 FPS across five views on the RTX 4070 Ti SUPER. This is one controlled pair within v0.5, not a whole-game or v0.4 comparison; Manor Lords visual parity remains unmet. See [Graphics Performance 0.5](game-design/GRAPHICS_PERFORMANCE_0_5.md) and the [original construction asset record](../Art/Construction/README.md).

| v0.5 check | Recorded result | Evidence and boundary |
| --- | --- | --- |
| Full native automation | **29 passed: 28 clean + 1 with warnings; 0 failed, 0 unrun** | `Saved/Automation/v05-final/index.json`; simulation, construction/services, AI, saves, terrain/camera/privacy, frontend and real controller/HUD routing. |
| Native warning | Unreal editor background HTTP retry/failure to `google.com/generate_204` | Recorded during `Seige.AI.StartingAndDevelopedColonies`, which passed with zero errors. It does not establish the network behavior of a v0.5 Shipping build. |
| Construction and service coverage | Physical reservation/delivery/completion, local upkeep and support limits passed | Native tests include deterministic mid-construction continuation and invalid-save rejection; they do not establish every production layout's viability. |
| First-playable viability | **12 newly manufactured components at 435 simulation seconds** | Normal-action objective test, without inventory grants or instant-construction bypasses; verifies one finite winning strategy, not indefinite sustainability. |
| Starting/developed AI | Native production and preparation checks passed | Starting AI manufactured 35 components by the configured 600-second preparation budget; deterministic save continuation passed. |
| External definitions | Configuration validation and **29 negative Rules cases** passed | Fourteen building definitions, including thirteen buildable entries, with construction/service policies and separate AI/UI/Graphics files. |
| Shipping build and packaged interaction | **Exit 0; 63 stages, 0 failures** | `Saved/package-v05.log` and `Saved/packaged-v0.5.0-UiSmoke-verification.json`; final process exited successfully. |
| Packaged network observation | **73 samples; all 0 TCP / 0 UDP endpoints** | Same verification JSON, sampling the launched process tree. This is bounded endpoint observation, not packet capture or proof for every possible run. |
| Loose definition staging | **All nine JSON files match source byte hashes** | Five Rules files, two AIFILES definitions, Interface and Graphics checked against `Builds/v0.5.0/Windows/seige2222/Binaries/Win64`. |
| Source repository | **main branch** | Repository: [site815/seige2222](https://github.com/site815/seige2222). Local binaries, caches and generated logs remain excluded from Git. |

The retained [v0.5 verification record](verification/v0.5.0.json) contains test counts, packaged executable and definition hashes, hardware, and benchmark report paths. Full generated logs remain local.

Retained **v0.5 Shipping captures**: [shuttle deployment](../Art/Previews/v05_shuttle.png), [construction](../Art/Previews/v05_construction.png), [completed service bay](../Art/Previews/v05_service.png), [sector boundary](../Art/Previews/v05_boundary.png), and [meadow](../Art/Previews/v05_meadow.png). The meadow image comes from the matched Epic/100% Shipping benchmark; the other four come from the packaged interaction route. Runtime inspection checked PBR preservation on the core, visible builders, shuttle docking, scaffold and completed structures. The service bay in the capture had `supported_here=0`, so its empty appearance was expected; the occupied-bay visual state was not verified. The final controlled Epic/100% benchmark is separate from these screenshots and is recorded in [Graphics Performance 0.5](game-design/GRAPHICS_PERFORMANCE_0_5.md). The smoke's instantaneous FPS is not a benchmark result.

No separate v0.5 packaged save/load roundtrip is claimed. Native tests cover simulation construction state and neighborhood persistence. Construction cancellation/refunds, individual builder pathfinding, per-robot batteries, full morale/revolt, controllable fleets, cross-colony trade/raiding and persistent multiplayer remain outside the implementation. The v0.4 appearance and performance limitations below remain historical evidence, not current benchmark results.

## Historical v0.4.0 local delivery verified

**The v0.4.0 Windows Shipping package was built and verified in `Builds/v0.4.0/Windows`; the launcher selected it for that release. Native tests passed 27 cases, and the packaged route passed all 53 stages with exit 0.** The native result comprises 26 clean successes and one success with editor background HTTP warnings, not a gameplay assertion failure. No Manor Lords visual parity or final user acceptance is claimed.

The interface has resource overlays with alerts beneath, floating bottom construction, captured middle-button orbit, a cartographic region view, focused-sector detail, and complete building dossiers. Armed/unarmed and zero-power states are explicit. Rules `prototype-4.0` execute weapon shots/reloads and preserve cooldown state; **start a new scenario**, because prior-rule saves are intentionally incompatible. These prototype choices do not finalize the full game or add persistent multiplayer.

On **2026-10-05**, the installed Manor Lords game's latest Autosave was loaded, paused for wide/close-ground/map inspection, and exited without saving. The user rejected the preceding flat-ground/texture pass. The resulting terrain uses a focused 1024-subdivision grid at approximately 3.52 m spacing, rolling/ridge relief, compact foundations, natural unbuilt deposits, continuous grass/soil variation, and cloud lighting. The 8–80° orbit lowers smoothly near the 120 minimum zoom while preserving the chosen angle; minimum camera ground clearance is 160 cm.

Final grass patches retain their bounds and add **920 low Bermuda tufts beneath the taller swards; each of the two meshes has 77,572 source triangles**. Runtime scale is 1.0–1.3, with bounds-aware foundation clearance, 500–900 m culling, and 9×9 streamed cells using 350,000 candidates. The imported 8 m photographic material layer adds desaturated tonal variation, normals, and roughness without geometric displacement. Measured filtered luminance informed a center of 0.25 and contrast 4, retaining the 0.86–1.14 multiplier clamp. See the [grass import report](../Art/EnvironmentV04/meadow_import_report.json), [grass calibration](../Art/EnvironmentV04/meadow_calibration_report.json), and [terrain report](../Art/EnvironmentV04/terrain_import_report.json).

| v0.4 check | Recorded result | Evidence and boundary |
| --- | --- | --- |
| Native automation | **27 passed: 26 clean + 1 with warnings; 0 failed, 0 unrun** | `Saved/Automation/v04-verified/index.json`; includes uphill/horizontal terrain picking, incremental sector-edge seams, compact pads, privacy, camera, UI, simulation, AI, and persistence. |
| Native warning | Editor background request/retry to `google.com/generate_204` failed | `Saved/native-v04-verified.log`; warning messages were captured during HiddenNeighborTerrain, which passed. This is distinct from a gameplay failure; Shipping network observations are recorded separately below. |
| Final rendered route | **53 stages, 0 failures** | `Saved/render-v04-delivery.log` and `Saved/PresentationSmoke.json`; final observer snapshot reports 47.2 FPS. |
| External definitions | Configuration validation passed | Rules, AI, Interface, and ten nature roles resolve; 22 deliberately invalid Rules cases are rejected. |
| Shipping build and packaged interaction | **Exit 0; 53 stages, 0 failures** | `Saved/package-v04.log`, `Saved/packaged-v0.4.0-UiSmoke-verification.json`; final packaged snapshot 48.83 FPS. |
| Packaged network observation | **45 samples; all 0 TCP / 0 UDP endpoints** | Same verification JSON, sampling the launched process tree. This is bounded endpoint observation, not packet capture or proof for every possible run. |
| Loose definition staging | **10 files matched source hashes** | `Saved/staged-v04-files.json`; Rules, AIFILES, Interface, and Graphics. |

Close and middle-distance captures are materially fuller, with real hills. Distant ground remains smooth/olive and the forest remains visually uniform; the result is still below the Manor Lords reference. Retained **Shipping captures**: [colony](../Art/Previews/v04_gameplay.png), [meadow](../Art/Previews/v04_meadow.png), [terrain](../Art/Previews/v04_terrain.png), [regional map](../Art/Previews/v04_regions.png), and [weapons dossier](../Art/Previews/v04_weapons.png).

| Editor-view snapshot | Earlier capture FPS | Final editor capture FPS |
| --- | --- | --- |
| Play | 51 | 47 |
| Middle meadow | 45 | 34 |
| Ground | 44 | 33 |
| Hills | 50 | 48 |

These instantaneous readings illustrate the cost of denser cover in the observed views. They are not averages, controlled comparisons, hardware benchmarks, or a minimum-FPS guarantee.

Known retained limitation: circular simulation spacing can allow diagonally placed square foundations to overlap at their corners. Building-art/footprint alignment remains deferred. Incremental terrain changes preserve shared sector edges and hidden-neighbor information boundaries, with native coverage.

Earlier v0.4 history is retained in logs: the 41-stage action route passed but weapons/unarmed/regional-AI captures were mistimed; the later 53-stage terrain and calibrated routes corrected them. Passing those paths did not make the rejected earlier art acceptable. The [v0.4 milestone](game-design/GRAPHICS_MILESTONE_0_4.md) records current scope and remaining release work.

## Historical v0.3.0 delivery status

**The v0.3.0 Windows Shipping package is built and verified.** Nineteen native tests passed cleanly, and the packaged interaction route completed 28 stages with zero failures and exit code 0. The historical package is in `Builds/v0.3.0/Windows`; the launcher selected it for that release. The source repository is [site815/seige2222](https://github.com/site815/seige2222); local versioned binaries are excluded from Git. The v0.2 record below is historical evidence for that version.

## Implemented v0.3 presentation

The camera uses perspective projection with orbit yaw/pitch and terrain clearance. Q/E rotate it; middle-mouse drag rotates and tilts; WASD/arrows pan relative to its direction. The wheel zooms and Home returns to the colony view. Construction/menu shortcuts take priority over camera movement. `Graphics/scene.json` owns physical scale, FOV, starting angles, zoom limits, forest density, and nature-asset references.

The default conversion is **six rendered centimeters per logical simulation unit**. Each sector retains its 60,000-unit logical side and renders at **3.6×3.6 km**; the 3×3 neighborhood renders at **10.8×10.8 km**. This presentation mapping does not alter deposit coordinates, logical movement times, costs, production, jobs, combat, or AI behavior. Imported nature uses physical centimeter dimensions independently of that conversion.

The environment uses licensed CC0 Poly Haven fir/broadleaf trees, ferns, and mossy rocks. Current configuration selects seven of eight prepared nature meshes plus the original grass-blade mesh. Trees/rocks use Nanite; trees preserve surface area, and leaves use masked two-sided foliage materials. Fir C now retains its full 505,494-triangle source LOD0. Foliage-only texture sampling uses alpha mip bias −2 and color bias −1; there is no global mip override. The ground blends grass/meadow surfaces at two texture frequencies, with clustered rocks. Charlotte Baglioni's Leafy Grass supplies additional color, normal, roughness, and ambient-occlusion maps. [Third-party attribution](../Art/THIRD_PARTY_ASSETS.md) and the [nature register](../Art/Nature/ATTRIBUTION.md) retain creators, sources, hashes, and preparation details.

Six original Blender buildings occupy the existing `/Game/Art/SM_*` paths. The command campus now has a smooth 48-sided curved roof with radial standing seams, flange joints, and fixings. Its 71,656 source triangles bring the set to **215,700 triangles**. The factory, extractor, depot, sensor, and turret retain differentiated industrial forms. Each building has three imported LODs; the set uses nine original 1K PBR maps and eleven material instances. The [Blender source](../Art/Source/Seige_Industry_Architecture.blend), [generator](../Tools/create_industry_assets.py), and [importer](../Tools/import_industry_assets.py) remain editable. Robot/bug art is retained; no new character animation was added.

Scenario save format 2 accepts optional yaw/pitch fields. Older format-2 saves without those fields receive stable default angles; invalid orientations are rejected before state changes. Rule/AI fingerprints govern simulation compatibility. This behavior has native test coverage; a separate packaged save/load roundtrip was not run for v0.3.

## v0.3 verification

| Check | Recorded result | Evidence and boundary |
| --- | --- | --- |
| Editor and Shipping compilation/package | Passed, package exit 0 | `Saved/build-v03-editor.log`, `Saved/package-v03.log`; standalone output in `Builds/v0.3.0/Windows`. |
| Native automation | **19 passed, 0 failed, 0 test warnings** | `Saved/Automation/v03-final/index.json`: six simulation, four AI, three interaction, three frontend, and three camera tests. The delivered v0.3 source did not change after this run; v0.4 is separate work. |
| Editor rendered interaction | **28 stages, 0 failures** | `Saved/render-v03-second.log`; perspective landing, construction, selection, rotated views, and low-angle building selection. The first run's 11 failures remain in `Saved/render-v03-first.log` as diagnosis history. |
| Packaged rendered interaction | **28 stages, 0 failures, exit 0** | `Saved/packaged-v0.3.0-UiSmoke-verification.json`; final observer scenario has two AI neighbors and seventeen center buildings. |
| Packaged network observation | **18 samples, each 0 TCP / 0 UDP endpoints** | Same verification JSON, sampling the launched process tree during the smoke route. This bounded observation is not a packet capture or a guarantee about every possible execution. |
| Loose definition staging | **10 files matched source hashes** | `Saved/staged-v03-files.json`; five Rules JSON files, two AI JSON files plus AI README, Interface, and Graphics. |
| Industrial assets | Import passed with 0 errors / 0 warnings | [Report](../Art/industry_import_report.json), `Saved/industry-core-refinement-import.log`; core physical bounds and ground pivot retained, correct active materials, three LODs. |
| Nature and ground | Imported and present in final package | [Nature report](../Art/Nature/import_report.json), [ground report](../Art/ground_v03_import_report.json), and final rendered captures. |
| Source and binary distribution | Separate | [site815/seige2222](https://github.com/site815/seige2222) is the source repository; versioned packages and generated verification logs remain local and are excluded from Git. |

The native normal-action colony strategy completed at 360 simulation seconds with twenty-two newly manufactured components. It proves one viable strategy, not indefinite sustainability or every AI scenario. Native tests also cover simulation save continuation, neighborhood state, camera-format compatibility, and rejection of invalid data. The packaged smoke exercises real controller/menu and screen-to-world routes, but does not constitute a complete human play-through or a packaged save/load test.

`Tools/validate_configuration.mjs` checks Rules, AIFILES, Interface, and Graphics. Focused invalid variants cover versions, references, bounds, shortcuts, vegetation counts, and zoom ordering. A controlled packaged balance edit was verified for v0.2; v0.3 verified identical staging and successful loading, without repeating that edit experiment.

Unreal Editor made a `google.com/generate_204` probe during the first native run; the final native report is clean. Editor services and development downloads are separate from the Shipping process-tree observations above.

## Visual quality and remaining scope

Retained game captures show the [colony](../Art/Previews/v03_gameplay.png), [close-up](../Art/Previews/v03_closeup.png), and [neighborhood](../Art/Previews/v03_neighborhood.png). Separate [command-hub](../Art/Previews/industry_command_closeup.png) and [factory](../Art/Previews/industry_factory_closeup.png) previews are Blender renders. Instantaneous on-screen FPS is not a hardware benchmark.

This milestone does **not** match Manor Lords' finished visual quality. Distant canopy thinning, visible terrain repetition, and region-level presentation remain polish work. Neither passing interaction tests nor licensed detailed assets establish that all art is finished.

This pass adds no networking, controllable fleets, privateering, cross-sector economy/travel, full robot needs or revolt, abandoned-region scavenging, or shuttle loading/relocation. Neighbor colonies remain independent simulations; Multiplayer remains Coming soon. See [Graphics Milestone 0.3](game-design/GRAPHICS_MILESTONE_0_3.md) and [First Playable Scope](game-design/FIRST_PLAYABLE_SCOPE.md) for boundaries.

## Historical v0.2.0 record — 2026-10-04

Everything in the following sections describes the verified v0.2 revision. Its map scale, fixed orthographic camera, older art, packaging, and network observations must not be read as current v0.3 validation.

### v0.2 implementation

The single-player prototype now starts at a main menu. Scenario setup configures a human or AI center and eight empty, starting-AI, or developed-AI neighbors. A human scenario pauses the whole world until the command center is placed. An AI center enables observation, with the same colony rules and no player construction orders.

Each sector is 600×600 meters: six times the original side length and thirty-six times its area. The full 3×3 neighborhood is 1.8 kilometers across. The resource template contains twenty-five irregularly clustered deposits, including distant groups. Terrain is Earth-like, with photographic CC0 surfaces, original detailed futuristic buildings, and Blender vegetation. Individual neighboring sectors currently reuse the deposit template; the landscape varies across the larger world.

The interface has version/FPS at the upper left, top-bar menus and major summaries, hover details, and a floating B-key construction catalog. There is no permanent lower construction bar. Settings cover graphics quality and display mode. Credits are maintained in the external interface definition.

The simulation retains automatic jobs, manufactured robot population, local production, physical cargo, repairs, sensors, roaming bugs and scaled invasions, a first-playable objective, and emergency escape. Scenario save/load now includes all colonies, AI configuration fingerprints, time controls, and camera position. AI definitions live in `AIFILES`; content and balance live in `Rules`; menus, shortcuts, summaries, and credits live in `Interface`.

### v0.2 crash and presentation fixes

The reported left-click crashes were traced to `ASeigeHUD::Click` reading the transient Unreal drawing canvas during controller input. Input now uses cached hit rectangles and viewport dimensions. The regression test calls the real controller/HUD path with a null canvas, including world selection and construction.

Expanding the orthographic clipping range exposed an Unreal camera-origin correction that placed foliage outside its distance-culling range. The project now disables that correction for its explicit orthographic planes. Trees use authored canopy-preserving detail levels and remain visible at regional zoom; small ground details may still cull. The runtime sun is explicitly movable, and Lumen mesh-distance-field generation is enabled. Runtime screenshots are checked separately from Blender asset previews.

### v0.2 recorded verification

- The revised editor target compiled with the installed Visual Studio 2026 toolchain. UBT reports that this compiler family is newer than Epic's preferred version; no engine-source changes were required.
- All sixteen native tests passed with zero test failures or warnings: six simulation, four AI, three interaction, and three frontend tests. Report: `Saved/Automation/v02-final/index.json`.
- A normal-action strategy covering three resource approaches with sensors and turrets completed the objective at 360 simulation seconds with twenty-two manufactured components and no building losses. No stock or health edits were used in that strategy test. This validates one viable strategy, not indefinite balance.
- Starting AI manufactured components by 300 simulation seconds. Tests also covered the developed preset, rejected invalid AI files, relocated-core threat spawning, and deterministic AI save continuation.
- Frontend tests covered default empty neighbors, mode cycling, paused landing, rejected and accepted landing clicks, observer input restrictions, settings pause/return, exact center-and-neighbor save restoration, and rejection of corrupt scenario metadata without changing the running state.
- The rules validator passed the current rule set and fifteen deliberately invalid variants. The combined configuration validator also checks AI and interface references; five focused invalid-reference/key/timing mutations were rejected.
- The rendered `-UiSmoke` route completed twenty-three stages with zero assertions: main menu, setup, landing, real controller construction clicks, construction shortcuts, neighborhood overview, credits, and AI observation. It saves screenshots and a `PresentationSmoke.json` report without touching player saves. Native input-route coverage and programmatic rendered interaction are not a claim of a complete human mouse-driven play-through.
- Art import verified fourteen revised building/environment meshes, twelve PBR textures, five material masters, bounds, pivots, and material slots. Original robot and bug models remain in use. Texture source URLs, creators, licenses, and hashes are recorded in [third-party attribution](../Art/THIRD_PARTY_ASSETS.md).

### v0.2 packaged release checks

The offline Win64 Shipping package built successfully in `Builds/v0.2.0/Windows`. The root launcher selected this version at the time of the v0.2 checks. The packaged controller-driven check completed all twenty-three stages with zero failures, including the human landing/build path and an observer scenario with two AI neighbors. Cooked screenshots verified the main menu, top bar, build catalog, neighborhood labels, credited assets, and rendered forest/industry.

All nine loose files staged from `Rules`, `AIFILES`, and `Interface` matched their source hashes. A controlled edit to the packaged scenario changed starting population from six to eight without recompiling. After fifteen simulation seconds the report showed seven robots, reflecting the normal automatic retirement interval. The exact original rule bytes were restored and their hash rechecked. This verifies numerical definition loading in the executable; new simulation mechanisms still need code.

Local evidence: `Saved/package-v02-final.log`, `Saved/packaged-UiSmoke-verification.json`, and `Saved/packaged-PrototypeSmoke-verification.json`. Packaged screenshots and runtime reports are under `%LOCALAPPDATA%/seige2222/Saved`. Test routes do not modify player saves. Source publication uses the private `site815/seige2222` repository; executable archives and local verification logs remain excluded from Git.

Tracked cooked-game captures: [colony view](../Art/Previews/v02_gameplay.png), [scenario setup](../Art/Previews/v02_scenario.png), and [nine-sector overview](../Art/Previews/v02_neighborhood.png). These are runtime screenshots, not target-art mockups.

### v0.2 offline operation

The deliverable uses Shipping configuration, which avoids Unreal's development profiling listener. HTTP transport, UDP/TCP messaging, telemetry, and unused online-service plugins are disabled. The game requires no account or server. Its third-party textures are local packaged assets. Development downloads and GitHub publishing are separate from runtime behavior.

The v0.2 Shipping game was observed with zero TCP sockets and UDP endpoints in every live-process sample during the menu/gameplay/observer check and the separate rule-edit run. This is an observation of those executed paths, not a packet capture of every possible future session. Editor commandlets may initialize additional development plugins and are not the single-player deliverable.

Earlier sandboxed Unreal build attempts coincided with the reported dotnet dialogs. Subsequent engine/compiler calls use the required filesystem access. No .NET or engine reinstall was performed.

### v0.2 remaining scope

Some forest views still show an abrupt pale foliage shading band at a distance. Explicit movable sunlight and a separate far-shadow-culling diagnostic did not eliminate it; the diagnostic override is not included in the release. This remains visual polish to investigate, alongside broader terrain variety and art refinement. The presentation is an early art pass, not finished production graphics.

Neighbor colonies are independent instances of the same simulation, not a shared multiplayer economy or battlefield. Fleet control, privateering, cross-sector extraction/trade/travel, full robot needs and revolt, abandoned-region scavenging, shuttle loading/relocation, and persistent networking remain future work. Multiplayer is explicitly marked Coming soon in the menu. The full design remains the persistent-world baseline, with single-player developed first.

See [first-playable scope](game-design/FIRST_PLAYABLE_SCOPE.md), [scenario setup](game-design/SCENARIO_AND_AI_SETUP.md), and the [design index](game-design/README.md). The design documents distinguish confirmed intent, prototype choices, and unimplemented systems.

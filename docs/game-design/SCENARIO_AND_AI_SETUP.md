# seige2222 — Scenario and AI Setup

[Design index](README.md) · [Status definitions](README.md#design-status)

This document records the confirmed local setup flow and the current prototype implementation. It does not turn its finite map, AI presets, or numerical balance into final full-game rules.

## Starting a scenario

**Confirmed direction:** The game opens to a main menu with single-player setup, loading, settings, credits, and exit. The player configures a 3×3 neighborhood and can inspect it at a broad zoom. Human play starts with time paused while choosing the central command-core site. An AI-controlled center provides an observer mode.

| Cell | Current choices | Meaning |
| --- | --- | --- |
| Center | Player, starting AI, developed AI | Player enters paused landing selection. Either AI choice starts observation after setup. |
| Each of eight neighbors | Empty, starting AI, developed AI | Empty is the default. An occupied slot runs an independent local colony simulation. |

The default setup is one player center and eight empty neighbors. Before a human landing, the view surveys the resource deposits; placement uses the simulation's sector-edge and deposit-clearance validation. This selects the site for the player's only core; v0.5 source then advances worker-driven deployment from the landing shuttle's carried kit. It does not introduce a second core or move an established colony.

## Map and information

**Current prototype:** Each colony uses a sector 60,000 logical units square and the same template of 25 irregularly clustered resource nodes. The v0.3 graphics conversion maps these to 3.6 km sectors and a 10.8 km neighborhood. The earlier v0.2 values of 600 m and 1.8 km came from its smaller rendering conversion. Logical distances, travel times, and balance are unchanged. These are editable scenario and presentation values, not randomized planetary geography. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md); broader wilderness and remote-outpost proportions remain design targets in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

After human landing, live information uses finite sensor coverage. Seeing sector borders does not reveal its buildings or threats. Observer mode shows the simulated colonies so the player can watch AI behavior; this is an observation tool, not a scouting advantage in human play.

The current presentation retains the v0.4 cartographic neighborhood and detailed focused sector, with v0.5 adding continuous coarse neighboring terrain and sparse background woodland at the same scale conversion. Selecting a sector changes the view, not ownership or sensor knowledge. Human commands remain restricted to the home colony; observer inspection does not grant construction control. See [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md) for verification status.

## AI behavior and editable files

**Confirmed requirement:** AI definitions live separately in `AIFILES`. Shared resource, building, production, and threat rules stay in `Rules`.

| File | Current role |
| --- | --- |
| [colony_ai.json](../../AIFILES/colony_ai.json) | Ordered building targets, decision interval, action budget, placement search, and sensor expansion. |
| [developed_start.json](../../AIFILES/developed_start.json) | Finite initial core stock and population for developed colonies; initial buildings pay ordinary costs from that stock. |
| [Rules/scenario.json](../../Rules/scenario.json) | Ordinary starting inventory and population, core, deposit template, world extent, and seed. |
| [Interface/ui.json](../../Interface/ui.json) | Construction groups and shortcut mappings, displayed summary resources, descriptions, and credits. |
| [Graphics/scene.json](../../Graphics/scene.json) | Perspective camera, orbit sensitivity, regional-view threshold, rendering scale, terrain material, vegetation density, and nature asset references; separate from logical gameplay balance. |

A starting AI begins with the ordinary colony start. Both AI types seek nearby matching deposits, place defenses and perimeter sensors, expand service capacity, and build the industrial chain. The external `complete_and_staff_in_order` target policy waits for each stage to be completed and staffed before spending on later stages. Sensors and turrets anchor toward nearby deposit approaches; their placement distance is external data. Missing earlier targets take priority over further expansion. They use ordinary placement commands and construction costs. Staffing, physical couriers, input consumption, repair, and alien pressure follow the same simulation as the player. The developed preset grants finite seed assets once, then runs actual construction, couriers, workers, services and threats within external setup time/action bounds. Required buildings genuinely finish, so preparation advances time and may manufacture goods. There is no recurring free inventory.

This is a deterministic construction controller. It does not implement diplomacy, rival fleet planning, cross-sector combat, or trade. Each occupied slot has its own stock and local alien threats. A common rendered neighborhood does not create shared movement or economy.

## Time, settings, and saves

Only active, unpaused play advances the colony simulations. Main menu, setup, landing, settings, and credits stop their clocks. Settings currently offer Low/Medium/High/Ultra graphics quality and windowed/fullscreen selection. Observer mode supports camera movement, pause, and saving while disabling player construction and colony commands.

Local scenario saves include every occupied colony, scenario selections, camera, speed, pause state, and AI configuration fingerprints. Loading validates all snapshots before replacing the active scenario. AI cadence derives from saved simulation time; it has no separate hidden timer. Changed Rules or AI definitions invalidate incompatible saves. v0.3 adds optional bounded yaw/pitch fields with defaults for older format-2 saves. Restart scenarios to apply gameplay definition changes, and restart the application after interface or graphics edits. Live reload and general gameplay-save migration are not implemented.

Current v0.5 source uses Rules `prototype-5.0` and simulation save format 2 to persist construction, local maintenance and cargo purpose alongside weapon state. Earlier-rule saves are intentionally rejected with a new-scenario diagnostic; optional camera-field compatibility does not override the rule fingerprint.

These single-player controls do not establish how a future persistent multiplayer world pauses, simulates offline colonies, or transfers a relocating core.

## Validation and remaining work

Run `node Tools/validate_configuration.mjs` before building. It checks Rules plus AI/UI references, bounds, construction-menu coverage, shortcut conflicts, developed preset capacity and necessary cost bounds, and Graphics settings and asset references. [Tools/build.ps1](../../Tools/build.ps1) and the [GitHub workflow](../../.github/workflows/rules.yml) run it automatically. Geometric feasibility and long-term survival still require simulation tests and playtesting.

**Current v0.6 source boundary:** AI decision rules, preparation and local simulation rates are unchanged. The Rules schema adds stockpile/work presentation metadata; visual snapshots reset after a new scenario, landing relocation or load, so reused entity IDs cannot inherit previous motion. This does not alter AI inventory, population or construction. The v0.6 editor compiled, all 34 native tests passed cleanly (zero warnings, failed or unrun) and the 79-stage editor route passed with zero failures. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; the evidence below belongs to v0.5.

**Historical v0.5 status:** Static configuration and 29 negative Rules cases pass. The final native suite passed 29 tests (28 clean, one editor background HTTP-warning success; zero failed or unrun), including AI development, landing, observation and neighborhood save continuation. The first objective succeeds at 435 simulation seconds, and starting AI manufactures 35 components by the configured 600-second budget. The Windows Shipping package passed 63 stages with zero failures and exit 0; all 73 process-tree samples showed zero TCP/UDP endpoints, and nine external JSON files matched source byte hashes. The [development report](../DEVELOPMENT_REPORT.md) distinguishes these bounded packaged checks from native save coverage; no separate packaged save/load roundtrip is claimed.

**Historical verification:** v0.4.0 passed 27 native tests (26 clean plus one with editor background HTTP warnings), with zero failures/unrun tests, and 53 Shipping interaction stages with zero failures and exit 0. Coverage includes AI, frontend, saves, terrain privacy, uphill picking, and incremental seams. The editor warning is separate from gameplay correctness. All 45 packaged process-tree samples showed zero TCP/UDP endpoints and ten staged files matched source hashes. These are bounded observations, not proof of every future path. Start a new scenario for `prototype-4.0`; earlier-rule saves are incompatible. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) and the [development report](../DEVELOPMENT_REPORT.md).

**Later work:** Cross-sector travel and physical transfer, neighboring extraction, trade, privateering, relocation, abandoned territories, richer AI strategies, distinct sector generation, and persistent multiplayer. Changing occupied AI slots during play is not supported. No existing test result establishes those features or indefinite AI survival.

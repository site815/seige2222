# seige2222 — Scenario and AI Setup

[Design index](README.md) · [Status definitions](README.md#design-status)

This document records the confirmed local setup flow and the current prototype implementation. It does not turn its finite map, AI presets, or numerical balance into final full-game rules.

**v0.8 source revision; release verification pending:** Workers deploy through the current external construction phases and durations. The core reserves nine times its initial body area and keeps defense/sensing during deployment. Local playback includes Paused/1×/5×/10×; new scenarios begin at 1×, while the automated construction/economy route uses 10×. Current `prototype-8.0` Rules include physical access-port routing and road tiers. Current format-5 saves intentionally reject earlier scenarios. See [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md).

## Starting a scenario

**Confirmed direction:** The game opens to a main menu with single-player setup, loading, settings, credits, and exit. The player configures a 3×3 neighborhood and can inspect it at a broad zoom. Human play starts with time paused while choosing the central command-core site. An AI-controlled center provides an observer mode.

| Cell | Current choices | Meaning |
| --- | --- | --- |
| Center | Player, starting AI, developed AI | Player enters paused landing selection. Either AI choice starts observation after setup. |
| Each of eight neighbors | Empty, starting AI, developed AI | Empty is the default. An occupied slot runs an independent local colony simulation. |

The default setup is one player center and eight empty neighbors. Before a human landing, the view surveys the resource deposits; placement uses sector-edge and deposit-clearance validation for the full reserved core plot. The only core deploys from the landing shuttle's carried kit with its initial crew. It does not introduce a second core or move an established colony.

## Independent alien pressure options

**Implemented in the verified v0.7 package:** Single-player setup offers independent **Background bugs** and **Periodic attacks** switches. Both default to ON. The choices apply to the human or AI center and every occupied neighbor, including the actual simulation used to prepare a developed AI colony.

| Background bugs | Periodic attacks | Pressure in this scenario |
| --- | --- | --- |
| ON | ON | Roaming bugs and scheduled invasion pulses; preserves the previous behavior. |
| ON | OFF | Roaming bugs only. |
| OFF | ON | Scheduled invasion pulses only. |
| OFF | OFF | Neither source spawns bugs. Economy, construction and objectives continue normally. |

The switches are available only before starting a scenario. The live interface shows **Disabled** instead of a pulse countdown when periodic attacks are OFF; the pressure details show both choices. These options do not introduce privateers or any other threat system.

Format-3 save snapshots and neighborhood metadata require both strict boolean settings. Loading restores them rather than adopting current setup choices. Missing, non-boolean or inconsistent settings are rejected before replacing the active scenario. The older v0.7 missing-pair default applied only to its compatible format-2 saves and is not a v0.8 migration path. Rules and AI fingerprint checks remain mandatory. Disabled schedules spawn no bugs and accumulate no delayed attacks.

## Map and information

**Current prototype:** Each colony uses a sector 60,000 logical units square and the same template of 25 irregularly clustered resource nodes. The v0.3 graphics conversion maps these to 3.6 km sectors and a 10.8 km neighborhood. The earlier v0.2 values of 600 m and 1.8 km came from its smaller rendering conversion. That v0.3 presentation change left logical distances unchanged; v0.8 separately changes travel and construction rules. These are editable scenario and presentation values, not randomized planetary geography. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md); broader wilderness and remote-outpost proportions remain design targets in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

After human landing, live information uses finite sensor coverage. Seeing sector borders does not reveal its buildings or threats. Observer mode shows the simulated colonies so the player can watch AI behavior; this is an observation tool, not a scouting advantage in human play.

The current presentation retains the cartographic neighborhood and detailed focused sector. v0.7 uses the same deterministic forest population throughout all nine sectors, with simpler distant geometry replacing the earlier sparse neighboring woodland. Selecting a sector changes the view, not ownership or sensor knowledge. Human commands remain restricted to the home colony; observer inspection does not grant construction control. See [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md) for presentation evidence and limitations.

## AI behavior and editable files

**Confirmed requirement:** AI definitions live separately in `AIFILES`. Shared resource, building, production, and threat rules stay in `Rules`.

| File | Current role |
| --- | --- |
| [colony_ai.json](../../AIFILES/colony_ai.json) | Ordered building targets, decision interval, action budget, placement search, and sensor expansion. |
| [developed_start.json](../../AIFILES/developed_start.json) | Finite initial core stock and population for developed colonies; initial buildings pay ordinary costs from that stock. |
| [Rules/scenario.json](../../Rules/scenario.json) | Finite starting inventory and population, zero credits, core, seeded 3+2 deposit generation, world extent and seed. |
| [Interface/ui.json](../../Interface/ui.json) | Construction groups and shortcut mappings, displayed summary resources, descriptions, and credits. |
| [Graphics/scene.json](../../Graphics/scene.json) | Perspective camera, orbit sensitivity, regional-view threshold, rendering scale, terrain material, vegetation density, and nature asset references; separate from logical gameplay balance. |

A starting AI begins with the ordinary colony start. Both AI types seek nearby matching deposits, place defenses and perimeter sensors, expand service capacity, and build the industrial chain. The external `complete_and_staff_in_order` target policy waits for each stage to be completed and staffed before spending on later stages. Sensors and turrets anchor toward nearby deposit approaches; their placement distance is external data. Missing earlier targets take priority over further expansion. They use ordinary placement commands and construction costs. Staffing, physical couriers, input consumption, repair, and alien pressure follow the same simulation as the player. The developed preset grants finite seed assets once, then runs actual construction, couriers, workers, services and threats within external setup time/action bounds. Required buildings genuinely finish, so preparation advances time and may manufacture goods. There is no recurring free inventory.

The v0.8 controller also constructs paid road-grid connections and uses external trading ports. It lands beside one generated standard resource, exports that local raw and imports missing recipe inputs; it does not assume every deposit exists nearby. Diplomacy, rival fleets, cross-sector combat and direct neighbor-to-neighbor trade remain outside this controller. Each occupied slot has its own stock, credits, electricity grid and local alien threats. A common rendered neighborhood does not create shared movement or inventory.

## Time, settings, and saves

Only active, unpaused play advances the colony simulations. Main menu, setup, landing, settings, and credits stop their clocks. Settings use the calibrated Medium preset with render resolution and windowed/native-borderless selection; the game menu preserves the previous pause state when returning to play. Observer mode supports camera movement, pause, and saving while disabling player construction and colony commands.

Local scenario saves include every occupied colony, scenario selections, camera, speed, pause state, and AI configuration fingerprints. Loading validates all snapshots before replacing the active scenario. AI cadence derives from saved simulation time; it has no separate hidden timer. Changed Rules or AI definitions invalidate incompatible saves. v0.8 requires bounded yaw/pitch fields in format-5 metadata. Restart scenarios to apply gameplay definition changes, and restart the application after interface or graphics edits. Live reload and general gameplay-save migration are not implemented.

v0.8 requires **format 5** for both neighborhood metadata and colony snapshots. It persists generated nodes/seeds, accounts, batteries, recipe transactions, shipments and Rex in addition to roads, routes, construction, maintenance and weapons. Camera angles, bounded position/zoom, supported nonzero speed, pause and threat flags are required. Earlier-rule/format saves are intentionally rejected with a new-scenario diagnostic; no optional camera fallback bypasses compatibility.

These single-player controls do not establish how a future persistent multiplayer world pauses, simulates offline colonies, or transfers a relocating core.

## Validation and remaining work

Run `node Tools/validate_configuration.mjs` before building. It checks Rules plus AI/UI references, bounds, construction-menu coverage, shortcut conflicts, developed preset capacity and necessary cost bounds, and Graphics settings and asset references. [Tools/build.ps1](../../Tools/build.ps1) and the [GitHub workflow](../../.github/workflows/rules.yml) run it automatically. Geometric feasibility and long-term survival still require simulation tests and playtesting.

**v0.7 verification status:** The final complete native suite passed 38 tests cleanly: zero warnings, failed or unrun tests (`Saved/Automation/v07-final/index.json`). The three threat-setting tests cover all four independent spawn combinations, deterministic save continuation, default ON behavior, malformed/partial flag rejection, real frontend button routes, starting/developed center and neighbor propagation, whole-scenario save/load, legacy defaults, and atomic rejection of inconsistent child snapshots. The setup panel fits 1366×768 by its existing proportional bounds, including its maximum four-line notice; this is a source-layout check, not a rendered screenshot check. The v0.7 Shipping route passed 79 stages with zero failures and exit 0; all nine loose JSON files matched source hashes. See the [verification record](../verification/v0.7.0.json).

**Historical v0.6 verification:** AI decision rules, preparation and local simulation rates are unchanged. The Rules schema adds stockpile/work presentation metadata; visual snapshots reset after a new scenario, landing relocation or load, so reused entity IDs cannot inherit previous motion. This does not alter AI inventory, population or construction. The v0.6 editor compiled, all 34 native tests passed cleanly (zero warnings, failed or unrun) and the 79-stage editor route passed with zero failures. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; the evidence below belongs to v0.5.

**Historical v0.5 status:** Static configuration and 29 negative Rules cases pass. The final native suite passed 29 tests (28 clean, one editor background HTTP-warning success; zero failed or unrun), including AI development, landing, observation and neighborhood save continuation. The first objective succeeds at 435 simulation seconds, and starting AI manufactures 35 components by the configured 600-second budget. The Windows Shipping package passed 63 stages with zero failures and exit 0; all 73 process-tree samples showed zero TCP/UDP endpoints, and nine external JSON files matched source byte hashes. The [development report](../DEVELOPMENT_REPORT.md) distinguishes these bounded packaged checks from native save coverage; no separate packaged save/load roundtrip is claimed.

**Historical verification:** v0.4.0 passed 27 native tests (26 clean plus one with editor background HTTP warnings), with zero failures/unrun tests, and 53 Shipping interaction stages with zero failures and exit 0. Coverage includes AI, frontend, saves, terrain privacy, uphill picking, and incremental seams. The editor warning is separate from gameplay correctness. All 45 packaged process-tree samples showed zero TCP/UDP endpoints and ten staged files matched source hashes. These are bounded observations, not proof of every future path. Start a new scenario for `prototype-4.0`; earlier-rule saves are incompatible. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) and the [development report](../DEVELOPMENT_REPORT.md).

**Current expansion and later work:** Player-directed privateering now has source support for cross-sector travel, combat, cargo capture and physical return; external trading-port shipments are also implemented. General inter-colony supply convoys and commerce, neighboring extraction, strategic AI privateer dispatch, relocation, abandoned territories and persistent multiplayer remain later work. Changing occupied AI slots during play is not supported. Expanded release acceptance is separate from the historical tests above, and no bounded test establishes indefinite AI survival.

# seige2222 — Scenario and AI Setup

[Design index](README.md) · [Status definitions](README.md#design-status)

This document records the confirmed local setup flow and the current prototype implementation. It does not turn its finite map, AI presets, or numerical balance into final full-game rules.

**Current v0.9 source, acceptance in progress:** A single upright core descends, opens its hatch and deploys a finite set of persistent worker bodies. It reserves nine times its level-one body area and retains sensing/defense during deployment. The accepted provisional 9.57 t loose kit, two-bank mixed lasers and 80 kg bodies are documented in [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md). Local playback remains Paused/1×/5×/10×. Rules `prototype-9.0` and format-7 saves supersede earlier scenarios; the [v0.8.1 verification](../verification/v0.8.1.json) is historical, not v0.9 acceptance.

## Starting a scenario

**Confirmed direction:** The game opens to a main menu with single-player setup, loading, settings, credits, and exit. The player configures a 3×3 neighborhood and can inspect it at a broad zoom. Human play starts with time paused while choosing the central command-core site. An AI-controlled center provides an observer mode.

| Cell | Current choices | Meaning |
| --- | --- | --- |
| Center | Player, starting AI, developed AI | Player enters paused landing selection. Either AI choice starts observation after setup. |
| Each of eight neighbors | Empty, starting AI, developed AI | Empty is the default. An occupied slot runs an independent local colony simulation. |

The default setup is one player center and eight empty neighbors. Before a human landing, the view surveys the resource deposits; placement uses sector-edge and deposit-clearance validation for the full reserved core plot. The only core deploys from the landing shuttle's carried kit with its initial crew. It does not introduce a second core or move an established colony.

**Confirmed October 6 setup change:** Developed AI starts from an explicit established-colony manifest, without simulating hours of historical growth during loading. Player and Starting AI still begin with six workers, the finite 9.57 t loose kit and zero credits. Developed initial assets are a separate one-time scenario definition, not production credited to that kit. The manifest loader and scenario integration are implemented in source; native and packaged acceptance remain pending.

## Independent alien pressure options

**Retained scenario controls:** Single-player setup offers independent **Background bugs** and **Periodic attacks** switches. Both default to ON. The choices apply to the human or AI center and every occupied neighbor during live simulation. Established setup does not replay an artificial history of threats before the scenario begins.

| Background bugs | Periodic attacks | Pressure in this scenario |
| --- | --- | --- |
| ON | ON | Roaming bugs and scheduled invasion pulses; preserves the previous behavior. |
| ON | OFF | Roaming bugs only. |
| OFF | ON | Scheduled invasion pulses only. |
| OFF | OFF | Neither source spawns bugs. Economy, construction and objectives continue normally. |

The switches are available only before starting a scenario. The live interface shows **Disabled** instead of a pulse countdown when periodic attacks are OFF; the pressure details show both choices. These options do not introduce privateers or any other threat system.

Current format-7 save snapshots and neighborhood metadata require both strict boolean settings. Loading restores them rather than adopting current setup choices. Missing, non-boolean or inconsistent settings are rejected before replacing the active scenario. The older v0.7 missing-pair default applied only to its compatible format-2 saves and is not a current migration path. Rules and AI fingerprint checks remain mandatory. Disabled schedules spawn no bugs and accumulate no delayed attacks.

## Map and information

**Current prototype:** Each sector is 60,000 logical units square, rendered as 3.6 km within a 10.8 km neighborhood. Every region, including empty ones, has exactly three unique standard and two unique rare deposits generated from the four-plus-four resource pool. Seeded positions stay within the authored inner area and satisfy the shared dry-terrain checks; empty regions have no invented colony entities. These values replace the historical 25-node template. Broader planetary generation and remote-outpost proportions remain design targets in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

After human landing, live information uses finite sensor coverage. Seeing sector borders does not reveal its buildings or threats. Observer mode shows the simulated colonies so the player can watch AI behavior; this is an observation tool, not a scouting advantage in human play.

The current presentation retains the cartographic neighborhood and detailed focused sector. v0.7 uses the same deterministic forest population throughout all nine sectors, with simpler distant geometry replacing the earlier sparse neighboring woodland. Selecting a sector changes the view, not ownership or sensor knowledge. Human commands remain restricted to the home colony; observer inspection does not grant construction control. See [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md) for presentation evidence and limitations.

## AI behavior and editable files

**Confirmed requirement:** AI definitions live separately in `AIFILES`. Shared resource, building, production, and threat rules stay in `Rules`.

| File | Current role |
| --- | --- |
| [colony_ai.json](../../AIFILES/colony_ai.json) | Ordered building targets, decision interval, action budget, placement search, and sensor expansion. |
| [developed_start.json](../../AIFILES/developed_start.json) | Separate established-colony manifest: explicit initial structures, local stores, batteries, worker bodies, infrastructure and account state; not an instruction to replay growth. |
| [Rules/scenario.json](../../Rules/scenario.json) | Finite starting inventory and population, zero credits, core, seeded 3+2 deposit generation, world extent and seed. |
| [Interface/ui.json](../../Interface/ui.json) | Construction groups and shortcut mappings, displayed summary resources, descriptions, and credits. |
| [Graphics/scene.json](../../Graphics/scene.json) | Perspective camera, orbit sensitivity, regional-view threshold, rendering scale, terrain material, vegetation density, and nature asset references; separate from logical gameplay balance. |

A starting AI begins with the ordinary finite-kit colony start. Developed AI instead loads the established manifest, validating its physical placement, roads, local capacities and worker identities before committing the scenario. Initial assets are explicitly authored; no simulated manufacture, trade income or survival history is claimed. Both AI types then seek matching deposits, maintain defenses and services, and operate or repair the industrial chain through ordinary placement commands, construction costs, physical couriers, inputs, electricity and alien pressure. The external `complete_and_staff_in_order` policy waits for each live target stage to be completed and staffed before spending on later stages. Missing targets receive normal rebuilding orders. There is no recurring free inventory or immunity for established colonies.

The current **provisional** manifest declares one core plus 23 facilities, with 38 assigned operators, six idle workers and four stored workers; 145 kWh spread across the authored batteries; and 0.5 initial credits. Its local age is 60 seconds. Building inventories total 7,936 kg and 5,515.8 L, including the four packed worker bodies; active bodies, installed building/road materials, weapons and the unchanged configured carried guard fleet are separate. These are authored starting totals, not earned production or a validated total transport payload. Each building names its own stock, charge and recipe. Deterministic placement tries the region's real standard deposits and bounded layout orientations, then validates dry plots, road connections and physical workstations. Invalid candidates are discarded before the live neighborhood is replaced.

`developed_initialization: established_manifest` selects this setup. The alternate `simulated_history` path is retained for diagnostics, not the default loading experience. Rendered setup can show region-loading progress and cancellation, but a single manifest/layout search is synchronous: the nominal frame budget is not a guarantee that every initialization call fits within six milliseconds. Initial-load and eight-neighbor performance still need measurement.

The v0.8 controller also constructs paid road-grid connections and uses external trading ports. It lands beside one generated standard resource, exports that local raw and imports missing recipe inputs; it does not assume every deposit exists nearby. Diplomacy, rival fleets, cross-sector combat and direct neighbor-to-neighbor trade remain outside this controller. Each occupied slot has its own stock, credits, electricity grid and local alien threats. A common rendered neighborhood does not create shared movement or inventory.

The candidate uses `economy.core_replication_policy: funded_shortage_first`: needed core goods prefer locally fundable batches without replacing committed work or bypassing automatic worker priority and staffed specialists. `bulk_input_policy: remaining_output_bill` budgets raw inputs from needed outputs of enabled installed recipes, counts coproducts once, and caps paid purchases by the useful outstanding bill, import batch, port/storage limits and credits. Manufactured/worker inputs retain their four-cycle buffers. Fuel purchasing separately targets four operating buffers and refills below half. Immediate road, power, support and port prerequisites retain priority; no goods or transport capacity are granted.

The live paid plan retains 23 facility targets, with the four-tower perimeter before industry and bulk battery storage afterward. These priorities govern Starting-AI growth and ordinary repairs/expansion after established setup. The former 18/36-hour history-generation limits are historical diagnostics, not the current Developed-AI loading requirement. Their failed runs remain evidence of limitations in autonomous growth and defense; they are not reclassified as successful established-state tests.

## Time, settings, and saves

Only active, unpaused play advances the live colony simulations and common calendar. Every live region shares 1,800 seconds daylight/1,800 seconds night and 30 days per season. Any authored established-colony age is a local initial-state field, not elapsed loading work, earned production or a different date. The common world date is bound at scenario commit, so established neighbors share the player's sunrise and season. Winter adds snow visuals without new gameplay penalties. Main menu, setup, landing, settings, and credits stop their clocks. Settings use the calibrated Medium preset with render resolution and windowed/native-borderless selection; the game menu preserves the previous pause state when returning to play. Observer mode supports camera movement, pause, and saving while disabling player construction and colony commands.

Local scenario saves include every occupied colony, scenario selections, camera, speed, pause state, and AI configuration fingerprints. Loading validates all snapshots before replacing the active scenario. AI cadence derives from saved simulation time; it has no separate hidden timer. Changed Rules or AI definitions invalidate incompatible saves. v0.9 requires bounded yaw/pitch fields and the common calendar timestamp in format-7 metadata. Restart scenarios to apply gameplay definition changes, and restart the application after interface or graphics edits. Live reload and general gameplay-save migration are not implemented.

v0.9 requires **format 7** for both neighborhood metadata and colony snapshots. It persists worker-body identities and their real task/container state, a shared calendar timestamp, world offsets, mine-to-deposit bindings, generated nodes/seeds, accounts, batteries, recipe transactions, shipments and Rex in addition to roads, routes, construction, maintenance and weapons. Camera angles, bounded position/zoom, supported nonzero speed, pause and threat flags are required. Earlier-rule/format saves are intentionally rejected with a new-scenario diagnostic; no optional camera fallback bypasses compatibility.

These single-player controls do not establish how a future persistent multiplayer world pauses, simulates offline colonies, or transfers a relocating core.

## Validation and remaining work

Run `node Tools/validate_configuration.mjs` before building. Rules, AI/UI references, bounds, construction-menu coverage, shortcut conflicts, manifest capacities and Graphics references need validation; the new established schema and runtime placement checks require fresh evidence. [Tools/build.ps1](../../Tools/build.ps1) and the [GitHub workflow](../../.github/workflows/rules.yml) run configuration checks automatically. Acceptance must cover deterministic established initialization, real roads and worker identities, finite local stock, normal post-start spending/threats, atomic failure and save continuation. It must separately retain the 24 finite-kit player/Starting-AI resource combinations. Neither setup success nor a bounded continuation test establishes indefinite survival.

**Current candidate evidence:** WorldReview16 completed 38 rendered stages with zero assertions and 1,296.8 seconds of colony simulation (`Saved/WorldReview-v09-build16.json`). The reviewed hatch/deployed-core and daytime/night/snow captures are bounded visual evidence; season changes were isolated fixtures, and the lake remains conspicuously circular. Earlier paid-growth AI failures remain historical diagnostics of that strategy. They do not test the new established manifest, whose native, rendered and Shipping acceptance is pending.

**Historical v0.7 verification status:** The final complete native suite passed 38 tests cleanly: zero warnings, failed or unrun tests (`Saved/Automation/v07-final/index.json`). The three threat-setting tests cover all four independent spawn combinations, deterministic save continuation, default ON behavior, malformed/partial flag rejection, real frontend button routes, starting/developed center and neighbor propagation, whole-scenario save/load, legacy defaults, and atomic rejection of inconsistent child snapshots. The setup panel fits 1366×768 by its existing proportional bounds, including its maximum four-line notice; this is a source-layout check, not a rendered screenshot check. The v0.7 Shipping route passed 79 stages with zero failures and exit 0; all nine loose JSON files matched source hashes. See the [verification record](../verification/v0.7.0.json).

**Historical v0.6 verification:** AI decision rules, preparation and local simulation rates are unchanged. The Rules schema adds stockpile/work presentation metadata; visual snapshots reset after a new scenario, landing relocation or load, so reused entity IDs cannot inherit previous motion. This does not alter AI inventory, population or construction. The v0.6 editor compiled, all 34 native tests passed cleanly (zero warnings, failed or unrun) and the 79-stage editor route passed with zero failures. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; the evidence below belongs to v0.5.

**Historical v0.5 status:** Static configuration and 29 negative Rules cases pass. The final native suite passed 29 tests (28 clean, one editor background HTTP-warning success; zero failed or unrun), including AI development, landing, observation and neighborhood save continuation. The first objective succeeds at 435 simulation seconds, and starting AI manufactures 35 components by the configured 600-second budget. The Windows Shipping package passed 63 stages with zero failures and exit 0; all 73 process-tree samples showed zero TCP/UDP endpoints, and nine external JSON files matched source byte hashes. The [development report](../DEVELOPMENT_REPORT.md) distinguishes these bounded packaged checks from native save coverage; no separate packaged save/load roundtrip is claimed.

**Historical verification:** v0.4.0 passed 27 native tests (26 clean plus one with editor background HTTP warnings), with zero failures/unrun tests, and 53 Shipping interaction stages with zero failures and exit 0. Coverage includes AI, frontend, saves, terrain privacy, uphill picking, and incremental seams. The editor warning is separate from gameplay correctness. All 45 packaged process-tree samples showed zero TCP/UDP endpoints and ten staged files matched source hashes. These are bounded observations, not proof of every future path. Start a new scenario for `prototype-4.0`; earlier-rule saves are incompatible. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) and the [development report](../DEVELOPMENT_REPORT.md).

**Current expansion and later work:** Player-directed privateering now has source support for cross-sector travel, combat, cargo capture and physical return; external trading-port shipments are also implemented. General inter-colony supply convoys and commerce, neighboring extraction, strategic AI privateer dispatch, relocation, abandoned territories and persistent multiplayer remain later work. Changing occupied AI slots during play is not supported. Expanded release acceptance is separate from the historical tests above, and no bounded test establishes indefinite AI survival.

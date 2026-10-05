# seige2222 — First Playable Scope

[Design index](README.md) · [Status definitions](README.md#design-status)

**Current delivery: v0.6.0 Windows Shipping package, built and verified locally. All 34 native tests passed cleanly; both editor and Shipping interaction routes passed 79 stages with zero failures.** The user requested a native Unreal single-player game, a packaged playable build, and a GitHub push. The current name is **seige2222**; SEIGE and “Robot Manor Lords” are earlier labels. This document records the implemented slice, its limitations, and version-specific evidence.

## Current v0.6 presentation work

Rules `prototype-6.0` add explicit `stockpile_visual` resource metadata and `inventory_presentation` / `worker_activity` building metadata. Ore/carbon/silica use irregular bulk rock piles, alloy uses ingots and manufactured goods use crates; indoor inventories remain inside their declared buildings. Costs, production/construction rates, population/support rules, AI plans and physical delivery mechanics are unchanged from v0.5. Static checks pass 32 deliberately invalid Rules cases and the complete configuration check. The final native report passed 34 tests cleanly (zero warnings, failed or unrun). The editor interaction route passed all 79 stages with zero failures and recorded 413 courier-motion frames between fixed simulation ticks at 1× speed. The v0.6 Shipping interaction route also passed 79 stages with zero failures and exit 0, recording 831 courier-motion frames between fixed ticks at 1× speed (`Saved/packaged-v0.6.0-UiSmoke-verification.json`).

Couriers display their actual resource payload and wait visibly at directional exterior loading ports near building endpoints, preserving the actual dispatch/arrival clock and inventory transfer. Construction stacks show only delivered material not yet incorporated into the rising structure. Assigned builders and operating workers use exterior stations and tool-fetch/work cycles rather than continuous circling. These are aggregate representations of existing staffing, not extra simulated robots or independent worker pathfinding. Read-only `HasActiveWork` gates work animation on actual staffing, inputs, output space, support and maintenance conditions; idle tools do not manufacture goods.

ID-keyed snapshots interpolate courier/bug positions and construction progress between fixed simulation steps. Builder/service motion uses the same interpolated simulation clock and freezes while paused. This makes motion update each rendered frame without raising the simulation rate or changing transport speed. Snapshot history resets after scenario/landing/load transitions; it is derived presentation state, not saved economic state. Passing native regressions cover interpolation by identity/reset, exterior geometry and work eligibility. Evidence: `Saved/Automation/v06-final/index.json`, `Saved/render-v06-final.log` and `Saved/PresentationSmoke.json`. The rebuilt final native report is clean; it does not establish packaged network behavior. See [Colony Operations](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md) and [Rules Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md).

Completed outdoor yards compactly place only resources actually present and choose deterministic exterior positions clear of known living buildings and earlier piles. Workstations also avoid those bounds and visible piles. Hidden neighbor buildings do not influence these choices. If no clear position exists, that visual representation is omitted; the inventory remains unchanged. Stockpieces, scaffolds and visible worker counts are representative, not literal unit counts or a new collision/pathfinding system. Exact amounts remain in the building information panel; construction reveals the structure progressively rather than simulating individual installed parts.

The packaged test verifies the exercised mechanics and interface paths; it is not a separate packaged save/load roundtrip, an occupied-service-bay acceptance test, or a controlled performance benchmark. Native tests cover deterministic construction/cargo and neighborhood save continuation. Display and benchmark results are documented separately by the release report.

## Historical v0.5 construction and services

The user requires buildings assembled by workers from physically delivered materials, initial core deployment from an orbital shuttle, and expandable robot charging/maintenance facilities. These are implemented under Rules `prototype-5.0`. Static validation passes 29 negative Rules cases and the complete configuration check. The final native suite passed **29 tests: 28 clean and one with an Unreal editor background HTTP warning; zero failed or unrun** (`Saved/Automation/v05-final/index.json`). The first-objective test manufactured 12 components at 435 simulation seconds without inventory grants or construction bypasses; the starting AI manufactured 35 by its configured 600-second preparation budget.

The Shipping package in `Builds/v0.5.0/Windows` passed **63 interaction stages with zero failures and exit 0**. Its 73 live process-tree samples each showed zero TCP/UDP endpoints, and all nine external JSON files matched source byte hashes. The launcher selected this version for that release. These bounded checks do not prove every play path or indefinite sustainability; no separate packaged save/load roundtrip or occupied-service-bay visual check is claimed. Native tests cover construction/service persistence and neighborhood saves. See `Saved/packaged-v0.5.0-UiSmoke-verification.json` and the [development report](../DEVELOPMENT_REPORT.md) for full boundaries.

The current data specifies six starting robots, a six-second core deployment, eight starter support berths, and a service bay providing sixteen additional berths with one operating job. The bay takes two builders and fifteen seconds at full efficiency. All quantities remain editable prototype balance. Starter validation checks enough support for core staffing plus construction of the first service expansion, and finite inventory for construction, new workers and retained repair buffers. Current starter alloy is 420 units; this is balance data, not an engine constant. Charging means automated support capacity and local maintenance supplies, without a kW grid or per-robot battery simulation.

## Historical v0.4 graphics revision

**v0.4.0 was packaged and verified in `Builds/v0.4.0/Windows`; the launcher selected it for that release.** The slice includes responsive orbit, regional map/focused detail, resource overlays/alerts, floating bottom construction, complete building statistics, and rebuilt terrain with dense low/tall grass. The final native suite passed **27 tests: 26 clean plus one with editor background HTTP warnings; zero failed or unrun**. Shipping packaging exited 0; its interaction route passed **53 stages with zero failures**, 45 process-tree samples each showed zero TCP/UDP endpoints, and ten staged files matched source hashes. The editor warning was not a gameplay assertion failure; the packaged observations are bounded checks, not a guarantee about all runs. Close/middle views improve while distant ground/forest uniformity remain below the reference. **Start a new scenario:** Rules `prototype-4.0` use actual weapon shots/reloads and explicit unarmed/zero-power information; older-rule saves are rejected. See [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md) and the [development report](../DEVELOPMENT_REPORT.md).

**Historical v0.3 verification:** The rotatable perspective camera, credible physical scale, detailed original industry, licensed CC0 nature, and revised ground surfaces are included. [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md) records implementation and remaining visual polish. Logical content/balance is unchanged; six rendered centimeters per logical unit gives 3.6 km sectors and a 10.8 km neighborhood.

- Win64 Shipping packaging completed with exit 0 in `Builds/v0.3.0/Windows`.
- Nineteen native tests passed with zero failures or warnings (`Saved/Automation/v03-final/index.json`). The delivered v0.3 source did not change after that run; current v0.4 work requires separate checks.
- The packaged rendered interaction route completed 28 stages with zero failures and exit 0, including frontend, human landing/construction, observation, and perspective selection in rotated/low-angle views.
- Eighteen samples of the packaged process tree each showed zero TCP/UDP endpoints. This is a bounded endpoint observation, not packet capture or proof about all execution paths.
- Ten loose files across Rules, AIFILES, Interface, and Graphics matched source hashes and loaded successfully. The controlled packaged balance-edit experiment remains v0.2 evidence and was not repeated for v0.3.
- Native tests verify format-2 camera compatibility and neighborhood save continuation. No separate packaged save/load roundtrip was run for v0.3.
- Remaining art work includes distant canopy thinning, terrain repetition, and regional presentation; the build does not claim Manor Lords parity. Existing robot/bug art is retained with no new character animation.

See the [development report](../DEVELOPMENT_REPORT.md) for logs and actual game captures. These results establish the tested prototype paths, not exhaustive human playability, indefinite sustainability, or finished art.

## Historical v0.2 baseline

- The revised Unreal editor and Win64 Shipping targets compiled, and the standalone v0.2.0 package launched successfully outside the editor.
- The external rule validator passed for nine items, six recipes, and thirteen building definitions, including fifteen invalid-data cases. Its dependency and startup-stock checks do not prove live economic or defensive solvability.
- All sixteen native tests passed with zero failures or warnings: six simulation tests, four AI tests, three controller/HUD interaction tests, and three frontend tests. The interaction tests call the real controller/HUD route without a drawing canvas and check menu isolation; they do not simulate a human's complete rendered play session.
- On the revised resource map, a normal-action strategy with sensors and turrets covering the three resource approaches completed the objective at 360 simulation seconds with twenty-two manufactured components, thirty-three robots filling thirty-three jobs, and no buildings lost. This verifies one winning strategy, not indefinite self-sufficiency.
- AI tests covered invalid definitions, starting/developed production, deterministic save continuation, and relocated-core threat spawning. The starting AI manufactured five components by 300 simulation seconds; this does not establish that the simple AI wins every scenario.
- The final native report is `Saved/Automation/v02-final/index.json`: sixteen successes, zero failures, warnings, or unrun tests. Generated reports are not design specifications or tracked source artifacts.
- The rendered `-UiSmoke -ForceRes -ResX=1600 -ResY=900` route completed twenty-three stages with zero failures, covering frontend navigation, landing, controller construction clicks, shortcuts, the neighborhood view, credits, and AI observation. `Saved/PresentationSmoke.json` records the result. This is programmatic rendered coverage, not a complete human play-through or a benchmark across hardware.
- The packaged game passed its twenty-three-stage rendered interaction route with zero failures and zero observed TCP/UDP endpoints. A temporary packaged starting-population edit took effect without rebuilding, and the exact original bytes were restored. The [development report](../DEVELOPMENT_REPORT.md) records these release checks and the remaining scope. Further code or content changes need appropriate revalidation.

## Purpose

Test whether a visible robotic colony is engaging when the player establishes an interconnected industrial chain, keeps materials moving, and prepares automatic defenses against alien pressure. The authorized expansion adds a local scenario setup, simple AI colonies, and observation of an AI-controlled center before persistent multiplayer.

The complete design remains broader than this prototype. Scenario cells can be empty, starting AI, or developed AI; empty neighbors are the default. Assigning AI to the center selects observer play. The first AI implementation uses independent instances of the same colony simulation. Cross-colony combat, trade, and fleet missions are not included. The persistent-world design baseline remains unchanged.

## Current external definition set

The following files exist in [Rules](../../Rules/resources.json). They are prototype content and balance, not a finalized catalog.

| Definition file | Contents currently represented |
| --- | --- |
| [resources.json](../../Rules/resources.json) | Four raw materials: iron ore, copper ore, silica, carbon. Five manufactured items: alloy stock, conductors, substrates, circuits, and robot components. Each declares bulk/ingot/crate stockpile presentation. |
| [recipes.json](../../Rules/recipes.json) | Alloy, conductor, substrate, circuit, and component recipes; a separate robot-assembly recipe consumed by population production. |
| [buildings.json](../../Rules/buildings.json) | One command-core definition; four extractor types; alloy refinery, conductor works, substrate works, circuit works, component works; sensor mast, sentinel turret, cargo depot, robot charging/service bay. Each declares construction duration/builders, support capacity, indoor/outdoor inventory presentation and worker activity. The core is supplied by the scenario and is not another build-menu option. |
| [policies.json](../../Rules/policies.json) | Simulation timing, staffing, population adjustment, physical delivery, repair/upkeep, visibility, threat behavior, scenario objectives, and numerical tuning. |
| [scenario.json](../../Rules/scenario.json) | First Landing title, initial core, starting population and inventory, separate landing deployment kit and escape-shuttle cargo, world extent, seed, and irregularly distributed resource-source locations. |
| [AI definitions](../../AIFILES/README.md) | A separate `AIFILES` folder defines construction priorities, decision timing, placement/sensor search, and a finite developed-colony preset. Runtime AI uses the normal simulation rules. |
| [Graphics definitions](../../Graphics/scene.json) | Camera limits/clearance and orbit sensitivity, regional-view threshold, logical-to-rendered scale, relief/pad settings, terrain/cloud materials and lighting, vegetation density/scales, and ten nature roles. These change presentation without changing logical costs, rates, or travel times. |

The proposed twelve-resource catalog is not all implemented. The current fourteen building definitions represent reusable prototype functions, not adoption of every proposed facility or upgrade. The JSON files own exact quantities and rates so this document does not become a second balance table.

## Implemented behavior for this slice

These behaviors form the current prototype. The checks above and development report establish specific tested paths; the table does not imply every scenario and edge case has been exhaustively verified.

| Area | Prototype behavior |
| --- | --- |
| Colony start | One shuttle-carried core kit, starting robots and finite inventory. Workers deploy the only core after site selection; ordinary operations begin on completion. |
| Scenario and observer | Local setup supports empty, starting-AI, and developed-AI cells. A human center chooses an initial core landing position; an AI center runs under observer controls. The controller does not receive free materials while ticking. |
| Construction | Orders reserve uncommitted core stock after protected operating buffers. Physical couriers deliver materials, then assigned builders assemble the site. Construction has no production, repair, sensor or weapon operation. Reservations prevent double spending. |
| Production | Matching extractors produce at their defined rates. Processors consume delivered local inputs and generate local output through external recipes. |
| Workforce | Automatic staffing and job-driven population production. A fully staffed operating requirement, a minimum population, and delayed retirement without material refunds are prototype policies. The assembly-input buffer reserves components for future robots; it does not increase the population target above job demand. |
| Physical delivery | Automatic couriers carry limited cargo between source and destination inventories. Local production availability depends on actual delivery. |
| Robot support | Completed, enabled and staffed cores/service bays provide capacity. Growth needs open jobs and capacity. Each support building consumes local delivered components for its assigned robots; missing support or maintenance lowers efficiency. Capacity loss does not delete existing robots. |
| Repairs | Automatic repairs consume alloy delivered to the damaged building under the selected local-repair policy. |
| Visibility | Core and sensor definitions provide finite coverage. Construction/target information should follow the implemented visibility rules. |
| Threats | Roaming bugs and recurring waves create automatic combat pressure. Wave strength responds to prototype colony metrics, with rates and limits in data. |
| Defenses | Core and turrets engage threats automatically; building health and damage remain meaningful. |
| Emergency departure | Manual ejection or core destruction ends the local scenario while retaining only the separately preloaded shuttle cargo. The default scenario starts with an empty shuttle; no core-stock transfer occurs. |
| Objective | Survive for the configured duration and actually manufacture the configured component output while maintaining the required industrial building. Starting stock alone must not satisfy a production objective. |
| Local state | Local save/load includes inventory, cargo, timers, population, threats, random state, construction progress/site materials and cargo purpose, maintenance state, all occupied sectors, camera/time controls, and rule/AI fingerprints; native continuation tests cover it. It does not implement offline multiplayer progression. |

The precise local interface and input bindings belong with the delivered build's instructions. The full [interface direction](INTERFACE_AND_CONTROLS.md) remains the design guide; fleet controls are not implied to exist in this slice.

## Explicit simplifications and differences from the full design

- **Placement footprints:** Simulation spacing uses circles, while rendered foundations/pads are square. Diagonal placements can pass spacing checks while square corners overlap; building-art and footprint alignment work is deferred.
- **Construction:** Physical delivery and aggregate builders are implemented. Only core stock funds orders; factory input stock cannot be spent remotely. Pausing a site retains its reservation. Cancellation, partial construction bills and individual builder navigation are not implemented.
- **Workers:** Aggregate job allocation and decorative/representative robot presentation can stand in for fully individualized worker scheduling. Do not present population count as unlimited extraction capacity.
- **Population reduction:** Retirement without refunds is a selected prototype policy. It does not settle dismantling, deactivation, storage, or recovery for the complete game.
- **Civilian needs:** Local core/service-bay component upkeep and a shortage efficiency effect are prototype policies. Happiness, the tentative morale share, dissatisfaction, and revolt remain undesigned or unimplemented here.
- **Energy:** The resource proposal's carbon-powered starter generator is not part of the current definition set. Charging/service facilities represent capacity and locally supplied maintenance. No full power network or individual battery charge is simulated.
- **Transport:** Simple couriers demonstrate inventory movement. The full vehicle/mech fleet production, route planning, cargo loss/salvage, and privateering model is later work.
- **World:** The local neighborhood presents independent colony sectors. This does not implement cross-sector extraction, travel, shared combat, or persistent-world ownership. Configured sector dimensions and resource placement are prototype values, not final map-scale balance.
- **AI:** A deterministic target-building controller chooses nearby deposits, extends sensors, and replaces missing facilities when it can afford them. Developed colonies run the actual simulation from a finite stock/population seed until their required buildings finish, within external time/action bounds. Delivery, staffing, services and threats apply during preparation; elapsed time and manufactured output are genuine. This does not model strategic diplomacy, trade, or hostile fleet decisions.
- **Emergency escape:** A separate preloaded-cargo state and scenario-ending departure are implemented in the simulation. Interactive shuttle loading, boarding, destination selection, and world relocation are outside this slice. The default shuttle is empty; launch never copies core inventory. This is not a complete physical loading or relocation system.
- **Balance:** The external files supply playable test assumptions. The final invasion clock, production rates, starter inventory, core defense strength, and victory/loss rules remain subject to review.
- **Persistence:** Local save/load and any local time controls are prototype facilities. They do not settle the persistent game's authority, server timeline, or offline behavior.

## Deliberately later work

The single-player target is configured for offline Shipping packaging. HTTP, network discovery, and telemetry plugins are disabled, and the Shipping configuration avoids Unreal's development profiling listener. The v0.3 packaged interaction check completed, with zero TCP/UDP endpoints in eighteen observed process-tree samples. Future multiplayer work must introduce networking deliberately; it is not a dependency of this playable.

Strategic AI faction choices beyond the simple colony controller; privateers and fleet missions; aggression settings and fleet-level orders; trade; inter-sector extraction; sensor theft; full loot and salvage; orbital relocation and adjacent destinations; leaderless areas and scavenging; revolt; building upgrades; the remaining proposed resource branches; specialized low-population balance; complete robot-needs design; and persistent multiplayer services.

These omissions do not remove those ideas from their subject documents. The full single-player game should eventually represent the same gameplay rules intended for persistent multiplayer.

## Acceptance checklist and verification boundaries

This checklist remains useful for subsequent revisions. Current verification is recorded above; it is not a declaration that every item was rerun end to end in the packaged v0.5 build. In particular, save/load is covered natively, while the controlled packaged balance edit is historical v0.2 evidence.

1. Compile and launch the native Unreal editor/game targets; produce a standalone package that launches outside the editor.
2. Load the external definition set with useful errors for broken references or invalid rules. In the packaged build, verify a controlled numerical rule edit takes effect after the documented restart/reload path.
3. Place a viable extraction-to-components chain and demonstrate new production after consuming starting inputs; validate placement and insufficient-stock failures.
4. Observe goods leaving one inventory, travelling with limited capacity, and entering another without duplication. A processor lacking delivered inputs must wait.
5. Observe job vacancies, rate-limited robot production, automatic staffing, and the documented retirement behavior without adding manual population targets.
6. Observe shortages, repairs, and automatic defense under meaningful damage. Confirm the core can fail, launch preserves only preloaded shuttle cargo, and the slice does not claim complete shuttle relocation.
7. Show finite live visibility and explain blocked activity clearly enough to play without inspecting source files.
8. Check save/load across active production, threats, and cargo if the save feature is delivered. Record remaining unsupported behavior rather than declaring the full state model verified.
9. Record actual build, package, and gameplay results in the development report. Push only the intended project contents, with generated build/cache output excluded from source control.
10. Exercise actual controller clicks outside HUD draw passes, menu/observer input isolation, legal and rejected landings, starting/developed AI operation, and deterministic AI continuation after save/load. Verify scenario metadata protects the AI configuration fingerprint as well as each colony's rule fingerprint.

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md)
- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Building Proposal](BUILDING_PROPOSAL.md)
- [Production Dependencies and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md)
- [Design Review](DESIGN_REVIEW.md)

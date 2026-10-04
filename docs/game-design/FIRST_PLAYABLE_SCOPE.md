# seige2222 — First Playable Scope

[Design index](README.md) · [Status definitions](README.md#design-status)

**Authorized development scope, implementation in progress.** The user requested a native Unreal single-player game, a packaged playable build, and a GitHub push. The current working name is **seige2222**; SEIGE and “Robot Manor Lords” are earlier working labels. This file records the first implementation slice, its limitations, and the specific verification status below.

## Verification status

- An Unreal editor-target build has succeeded during development. A completed editor build alone does not verify the latest packaged game or gameplay outcomes.
- The external rule validator passed for nine items, six recipes, and thirteen building definitions, including fifteen invalid-data cases. Its dependency and startup-stock checks do not prove live economic or defensive solvability.
- Six native Unreal simulation tests passed: runtime rule validation, physical delivery and resource accounting, automatic staffing/repairs, deterministic save continuation, core loss with preloaded-only shuttle cargo, and first-objective solvability. The ordinary-action scenario completed at about 370 simulation seconds with twelve manufactured components; it lost the silica extractor and depot. This verifies one winning strategy, not indefinite self-sufficiency or the rendered interface.
- The private GitHub repository has been created. The project push has not yet been reported as completed.
- Final revision build checks after further changes, visual playtesting, packaged launch, and packaged rule editing remain pending until their results are recorded. Detailed local test output is generated under Saved and is not a design specification.

## Purpose

Test whether a visible robotic colony is engaging when the player establishes an interconnected industrial chain, keeps materials moving, and prepares automatic defenses against alien pressure. Build a playable local scenario before expanding to neighboring factions or persistent multiplayer.

The complete design remains broader than this prototype. AI opponents are part of the intended single-player game, but neighboring colonies are not included in this first slice. The persistent-world design baseline remains unchanged.

## Current external definition set

The following files exist in [Rules](../../Rules/resources.json). They are prototype content and balance, not a finalized catalog.

| Definition file | Contents currently represented |
| --- | --- |
| [resources.json](../../Rules/resources.json) | Four raw materials: iron ore, copper ore, silica, carbon. Five manufactured items: alloy stock, conductors, substrates, circuits, and robot components. |
| [recipes.json](../../Rules/recipes.json) | Alloy, conductor, substrate, circuit, and component recipes; a separate robot-assembly recipe consumed by population production. |
| [buildings.json](../../Rules/buildings.json) | One command-core definition; four extractor types; alloy refinery, conductor works, substrate works, circuit works, component works; sensor mast, sentinel turret, cargo depot. The core is supplied by the scenario and is not another build-menu option. |
| [policies.json](../../Rules/policies.json) | Simulation timing, staffing, population adjustment, physical delivery, repair/upkeep, visibility, threat behavior, scenario objectives, and numerical tuning. |
| [scenario.json](../../Rules/scenario.json) | First Landing title, initial core, starting population and inventory, separate initial shuttle cargo, world extent, seed, and four resource-source locations. |

The proposed twelve-resource catalog is not all implemented. The current thirteen building definitions represent reusable prototype functions, not adoption of every proposed facility or upgrade. The JSON files own exact quantities and rates so this document does not become a second balance table.

## Planned behavior for this slice

These are the implementation targets being built and reviewed. A definition or planned system is not evidence that its execution has passed testing.

| Area | Prototype behavior |
| --- | --- |
| Colony start | One command core, a small robotic workforce, and finite inventory supplied by the scenario. |
| Construction | Place supported buildings, with costs paid directly from available core inventory. Invalid placement or insufficient stock should be rejected. |
| Production | Matching extractors produce at their defined rates. Processors consume delivered local inputs and generate local output through external recipes. |
| Workforce | Automatic staffing and job-driven population production. A fully staffed operating requirement, a minimum population, and delayed retirement without material refunds are prototype policies. The assembly-input buffer reserves components for future robots; it does not increase the population target above job demand. |
| Physical delivery | Automatic couriers carry limited cargo between source and destination inventories. Local production availability depends on actual delivery. |
| Robot support | Component upkeep is collected at the core as a simplified service hub; shortages reduce efficiency. This is a test policy, not the final robot-needs or happiness model. |
| Repairs | Automatic repairs consume alloy delivered to the damaged building under the selected local-repair policy. |
| Visibility | Core and sensor definitions provide finite coverage. Construction/target information should follow the implemented visibility rules. |
| Threats | Roaming bugs and recurring waves create automatic combat pressure. Wave strength responds to prototype colony metrics, with rates and limits in data. |
| Defenses | Core and turrets engage threats automatically; building health and damage remain meaningful. |
| Emergency departure | Manual ejection or core destruction ends the local scenario while retaining only the separately preloaded shuttle cargo. The default scenario starts with an empty shuttle; no core-stock transfer occurs. |
| Objective | Survive for the configured duration and actually manufacture the configured component output while maintaining the required industrial building. Starting stock alone must not satisfy a production objective. |
| Local state | Save/load is a prototype target including inventory, cargo, timers, population, threats, random state, and rule-version/fingerprint handling. It does not implement offline multiplayer progression. |

The precise local interface and input bindings belong with the delivered build's instructions. The full [interface direction](INTERFACE_AND_CONTROLS.md) remains the design guide; fleet controls are not implied to exist in this slice.

## Explicit simplifications and differences from the full design

- **Construction delivery:** Costs are debited from core inventory immediately. Material hauling to construction sites, construction jobs, and staged assembly are not yet simulated.
- **Workers:** Aggregate job allocation and decorative/representative robot presentation can stand in for fully individualized worker scheduling. Do not present population count as unlimited extraction capacity.
- **Population reduction:** Retirement without refunds is a selected prototype policy. It does not settle dismantling, deactivation, storage, or recovery for the complete game.
- **Civilian needs:** Core-collected component upkeep and a shortage efficiency effect are provisional. Happiness, the tentative morale share, dissatisfaction, and revolt remain undesigned or unimplemented here.
- **Energy:** The resource proposal's carbon-powered starter generator is not part of the current definition set. No full power network, battery charge, or charging-facility simulation is promised by this slice.
- **Transport:** Simple couriers demonstrate inventory movement. The full vehicle/mech fleet production, route planning, cargo loss/salvage, and privateering model is later work.
- **World:** One local scenario does not implement the nine-sector neighborhood, remote extraction across sector boundaries, or final map scale and wilderness proportions.
- **Emergency escape:** A separate preloaded-cargo state and scenario-ending departure are implemented in the simulation. Interactive shuttle loading, boarding, destination selection, and world relocation are outside this slice. The default shuttle is empty; launch never copies core inventory. This is not a complete physical loading or relocation system.
- **Balance:** The external files supply playable test assumptions. The final invasion clock, production rates, starter inventory, core defense strength, and victory/loss rules remain subject to review.
- **Persistence:** Local save/load and any local time controls are prototype facilities. They do not settle the persistent game's authority, server timeline, or offline behavior.

## Deliberately later work

The current single-player executable is packaged for offline use. HTTP, network discovery, and telemetry plugins are disabled, and the Shipping build avoids Unreal's development profiling listener. Future multiplayer work must introduce networking deliberately; it is not a dependency of this playable.

AI neighboring colonies and their development choices; privateers and fleet missions; aggression settings and fleet-level orders; trade; inter-sector extraction; sensor theft; full loot and salvage; orbital relocation and adjacent destinations; leaderless areas and scavenging; revolt; building upgrades; the remaining proposed resource branches; specialized low-population balance; complete robot-needs design; and persistent multiplayer services.

These omissions do not remove those ideas from their subject documents. The full single-player game should eventually represent the same gameplay rules intended for persistent multiplayer.

## Acceptance checks — complete only when individually verified

1. Compile and launch the native Unreal editor/game targets; produce a standalone package that launches outside the editor.
2. Load the external definition set with useful errors for broken references or invalid rules. In the packaged build, verify a controlled numerical rule edit takes effect after the documented restart/reload path.
3. Place a viable extraction-to-components chain and demonstrate new production after consuming starting inputs; validate placement and insufficient-stock failures.
4. Observe goods leaving one inventory, travelling with limited capacity, and entering another without duplication. A processor lacking delivered inputs must wait.
5. Observe job vacancies, rate-limited robot production, automatic staffing, and the documented retirement behavior without adding manual population targets.
6. Observe shortages, repairs, and automatic defense under meaningful damage. Confirm the core can fail, launch preserves only preloaded shuttle cargo, and the slice does not claim complete shuttle relocation.
7. Show finite live visibility and explain blocked activity clearly enough to play without inspecting source files.
8. Check save/load across active production, threats, and cargo if the save feature is delivered. Record remaining unsupported behavior rather than declaring the full state model verified.
9. Record actual build, package, and gameplay results in the development report. Push only the intended project contents, with generated build/cache output excluded from source control.

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md)
- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Building Proposal](BUILDING_PROPOSAL.md)
- [Production Dependencies and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md)
- [Design Review](DESIGN_REVIEW.md)

# seige2222 — Design Review and Recommended Next Documents

[Design index](README.md) · [Status definitions](README.md#design-status)

This review distinguishes the complete design from the authorized first playable. Recommendations are assistant analysis. The user has authorized actual native Unreal development, packaging, and a GitHub push; that authorization does not make every proposed mechanic a finalized requirement.

## Current assessment

The concept is coherent: sustain a robotic colony through visible industry and physical logistics, gain capabilities from resources and buildings, and prepare defenses against automatically executed threats. Persistent multiplayer remains the design baseline, with single-player implemented first.

The project has a concrete [first-playable scope](FIRST_PLAYABLE_SCOPE.md), separate [building](BUILDING_PROPOSAL.md) and [resource](RESOURCE_PROPOSAL.md) proposals, a [production dependency review](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md), and [rules architecture](RULES_AND_SIMULATION_ARCHITECTURE.md). External JSON defines content, balance, and local AI. The verified v0.7 package adds independent scenario threat settings, scenery distance detail and budgeted streaming. All 38 native tests passed cleanly; Shipping passed 79 interaction stages with exit 0 and four actual display states, all with zero failures. Its 86 observed process-tree socket samples were zero, and nine loose JSON files matched source hashes. See the [v0.7 verification record](../verification/v0.7.0.json). Historical v0.4.0 verification recorded 27 native passes (one with editor HTTP warnings), 53 Shipping stages with exit 0, matching staged definitions, and zero observed process-tree sockets. Full details and limits are in the [development report](../DEVELOPMENT_REPORT.md); current scenery evidence is in [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md). Final performance benchmarks remain separate, and reference-game visual parity remains unestablished.

The previous delivered v0.3 passed nineteen native tests and twenty-eight packaged interaction stages. Eighteen socket samples observed zero TCP/UDP endpoints, and ten loose files matched source hashes. Those results remain evidence for that version. Source and provenance belong in [site815/seige2222](https://github.com/site815/seige2222); local versioned binaries are excluded from Git. Broader human playability, indefinite sustainability, and Manor Lords visual parity are not established by these checks; see the [development report](../DEVELOPMENT_REPORT.md).

The current name is **seige2222**. SEIGE is the earlier project label retained in the workspace path, and “Robot Manor Lords” was an earlier informal name.

## Document-by-document review

| Document | What is clear | Next useful refinement |
| --- | --- | --- |
| [Design index](README.md) | Status definitions, current name, development authorization, and document boundaries. | Keep links and implementation status synchronized as the build changes. |
| [Vision and Setting](VISION_AND_SETTING.md) | Functioning sci-fi civilization, robotic colony, humanity's alien war, and the limited use of reference games. | Evaluate whether actual play communicates the intended colony/industry/defense tradeoff. |
| [Art Direction](ART_DIRECTION.md) | Realistic Earth-like landscapes, detailed realistic futuristic buildings, empathetic robots, and threatening bugs; earlier conflicting art guidance is superseded. | Evaluate material detail, terrain, lighting, and readable industrial feedback in the running game; procedural art alone does not establish the requested realism. |
| [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md) | The delivered perspective view, physical scale, assets, and unchanged logical economy have explicit boundaries. | Preserve this version's native/package evidence as history; new rendering changes need new checks. |
| [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md) | Background-first art, responsive orbit, regional map/focused detail, floating overlays, complete statistics, and initial native evidence are separated from final verification. | Review actual appearance, real pointer gestures, information boundaries, sector transitions, and movement performance before packaging. Matching the visual reference remains a target. |
| [Interface and Controls](INTERFACE_AND_CONTROLS.md) | Resource overlays with alerts, floating bottom construction, top-left version/FPS, complete building dossiers, automatic colony work, and future fleet-level commands. | Test real clicks, menu isolation, shortcuts, placement, shortages, workforce feedback, and access to all information sections. Fleet interfaces remain later work. |
| [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md) | The complete intended colony loop and its competing priorities. | Keep this broader design distinct from the narrower implementation scope. |
| [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) | The implemented external content set, prototype behavior, explicit shortcuts, omissions, and scoped verification. | Record observed build/gameplay results separately; revise scope if actual behavior changes. |
| [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) | Nine-sector configuration, paused core selection, local AI/observer behavior, external definitions, and local saves. | Test the full rendered neighborhood, improve AI resilience, and design actual cross-sector transactions before treating independent colonies as a connected world. |
| [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md) | Automatic staffing, production, hauling, repair, and job-driven population adjustment. | Evaluate scarcity allocation and job-demand behavior; prototype full staffing and retirement policies do not finalize them. |
| [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md) | Large wilderness sectors, a nine-sector overview, finite sensors, one central core, remote extensions, and preloaded shuttle escape/relocation direction. | Choose connected-world distances and actual relocation behavior later. Independent local colony instances and an end screen do not implement these systems. |
| [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md) | Surviving abandoned territory can retain costly-to-recover stockpiles. | Design continuing AI behavior, ownership, and persistence before implementing scavenging. |
| [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md) | Material access replaces research; layered manufacturing and rate limits make geography matter. | Test the initial chain's throughput and bottlenecks, then expand toward meaningful resource specialization. |
| [Population, Necessities, and Morale](POPULATION_AND_MORALE.md) | Robots grow to meet jobs, production is rate/input constrained, and population declines with demand. | Define robot needs and satisfaction. Component upkeep, efficiency penalties, and no-refund retirement are prototype assumptions. |
| [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md) | Roaming threats, scaled invasions, automatic combat, privateering, and removal of formal war from current scope. | Test readable local defense first; raid damage rules, strength measures, and final timing remain open. |
| [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md) | Land fleets, cargo capacity, physical hauling, automatic highest-tier-first loot, and fleet-level control. | Couriers alone do not implement fleet missions, aggression settings, salvage, privateering, or full loot. Develop those as a later slice. |
| [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md) | Shared gameplay direction with single-player first and later persistent multiplayer. | Preserve state/command boundaries while testing locally; do not claim network authority or offline progression already exists. |
| [Initial Provisional Catalog](PROVISIONAL_CATALOG.md) | Earlier material, building, and recipe proposals are retained with their original status. | Use it as design history, not the definitive implementation roster; human food remains deferred. |
| [Resource Proposal](RESOURCE_PROPOSAL.md) | Latest confirmed pool is four standard plus four rare types; each region draws three plus two unique deposits. | Test all 24 resource subsets with the zero-credit road/solar/trading-port bootstrap. Recipe quantities and energy/storage values remain proposals. |
| [Building Proposal](BUILDING_PROPOSAL.md) | Resource/logistics/defense organization allows secondary capabilities; all sixteen earlier functions are reviewed for robots. | Compare candidate functional groupings against play. Current JSON buildings are a concrete subset, not final adoption of the whole catalog. |
| [Production Dependencies and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) | Shared manufactured inputs create choices across robots, hauling, sensors, and defense. | Keep broad example chains separate from the actual prototype chain and validate bootstrap/throughput with real state. |
| [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) | External data, generic execution, validation, state, presentation, packaging, and future authority have defined boundaries. | Check invalid definitions and packaged rule editing. New mechanisms and long-term migration still require engineering. |
| [Open Decisions and Design Evolution](DESIGN_DECISIONS.md) | Corrections and open choices protect against treating past proposals as current rules. | Record full-game decisions separately from provisional values chosen to make the prototype playable. |

## Supporting documents now created

The earlier recommendations for colony operations and production dependencies now have documents, alongside separate resource/building proposals, the concrete first-playable scope, and a rules architecture document. Further paperwork should support an observed problem or the next implementation slice rather than postpone trying the game.

The largest current validation questions are whether the economy can replenish itself, whether automatic logistics is legible, whether staffing and resource shortages have understandable consequences, and whether defense pressure allows meaningful construction choices. Actual play should inform revisions.

## Additional documents worth creating later

| Priority and timing | Proposed document | What it should resolve |
| --- | --- | --- |
| Before expanding beyond the local scenario | World Scale and Information Model | Sector connectivity, distances, roads/paths, ground travel, sensor coverage and memory, and consistent game-time units. The existing rules architecture covers execution boundaries but does not settle the world's scale. |
| Before introducing player-controlled fleets | Fleet Missions and Encounter Lifecycle | Mission/direct movement interaction, aggression, encounters, disengagement, hauling, automatic loot, loss, salvage, and return delivery. |
| Before implementing a lasting needs/revolt system | Robot Needs and Colony Stability | Robot-serving goods, satisfaction, shortage consequences, possible revolt, warnings, recovery, and interaction with job-driven production. |
| Before persistent multiplayer implementation | Persistent Multiplayer Operations | Authority, ruleset versions, continuing simulation, cross-sector transactions, reconnect/recovery, abandonment, and relocation ownership/destination reservations. |

These are proposals, not instructions to implement all systems immediately. Complete lore, exhaustive unit catalogs, and final numerical balance should not block the current playable experiment.

## Validation discipline

Use the acceptance checks in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md). Keep a concrete distinction between files saved, code compiled, behavior exercised, a package launched, and a repository pushed. Do not mark a system complete because its configuration exists or because a different subsystem passed a test.

Keep prototype departures explicit. Direct core-inventory payment for construction is a documented temporary simplification. It does not overturn physical logistics. Emergency departure now retains a separate preloaded shuttle inventory, initially empty, and never takes goods from core stock. That scenario-ending behavior does not implement interactive loading, boarding, or world relocation.

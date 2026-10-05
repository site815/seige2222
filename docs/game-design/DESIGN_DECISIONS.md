# seige2222 — Open Decisions and Design Evolution

[Design index](README.md) · [Status definitions](README.md#design-status)

A cross-system decision register and the corrections that prevent superseded ideas from returning as current rules.

**Current development decision:** The working name is **seige2222**. Native Unreal Engine 5.8.3 single-player development, a playable package, and a GitHub push are authorized. Gameplay definitions and numerical balance must remain externally editable. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records the limited implementation target; its provisional policies do not settle every open design question below.

## Prioritized open decisions

**Latest resource/economy correction, documentation first:** Exactly three unique standard and two unique rare deposits per region, randomized within its inner 75% area, from a fixed four-standard/four-rare pool. Credits are only for external trade, through a level-1–3 port, anchored at 1 credit per 1 kg of gold. Start with zero credits and enough landed material for road/solar/port, then export local raw goods before importing missing standards. Passive and transaction electricity share connected-road grids with battery storage. Organic food is a product/export option, not worker nutrition. [Resource Proposal](RESOURCE_PROPOSAL.md) and [Product Recipes](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) own exact confirmed lists and clearly marked recipe suggestions; none is implemented merely by appearing here.

**Current v0.8 expansion:** Player-facing robots are workers. Core levels grow through 1×/2×/3× widths while reserving the largest plot; vegetation clears that reservation but grading follows the built body. Solar/factory/tower/wall families have three fixed-footprint levels. A slow selectable core replicator and efficient worker factory use materials/energy; inactive workers occupy physical storage, support configurable colony/port targets, and recycle for parts at 1 kWh when requested, needed or out of space. Construction uses delivered/installed ledgers and travelling crews; roads provide 2×/4×/8× transport. Menu owns general actions; manual launch requires the selected own core. Gameplay starts at 1×; the test route uses 10×. [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns details. The expanded format-5 source and combat modules require fresh verification; old checkpoints do not prove acceptance.

The following order is an organizational recommendation for future discussion, not a user-approved development roadmap. The user prefers broad design exploration over premature detailed questionnaires.

| Priority | Decision area | What remains unresolved |
| --- | --- | --- |
| 1 | World and simulation scale | Final sector dimensions, remote-outpost distances, measurement of developed land, movement media beyond ground vehicles/mechs, and game time versus real time; v0.3 maps the unchanged logical world to 3.6 km sectors and a 10.8 km neighborhood, while city share of roughly 5–10% and overall development of no more than roughly 10–20% remain provisional targets |
| 1 | Persistence and authority | Who simulates offline sectors, owns persistent saves, and resolves shared outcomes; local single-player pause, paused landing/setup, and neighborhood save/load are now implemented but do not settle persistent multiplayer behavior |
| 1 | Viability, loss, and escape | Starting access, colony survival requirements, shuttle availability and loading, final defeat conditions, and relocation destinations; automatic launch on core destruction and carrying existing onboard contents are established |
| 2 | Industrial structure | Final raw resources, distribution fairness, buildings, recipe graph, upgrades, and trade access |
| 2 | Building organization | Exact building roster and how storage overlaps categories; resources, logistics, and defense are established broad categories, with possible secondary defensive functions |
| 2 | Colony operation | Automatic worker and input allocation under scarcity, partial staffing, hauling jobs, recipe selection, building-off behavior, and specialization; automatic staffing, production, goods movement, and building off switches are established |
| 2 | Conflict and mission model | Fleet control, combat details, invasion interval and colony-strength measure, defensive hostility, automatic repairs, and raid damage limits including core destruction; formal war is removed while privateering and theft remain |
| 2 | Civilian simulation | Robot needs and inputs, exact rates, job-demand counting, population-reduction mechanics, core-exclusive production proposal, goods delivery, shortages, and adaptation of morale; automatic production for vacancies and reduction when jobs fall are established |
| 2 | Internal colony loss | Whether to adopt the proposed robot revolt, its robot-appropriate dissatisfaction model, triggers and recovery, escape behavior, and relation to permanent defeat |
| 2 | Leaderless areas | Continuing robot behavior, production and consumption, resource persistence, ownership, and reclamation; surviving areas become chaotic AI-controlled territory after shuttle departure, with scavenging returns that may not cover costs |
| 2 | Sensor infrastructure | Ranges, resource costs, upkeep, and theft/transport of installed sensors; distributed coverage and sensors as valuable theft targets are established |
| 3 | Visual direction | Concrete assets, rendering, and animation within the current direction: realistic Earth-like terrain inspired by Manor Lords, detailed realistic futuristic buildings, empathetic robots, and menacing realistic bugs. The current priority is the natural background; further building-art work is deferred. Matching the reference's quality remains a target |
| 3 | Interface and fleet command | Detailed layout tuning, category shortcuts, mission controls, exact aggression behaviors, and fleet-level retreat. The current direction establishes resource overlays with alerts beneath, a floating bottom construction overlay, top-left version/FPS, and complete building information including zero/unarmed values |
| 3 | Logistics details | Cargo and salvage behavior, loot comparison units, ties, and existing loads |
| 3 | Information and setup details | Terrain memory, last-known positions, full strategic sensor parity, adjacent starts or relocation, and final AI starting-development balance; setup now offers empty/starting/developed neighbors and an AI center for observation, with external prototype presets |
| 3 | Neighbor mix and remote extraction | Whether to offer abandoned-neighbor setup, random mix and counts, and mechanics of extraction in empty neighbors; one central command center and central zone are established limits |
| 3 | Setting era | Around 2200 is a possible setting, not a selected date |
| 3 | Workforce interface | Presentation of required workers and open jobs; visibility of those metrics is established |
| Deferred | Explicitly postponed topics | Changes to occupied AI slots during an active scenario, fleet-cap modifiers, broader morale sources beyond Rex, and human/mixed populations or immigration; formal war and its related diplomacy, sanctions, and coalition ideas are outside current scope. Base fleet capacity is confirmed at 50 points; chassis costs are 1/2/4/8 and command-center levels grant 1/2/3 total shared fleet slots. These fleet rules await implementation. |

Detailed costs, numerical formulas, exact recipes, and tuning should remain visibly provisional while the larger design is explored. An open question is not authorization to fill it with an assumed decision.

## Evolution and superseded ideas

- **Prototype orthographic presentation → graphics-first realistic perspective rebuild.** The user selected full 3D realism, a rotatable/tiltable view, and a Manor Lords landscape quality target. v0.3's six-centimeter mapping enlarges rendered space without changing logical simulation balance. Matching that visual quality remains a goal to verify in the running game.
- **Flat textured ground → terrain relief and continuous ground cover.** The user rejected the preceding v0.4 visual pass. Paused inspection of the installed Manor Lords Autosave on October 5 informed a rebuild with finer terrain, compact foundations, dense mixed grass, coherent soil, cloud lighting, and closer viewing. The first 53-stage terrain interaction route passed, but surface-detail review still failed and vegetation/material corrections continue. Passing UI tests does not settle visual acceptance.
- **Uniform neighborhood detail → regional map and focused detail.** Surrounding sectors should use a less detailed map presentation; zooming into an area should reveal its detailed landscape. This changes presentation, not ownership, sensor knowledge, or the current absence of cross-sector movement.
- **Low-sensitivity orbit → responsive third/middle-button orbit.** The current source captures pointer motion during orbit and exposes sensitivity in Graphics definitions. Exact feel and real gesture behavior require rendered verification.
- **Immediate colony start → main menu, setup, and paused landing.** Single-player setup exposes the larger nine-sector view. A human player surveys resource locations and selects a valid core site before the clock starts. Settings and credits are included in the frontend.
- **AI neighbors only → selectable AI center for observation.** Each neighbor can be empty, newly founded, or developed at setup; choosing an AI center creates an observer scenario. The current controller operates independent local economies through normal simulation commands. Cross-sector travel, trade, and combat remain unimplemented.
- **AI tuning mixed into game logic → a separate AIFILES folder.** AI priorities, decision cadence, placement search, and finite developed starting stock must be externally editable. Shared costs and economic behavior remain in Rules; menus, summaries, and credits are defined in Interface. New mechanisms can still require code.
- **Bright Pandora landscape and cute colony buildings → realistic environment and industry.** The latest brief calls for Earth-like Manor Lords-style terrain and detailed, realistic futuristic structures. Robot empathy remains part of the direction.
- **Bottom strip → top-only B menu → floating bottom construction overlay.** The latest explicit request supersedes the no-bottom restriction. Resources appear as an overlay with alerts underneath; construction uses a floating bottom overlay with icons, names on hover, and logical shortcuts. Version and FPS remain at the top left.
- **Abbreviated building summaries → complete explicit statistics.** Buildings must expose resources, power, weapon damage, reload, DPS, and other relevant facts even when zero or unarmed. The v0.4 prototype uses actual data-defined weapon shots and displays zero power with an explicit absent-grid limitation; it does not settle the future energy system.

- **SEIGE / informal “Robot Manor Lords” labels → seige2222.** This is the current user-selected working name; the existing workspace path may retain the older label.
- **Documentation-only task → authorized Unreal prototype development.** The user requested a native playable single-player build, packaging, and a GitHub push. The complete persistent multiplayer design remains a later implementation stage.

- **Pirates as the main enemy → aliens as the primary hostile faction.** Pirate and privateer fleets can still feature in fleet missions.
- **Permanent online-only direction → single-player first, persistent multiplayer later.** Earlier online-only discussion does not settle connectivity requirements.
- **About 10% morale → perhaps 30% morale.** The approximate 70/30 balance remains tentative.
- **Manual loot priorities → automatic loot order.** Player priority lists and loading orders were explicitly rejected.
- **Privateer-only framing → multiple fleets with different assignments.** Privateering is one possible role.
- **Core safe from neighboring fixed bombardment → range protection only.** Earlier mobile-fleet assaults could destroy it; after removal of formal war, whether privateer raids can destroy a core is open. General core vulnerability remains.
- **Ambiguous direct privateer control → fleet-level movement and missions.** The player can direct fleet movement or autonomous missions with aggression settings; individual-unit combat remains automatic. Exact settings and fleet-level retreat controls remain open.
- **Medieval alternative → sci-fi focus.** The alternative was deferred.
- **Disabled AI as inactive neighbors → empty neighboring sectors.** Startup selections are now established. Changing occupied neighboring slots during play remains unresolved and unimplemented.
- **High-tier industrial escape project proposal → emergency orbital ejection seat.** Command-core destruction automatically launches the shuttle with whatever is already aboard; valuable cargo can allow a wealthy restart. Construction requirements, loading, capacity, and availability remain open.
- **Possible protected opening → no separate protection period.** Roaming threats can arrive immediately. Main alien invasions are periodic and send forces proportionate to colony strength; thirty minutes is only an example interval.
- **Multiplayer considered as a later feature → persistent multiplayer as the gameplay design baseline.** Single-player is still implemented first, with AI opponents and the same rules.
- **Possible human/robot population choice → robots only for current scope.** Human and mixed populations may be reconsidered later. Human growth and immigration are deferred; robots are produced, while their detailed production and needs remain to be designed. Earlier food and starvation ideas are preserved as historical needs concepts, not assumed robot mechanics.
- **General automatic population growth → job-driven robot production and reduction.** The core produces robots for vacancies and reduces population when jobs decline. Production has a maximum rate and input constraints. Core exclusivity, reduction mechanics, and the role of satisfaction remain open.
- **Manual-versus-automatic work assignment question → automatic colony operation.** Robots fill jobs and buildings produce and move goods automatically when requirements are met; the player can disable buildings and see worker demand. Scarcity allocation remains open.
- **Formal war between colonies → privateering, raids, and theft only for current scope.** War declarations, war-fleet roles, and formal war attacks are removed for now. Raid damage limits and defensive hostility criteria need clarification; humanity's background war against aliens remains.
- **StarCraft reference limited to starting setup → also a secondary interface reference.** Manor Lords remains the closer interface analog; StarCraft II contributes polish inspiration without introducing individual-unit combat control.

## Related documents

- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)
- [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md)

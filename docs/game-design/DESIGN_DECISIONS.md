# seige2222 — Open Decisions and Design Evolution

[Design index](README.md) · [Status definitions](README.md#design-status)

A cross-system decision register and the corrections that prevent superseded ideas from returning as current rules.

**Current development decision:** The working name is **seige2222**. Native Unreal Engine 5.8.3 single-player development, a playable package, and a GitHub push are authorized. Gameplay definitions and numerical balance must remain externally editable. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records the limited implementation target; its provisional policies do not settle every open design question below.

## Prioritized open decisions

The following order is an organizational recommendation for future discussion, not a user-approved development roadmap. The user prefers broad design exploration over premature detailed questionnaires.

| Priority | Decision area | What remains unresolved |
| --- | --- | --- |
| 1 | World and simulation scale | Final sector dimensions, remote-outpost distances, measurement of developed land, movement media beyond ground vehicles/mechs, and game time versus real time; the current 600 m sectors and 1.8 km neighborhood are prototype values, while city share of roughly 5–10% and overall development of no more than roughly 10–20% remain provisional targets |
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
| 3 | Visual direction | Concrete assets, rendering, and animation within the current direction: realistic Earth-like terrain inspired by Manor Lords, detailed realistic futuristic buildings, empathetic robots, and menacing realistic bugs |
| 3 | Interface and fleet command | Detailed hover explanations, category shortcuts, mission controls, exact aggression behaviors, and fleet-level retreat; a top bar, top-left version/FPS, and B-opened construction icons are established, with no persistent bottom construction menu |
| 3 | Logistics details | Cargo and salvage behavior, loot comparison units, ties, and existing loads |
| 3 | Information and setup details | Terrain memory, last-known positions, full strategic sensor parity, adjacent starts or relocation, and final AI starting-development balance; setup now offers empty/starting/developed neighbors and an AI center for observation, with external prototype presets |
| 3 | Neighbor mix and remote extraction | Whether to offer abandoned-neighbor setup, random mix and counts, and mechanics of extraction in empty neighbors; one central command center and central zone are established limits |
| 3 | Setting era | Around 2200 is a possible setting, not a selected date |
| 3 | Workforce interface | Presentation of required workers and open jobs; visibility of those metrics is established |
| Deferred | Explicitly postponed topics | Changes to occupied AI slots during an active scenario, fleet caps, detailed morale sources, and human/mixed populations or immigration; formal war and its related diplomacy, sanctions, and coalition ideas are outside current scope |

Detailed costs, numerical formulas, exact recipes, and tuning should remain visibly provisional while the larger design is explored. An open question is not authorization to fill it with an assumed decision.

## Evolution and superseded ideas

- **Immediate colony start → main menu, setup, and paused landing.** Single-player setup exposes the larger nine-sector view. A human player surveys resource locations and selects a valid core site before the clock starts. Settings and credits are included in the frontend.
- **AI neighbors only → selectable AI center for observation.** Each neighbor can be empty, newly founded, or developed at setup; choosing an AI center creates an observer scenario. The current controller operates independent local economies through normal simulation commands. Cross-sector travel, trade, and combat remain unimplemented.
- **AI tuning mixed into game logic → a separate AIFILES folder.** AI priorities, decision cadence, placement search, and finite developed starting stock must be externally editable. Shared costs and economic behavior remain in Rules; menus, summaries, and credits are defined in Interface. New mechanisms can still require code.
- **Bright Pandora landscape and cute colony buildings → realistic environment and industry.** The latest brief calls for Earth-like Manor Lords-style terrain and detailed, realistic futuristic structures. Robot empathy remains part of the direction.
- **Persistent bottom construction strip → B construction menu.** Use icons, building names on hover, logical shortcuts, and a top bar with hover information. Version and FPS belong at the top left.

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

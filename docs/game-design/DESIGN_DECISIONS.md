# seige2222 — Open Decisions and Design Evolution

[Design index](README.md) · [Status definitions](README.md#design-status)

A cross-system decision register and the corrections that prevent superseded ideas from returning as current rules.

**Current development decision:** The working name is **seige2222**. Native Unreal Engine 5.8.3 single-player development, a playable package, and a GitHub push are authorized. Gameplay definitions and numerical balance must remain externally editable. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records the limited implementation target; its provisional policies do not settle every open design question below.

## Prioritized open decisions

The following order is an organizational recommendation for future discussion, not a user-approved development roadmap. The user prefers broad design exploration over premature detailed questionnaires.

| Priority | Decision area | What remains unresolved |
| --- | --- | --- |
| 1 | World and simulation scale | Actual sector dimensions, remote-outpost distances, measurement of developed land, movement media beyond ground vehicles/mechs, and game time versus real time; city share of roughly 5–10% and overall development of no more than roughly 10–20% are provisional targets |
| 1 | Persistence and authority | Who simulates offline sectors, owns persistent saves, and resolves shared outcomes; single-player pause/save behavior |
| 1 | Viability, loss, and escape | Starting access, colony survival requirements, shuttle availability and loading, final defeat conditions, and relocation destinations; automatic launch on core destruction and carrying existing onboard contents are established |
| 2 | Industrial structure | Final raw resources, distribution fairness, buildings, recipe graph, upgrades, and trade access |
| 2 | Building organization | Exact building roster and how storage overlaps categories; resources, logistics, and defense are established broad categories, with possible secondary defensive functions |
| 2 | Colony operation | Automatic worker and input allocation under scarcity, partial staffing, hauling jobs, recipe selection, building-off behavior, and specialization; automatic staffing, production, goods movement, and building off switches are established |
| 2 | Conflict and mission model | Fleet control, combat details, invasion interval and colony-strength measure, defensive hostility, automatic repairs, and raid damage limits including core destruction; formal war is removed while privateering and theft remain |
| 2 | Civilian simulation | Robot needs and inputs, exact rates, job-demand counting, population-reduction mechanics, core-exclusive production proposal, goods delivery, shortages, and adaptation of morale; automatic production for vacancies and reduction when jobs fall are established |
| 2 | Internal colony loss | Whether to adopt the proposed robot revolt, its robot-appropriate dissatisfaction model, triggers and recovery, escape behavior, and relation to permanent defeat |
| 2 | Leaderless areas | Continuing robot behavior, production and consumption, resource persistence, ownership, and reclamation; surviving areas become chaotic AI-controlled territory after shuttle departure, with scavenging returns that may not cover costs |
| 2 | Sensor infrastructure | Ranges, resource costs, upkeep, and theft/transport of installed sensors; distributed coverage and sensors as valuable theft targets are established |
| 3 | Visual direction | Concrete assets, rendering, and animation within the confirmed direction: brighter colorful Pandora-like terrain, cute futuristic WALL-E / EVE-inspired robots and buildings, and menacing realistic bugs |
| 3 | Interface and fleet command | Detailed layouts, mission controls, exact aggression behaviors, and fleet-level retreat; Manor Lords is the primary UI reference, StarCraft II supplies polish inspiration, and individual-unit micromanagement is excluded |
| 3 | Logistics details | Cargo and salvage behavior, loot comparison units, ties, and existing loads |
| 3 | Information and setup details | Terrain memory, last-known positions, sensor parity, adjacent starts or relocation, and exact AI starting-development profiles; the choice of developed or newly founded AI neighbors is established |
| 3 | Neighbor mix and remote extraction | Whether to offer abandoned-neighbor setup, random mix and counts, and mechanics of extraction in empty neighbors; one central command center and central zone are established limits |
| 3 | Setting era | Around 2200 is a possible setting, not a selected date |
| 3 | Workforce interface | Presentation of required workers and open jobs; visibility of those metrics is established |
| Deferred | Explicitly postponed topics | AI-neighbor toggle timing, fleet caps, detailed morale sources, and human/mixed populations or immigration; formal war and its related diplomacy, sanctions, and coalition ideas are outside current scope |

Detailed costs, numerical formulas, exact recipes, and tuning should remain visibly provisional while the larger design is explored. An open question is not authorization to fill it with an assumed decision.

## Evolution and superseded ideas

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
- **Disabled AI as inactive neighbors → empty neighboring sectors.** Toggle timing remains unresolved.
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

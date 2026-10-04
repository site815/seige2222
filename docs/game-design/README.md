# seige2222 — Game Design

This document set is the current source of truth for the game's design. Each subject document owns its rules, tentative ideas, and unresolved questions. The decision register gathers cross-system priorities and important corrections without replacing those subject documents.

**Current working name: seige2222.** SEIGE is the earlier project label retained in the existing workspace folder; “Robot Manor Lords” was an earlier informal suggestion. No folder rename is implied by the title change.

**Development is authorized:** Build a native Unreal Engine 5.8.3 single-player prototype, package a playable game, and push the intended project to GitHub. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) distinguishes the current slice from the full design. [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) records the requirement for editable external gameplay rules. Build and test completion must be established separately from these documents.

**Current implementation:** v0.3.0 is packaged locally and verified by nineteen clean native tests and a twenty-eight-stage packaged interaction run with zero failures. Source is maintained in [site815/seige2222](https://github.com/site815/seige2222); versioned local packages are excluded from Git. The [development report](../DEVELOPMENT_REPORT.md) records evidence and limits; remaining visual polish does not change the broader design status.

The project is a science-fiction colony simulation: a local ruler sustains a robotic population and industry on newly settled planets while humanity is losing a war against insect-like aliens. Visible production, physical logistics, resource-driven capabilities, and planned, automatically executed combat form the central direction. All gameplay concepts use a fully persistent multiplayer world as their design baseline. Single-player is implemented first with AI opponents and the same gameplay rules; persistent multiplayer implementation follows later.

## Documents

| Document | Scope |
| --- | --- |
| [Vision and Setting](VISION_AND_SETTING.md) | The setting, player role, and intended scope of the game's references. |
| [Art Direction](ART_DIRECTION.md) | Realistic Earth-like landscapes, detailed futuristic buildings, empathetic robots, menacing realistic bugs, and the superseded earlier visual direction. |
| [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md) | Delivered v0.3 perspective interaction, physical scale versus logical units, asset provenance, package evidence, and remaining visual polish. |
| [Interface and Controls](INTERFACE_AND_CONTROLS.md) | Top bar and hover information, top-left version/FPS, B construction icons and logical shortcuts, automatic colony operation, and future fleet-level commands. |
| [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md) | Robotic population, command-center start, competing civilian/defense/expansion priorities, starting pressures, and unresolved simulation pace. |
| [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) | The authorized Unreal implementation slice, current external definitions, explicit simplifications, later systems, and acceptance checks. |
| [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) | Nine-sector selection, paused human landing, AI observation, separate editable AI definitions, local time/saves, and current cross-sector limits. |
| [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md) | Automatic staffing, production, goods movement, repairs, building off switches, workforce metrics, and population tradeoffs. |
| [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md) | Sector layout, information coverage, adjacent colonies, escape, and permanent loss. |
| [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md) | Chaotic robot-controlled territory left after departure, surviving outposts and stockpiles, and uncertain returns from scavenging. |
| [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md) | Resource geography, settlement patterns, trade access, manufacturing chains, and building upgrades. |
| [Population, Necessities, and Morale](POPULATION_AND_MORALE.md) | Robot-only population scope, needs and growth still to develop, and earlier human-oriented needs and morale ideas requiring adaptation. |
| [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md) | Alien pressure, automatic combat, privateering, theft, and defenses; formal regional war is outside current scope. |
| [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md) | Fleet assignments, physical cargo, raiding, salvage, and the confirmed automatic loading order. |
| [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md) | The persistent multiplayer design baseline, single-player-first implementation, unresolved architecture, and design constraints. |
| [Provisional Resource, Building, and Product Catalog](PROVISIONAL_CATALOG.md) | Assistant-proposed content for further discussion. No entry here becomes a settled requirement by appearing in this catalog. |
| [Resource Proposal](RESOURCE_PROPOSAL.md) | A revised twelve-material candidate catalog and a four-material starter recommendation. |
| [Building Proposal](BUILDING_PROPOSAL.md) | Resource, logistics, and defense categories; robot-oriented starter functions and mapping of the sixteen earlier building ideas. |
| [Production Dependencies and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) | Review of the original catalog, suggested interconnected products, and starter dependency checks. |
| [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) | External editable definitions, generic execution, validation, local state, packaging, and boundaries for later multiplayer authority. |
| [Open Decisions and Design Evolution](DESIGN_DECISIONS.md) | A cross-system decision register and the corrections that prevent superseded ideas from returning as current rules. |

## Review and next documents

[Design Review and Recommended Next Documents](DESIGN_REVIEW.md) assesses every document and proposes the additional planning needed for a first playable game and, later, persistent multiplayer. Its recommendations are analysis, not settled game rules.

## Design status

- **Confirmed / Confirmed direction:** The user's established direction. Approximate counts and desired possibilities remain approximate even within these sections.
- **Provisional / Tentative:** A working parameter or interpretation that has not been finalized.
- **Proposal / Assistant proposal:** An assistant suggestion, not an approved requirement.
- **User proposal:** An idea the user is exploring, not yet a finalized rule. Explicit current-scope decisions are labeled confirmed separately.
- **Broadly accepted proposal:** The direction received broad acceptance, while its exact mechanics remain unresolved.
- **User speculation:** An explored possibility, not a settled architecture or rule.
- **Open:** A decision still needed. Its presence does not authorize inventing an answer.
- **Example:** Illustrative content only, unless explicitly stated otherwise.
- **Prototype policy / Implementation assumption:** A concrete choice made to build and test the current slice. It is editable and does not silently settle the corresponding full-game design question.
- **Verified:** An observed result supported by an actual completed check. A saved definition or planned acceptance check alone is not verification of gameplay.

The [provisional catalog](PROVISIONAL_CATALOG.md) is entirely proposed content. Its material classes, building list, uses, and recipes are not a confirmed specification. The priorities in the [decision register](DESIGN_DECISIONS.md) are organizational recommendations, not an approved development roadmap.

## Continuing the design

Begin with [Vision and Setting](VISION_AND_SETTING.md), then use the subject documents for the area being discussed. For current development, read [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) and [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md). The user prefers broad design exploration before detailed questionnaires. Preserve status distinctions as design and implementation develop; keep prototype balance in external data and record departures from the complete design explicitly.

# seige2222 — Game Design

This document set is the current source of truth for the game's design. Each subject document owns its rules, tentative ideas, and unresolved questions. The decision register gathers cross-system priorities and important corrections without replacing those subject documents.

**Current working name: seige2222.** SEIGE is the earlier project label retained in the existing workspace folder; “Robot Manor Lords” was an earlier informal suggestion. No folder rename is implied by the title change.

**Development is authorized:** Build a native Unreal Engine 5.8.3 single-player prototype, package a playable game, and push the intended project to GitHub. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) distinguishes the current slice from the full design. [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) records the requirement for editable external gameplay rules. Build and test completion must be established separately from these documents.

**Current v0.9 implementation, acceptance in progress:** [Provisional Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md) records physical 80 kg worker identities, the finite 9.57 t kit, tonne-scale bills, two-bank mixed laser loadout and shared day/night/season calendar. Rules are `prototype-9.0`; snapshots and neighborhood metadata require format 7 and a new scenario. All 24 no-threat starter compositions and three calendar/solar tests passed in a bounded native run; broader native, rendered and release acceptance remain incomplete. The v0.8.1 record is evidence for the preceding release.

**Confirmed scenario change, implementation acceptance pending:** Developed AI loads a separate authored established colony, rather than replaying hours of historical growth at load. Player and Starting AI retain the finite 9.57 t kit. All use ordinary paid simulation and selected threats after initialization. [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) records the distinction; previous growth failures remain diagnostics, not current setup gates.

**Historical verified v0.8.1 release:** [Interface and Controls](INTERFACE_AND_CONTROLS.md) records the five resource HUD groups—Credits, Energy, Raw materials, Basic production and Adv production—and the single deposit-bound Extraction Mine. [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns the retained construction, transport, tiered facilities, replication and inactive-worker rules; [combat/fleets](WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) and [Companions and Rex](COMPANIONS_AND_REX.md) own their implemented systems. That release used Rules `prototype-8.1`; simulation and neighborhood saves use format 6, requiring a new scenario. The final v0.8.1 Shipping executable passed 117 interaction stages, boot/offline checks and four display states; reconciled native coverage contains 88 unique clean results. The [v0.8.1 verification record](../verification/v0.8.1.json) identifies the exact evidence and retained limitations. The completed [v0.8 verification record](../verification/v0.8.0.json) is historical evidence for the preceding package, not acceptance of this revision.

**Historical verified release: v0.7.0.** Independent **Background bugs** and **Periodic attacks** switches default ON and apply to every colony before AI preparation. That release retained compatible v0.6 saves under `prototype-6.0`; this compatibility does not extend to v0.8. Scenery keeps stable tree placements across sectors with simpler distant geometry and budgeted ground-cover streaming. One Medium profile selects native TAA at full render resolution and TSR below full resolution. All **38 native tests passed cleanly**; Shipping passed **79 stages with zero failures and exit 0**, four display states, 86 zero-endpoint socket samples and nine matching JSON hashes. The [v0.7 verification record](../verification/v0.7.0.json) and [graphics record](GRAPHICS_PERFORMANCE_0_7.md) retain the measured boundaries and remaining performance/appearance limits.

**Historical v0.6.0 delivery:** It added wider 3D survey/eased map zoom, a single Medium profile and clearer atmosphere/ground filtering, native borderless/windowed controls, direct game menus, 1×/5×/10× speeds, visible work and stockyards, and courier interpolation between simulation ticks. **34 native tests passed cleanly**, the Shipping route passed **79 stages with zero failures and exit 0**, and actual display changes passed four states. All 84 process-tree samples showed zero TCP/UDP endpoints; nine staged JSON files match source hashes. v0.6 introduced `prototype-6.0`, rejecting earlier-rule saves; v0.7 retains that Rules compatibility. Performance at native monitor resolution and Manor Lords visual parity remain unfinished. [Source repository](https://github.com/site815/seige2222) · [current and historical verification](../DEVELOPMENT_REPORT.md).

Actual v0.7 captures: [scenario setup](../../Art/Previews/v07_scenario.png), [colony](../../Art/Previews/v07_colony.png), and [ground detail](../../Art/Previews/v07_ground.png). Grass proxy geometry is original; tree proxies derive from retained CC0 source assets. The [asset record](../../Art/EnvironmentV07/README.md) preserves source and license attribution. No seamless-transition or reference-game parity claim is made.

The project is a science-fiction colony simulation: a local ruler sustains a robotic population and industry on newly settled planets while humanity is losing a war against insect-like aliens. Visible production, physical logistics, resource-driven capabilities, and planned, automatically executed combat form the central direction. All gameplay concepts use a fully persistent multiplayer world as their design baseline. Single-player is implemented first with AI opponents and the same gameplay rules; persistent multiplayer implementation follows later.


## Documents

| Document | Scope |
| --- | --- |
| [Vision and Setting](VISION_AND_SETTING.md) | The setting, player role, and intended scope of the game's references. |
| [Art Direction](ART_DIRECTION.md) | Realistic Earth-like landscapes, detailed futuristic buildings, empathetic robots, menacing realistic bugs, and the superseded earlier visual direction. |
| [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md) | Delivered v0.3 perspective interaction, physical scale versus logical units, asset provenance, package evidence, and remaining visual polish. |
| [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md) | Direct reference inspection, rejected visual pass, rebuilt terrain/close camera, completed overlays/statistics/map work, native/rendered/package evidence, and remaining visual limitations. |
| [Graphics Performance 0.5](GRAPHICS_PERFORMANCE_0_5.md) | Continuous neighboring scenery, configurable foliage lighting, GPU investigation, matched-benchmark protocol and remaining performance/visual limits. |
| [Graphics Performance 0.6](GRAPHICS_PERFORMANCE_0_6.md) | Medium profile, native monitor resolution, grass filtering/atmosphere, measured native and windowed performance, and remaining visual limits. |
| [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md) | Stable scenery placements, distance geometry, budgeted streaming, native TAA/reduced-resolution TSR, benchmark evidence and remaining appearance/performance limits. |
| [Interface and Controls](INTERFACE_AND_CONTROLS.md) | Resource overlays and alerts, floating bottom construction, top-left version/FPS, building dossiers, worker targets, fleet-level orders and paid equipment controls. |
| [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) | Retained construction/transport behavior and historical v0.8 save boundary; the v0.9 economy document owns the newer worker/calendar baseline. |
| [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md) | Robotic population, command-center start, competing civilian/defense/expansion priorities, starting pressures, and unresolved simulation pace. |
| [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) | The authorized Unreal implementation slice, current external definitions, explicit simplifications, later systems, and acceptance checks. |
| [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) | Nine-sector selection, independent default-ON threat switches, paused human landing, AI observation, editable AI definitions, local time/saves and compatibility, and current cross-sector limits. |
| [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md) | Automatic staffing, production, goods movement, repairs, building off switches, workforce metrics, and population tradeoffs. |
| [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md) | Sector layout, information coverage, adjacent colonies, escape, and permanent loss. |
| [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md) | Chaotic robot-controlled territory left after departure, surviving outposts and stockpiles, and uncertain returns from scavenging. |
| [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md) | Resource geography, settlement patterns, trade access, manufacturing chains, and building upgrades. |
| [Population, Necessities, and Morale](POPULATION_AND_MORALE.md) | Robot-only population scope, needs and growth still to develop, and earlier human-oriented needs and morale ideas requiring adaptation. |
| [Companions and Rex](COMPANIONS_AND_REX.md) | Physical organic-food consumption, capped local morale, autonomous walking, optional 1× first-person controls, persistence/evacuation and private-reference art provenance. |
| [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md) | Alien pressure, automatic combat, privateering, theft, and defenses; formal regional war is outside current scope. |
| [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md) | Fleet assignments, physical cargo, raiding, salvage, and the confirmed automatic loading order. |
| [Weapons, Defenses and Vehicle Outfitting](WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) | Four weapon families, shields/armor, physical shots and splash, twelve chassis types, hardpoints, 50-point fleets and level-based fleet slots. |
| [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md) | The persistent multiplayer design baseline, single-player-first implementation, unresolved architecture, and design constraints. |
| [Provisional Resource, Building, and Product Catalog](PROVISIONAL_CATALOG.md) | Assistant-proposed content for further discussion. No entry here becomes a settled requirement by appearing in this catalog. |
| [Provisional Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md) | Accepted provisional body mass, finite kit, full construction/recipe tables, 24 composition tests and shared day/night/seasons. |
| [Resource Proposal](RESOURCE_PROPOSAL.md) | Confirmed four-standard/four-rare pool, exact three-plus-two region deposits, external trade/credits and connected-road energy; proposed units and bootstrap checks. |
| [Building Proposal](BUILDING_PROPOSAL.md) | Resource, logistics, and defense categories; robot-oriented starter functions and mapping of the sixteen earlier building ideas. |
| [Product Recipes and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) | Confirmed product stages; suggested alloys/glass/conductors/batteries and material-balanced transactions; zero-credit road/solar/port bootstrap and power/storage questions. |
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

# seige2222 — Multiplayer, Persistence, and Development

[Design index](README.md) · [Status definitions](README.md#design-status)

The persistent multiplayer design baseline, single-player-first implementation sequence, unresolved architecture, and design constraints.

## Development sequence and persistence

**Confirmed direction**

1. Design all gameplay concepts against a fully persistent multiplayer world.
2. Build and polish the single-player version first, with AI factions in place of human opponents and the same gameplay rules.
3. Implement persistent multiplayer later.

AI neighboring factions serve as stand-ins for human neighbors and should share their rules. Aliens are a separate primary hostile faction.

**Authorized local prototype expansion:** Scenario setup offers empty neighbors by default and starting/developed AI colony options, including an AI-controlled center for observer play. The first controller runs each colony through the same local simulation rules and keeps AI choices and finite developed-start setup in [AIFILES](../../AIFILES/README.md). Independent local colonies do not establish cross-sector combat, trade, multiplayer authority, or persistent services. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for implementation limits and verification status.

**Confirmed direction:** Single-player can represent a persistent world containing players at different stages of development. During AI selection, the player can choose developed neighbors representing established players or newly founded neighbors starting out like the player. Exact starting development profiles remain open; see [AI-neighbor setup](WORLD_AND_RELOCATION.md).

**User proposal:** Also offer abandoned areas and a random mix of developed, new, abandoned, and empty neighbors. Suggested counts of one or two of each are examples, not selected distribution rules.

**Confirmed direction:** Single-player is a version of the persistent-world game with AI opponents, not a different colony design. There is no separate protection period for a new colony in either version. Roaming threats can arrive immediately; main alien invasions occur periodically and allocate forces proportionate to each colony's strength. The starting core's ability to repel ordinary attacks, and its vulnerability to repeated unrepaired damage, fit those shared rules. The final invasion interval and scaling method remain open.

In the intended persistent multiplayer game, colonies and expeditions continue while their players are offline. They can be attacked, lose assets, and be destroyed.

**Confirmed fleet-control direction:** Players can leave fleets on autonomous missions with a chosen aggressiveness while offline. A cautious posture may reduce exposure; an aggressive posture can lead to greater risk, including the fleet being lost before the player returns. No posture guarantees safety. The exact behaviors are still to be designed in [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md).

**Confirmed departure aftermath:** When the player leaves by shuttle, surviving colony assets can remain as a leaderless, chaotic robot-controlled AI area, with stockpiles that others may recover through costly expeditions. This is a departure/abandonment state, distinct from normal offline persistence. See [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).

**Confirmed direction:** The [orbital shuttle system](WORLD_AND_RELOCATION.md) connects emergency escape to multiplayer relocation, including movement between servers. Automatic launch on command-core destruction carries the existing onboard contents, potentially allowing the player to relocate with valuable assets. The transfer mechanism, destination reservations, and exact mapping between planets and servers remain open.

**Open:** Single-player pause and save behavior was not explicitly settled. A transcript reference to “possible” may have meant “pausable”; neither interpretation establishes a requirement. Permanent online-only operation was considered earlier and should not be treated as settled after the single-player-first direction emerged.

### Networking and simulation ownership

**User speculation, not an architecture decision:** Peers might communicate directly, with mostly independent sector activity and heavier traffic for trading and combat.

**Assistant analysis and proposal:** Low network traffic does not establish low simulation cost. Persistent saves, offline simulation, and shared combat or trade outcomes need defined ownership and authority. A trusted service owning persistent state and shared outcomes was proposed, not approved as the architecture.

**Constraint for future planning:** Do not promise inexpensive persistence or a seamless conversion from single-player to multiplayer. Authority, simulation ownership, and persistence remain design and engineering questions.

### Simulation pace

**Confirmed direction:** The persistent world and its single-player version need a deliberate relationship between game time and real time. Five or ten times real time were examples, not chosen multipliers. The robot-only population scope is intended to make rapid population growth plausible without having to design human reproduction or immigration now.

**Open:** The time scale, any player speed control, and which processes use game time versus real time. In particular, the tentative thirty-minute invasion interval is not yet assigned to either clock. Single-player pause behavior remains unresolved.

## Development constraints

- Preserve the single-player-first implementation sequence while using persistent multiplayer as the baseline for every gameplay concept.
- Treat human neighbors and AI stand-ins as sharing gameplay rules, with aliens as a distinct hostile faction.
- Preserve meaningful production simulation, physical goods, physical hauling, finite sensors, and automatic combat as central requirements.
- Do not introduce research currencies or timers, player loot-order controls, or core invulnerability.
- Keep networking proposals separate from confirmed requirements. Low traffic, low operating cost, and easy multiplayer conversion are not established conclusions.
- Keep the provisional catalog and approximate parameters editable. Do not present them as a settled specification.
- Continue with broad design exploration; do not force a long detailed questionnaire before discussing the overall game.
- The user has authorized native Unreal single-player development, a playable package, and a GitHub push. The [first playable](FIRST_PLAYABLE_SCOPE.md) is an intentionally limited slice; unresolved full-game choices remain open and prototype policies must be identified as such.
- Gameplay content and numerical balance must be editable outside the main execution code; see [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md). This does not imply that persistent multiplayer is already implemented.

## Related documents

- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)
- [Open Decisions and Design Evolution](DESIGN_DECISIONS.md)

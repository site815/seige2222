# seige2222 — Aliens, Combat, and Raiding

[Design index](README.md) · [Status definitions](README.md#design-status)

Alien pressure, automatic combat, privateering, raids, theft, and colony defenses.

## Current conflict scope

**Confirmed scope change:** War against other colonies or regions is removed for now. Players can still privateer, attack neighbors, and steal goods. Formal war declarations and war-fleet missions are not current mechanics.

The wider setting of humanity's war against the aliens remains. This change concerns conflict between neighboring colonies.

## Aliens and combat control

**Confirmed**

- Aliens are the primary hostile faction.
- Small, weak roaming creatures create a continuing threat resembling hostile wildlife.
- Main alien invasions occur periodically in the persistent server, in addition to the continuing roaming threats.
- Alien invasion forces sent against a colony are proportionate to that colony's strength. Colonies do not all receive equal attacking forces; a newly established, weak colony should face a smaller force than a strong developed colony.
- There is no separate protection period for a new settlement. Roaming threats can arrive immediately after it is established.
- The player controls combat planning rather than micromanaging fighting. Combat executes or resolves automatically.

**Confirmed control boundary:** The player can command a whole fleet's movement or give it an autonomous mission. Combat within the fleet is automatic, with no individual-unit micromanagement. Configurable aggression guides autonomous behavior, including while the player is offline; the exact settings and fleet-level retreat controls remain open. See [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md).

**Direction for ground combat:** Fleet compositions can include vehicles and mechs. Other movement media and exact unit designs remain open.

**Provisional example:** A main invasion pulse approximately once every thirty minutes. The interval is not fixed.

**Open:** The final invasion interval, warnings, how colony strength is measured, how that measure determines the force sent, when it is evaluated, and the detailed combat model. Proportionate scaling is confirmed; its formula and inputs are not. These scaling rules concern the main alien invasions.

## Privateering, fixed defenses, and colony attacks

**Confirmed direction**

- Hostile fixed defenses in mutual range fire automatically.
- Fixed weapons in a neighboring sector must never have enough range to hit the core.
- This range restriction does not make the core generally invulnerable. Strong, upgradeable core defenses should make destruction difficult; permitted damage from privateer raids is still to be clarified under the revised scope.
- The starting core should repel ordinary incoming attacks, while repeated damage without repair makes it vulnerable. The intended opening depends on defensive strength and upkeep; exact attack thresholds and repair mechanics remain open.
- All repairs are automatic, including repairs to the core. Individual repair orders are not required; repair resources, labor, speed, priorities, and other operating conditions are not yet specified.
- Destruction of the command core triggers the [orbital shuttle's automatic emergency launch](WORLD_AND_RELOCATION.md) with its existing onboard contents. Core destruction therefore does not by itself establish permanent player defeat; shuttle availability and final defeat conditions remain open.
- Hired mercenary privateers remain a desired possibility; their arrangements are open.
- Developed neighboring factions may send privateers against the colony. This is a possible source of early pressure alongside roaming aliens and periodic alien invasions, not an automatic declaration that all developed neighbors are hostile.
- After the player leaves an area, surviving robots can remain in a chaotic, leaderless AI-controlled state. Raiding their remaining stockpiles can be lucrative or cost more than it yields; see [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).
- Privateers and pirates can target distributed sensors for theft. Sensor theft and remote outpost exposure are part of the challenge of covering a mostly wilderness sector.

**Open:** Mercenary arrangements, permitted raid targets and damage, whether privateers can destroy a command core or whole colony, defense ranges, and how hostility and automatic defensive engagement are determined without formal war declarations.

The earlier mobile-fleet assault concept allowed core destruction. With formal war removed, that is not enough to settle the damage limits of privateer raids. Core destruction and the automatic shuttle escape remain possible game events; this scope change does not decide which raiders can cause them.

## Earlier war-related ideas — deferred

War declarations, the declared-enemy war-fleet role, and formal war attacks are removed from current scope. Sanctions, formal diplomatic war states, and coordinated war coalitions remain earlier ideas for possible reconsideration, not active requirements.

Multiple players combining forces was a desired possibility in the earlier discussion. Whether and how that applies to the retained raiding system is open.

## Related documents

- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)
- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)

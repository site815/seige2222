# seige2222 — Fleets, Physical Logistics, and Loot

[Design index](README.md) · [Status definitions](README.md#design-status)

Fleet assignments, physical cargo, raiding, salvage, and the confirmed automatic loading order.

## Fleets and assignments

**Confirmed direction**

- Players can have multiple general-purpose fleets. Privateering is an assignment, not the sole fleet type.
- Assignments include privateering and raids, perimeter protection, convoy escort between zones, and hunting hostile pirate or privateer fleets in an area.
- Formal war, declared-enemy war-fleet roles, and war-attack assignments are outside current scope. Neighbor attacks and theft through privateering remain.
- Many encounters occur between fleets before attackers reach a settlement.
- Cargo capacity is a meaningful fleet consideration alongside combat strength.

**Provisional:** A fleet cap might be tied to the core. This was deferred, not decided.

**Direction for ground combat:** Ground fleets should consist of vehicles and mechs, potentially in mixed formations. This does not settle whether all ordinary fleets are ground-only or whether other movement media exist. The relocation vessel remains explicitly an orbital spacecraft.

**Open:** Fleet limits, exact unit roster and composition rules, movement media beyond the ground-fleet direction, and detailed mission behavior.

**Open after the scope change:** The permitted targets and damage of a privateer raid, including whether raiders can destroy the command core. See [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md).

## Fleet-level command and autonomous behavior

**Confirmed direction:**

- The player controls whole fleets, not individual units.
- Autonomous fleets receive mission orders.
- The player can also directly order a fleet's movement as a group.
- Once engaged, the fleet fights automatically; the player cannot micromanage individual combatants.
- Aggressiveness can be set for autonomous operation and remains relevant while the player is offline.

**Illustrative behavior range:** Seek engagements actively, take a middle approach, or fight only when necessary. These are examples, not a finalized list of modes or targeting rules.

**Offline example:** A player may leave a fleet less aggressive before logging off to reduce exposure, or accept the greater risk of a more aggressive posture. Neither survival nor fleet loss is guaranteed by a setting; losses, including losing the fleet, remain possible.

**Open:** Mission selection, rules for legal or preferred targets, interaction between direct movement and autonomous missions, fleet-level retreat or redirection during combat, and exact aggression behavior. Low aggression does not establish immunity or a safe-logout rule.

## Physical logistics, raiding, and automatic loot

**Confirmed**

- All raw materials and manufactured products exist as movable batches or chunks in the world, including in stockpiles and transports.
- Routine movement of colony goods is automatic, as part of [colony operations](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md). Automation still requires physical transport; routing, transport jobs, and carrier details remain open.
- Destroying a carrier can destroy its cargo or leave salvage.
- A vulnerable outpost with a full silo can be raided without destroying the core.
- Loot must be physically hauled home. Victory does not teleport goods or instantly transfer them to the home colony.
- A combat-focused fleet may win but carry little or nothing away.
- Defenders can reclaim stock left behind and rebuild.
- Leaderless AI-controlled areas can retain abandoned city and outpost stockpiles. Expeditions may recover them gradually, but combat costs and losses can exceed the value hauled home; see [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).
- Distributed sensors are valuable theft targets for privateers and pirates. How sensors are taken and transported remains open; the existing automatic loot order is unchanged.

Trucks and trains were examples of transport, not selected vehicle types.

### Confirmed automatic loading order

1. Select the highest-tier goods first.
2. Within that tier, prefer goods the home colony has least of.
3. After exhausting higher-tier goods, descend to lower tiers while cargo capacity remains.

**Explicit exclusion:** No player-defined loot priority lists or loading orders. A brief spoken reference to “below” was superseded by the user's clear restatement of highest-tier-first loading.

**Open:** How tiers are defined; how quantities of different goods are normalized for “has least of”; tie-breaking; treatment of existing cargo; and exact cargo destruction or salvage rules. Do not invent these details during implementation.

## Related documents

- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)

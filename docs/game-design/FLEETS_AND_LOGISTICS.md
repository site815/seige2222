# seige2222 — Fleets, Physical Logistics, and Loot

[Design index](README.md) · [Status definitions](README.md#design-status)

Fleet assignments, physical cargo, raiding, salvage, and the confirmed automatic loading order.

**Retained transport and fleet prototype, checked in v0.8.1:** Couriers and aggregate construction crews route around reserved plots through declared access ports. Powered Road / Road + rail / Road + rail + vacuum links provide 2×/4×/8× walking speed; upgrades retain their earlier tier during construction. External trading ports use physical shipments and credit settlement. Separate combat modules provide fleet orders, outfitting, privateer travel, encounters and physical loot/return. A general cross-sector convoy economy is still separate future scope. [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns local transport; the [resource/economy document](RESOURCE_PROPOSAL.md) owns port trade.

## Fleets and assignments

**Confirmed direction**

- Players can have multiple general-purpose fleets. Privateering is an assignment, not the sole fleet type.
- Assignments include privateering and raids, perimeter protection, convoy escort between zones, and hunting hostile pirate or privateer fleets in an area.
- Formal war, declared-enemy war-fleet roles, and war-attack assignments are outside current scope. Neighbor attacks and theft through privateering remain.
- Many encounters occur between fleets before attackers reach a settlement.
- Cargo capacity is a meaningful fleet consideration alongside combat strength.

**Confirmed October 5:** Command-center levels 1/2/3 grant 1/2/3 total fleets, shared across defense, escort and privateering. Each fleet starts with 50 capacity points; small/medium/large/behemoth chassis cost 1/2/4/8 points. AI-control infrastructure can modify the cap; exact modifiers remain open. This replaces the earlier tentative limit and the later 100-point example. [Weapons and outfitting](WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) owns the current roster and physical shuttle rules.

**Direction for ground combat:** Ground fleets should consist of vehicles and mechs, potentially in mixed formations. This does not settle whether all ordinary fleets are ground-only or whether other movement media exist. The relocation vessel remains explicitly an orbital spacecraft.

**Confirmed roster:** Four sizes each of wheeled vehicles, tracked vehicles and mechs. **Open:** Exact hardpoints/equipment, capacity modifiers, movement media beyond ground fleets, and detailed mission behavior.

**Open after the scope change:** The permitted targets and damage of a privateer raid, including whether raiders can destroy the command core. See [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md).

## Fleet-level command and autonomous behavior

**Confirmed direction:**

- The player controls whole fleets, not individual units.
- Autonomous fleets receive mission orders.
- The player can also directly order a fleet's movement as a group.
- Once engaged, the fleet fights automatically; the player cannot micromanage individual combatants.
- Aggressiveness can be set for autonomous operation and remains relevant while the player is offline.

**Current prototype modes:** Passive, defensive and aggressive. Defensive/escort guards may move around a friendly building to obtain a clear shot while respecting known targets, weapon range and guard leash. Passive fleets never pursue. These explicit prototype behaviors do not settle all future targeting, retreat or mission rules.

**Current controls:** Select an own command core, factory or defense platform and open **Fleets / chassis / hardpoints**. Create a fleet within the core's slot limit, assign completed deployed vehicles within its point cap, and issue group orders. **Move fleet** selects a home-sector ground destination; **Defend** uses the selected home building's access point or the command service port. **Escort** requires a selected home building and follows its local courier traffic. Cross-sector convoy escort and pirate-hunting missions remain wider design goals. **Privateer neighbor** dispatches the selected fleet into the currently viewed occupied neighbor; it does not create a force or reveal that colony's full inventory. A later home defense or movement order physically returns survivors and their cargo. **Board shuttle** is a separate action after the whole surviving fleet reaches the home command service range.

**Offline example:** A player may leave a fleet less aggressive before logging off to reduce exposure, or accept the greater risk of a more aggressive posture. Neither survival nor fleet loss is guaranteed by a setting; losses, including losing the fleet, remain possible.

**Open beyond the prototype:** Richer mission selection, legal or preferred raid targets, retreat policy and exact aggression balance. The current explicit fleet order replaces its previous mission and persists with the fleet; it does not establish the final persistent-world rules. Low aggression does not establish immunity or a safe-logout rule.

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

**Provisional implementation:** The external combat rules order each accessible cargo batch by authored resource tier descending, the home colony's owned quantity ascending, then resource ID for deterministic ties. Owned quantity includes goods in couriers and owned fleet cargo. The quantity comparison uses each resource's authored native unit (such as kg, L or worker count); it is not a normalized economic-value comparison. This sorts the goods available in the current batch, not all stockpiles in the region into a global collection plan.

**Still open:** Final tier assignments and balance, normalization across different resource units, and the fuller destruction/salvage policy. These provisional comparison details do not introduce player-defined loading lists.

## Related documents

- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)

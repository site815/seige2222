# seige2222 — Interface and Controls

[Design index](README.md) · [Status definitions](README.md#design-status)

Interface direction and the boundary between player decisions and automatic simulation. Detailed screens, layouts, and input bindings are not yet selected.

## Interface references

**Confirmed direction:** Manor Lords is the closer primary interface reference because the game is mainly a settlement-building simulation. StarCraft II is a secondary reference for polish and clarity.

The user wants to improve on aspects of Manor Lords using the perceived polish of StarCraft II. No particular interface flaw, borrowed panel, or exact control scheme has been specified yet. This expands the earlier StarCraft reference beyond the starting command-center-and-workers setup; it does not adopt individual-unit combat micromanagement.

## Colony controls

**Confirmed direction:**

- Buildings automatically fill jobs and operate when workers and resources are available.
- Production, goods movement, repairs, and adjustment of robot population to job demand are automatic.
- The player can turn buildings off and should be able to see required workers or open jobs.
- Routine manual adjustment should be minimal.

The detailed controls for construction, upgrades, shortages, robot needs, storage, and automatic-work explanations remain open. See [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md).

## Fleet controls

**Confirmed direction:**

- The player directs fleets, not individual combat units.
- A fleet can receive mission orders for autonomous operation.
- Under direct control, the player can direct where the fleet moves as a group.
- During an engagement, combat executes automatically; individual units cannot be micromanaged.
- Fleet aggressiveness can be configured for autonomous behavior, including behavior left in place while the player is offline.

**Illustrative aggression choices:** Actively seek engagements, take a middle approach, or fight only when necessary. Exact names, number of settings, target criteria, and actions are not yet defined.

**Open:** How mission orders and direct fleet movement interact, what fleet-level changes can be made during combat, retreat and disengagement, order persistence, and presentation of the risks associated with different aggression settings.

## Information boundaries

The interface must respect finite sensor coverage: a sector map is not a live view of all activity. The world includes remote outposts and distributed sensors that may be stolen. Exact last-known information and terrain-memory behavior remain open.

## Related documents

- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)
- [Art Direction](ART_DIRECTION.md)

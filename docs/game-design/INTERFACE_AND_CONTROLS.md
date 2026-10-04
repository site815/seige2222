# seige2222 — Interface and Controls

[Design index](README.md) · [Status definitions](README.md#design-status)

Interface direction and the boundary between player decisions and automatic simulation. The current top bar and construction-menu requirements below are selected user direction; remaining screens and detailed behavior still need development and playtesting.

## Interface references

**Confirmed direction:** Manor Lords is the closer primary interface reference because the game is mainly a settlement-building simulation. StarCraft II is a secondary reference for polish and clarity.

The user wants to improve on aspects of Manor Lords using the perceived polish of StarCraft II. The specific requirements below now refine that direction. These references do not adopt individual-unit combat micromanagement.

## Current layout and construction requirements

**Confirmed user requirements:**

- Show the current game version and FPS at the top left.
- Put colony summary information in a top bar, with useful explanations available on hover.
- Open construction through the **B** key. Do not use the earlier persistent bottom construction menu.
- Represent construction choices with icons and reveal their building names on hover.
- Organize construction logically and provide understandable keyboard shortcuts. Exact category and item keys are implementation choices until verified in play.

**Implementation guidance:** Keep the resource-related, logistics, and defense building categories recognizable; a more detailed industrial group can help organize production without changing building capabilities. Explain a selected building's cost, workers, purpose, and unavailable prerequisites where useful. The interface should let the player understand a blocked action without reading source files.

Hover and keyboard behavior must preserve ordinary world controls. Interface clicks must not place a building behind a menu or dialog, and menu shortcuts must not simultaneously move the camera. Input handling must work between rendering passes; drawing state cannot be required to process a click.

The displayed version must match the delivered build's version. These are acceptance requirements, not a claim that the latest package has passed interaction or visual testing.

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

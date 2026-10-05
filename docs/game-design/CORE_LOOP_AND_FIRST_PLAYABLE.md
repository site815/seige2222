# seige2222 — Core Loop and First Playable

[Design index](README.md) · [Status definitions](README.md#design-status)

Working design document. The opening, current robotic population scope, and main competing priorities are established; detailed colony operation is still being developed. This is not yet an implementation specification.

The user has now authorized implementation. [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records the concrete Unreal prototype subset and its deviations; this document continues to describe the broader intended colony loop.

## Starting colony

**Confirmed direction:** The player starts with a command center and some robotic workers. The exact worker count and starting supplies remain open.

Only one central command center and central zone may be maintained at a time. The core contains central command and the orbital escape shuttle; moving the home center requires relocation.

## Workforce and population

**Confirmed current scope:** The colony has one population type: robots. This applies to the population as a whole, not only the starting workers. Human populations, mixed populations, and a choice between population types are deferred for possible later reconsideration.

**User rationale:** The game needs a deliberate pace relative to real time. Robots make rapid population expansion more plausible without having to resolve human reproduction or immigration.

**Confirmed direction:** New population is produced as robots. Growth is constrained by a maximum robot-production rate and by resource production at defined rates per time unit.

The core automatically produces robots to fill open jobs and automatically reduces population when job demand falls. The population-reduction mechanism and response rates remain open.

**Provisional direction:** New robots can only be produced at the command core, possibly because it contains a unique advanced manufacturing capability. A high-end lithography plant is an illustrative explanation, not a selected prerequisite.

**Open:** Robot needs, job-allocation and demand-counting rules, production inputs, exact rates, population-reduction mechanics, and how workers relate to the broader robotic population. Staffing, building operation, and population adjustment are automatic. See [Population, Necessities, and Morale](POPULATION_AND_MORALE.md).

## Core decisions and pressures

**Confirmed direction:** The player balances three competing uses of colony development:

1. Produce the commodities that support the population and its happiness.
2. Build up defenses and prepare for attacks.
3. Expand to obtain additional resources.

The recurring choice is what to build and prepare first. These priorities operate under three sources of pressure: roaming background aliens, periodic alien invasion pulses, and possible privateer attacks sent by developed neighbors. The latter is a possible consequence of neighboring an established faction, not a mandatory attack from every such faction.

**Current local prototype:** v0.7 scenario setup independently enables background bugs and periodic attacks, both ON by default. The choices apply to every occupied colony and developed AI preparation, persist in saves, and remain fixed during play. Privateers are not implemented. These local options do not select persistent-server settings; see [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md).

**Current scope:** Formal war against neighboring regions is removed. Privateering, attacks on neighbors, and theft remain part of the loop.

**Confirmed direction:** Population needs should make sense for robots. The precise goods and the robot-appropriate meaning or replacement for happiness remain open; this adaptation preserves the intended economic tradeoffs.

**User proposal:** Unmet needs or losses could lead to a robot revolt that forces the player to escape by orbital shuttle. This possible colony-loss route is not finalized; see [Population, Necessities, and Morale](POPULATION_AND_MORALE.md).

## Game pace and real time

**Confirmed direction:** Establish a deliberate relationship between game time and real time that supports the intended pace in the persistent world and its single-player version.

**Illustrative only:** Five or ten times real time. Neither multiplier is selected.

**Current local controls:** Pause and 1×, 5×, and 10× playback are implemented; see [Interface and Controls](INTERFACE_AND_CONTROLS.md). The persistent world's final game-time rate and which timers use game time versus real time remain open. The tentative thirty-minute alien invasion interval also needs that clock defined.

## Everyday operation and settlement strategies

**Confirmed direction:** Constructed buildings automatically fill their jobs and operate when sufficient workers and resources are available. Production and goods movement are automatic; the player can turn buildings off. Required workers and open jobs should be visible, and routine manual adjustment should be minimal.

The game should support both large colonies that benefit from abundant labor and defensive numbers, and small, specialized colonies that remain functional as either outposts or main settlements. A larger population also consumes more and does not by itself increase extraction beyond the defined rates.

See [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md) for the rules and unresolved operational details.

## Colony-loop topics still to develop

The following is a discussion outline, not a selected set of mechanics:

1. How automatic staffing and resource allocation behave when workers or inputs are scarce.
2. How the initial colony obtains supplies, constructs buildings, and sustains its robots.
3. How extraction, production, hauling, and consumption connect into everyday activity.
4. What motivates expansion and how threats affect its pace.
5. Which part of that experience belongs in the first playable scenario, and how it will be evaluated.

## Opening pressure and core upkeep

**Confirmed direction:**

- The starting core should be able to repel ordinary incoming attacks. Repeated damage without repair should make it vulnerable.
- All repairs are automatic; the player does not need to issue individual repair orders. The labor, materials, rates, priorities, and other conditions for repairs remain open.
- Single-player colony gameplay should follow the same rules intended for the persistent world, rather than relying on a different protected opening.
- There is no separate protection period. Roaming threats can arrive immediately after the colony is established.
- Main alien invasions occur periodically on the server, with forces sent to each colony proportionate to its strength. A newly landed colony should face a smaller force than a strong established colony.

**Desired pacing:** Give a new settlement room to establish itself through manageable threats and a capable starting core, within the persistent world's ongoing activity. This is not a protected time window.

**Assistant interpretation:** The core's initial defensive strength and continued repair can provide room to develop the colony within those shared rules. This does not establish invulnerability or protection from every possible attack.

**Open:** The attack strength the starting core can handle, damage and automatic-repair requirements and rates, the measure of colony strength, and the final interval between main invasions. See [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md) for the tentative invasion interval.

## Neighbor development at the start

**Confirmed direction:** The player can make AI selections among the eight neighboring sectors. For each AI neighbor, the player can choose a developed colony representing an established human player or a newly founded colony starting out like the player. These choices allow single-player to represent different stages of settlement in a persistent world.

Exact starting assets and development profiles remain open. See [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md) for AI-neighbor setup.

**User proposal:** Include abandoned areas as another setup option and offer a random mix of developed, new, abandoned, and empty neighboring sectors. One or two of each was an illustration, not a fixed distribution.

**Confirmed direction:** Empty neighboring sectors may support risky remote resource extraction. Distance and pirate exposure matter, and this does not permit a second central command center.

## Sector space and aftermath

**Confirmed direction:** Sectors should be large and mostly wilderness, with remote outposts and resource-costly sensors that can be stolen. The approximate city and developed-area proportions are recorded in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

Leaving by shuttle can leave an entire leaderless area of surviving robots, settlements, and stockpiles behind. Such AI-controlled areas offer opportunities for scavenging, with no guarantee that the haul exceeds the cost of obtaining it; see [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).

## Related documents

- [Vision and Setting](VISION_AND_SETTING.md)
- [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Design Review and Recommended Next Documents](DESIGN_REVIEW.md)

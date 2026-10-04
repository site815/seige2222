# seige2222 — World, Visibility, and Relocation

[Design index](README.md) · [Status definitions](README.md#design-status)

Sector layout, information coverage, adjacent colonies, escape, and permanent loss.

## World, sectors, and visibility

**Confirmed**

- A planet is divided into sectors. The player owns a center sector and sees eight neighboring sectors in a 3×3 grid.
- The camera should provide a broad overview of all nine sectors as well as a closer colony view.
- Before a single-player scenario, each neighbor can be empty, a starting AI colony, or a developed AI colony. Disabled means empty, not frozen inhabitants.
- The center can instead be AI-controlled for an observer scenario. Human play begins with a paused survey and selection of the one command-core site.
- Seeing a sector on the map does not reveal its current activity.
- Units and buildings have different sensor ranges. The core has broad but finite coverage. There is no live knowledge outside the player's own coverage.

**Open**

- Final sector size, travel distances, and the relationship between the grid and traversable space. The current prototype dimensions below are implementation values.
- Whether occupied neighbors can be changed during an active scenario; pre-game selection is established.
- Explored-terrain memory and last-known enemy positions.

**Proposal:** AI factions should obey equivalent sensor limitations across the full game. The current local construction controller already uses normal placement and live-coverage checks; this does not settle all future AI information or scouting rules.

### Current local scenario implementation

Each sector is **600 m × 600 m**, giving the 3×3 overview a **1.8 km side**. The source template contains **25 irregularly clustered resource nodes**, repeated in each instantiated colony's local sector. This is a finite prototype map, not procedural planetary geography or a final scale decision.

Human core selection pauses the scenario and displays a resource survey. Placement rejects sector-edge and deposit conflicts through normal simulation validation. After placement, ordinary finite live coverage applies. Neighbor building and threat rendering respects the human colony's coverage; observer mode deliberately shows the simulated colonies for observation.

Each occupied sector runs an independent local economy and alien threat simulation. The shared view does **not** yet permit cross-sector travel, goods transfer, trade, raiding, or a player's extraction in a neighboring sector. See [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) for startup, controller, and save details.

## Sector scale, wilderness, and remote outposts

**Confirmed direction:** Each sector should be large enough for a substantial city to occupy only a small part of it. Most of the territory should remain wilderness, and outposts should be remote from the main city.

**Provisional scale targets:** A large city might occupy roughly 5–10% of its sector. Total non-wilderness or developed area should not exceed roughly 10–20%, leaving about 80–90% as wilderness. These are approximate design targets, not selected map dimensions or an implemented building-limit rule.

**Open:** Actual sector dimensions, travel distances, how developed area is measured, whether roads and sensor installations count, and how the intended proportions are achieved.

## Distributed sensors

**Confirmed direction:**

- Covering the large sector requires sensors distributed around the territory.
- Establishing and maintaining sensor coverage has real resource costs and should be difficult to sustain.
- Sensors can be stolen and should be valuable targets for privateers or pirates.
- Finite sensor coverage remains central: viewing the sector grid does not reveal all activity within it.

**Open:** Sensor ranges, construction and upkeep requirements, how an installed sensor is taken and transported, and the treatment of captured equipment. No currency system or monetary price is selected by this direction.

## AI neighbor starting development

**Confirmed direction:** The player can make AI selections among the eight neighboring sectors. Each AI neighbor can start as either of the following:

- A developed colony, representing a well-established player already situated in the persistent world.
- A newly founded colony, representing a player starting out alongside the player's new colony.

The distinction concerns starting development. The prototype has externally editable starting priorities and a finite developed preset in [AIFILES](../../AIFILES/README.md). The latter pays ordinary construction costs from its setup stock. Final buildings, population, stockpiles, forces, and additional development profiles remain open; the current preset does not settle full-game balance or diplomatic relationships.

The disabled-neighbor rule means an empty sector. Startup selections are now implemented; changing occupied cells during play is not supported.

**User proposal:** Starting setup could also offer abandoned, leaderless areas. A random mix might include one or two developed neighbors, one or two new neighbors, one or two abandoned areas, and one or two empty sectors. These are illustrative possibilities, not a fixed distribution or selected default for all eight slots.

## One central zone and neighboring resource expeditions

**Confirmed direction:** The player has only one central command center and one central zone at a time. The command center houses the central command system and the orbital escape shuttle. Moving the center of the colony requires relocating it; expansion does not allow a second central command center.

The player may attempt resource extraction in empty neighboring sectors. Those resources are distant and expose operations and transport to risks such as pirates. Such activity is an extension from the single home center, not a second central zone.

**Open:** How cross-sector extraction is established, supported, and accessed; destination and ownership rules; the exact relocation sequence; and what happens to a surviving former core after departure. The one-center limit is distinct from the still-provisional rule that only the core can manufacture new robots.

## Loss, escape, and relocation

**Confirmed direction**

- Permanent defeat is possible.
- Players can build an orbital spacecraft to relocate to another sector on the same planet or to another planet.
- The orbital shuttle's primary role is an emergency ejection seat for the colony.
- Destruction of the command core automatically launches the orbital shuttle into orbit.
- The escape payload is whatever is already physically aboard the shuttle when it launches. Launch does not automatically transfer the colony's other stock into the shuttle.
- A shuttle loaded with valuable, high-tier products can let the player escape wealthy and resettle with substantial assets, even after losing the colony.
- The same shuttle system also supports multiplayer relocation, including moving between servers. Its emergency escape and relocation roles belong to one connected system.
- Friends starting together should be able to start in adjacent sectors.
- Later, both friends should be able to build ships and relocate to adjacent sectors again.
- Retreating to a less populated region and rebuilding is possible if an escape is available.

**Tentative:** A different planet probably corresponds to a server shard, but this is not an established architecture.

The earlier assistant proposal to frame orbital escape as a high-tier industrial project is superseded by the emergency ejection-seat role. Exact construction requirements and availability are still open.

**Open:** Shuttle availability and construction requirements; loading controls, cargo eligibility, and capacity; which robots or other non-cargo assets can accompany the player; destination selection and adjacency reservations; conditions affecting shuttle readiness or survival; and the precise condition for permanent defeat if escape is unavailable. Core destruction as the automatic launch trigger and carriage of the existing onboard contents are established.

### Possible escape after a robot revolt

**User proposal:** A dissatisfied robotic population could revolt, forcing the player to flee by orbital shuttle and lose the colony. This adds a possible internal source of defeat alongside external attack.

The revolt system, its conditions, and the consequences of failing to escape remain open. Whether revolt automatically launches the shuttle or instead requires the player to flee has not been decided. See [Population, Necessities, and Morale](POPULATION_AND_MORALE.md) for the proposal.

## Leaderless areas after departure

**Confirmed direction:** Surviving robots, cities, remote outposts, and stockpiles left behind when the player leaves by shuttle form a leaderless, chaotic AI-controlled area. The intended state resembles a revolt's aftermath. This is distinct from ordinary player logout.

Such areas can be scavenged gradually by forces capable of taking resources from them. Rich stockpiles can yield a major gain, but an expedition can also cost more than it recovers. The detailed continuing behavior of these areas is open; see [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).

## Related documents

- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)
- [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md)

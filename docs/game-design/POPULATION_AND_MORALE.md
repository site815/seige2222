# seige2222 — Population, Necessities, and Morale

[Design index](README.md) · [Status definitions](README.md#design-status)

The current robotic population scope, population-serving commodities, growth, and the earlier needs and morale ideas that require adaptation.

## v0.6 visible population boundary

Current source displays aggregate examples of existing builders, operating workers and supported service robots. Exterior tool-fetch/workstation cycles are presentation, not extra population or an independent worker simulation. Real staffing and supplies determine whether work is active, and interpolated simulation time freezes animations when paused. Capacity, growth, maintenance and retirement rates are unchanged. The v0.6 editor compiled; all 34 native tests passed cleanly (zero warnings, failed or unrun), and the 79-stage editor route passed with zero failures, including 413 interpolated courier-motion frames at 1×. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; older release evidence remains version-specific.

## v0.5 robot services — current implementation

The user now requires a robot charging/maintenance building analogous to housing. The source implements a service bay with a real population-support capacity and a staffed operating requirement. The core supports the initial crew, so the player can establish the first bay before expansion. Exact capacities, worker counts, construction duration and upkeep remain external prototype values in `Rules/buildings.json` and `Rules/policies.json`.

Manufacturing requires open job demand, local assembly inputs and usable service capacity. Each completed, enabled and staffed core/service bay supports part of the population and consumes components from its own delivered stock at upkeep intervals. Uncovered or unsupplied robots lower workforce efficiency; capacity loss does not directly delete them. Retirement still follows reduced job demand without refunds. Charging is represented by automated support, not individual battery meters or a separate electricity grid.

This settles the first playable's service mechanism, not happiness, revolt, permanent population loss or all full-game needs. The verified local v0.5 package uses Rules `prototype-5.0`. Native service-capacity/local-maintenance tests are part of the 29-test passing suite, and the packaged route passed 63 stages. The service-bay capture had no supported robots assigned, so occupied-bay presentation remains unverified. Evidence and limits are tracked in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md).

## Current population scope

**Confirmed current scope:** Robots are the only colony population type for now. Human and mixed populations, immigration, and a player choice between population types are deferred for possible future reconsideration.

The user chose this scope to support plausible growth at an accelerated game pace without resolving human reproduction or migration. Population increases by producing robots; the production rules below are being developed.

## Robot production

**Confirmed direction:**

- The robotic population is produced.
- The core automatically produces robots to fill open jobs.
- When the colony's job demand falls, the core automatically reduces robot population accordingly. Population adjustment should not require a player-set population target.
- Robot production has a maximum rate: a ceiling on the number produced per unit of time.
- Resource production provides a second bottleneck, with resources produced at defined rates per time unit. Having more production capacity does not remove the need for sufficient inputs.

**Provisional direction:** The command core is the only place able to produce new robots.

**Possible explanation, not a selected building specification:** The core could contain the colony's only high-end lithography plant or another unique capability needed for robot production. The purpose is to make the core's role plausible; no exact technology, recipe, or additional structure is established.

**Open:** Exact input resources or components, output rate and time unit, effects of core upgrades, which roles count toward job demand, response delays, and the mechanism and rate of population reduction. Storage, deactivation, dismantling, and resource refunds have not been selected. Any effect of robot needs or satisfaction on output also remains open. A maximum production rate is not a decided absolute cap on total population.

## Population needs and growth

**Confirmed direction**

- Colony development includes producing commodities that support the population and its happiness, in competition with investment in defenses and resource expansion.
- Needs and population systems should make sense for robots. The earlier human-oriented model should be adapted accordingly, rather than assuming the same consumption requirements.

**Updated direction:** Population changes automatically with job demand: the core produces robots for open jobs and reduces population when demand falls. The earlier happiness-driven growth idea is superseded as the primary growth rule. Robot needs and any equivalent of happiness still require design; any effect they have on production or workforce behavior is open.

**Broadly accepted proposal:** Civilian goods must reach the population; merely existing elsewhere in the colony is insufficient. Exact delivery and consumption mechanics remain open.

**Open for robots:** The actual population-serving goods, the robot-appropriate meaning or replacement for happiness, the role of needs in robot production, and the relationship between population and available workers. No power, maintenance, component, or consumption recipe has been confirmed merely by selecting robots.

## Workforce and population size

**Confirmed direction:** Robots automatically fill jobs required by buildings. The player should see worker demand and unfilled jobs while avoiding routine manual assignment. Buildings operate automatically when enough workers and resources are available; see [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md).

Large populations offer more labor and the possibility of defense through numbers, but also require more consumption. Small specialized populations should remain viable, including in a main colony. Population alone does not raise resource extraction beyond its defined rate limits.

**Open:** Worker allocation under shortages, which robots take which roles, their relationship to military forces, and exact consumption rates or specialization benefits.

## Robot revolt — user proposal

**Proposed possibility, not a finalized rule:** If population satisfaction falls below an unspecified threshold, possibly because of unmet needs or losses, the robotic population could revolt. The player would have to escape in the orbital shuttle, making revolt another possible way to lose the colony.

**Established aftermath direction:** A revolted area is conceived as a leaderless, chaotic robot-controlled AI area, the same type of state that remains after the player leaves by shuttle. This defines the intended aftermath without settling revolt triggers or making revolt inevitable. See [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).

**Open:** What dissatisfaction means for robots, which shortages or losses contribute, thresholds and duration, warnings or recovery, what a revolt does, whether escape is automatic or player-triggered, and whether failure to escape is permanent defeat. The proposed revolt does not establish another automatic-launch trigger; command-core destruction is the confirmed trigger.

## Morale ideas requiring adaptation

**Provisional:** Morale was initially discussed as about 10% of happiness, then revised by the user to perhaps 30%. An approximate 70% material / 30% morale balance is a working idea, not a fixed formula.

**Possible morale sources, specifics deferred:** Repelling alien assaults, victories, gaining resources, and discovering caches. Wars were also mentioned earlier, but formal war between colonies is now outside current scope; no war-specific morale mechanic is active.

**Assistant proposals:** Warnings, reserves, and a progression through shortages before serious population losses; morale decay, caps, and peaceful milestones. None has a settled implementation. This shortage-response idea is not an attack-protection period.

**Open:** How these earlier morale ideas apply to robots, robot needs, growth and consumption curves, distribution mechanics, shortage tolerance, morale events and duration, and the final happiness model.

## Earlier human-oriented needs — deferred

The following ideas were established or proposed before the robot-only scope was selected. They remain recorded for possible later reconsideration, but are not assumed to describe robot biology or consumption:

- Earlier direction: sustained mismanagement can cause starvation and death, but reaching that outcome should not be easy.
- Earlier direction: food cannot be replaced by morale.
- Earlier assistant-proposed goods: food or rations, water, food variety, clothing, household goods, and hygiene products.

Robot-specific shortage consequences and any equivalent survival requirements remain open. The earlier goods list is also retained, with its status marked, in the [provisional catalog](PROVISIONAL_CATALOG.md).

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Provisional Resource, Building, and Product Catalog](PROVISIONAL_CATALOG.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)

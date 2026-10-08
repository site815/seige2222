# seige2222 — Population, Necessities, and Morale

[Design index](README.md) · [Status definitions](README.md#design-status)

The current robotic population scope, population-serving commodities, growth, and the earlier needs and morale ideas that require adaptation.

## Current worker terminology and v0.9 identities

The interface calls the robotic population **workers**. The v0.9 source gives each body a persistent identity across deployment, walking, operating, construction, hauling, storage and recycling. The finite shared workforce scheduler assigns jobs automatically; the player does not command individual workers. The slow core replicator and faster worker factory pay real materials and electricity. The accepted provisional body is 80 kg/160 L packed, with a 40 kg/60 L cargo capacity; an inactive worker moves by self-relocation instead of fitting inside that smaller payload. [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md) owns the current values and acceptance boundaries. [Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) retains the earlier routing/construction history.

The current economy source adds actual road-connected electricity and shared battery storage; this supersedes the historical v0.5 abstract-charging boundary below. [Rex](COMPANIONS_AND_REX.md) adds one specific companion morale mechanism: local physical organic-food consumption and a capped benefit near the fed dog. Workers still do not eat food, and the broader happiness/revolt model remains open. Rex's direct walking controls do not extend to worker micromanagement.

**Implemented candidate hauling demand:** the current worker policy reserves **min(8, 1 + floor(eligible completed facilities / 4))** desired logistics jobs. Eligible facilities are living, enabled and complete, excluding the `core` and `wall` roles. A power or supply shortage does not remove a completed facility from the count, so a shortage does not repeatedly shrink and regrow hauling demand. The initial core alone still requires one logistics job and receives no extra starting bodies.

The same derived count enters total job demand and the scheduler's reserve of idle workers plus workers already hauling. Ordinary paid manufacture or reactivation, physical arrival and service support must fill it; the formula does not create workers. Essential core and service staffing retain priority. Self-relocating packed workers are not counted as available haulers. The eight-job cap is a desired reserve, not a hard limit on deliveries already underway: active loads continue if the reserve shrinks. The policy and its provisional values are external in [workers.json](../../Rules/workers.json); compilation is complete, but its new native regressions and full v0.9 acceptance remain pending.

## Historical v0.6 visible population boundary

The historical v0.6 source displayed aggregate examples of existing builders, operating workers and supported service robots. Exterior tool-fetch/workstation cycles were presentation, not extra population or an independent worker simulation. Real staffing and supplies determined whether work was active, and interpolated simulation time froze animations when paused. Capacity, growth, maintenance and retirement rates were unchanged by that presentation pass. The v0.6 editor compiled; all 34 native tests passed cleanly (zero warnings, failed or unrun), and the 79-stage editor route passed with zero failures, including 413 interpolated courier-motion frames at 1×. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; older release evidence remains version-specific.

## Service mechanism introduced in v0.5

The user now requires a robot charging/maintenance building analogous to housing. The source implements a service bay with a real population-support capacity and a staffed operating requirement. The core supports the initial crew, so the player can establish the first bay before expansion. Exact capacities, worker counts, construction duration and upkeep remain external prototype values in `Rules/buildings.json` and `Rules/policies.json`.

Manufacturing requires open job demand, local assembly inputs and usable service capacity. Each completed, enabled and staffed core/service bay supports part of the population and consumes components from its own delivered stock at upkeep intervals. Uncovered or unsupplied robots lower workforce efficiency; capacity loss does not directly delete them. Retirement still follows reduced job demand without refunds. Charging is represented by automated support, not individual battery meters or a separate electricity grid.

This settles the first playable's service mechanism, not happiness, revolt, permanent population loss or all full-game needs. The verified local v0.5 package uses Rules `prototype-5.0`. Native service-capacity/local-maintenance tests are part of the 29-test passing suite, and the packaged route passed 63 stages. The service-bay capture had no supported robots assigned, so occupied-bay presentation remains unverified. Evidence and limits are tracked in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md).

## Current population scope

**Confirmed current scope:** Robots are the only colony population type for now. Human and mixed populations, immigration, and a player choice between population types are deferred for possible future reconsideration.

The user chose this scope to support plausible growth at an accelerated game pace without resolving human reproduction or migration. Population increases by producing robots; the production rules below are being developed.

## Robot production

**Confirmed direction:**

- The robotic population is produced.
- Core replication or a dedicated worker factory produces workers using materials and energy to fill jobs, with an optional configurable inactive-worker reserve target.
- When job demand falls, excess workers leave active staffing and occupy physical storage. A player need not micromanage their jobs.
- Robot production has a maximum rate: a ceiling on the number produced per unit of time.
- Resource production provides a second bottleneck, with resources produced at defined rates per time unit. Having more production capacity does not remove the need for sufficient inputs.

**Superseded:** Core-only worker manufacture. A separate worker factory is now confirmed, with faster assembly and lower transaction energy than the universal core replicator.

The earlier suggestion that only the core has lithography is historical speculation, not a current restriction on worker manufacturing.

**Confirmed:** Inactive workers are whole physical cargo items. Configurable colony and trading-port reserve targets can retain them for later work or export. Surplus workers disassemble automatically for a parts shortage or insufficient storage; manual surplus disassembly and target controls are implemented in the worker HUD, with bounded controls/lifecycle checks in the [v0.8.1 record](../verification/v0.8.1.json). Individual worker identities and the accepted 80 kg balance are implemented in the v0.9 source and still undergoing broader acceptance. Disassembly costs 1 kWh by default and returns 72 kg of the original 80 kg inputs, recording 8 kg of unrecovered material rather than creating matter. Exact assembly, response and recycling rates, energy, body mass, berth volume and refund quantities remain provisional external balance. Effects of broader satisfaction on production remain open. A maximum production rate is not a decided absolute population cap.

## Population needs and growth

**Confirmed direction**

- Colony development includes producing commodities that support the population and its happiness, in competition with investment in defenses and resource expansion.
- Needs and population systems should make sense for robots. The earlier human-oriented model should be adapted accordingly, rather than assuming the same consumption requirements.

**Updated direction:** Active workforce follows job demand while inactive workers can be retained or exported under reserve targets. The earlier happiness-driven growth idea is superseded as the primary growth rule. Broader robot needs and happiness effects remain open beyond implemented energy/maintenance and Rex's specific morale contribution.

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

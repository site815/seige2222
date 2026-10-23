# seige2222 — Population, Necessities, and Morale

[Design index](README.md) · [Status definitions](README.md#design-status)

The current robotic population scope, population-serving commodities, growth, and the earlier needs and morale ideas that require adaptation.

## Current v0.10 worker ownership and charging

The interface calls the robotic population **workers**. Each body has a persistent identity, a permanent building crew owner, a temporary task and a battery. The core alone manufactures workers from physical materials and electricity; the former worker factory is a Construction Hub. The provisional body remains 80 kg/160 L packed, with a 40 kg/60 L carried payload. Whole inactive workers relocate physically rather than fitting inside that smaller payload. [Production Grid 0.10.0](PRODUCTION_GRID_0_10_0.md) records the current producer and starter contracts; older economy baselines retain their historical acceptance boundaries.

The current economy source adds actual road-connected electricity and shared battery storage; this supersedes the historical v0.5 abstract-charging boundary below. [Rex](COMPANIONS_AND_REX.md) adds one specific companion morale mechanism: local physical organic-food consumption and a capped benefit near the fed dog. Workers still do not eat food, and the broader happiness/revolt model remains open. Rex's direct walking controls do not extend to worker micromanagement.

**Current crew policy:** Player and Starting-AI colonies land with five workers assigned to the core. Core crew capacity is 5/10/15 by level; this is the core's workplace capacity, not a global population limit. Buildings obtain their own permanent crews. Hauling and transport loading use those actual crew members, and `LogisticsJobs` contributes no extra phantom population demand. Construction Hubs specialize in building work.

**Current v0.10.4 spare reserve:** The reserve is automatic: Level 1 keeps five spare bodies, Level 2 ten, and Level 3 fifteen, derived directly from the current command building's authored job capacity. It is additional to active job demand and cannot be edited through the UI or restored from a different saved manual value. Pending upgrades retain the current level's reserve until completed. Trading-port stock targets remain independently editable and additive. No body is granted to fill a target, and active workers are not pulled away from jobs merely to satisfy spare stock. Assembly still pays its material/energy bill and waits for room in physical storage.

**Movement:** `Rules/mobility.json` is the only worker/transport base-speed authority. Empty and fully loaded workers both move at 10 km/h on easy terrain (previously 5); empty and fully loaded autonomous transports both move at 30 km/h (previously 15). Rough terrain applies the same multiplier to both cargo states: current workers 8 km/h, transports 7.5 km/h. Powered road, road+rail, and vacuum tiers multiply base speed by 2, 4, and 8; unpowered corridors give no bonus. Rex grief halves worker movement regardless of cargo, leaving autonomous transport unaffected. Loading/handling times and energy per distance are unchanged.

Workers have 0.2 kWh batteries under the current provisional rules. Charging requires physical travel to the core or a charging/maintenance bay, a berth and delivered grid electricity. A berth serves one worker at a time; the station has its own crew requirement. Charging temporarily interrupts work without changing permanent crew ownership. No abstract service-support cap deletes bodies or limits total ownership. Exact autonomy, charge time and crew balance remain editable and require current-source checks.

## Historical v0.6 visible population boundary

The historical v0.6 source displayed aggregate examples of existing builders, operating workers and supported service robots. Exterior tool-fetch/workstation cycles were presentation, not extra population or an independent worker simulation. Real staffing and supplies determined whether work was active, and interpolated simulation time froze animations when paused. Capacity, growth, maintenance and retirement rates were unchanged by that presentation pass. The v0.6 editor compiled; all 34 native tests passed cleanly (zero warnings, failed or unrun), and the 79-stage editor route passed with zero failures, including 413 interpolated courier-motion frames at 1×. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; older release evidence remains version-specific.

## Historical service mechanism introduced in v0.5

The user now requires a robot charging/maintenance building analogous to housing. The source implements a service bay with a real population-support capacity and a staffed operating requirement. The core supports the initial crew, so the player can establish the first bay before expansion. Exact capacities, worker counts, construction duration and upkeep remain external prototype values in `Rules/buildings.json` and `Rules/policies.json`.

Manufacturing requires open job demand, local assembly inputs and usable service capacity. Each completed, enabled and staffed core/service bay supports part of the population and consumes components from its own delivered stock at upkeep intervals. Uncovered or unsupplied robots lower workforce efficiency; capacity loss does not directly delete them. Retirement still follows reduced job demand without refunds. Charging is represented by automated support, not individual battery meters or a separate electricity grid.

This settles the first playable's service mechanism, not happiness, revolt, permanent population loss or all full-game needs. The verified local v0.5 package uses Rules `prototype-5.0`. Native service-capacity/local-maintenance tests are part of the 29-test passing suite, and the packaged route passed 63 stages. The service-bay capture had no supported robots assigned, so occupied-bay presentation remains unverified. Evidence and limits are tracked in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md).

## Current population scope

**Confirmed current scope:** Robots are the only colony population type for now. Human and mixed populations, immigration, and a player choice between population types are deferred for possible future reconsideration.

The user chose this scope to support plausible growth at an accelerated game pace without resolving human reproduction or migration. Population increases by producing robots; the production rules below are being developed.

## Robot production

**Confirmed direction:**

- The robotic population is produced.
- Only the command core produces workers, using materials and electricity to fill jobs and an automatic inactive-worker reserve equal to command-center capacity. Material processing, chassis and weapon manufacture require their specialist buildings.
- When job demand falls, excess workers leave active staffing and occupy physical storage. A player need not micromanage their jobs.
- Robot production has a maximum rate: a ceiling on the number produced per unit of time.
- Resource production provides a second bottleneck, with resources produced at defined rates per time unit. Having more production capacity does not remove the need for sufficient inputs.

**Superseded:** The v0.8/v0.9 universal replicator and faster worker factory. v0.10 restores core-only worker manufacture and repurposes the old factory as a Construction Hub.

**Current editable assembly baseline (verified against prototype-10.0 rules):** Each worker consumes 60 kg robotic parts, 12 kg construction alloy, 4 kg control circuits and 4 kg battery modules (80 kg total), plus 4 kWh per batch. The `assemble_robot` recipe supplies a 60-second base multiplied by the command center's 10× duration and 4× energy multipliers. All three core levels currently have the same nominal 600 simulation-second cycle: 10 real minutes at 1×, 1 at 10× or 30 seconds at 20×, at full operating efficiency. Calendar hours use the separate accelerated day/night clock. Actual assembly takes longer with unavailable operators, low support efficiency, missing local deliveries or insufficient energy; the configured zero-crew recovery exception runs at 10% of normal work fraction. The five starting workers are installed starting assets, separate from the loose cargo kit. Sources: [recipes](../../Rules/recipes.json), [building multipliers](../../Rules/buildings.json), [worker recovery](../../Rules/workers.json).

The earlier lithography explanation remains historical speculation; the current worker-only core restriction is an explicit production rule.

**Current v0.10 rule:** Inactive workers remain whole physical stock with colony and trading-port reserve targets for later work or export. Automatic disassembly for parts shortage or full storage is disabled, and the manual surplus-disassembly control is removed. Storage pressure must not silently destroy bodies. Legacy disassembly recipe values and earlier v0.8.1 tests remain historical data, not an active lifecycle policy. Active, stored and idle-surplus states are reported alongside total owned identities; combat and transport vehicles are separate counts.

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


## v0.10.3 worker durability and companion mourning

Each physical worker body has 100 HP, configured by `maximum_health` in `Rules/workers.json`. Health travels with that body's identity through jobs, storage, cargo and save/load. Exposed workers can be damaged by contact, direct or unintended projectiles, and splash. A lethal hit removes the worker from active tasks once: a hand courier's actual cargo is lost through the existing delivery ledger; a loading worker releases its handler claim while the independent transport and its cargo remain. Inactive bodies inside containers are protected until the enclosing container's destruction rules apply.

Charging maintenance repairs 2 HP per simulation second while the powered, attended berth services that body. It cannot revive a destroyed worker. Worker health requires save format 9; older saves are left intact and rejected with a new-colony explanation.

If Rex dies, every worker's physical movement is multiplied by 0.5 for thirty complete authoritative day/night cycles. This includes newly manufactured and reactivated workers, off-road movement and road travel. The road tier bonus still applies; production, construction labor and charging time receive no separate depression multiplier. The persistent deadline is checked directly against the world calendar and expires at exactly thirty days. Rex's companion rules own the duration and speed multiplier.

# seige2222 — Resources, Industry, and Progression

[Design index](README.md) · [Status definitions](README.md#design-status)

Resource geography, settlement patterns, trade access, manufacturing chains, and building upgrades.

## Resources, settlement geography, and trade

**Confirmed direction**

- Current resource pool: four standard types (water, metal ore, silica, biomass) and four rare types (rare metals, radioactive ore, crystalline material, hydrocarbons). The earlier approximately 10–15-type direction is superseded for current scope.
- Each region contains exactly three different standard deposits and two different rare deposits, with no duplicate resource type. Draw from the four-plus-four pool and randomize positions inside the inner 75% of the region's area. The current implementation chooses a centered square, stable region seeds and separated locations; fairness remains to verify.
- One **Extraction Mine** blueprint serves all raw types. Placement binds output to the underlying deposit; there is no independent resource selector. The current prototype permits one live mine per deposit and uses externally configured per-resource rates.
- Spatially separated resource sites should create meaningful choices between one large city and multiple smaller settlements or outposts.
- Both common and rare materials may unlock production branches or improve and specialize products. Rarity does not determine a material's role.
- Trade in raw materials and manufactured goods expands access to production, subject to having the necessary buildings.

**Proposal:** A hybrid of a main city and satellite outposts is another possible settlement pattern.

**Confirmed trade and operating economy:** Galactic credits are used only for external trade through a trading port upgradeable from level 1 to 3; 1 credit is anchored to the value of 1 kg of gold. The colony starts with zero credits. Landed materials must support a road, solar generation and the first port; exporting local raw goods funds missing-standard imports. This does not imply free credit, universal buyers or a gold deposit.

Energy is the main operating resource: passive consumption per simulation second plus production energy per transaction, including transactions with multiple outputs. Buildings on the same connected road network share a grid with battery storage. Organic food feeds Rex and supports exports; it is not worker nutrition. Confirmed stages are fuel/plastic pellets/organic food; control circuits/robotic parts; AI chips/fusion reactors.

**Retained v0.8 economy:** Seeded generation, physical kg/L cargo, litre storage, road-connected grids, batteries, paid external shipments and three port levels have provisional external definitions. [Resource Proposal](RESOURCE_PROPOSAL.md) identifies their authority; [Product Recipes and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) distinguishes executable recipes from larger illustrative alternatives. Fairness, depletion, final yields/prices, long-term viability, advanced battery limits and future upgrade balance remain open. The [v0.8 verification record](../verification/v0.8.0.json) records completed economy checks. The v0.8.1 single-mine binding and five resource HUD groups are newer changes. They have compiled and been staged; package boot and four display states passed. The final v0.8.1 Shipping executable passed 117 interaction stages, boot/offline checks and four display states; reconciled native coverage contains 88 unique clean results. The [v0.8.1 verification record](../verification/v0.8.1.json) identifies the exact evidence and retained limitations.

## Viable settlement strategies

**Confirmed design goal:** Multiple population strategies should be viable, including a large colony supported by abundant labor and defensive numbers, and a small, highly specialized but functional settlement. The latter can be an outpost or the main colony.

Resource extraction is constrained by defined rates per time unit, now selected from the mine definition by its bound raw-resource type. Merely adding more population should not create unlimited extraction or make a very large population inherently the best production strategy. A larger population provides labor and potential defensive manpower while also consuming more supplies.

**Open:** The mechanisms that reward specialization, exact labor and consumption requirements, how workers participate in defense, and whether extraction ceilings attach to deposits, facilities, or another unit. This direction does not decide that all sites share one rate or that upgrades can never change rates.

## Capabilities and manufacturing

### Building categories

**Confirmed direction:** Buildings should have at least three broad categories:

- **Resources:** Resource gathering and related resource facilities, potentially including storage.
- **Logistics:** Movement, trade, storage, and support for robots; charging and repair areas were given as examples.
- **Defense:** Facilities whose primary purpose is protecting the colony.

Storage was mentioned under both resources and logistics; its classification is not settled. Buildings may have more than one role: a resource or logistics facility may also have rudimentary or stronger defensive capability where that fits the setting. No rule requires every such building to be armed.

**Assistant proposal:** Classify buildings by their primary job and allow secondary capability tags. That would place shared depots under logistics while permitting storage buffers on extractors and factories. This resolves overlap as a proposal, not an approved classification rule.

### Production and upgrades

**Confirmed**

- No research tree, research timer, or research currency.
- Available resources and constructed buildings determine recipes and capabilities.
- Products can combine two, three, or more manufactured inputs. Those inputs may themselves require layered manufactured inputs.
- Ultimate products sit at the ends of deep, interconnected manufacturing chains.
- Which resource sites the player develops and defends shapes the colony's technological capabilities.
- The core and most major buildings upgrade. Economic, administrative, and defensive benefits are all desired.
- Buildings automatically fill worker requirements and produce when enough labor and input resources are available. Goods movement is automatic; buildings can be turned off. See [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md).

**Open:** Exact recipes, costs, throughput, prerequisites, upgrade benefits, and whether upgrades use numbered levels or renamed structures.

**Proposed design constraints:** A starting colony should remain viable without access to every deposit. Building and production prerequisites should avoid circular dependencies that prevent progression. These are assistant suggestions, not finalized rules.

## Production rates and population growth

**Confirmed direction:** Resource production has defined rates per time unit. These rates constrain the resources available to support expansion, including the production of new robots.

Robot production has its own maximum output rate, separate from the resource-supply bottleneck. Population is manufactured rather than grown biologically. Exact recipes, numerical rates, and the applicable game-time or real-time unit remain open.

**Provisional direction:** The command core alone can produce new robots. A unique high-end lithography capability was suggested as a possible explanation, not a finalized facility or recipe. See [Robot production](POPULATION_AND_MORALE.md) for the current direction.

**Confirmed population control:** The core automatically produces robots to fill open jobs and reduces population when job demand falls. Production remains constrained by rate and inputs; reduction mechanics and any resource recovery remain open.

## Remote infrastructure and recoverable stockpiles

**Confirmed direction:** Most sector land remains wilderness, with remote outposts and distributed sensors. Sensors require resources and are valuable theft targets. The approximate development proportions and unresolved distances are recorded in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

Stockpiles left in leaderless robot-controlled areas can become a source of recovered goods, but obtaining them can cost more than their value. This is recovery of physical resources left in the world, not a guarantee of profitable or automatically replenishing loot; see [Leaderless Areas and Scavenging](LEADERLESS_AREAS_AND_SCAVENGING.md).

**Confirmed direction:** Empty neighboring sectors may be used for risky resource extraction, despite long travel and pirate exposure. The player still has only one central command center; distant extraction does not establish another central zone.

## Related documents

- [Provisional Resource, Building, and Product Catalog](PROVISIONAL_CATALOG.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)

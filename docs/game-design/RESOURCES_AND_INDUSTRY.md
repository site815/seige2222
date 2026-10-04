# seige2222 — Resources, Industry, and Progression

[Design index](README.md) · [Status definitions](README.md#design-status)

Resource geography, settlement patterns, trade access, manufacturing chains, and building upgrades.

## Resources, settlement geography, and trade

**Confirmed direction**

- Approximately 10–15 raw resource types, including common and rare materials; the final set is undecided.
- Each sector receives a fair subset. “Three common plus two rare” was an illustration, not a distribution rule.
- Spatially separated resource sites should create meaningful choices between one large city and multiple smaller settlements or outposts.
- Both common and rare materials may unlock production branches or improve and specialize products. Rarity does not determine a material's role.
- Trade in raw materials and manufactured goods expands access to production, subject to having the necessary buildings.

**Proposal:** A hybrid of a main city and satellite outposts is another possible settlement pattern.

**Open:** Resource distribution, fairness criteria, site placement, extraction rates, depletion, trade mechanics, and the final catalog.

## Viable settlement strategies

**Confirmed design goal:** Multiple population strategies should be viable, including a large colony supported by abundant labor and defensive numbers, and a small, highly specialized but functional settlement. The latter can be an outpost or the main colony.

Resource extraction is constrained by defined rates per time unit. Merely adding more population should not create unlimited extraction or make a very large population inherently the best production strategy. A larger population provides labor and potential defensive manpower while also consuming more supplies.

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

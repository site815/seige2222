# seige2222 — Building Proposal

[Design index](README.md) · [Status definitions](README.md#design-status)

**Current requirements and historical assistant proposals.** The latest confirmed expansion below supersedes contradictory older candidate rows. Exact rates, bills, capacities and dimensions beyond the user's specified relationships remain provisional in [buildings.json](../../Rules/buildings.json). The [construction document](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns current implementation contracts and verification boundaries.

## Confirmed tiered catalog expansion

| Family | Levels and capability |
| --- | --- |
| Extraction Mine | One blueprint for all eight raw types; placement binds output to the underlying deposit. Per-resource rates remain external balance. |
| Command core | Parked shuttle at level 1; levels 2/3 double/triple body width. All reserve the level-3 plot. Fusion power, storage, four large lasers and a slow selectable universal replicator, including workers and combat production. |
| Solar array | Levels 1–3, unchanged footprint, increasing externally configured output. |
| Worker factory | Fast worker assembly with lower process electricity than core replication; physical inactive-worker berths. |
| Wheeled / tracked / mech factories | Separate families, levels 1–3 at fixed family footprints. Paid, timed manufacture uses combat-defined size access and throughput. |
| Laser / kinetic / missile / plasma towers | Four separate families, levels 1–3 at fixed family footprints; hardpoints and loadouts are combat data. |
| Ammunition works | Selectable kinetic-shell or missile manufacture, with physical kilogram cargo. |
| Trading port | Levels 1–3, physical import/export including whole inactive workers and a configurable worker reserve target. |
| Wall segments | Three levels, physical obstruction and construction; a dedicated polyline tool rather than individual catalog placement. |

Vegetation clears across each reserved largest-upgrade plot. Foundations and terrain grading follow the actual built body, leaving natural relief in the future expansion yard. Source-level upgrade bills are additional costs; the next definition supplies completion time and staffing. Upper levels are upgrades, not separate starting blueprints.

The v0.8.1 source catalog contains **52 definitions and 28 ordinary blueprints**. A single `extraction_mine` replaces the eight resource-specific extractor definitions; the interface adds Road, Upgrade road and Wall as three custom tools, for 31 catalog/tool entries. This consolidation has compiled and been staged; package boot and four display states passed. The final v0.8.1 Shipping executable passed 117 interaction stages, boot/offline checks and four display states; reconciled native coverage contains 88 unique clean results. The [v0.8.1 verification record](../verification/v0.8.1.json) identifies the exact evidence and retained limitations. Earlier v0.8 release evidence does not verify the new mine binding or revised HUD.

## Latest economy direction

**Confirmed functions:** The finite landed material kit supports a road, solar generation and a level-one trading port with zero starting credits. The external trading port upgrades through levels 1–3; connected road networks share electricity and battery storage. Footprints, capacities, costs and power values are authored in the current external rules as provisional balance. [Resource Proposal](RESOURCE_PROPOSAL.md) and [Product Recipes](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) supersede the earlier four-material starter assumptions. Every building dossier reports workers used/capacity, storage used/capacity and weapons/statistics, plus stored charge/capacity where batteries exist. These systems are retained from v0.8; its completed checks are recorded in the [v0.8 verification record](../verification/v0.8.0.json). Current mine/HUD checks passed separately in the [v0.8.1 record](../verification/v0.8.1.json).

## Historical v0.6 presentation metadata

The v0.6 source added explicit indoor/outdoor inventory location and worker-activity presentation to each implemented building. Construction sites show physically delivered stock outside the footprint, aggregate builders and a rising structure. Completed extraction/assembly workplaces show existing assigned workers at exterior stations; their tools stop when real operating conditions fail. This does not add population, individual pathfinding or new production rates. Bulk materials, ingots and crates use separate resource metadata. The v0.6 editor compiled; 34 native tests and the 79-stage editor route passed. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) tracks evidence.

## Superseded v0.5 service model

The v0.5 prototype introduced supplied construction, shuttle deployment and a worker charging/maintenance facility before a shared electricity model existed. Its abstract service-only model and fourteen-definition catalog are historical. The current worker service bay remains in Logistics, with staffing and local upkeep; the later shared road grid and batteries now govern its electrical operation. The earlier absence of a power model is not a current design exemption.

See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for version-specific evidence and [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) for current reservations, delivery, construction and save-state contracts.

## Organization

**Confirmed broad direction:** Buildings need at least resource-related, logistics, and defense categories. The current interface separates resource work into Extraction and Production, alongside Logistics and Defense. Logistics includes movement, trade, storage, power and worker charging/service. Storage overlaps functions, and resource or logistics buildings may have secondary defensive capabilities.

**Remaining organization proposal:** Secondary functional tags could supplement the current primary categories. The core combines several capabilities and is selected directly for its commands. Tags are an optional presentation proposal; current catalog entries and shortcuts are defined in [ui.json](../../Interface/ui.json).

| Primary category | Current functions | Possible secondary tags |
| --- | --- | --- |
| Extraction | One deposit-driven Extraction Mine | Storage, power, logistics access |
| Production | Processing and component manufacture | Storage, power, service, defensive |
| Logistics | Local transport, storage, power, external trade, fleet support, and worker service | Storage, repair, charging, manufacturing, defensive |
| Defense | Fixed protection, perimeter control, and protective infrastructure | Sensor, storage, logistics access |

Tags would describe existing capabilities; they would not automatically add weapons, generation or repairs. The prototype already uses local inventories and physical deliveries. Additional transfer-hub types and broader allocation policies remain future design work.

## Established constraints

- One central command center and central zone per player. The core houses central command and the shuttle and moves with relocation. Remote extraction, including risky extensions into empty neighboring sectors, does not establish another core.
- The core's slow universal replicator and a dedicated faster worker factory can both assemble workers from materials and electricity. They share one colony workforce. Surplus workers become physical inactive cargo; configurable reserve targets govern production, reactivation and recycling. Core-exclusive worker production and unconditional deletion of surplus workers are superseded ideas.
- Constructed buildings staff and operate automatically when inputs and labor are sufficient. The player can turn a building off and see demand or unfilled jobs. Repairs and hauling are automatic.
- Resource output is rate-limited. Additional population alone does not bypass extraction ceilings.
- The selected tiered families above have authored level 1–3 upgrade paths. Numerical benefits, costs and additional future families remain provisional.
- Manufactured goods remain physical cargo in the full design. Building placement does not authorize free transfers between distant inventories.

## Current starter functions

The older starter proposal assumed all four industrial materials could be obtained locally. That assumption is superseded: each region contains three distinct standard and two distinct rare deposits selected from the confirmed four-plus-four pool. Finite landed supplies, core replication and external trade must bridge missing local inputs. The following functions are present in the current catalog; their balance and complete bootstrap still require final expanded-version acceptance.

| Building/function | Category | Purpose in the starter loop | Current boundary |
| --- | --- | --- | --- |
| Command core | Central | Starting defense, fusion generation, storage, selectable replication and worker information | A parked shuttle at level 1; finite materials, process time and electricity govern manufacture. |
| Extraction Mine | Resource-related | Gather the raw resource at its bound deposit for local use or sale | One blueprint, at most one live mine per deposit, external per-resource rates; no selectable output or extra deposit. |
| Solar array and road connection | Logistics | Establish a shared generating network | Electricity follows completed connected roads; batteries store charge rather than create it. |
| Trading port | Logistics | Sell available goods, then import missing inputs | Credits start at zero. Orders use physical cargo, timed shipments and paid imports. |
| Materials, electronics and mechanical works | Resource-related | Make alloys, conductors, glass, circuits and parts | Authored recipes are provisional balance; local input stocks and energy are required. |
| Worker service bay and worker factory | Logistics | Support the active workforce and expand worker assembly | Service and manufacture are distinct functions; the factory assembles into the same workforce and inactive-worker inventory. |
| Logistics depot | Logistics; storage | Hold goods for automatic physical delivery | Stored volume and courier mass constrain transfers; no manual hauling orders are required. |
| Defensive tower and sensor | Defense | Extend protection and paid sensor coverage | Four tiered weapon families are confirmed. Damage, ranges, upkeep and bills remain editable prototype balance; full sensor theft remains future work. |

**Starter viability requirement:** Existing workers and finite landed stock must support extraction, a paid road/solar/trade bootstrap and subsequent replacement production. The starting kit and core replicator are selected mechanisms, not open alternatives. No first required facility may depend exclusively on output from that same unfinished facility. A missing local standard material must be obtainable through exports and paid imports without free credits or shared inventories.

Worker service, power, trade and combat factories are now confirmed catalog functions. This does not require constructing every family at the start. Human housing and a separate dedicated repair hub remain unselected; automatic repair alone does not establish another building type.

## Adapting all 16 earlier building functions

This historical crosswalk preserves the original sixteen ideas while stating their current treatment. It supersedes the old table's suggestions that power, charging, trade, dedicated worker production or individual weapon families were undecided. It does not add sixteen extra blueprints beyond the current catalog.

| Earlier proposed function | Current treatment | Status and purpose |
| --- | --- | --- |
| Core | Tiered command core | Confirmed shuttle, fusion power, storage, four large lasers and slow universal replication; exact rates and bills remain provisional. |
| Habitat | Reconsider as a robot service or social space, or omit | Human housing is deferred. Robot accommodation and its satisfaction effects are not selected. |
| Waterworks | Extraction Mine on water | The same mine blueprint gathers this standard resource. Water has industrial uses and does not imply worker drinking needs. |
| Cultivation | Biomass extraction and food manufacture | Biomass is currently a deposit resource. Organic food feeds Rex and can be exported; workers do not eat it. Cultivation as a separate renewable source remains a proposal. |
| Mine / extractor | Single deposit-driven Extraction Mine | Supersedes separate raw-specific extractors. Output follows the chosen deposit from the confirmed three-standard/two-rare regional distribution; rates and bills remain provisional. |
| Refinery | Material processing facilities | Current alloys, conductors and glass chains; quantities and facility balance remain provisional. |
| Chemical plant | Fuel/plastics processing | Current product chains include fuel and plastic pellets, with physical multi-output recipes. Additional chemistry remains proposed. |
| Power plant | Core fusion, tiered solar, fuel generation and batteries | Confirmed shared road-grid operation. Authored output, fuel, idle demand and storage values remain editable balance. |
| Machine works | Component manufacture | Current input source for workers, logistics, repairs and defense; no free remote inventory access. |
| Electronics facility | Circuits and AI-chip manufacture | Current product branches with authored recipes; additional specialist processes remain future design. |
| Advanced assembly | Worker factory and higher-order manufacture | Dedicated faster worker assembly is confirmed alongside the core's slow replicator. Both feed one workforce; advanced products have separate recipes. |
| Depot / logistics | Local storage and automatic deliveries | Implemented local inventory and courier rules; additional transfer-hub types remain proposals. |
| Trade terminal | Level 1–3 trading port | Current timed import/export, credits, physical goods and whole-worker trade; prices and shipment timing remain provisional. |
| Fleet yard | Wheeled, tracked and mech factory families | Current level 1–3 manufacture and outfitting, physical bills and separate combat vehicle state; these do not create colony workers. |
| Defensive emplacement | Laser, kinetic, missile and plasma towers | Confirmed level 1–3 families with hardpoint/loadout data; exact combat balance remains provisional. |
| Barriers / gates | Level 1–3 wall segments | Walls are confirmed physical obstacles with paid construction and upgrades. Gates and additional access-control behavior remain future work. |

The worker charging/service bay and distributed sensors are current functions with operating requirements. A separate repair hub and additional sensor-component facilities are optional future proposals, not prerequisites added by this historical crosswalk.

## What belongs in editable building definitions

**User requirement:** Content and numerical rules must be editable outside the engine's main execution code. Building definitions should reference stable IDs for their category/tags, visual, footprint, compatible resource sites, construction bill, jobs, storage, supported recipes, repair behavior, sensor/defense capabilities, and upgrades. Recipe quantities and time, population rates, costs, and balance belong in external definitions.

The engine supplies generic placement, inventory, production, hauling, and combat mechanisms. New numerical values or another building using those mechanisms should not require recompilation. A genuinely new behavior may require extending the supported mechanism and its validation; a data file is not unlimited executable logic. See [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md).

## Review recommendation

Evaluate the starter chain as a whole before multiplying buildings: can the colony replace workers, repair ordinary damage, and keep materials moving after starting components run out? Then add specialist resource, logistics, and defensive buildings where they create different decisions rather than merely adding another intermediate step.

## Related documents

- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Production Dependencies and Starter Viability](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md)
- [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md)
- [First Playable Scope](FIRST_PLAYABLE_SCOPE.md)
- [Initial Provisional Catalog](PROVISIONAL_CATALOG.md)

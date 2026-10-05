# seige2222 — Building Proposal

[Design index](README.md) · [Status definitions](README.md#design-status)

**Assistant proposal for discussion.** This is a candidate building catalog for a robotic colony. Established functions are identified separately from proposed names, facilities, recipes, and numerical values. The [first playable scope](FIRST_PLAYABLE_SCOPE.md) records the smaller implementation being developed.

## v0.6 presentation metadata

The current source adds explicit indoor/outdoor inventory location and worker-activity presentation to each implemented building. Construction sites show physically delivered stock outside the footprint, aggregate builders and a rising structure. Completed extraction/assembly workplaces show existing assigned workers at exterior stations; their tools stop when real operating conditions fail. This does not add population, individual pathfinding or new production rates. Bulk materials, ingots and crates use separate resource metadata. The v0.6 editor compiled; 34 native tests and the 79-stage editor route passed. The v0.6 Shipping route also passed 79 stages with zero failures and exit 0, recording 831 between-tick courier-motion frames at 1×; [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) tracks evidence.

## Current v0.5 implementation alongside this proposal

The broader catalog below remains proposed. The current user requirement adds physically supplied worker construction, an initial shuttle-deployed command core and a robot charging/maintenance facility. Source implements these with external construction costs, durations, builder counts and support capacities. The `robot_service_bay` is in Logistics with the B, L, C shortcut, one operating job and local component upkeep. Capacity is usable only after construction and staffing. It is an abstract automatic charging/service system; no battery-meter or kW-grid model is implied.

See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for the fourteen implemented definitions and [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) for reservations, delivery, construction and save-state contracts. Other proposed buildings and exact full-game balance are not automatically approved by this prototype.

## Organization

**Confirmed broad direction:** Buildings need at least resource-related, logistics, and defense categories. Resource-related buildings include gathering and storage; logistics includes movement, trade, storage, and possible robot charging or repair facilities. Storage overlaps categories. Resource and logistics buildings may have secondary defensive capabilities.

**Proposed organization:** Give a building one primary category and secondary functional tags. This makes construction menus readable without forcing each building to have only one role. The core can remain a central special entry because it combines multiple functions. Neither this menu arrangement nor the assignments below are settled gameplay rules.

| Primary category | Candidate functions | Examples of secondary tags |
| --- | --- | --- |
| Resource-related | Extraction, processing, component manufacture, and industrial storage | Storage, power, service, sensor, defensive |
| Logistics | Local transport, transfer storage, external trade, fleet support, and possible robot service | Storage, repair, charging, manufacturing, defensive |
| Defense | Fixed protection, perimeter control, and protective infrastructure | Sensor, storage, logistics access |

Tags describe capabilities; they do not automatically add weapons, power generation, repairs, or other functions. Storage's exact ownership and routing rules still need design.

## Established constraints

- One central command center and central zone per player. The core houses central command and the shuttle and moves with relocation. Remote extraction, including risky extensions into empty neighboring sectors, does not establish another core.
- The core automatically produces robots to fill jobs and reduces population when demand falls. Production has a maximum rate and input requirements. **Core-exclusive robot production remains provisional**, separately from the confirmed one-core limit.
- Constructed buildings staff and operate automatically when inputs and labor are sufficient. The player can turn a building off and see demand or unfilled jobs. Repairs and hauling are automatic.
- Resource output is rate-limited. Additional population alone does not bypass extraction ceilings.
- The core and most major buildings should upgrade, with economic, administrative, and defensive benefits. Exact upgrade paths are open.
- Manufactured goods remain physical cargo in the full design. Building placement does not authorize free transfers between distant inventories.

## Practical starter lineup — proposal

Use a small group of reusable functions to test the four-material industrial proposal. These names are design suggestions, not a promise that every entry is already present in the executable. See [Resource Proposal](RESOURCE_PROPOSAL.md) for the proposed iron, copper, silica, and carbon starter base.

| Candidate building/function | Suggested category | Purpose in the starter loop | Boundary or unresolved detail |
| --- | --- | --- | --- |
| Command core | Central | Starting defense, colony inventory access, robot manufacture, and workforce information | Do not assume unlimited stocks, free repairs, a selected energy source, or a complete shuttle system. |
| Resource extractor | Resource-related | Acquire the starter materials from world sources | Reusing a common extractor design for multiple deposits is a proposal. Site compatibility and rate limits belong in data. |
| Materials processor | Resource-related | Make structural material, conductors, and other selected processed inputs | Combining early refining functions reduces the number of prerequisites to test; material-specific facilities can follow later. |
| Electronics works | Resource-related | Produce the selected basic control-circuit path | The four-material starter alternative avoids requiring biomass or rare materials for initial electronics; recipes are fictional abstractions. |
| Machine works | Resource-related | Turn processed materials and circuits into actuators, parts, and equipment | Exact division of recipes between this facility and the processor remains adjustable. |
| Logistics depot | Logistics; storage | Organize storage and automatic physical deliveries | Carrier production, staffing, service radius, and route selection remain open; no manual hauling orders are required. |
| Defensive emplacement | Defense | Extend automatically executed defense beyond the starting core | Range, damage, target selection, operating costs, and construction costs need prototype values, not final balance. |
| Sensor installation | Defense or logistics; sensor | Make the cost and vulnerability of remote coverage visible | Its menu category is proposed. A tower is not automatically an additional manufacturing facility, and full sensor theft remains later work. |

**Starter viability recommendation:** Existing robots and finite starting stock should support the first extraction, processing, and delivery chain. Do not make the first machine works require a component obtainable only from that same unfinished machine works. Whether this is solved by carried components, built-in core functions, or another bootstrap is a prototype decision to record explicitly.

Charging stations, a standalone power plant, robot accommodation, a trade terminal, and a fleet yard need not all appear in the first experiment. Add them when their underlying system creates a useful player choice. Automatic repair alone does not prove that a dedicated repair building is necessary.

## Adapting all 16 earlier building functions

This preserves the original catalog's ideas while distinguishing deferred human functions from robot-appropriate proposals. The mapping does not approve sixteen separate buildings.

| Earlier proposed function | Suggested treatment | Status and purpose |
| --- | --- | --- |
| Core | Retain the central core | Central command, shuttle location, defense, and job-driven population adjustment are established; individual processes remain open. |
| Habitat | Reconsider as a robot service or social space, or omit | Human housing is deferred. Robot accommodation and its satisfaction effects are not selected. |
| Waterworks | Industrial water extraction/processing | Candidate resource-related facility for fluids, cooling, or chemicals; robots do not acquire a drinking-water need by implication. |
| Cultivation | Industrial biomass cultivation | Candidate resource-related facility for polymer, lubricant, or fuel feedstock; human food production stays deferred. |
| Mine / extractor | Generic or resource-specific extractors | Extraction is established; exact structures and deposit rules remain proposed. |
| Refinery | Starter materials processor, later specialist refining | Candidate place for base materials and resource-specific processing. |
| Chemical plant | Later chemical works | Candidate branch for polymers, fuels, fluids, and catalysts; no exact chemistry or consumption recipe is approved. |
| Power plant | Later standalone generation, if adopted | Power infrastructure and charging need a coherent design before this becomes mandatory. The proposed starter core generator is not a confirmed final system. |
| Machine works | Starter mechanical manufacturing | Candidate shared input source for robots, logistics, repair, and defense. |
| Electronics facility | Starter electronics works and possible later variants | Candidate circuits and sensor production; the core's possible unique lithography capability does not settle this facility's detailed processes. |
| Advanced assembly | Later higher-order equipment assembly | Candidate combination of manufactured inputs. It does not establish a second producer of population. |
| Depot / logistics | Local depots and possible transfer hubs | Automatic physical hauling and storage are established; distinct hub types and exact allocation rules remain proposals. |
| Trade terminal | Later external trade access | Trade is established direction; terminal behavior, prices, contracts, and shipment handling remain open. |
| Fleet yard | Later ground-vehicle/mech manufacture and servicing | Fleets use land vehicles and mechs. Recipes, repair location, outfitting, and relation to robot population remain open. |
| Defensive emplacement | Starter fixed defense and later specialist forms | Fixed defense is established; individual weapon families and upgrades remain proposed. |
| Barriers / gates | Later perimeter structures | Candidate protection and access control; path blocking, breaching, and vehicle passage need design. |

Possible logistics additions include charging and repair facilities; distributed sensors also need installation and upkeep. These are capabilities to evaluate, not mandatory extra buildings. Sensor components could be made by existing electronics and assembly functions.

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

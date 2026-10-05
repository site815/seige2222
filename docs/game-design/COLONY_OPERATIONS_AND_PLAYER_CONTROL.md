# seige2222 — Colony Operations and Player Control

[Design index](README.md) · [Status definitions](README.md#design-status)

How a constructed colony operates with minimal routine adjustment. Automatic behavior and the single-player setup flow are established; prototype algorithms and numerical values remain distinct from final design decisions.

## Current v0.8 workers, plots and construction

The player-facing population term is **workers**; the population remains robotic. [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns current behavior and pending verification. The latest expansion adds tiered facilities, selectable core replication, a worker factory, physical inactive-worker storage and export targets. Square reserved plots govern placement/routing and vegetation clearing; grading and foundations use the actual built footprint. The core reserves three times its level-one width and nine times its area for its three authored levels.

Orders reserve eligible physical stock after operating buffers. Couriers follow access-port routes around plots; aggregate construction crews must arrive before work. Delivered stock becomes installed material proportionally through Foundations, Frame and utilities, Enclosure and equipment, and Commissioning. Partial delivery can advance construction. The core's carried kit bootstraps its deployment, with an explicit defensive/sensor exception while other ordinary operations await completion. Completed road/rail/vacuum links provide 2×/4×/8× transport; upgrades keep earlier service during construction.

General actions use Menu; the removed Colony overlay no longer carries escape. Manual shuttle launch requires selecting the live own command core. Speed cycles include Paused/1×/5×/10× with Space restoring the previous nonzero rate. New facility upgrades do not add individual worker commands or a full traffic simulation.

## Historical v0.6 visible work and physical inventories

**Implemented and locally verified in v0.6 Shipping:** Existing assigned builders and operating workers have aggregate exterior workstations and tool-fetch/work cycles. The renderer does not create extra population, a second job allocator or independently simulated worker paths. Representative service robots likewise visualize supported population; counting visible models is not a census. Sensor/fog ownership checks apply before these models or nearby piles appear.

In v0.6, couriers carried visible payloads matching their real resource and amount. Stock left its source on dispatch and arrived only with the courier. Displayed couriers clamped to exterior loading ports while the older simulation retained center-based endpoints and core-only construction sourcing. **Current v0.8 supersedes those routing and source restrictions:** declared access ports are actual route endpoints, and construction couriers can collect eligible surplus from completed living buildings, including depots, after operating buffers and outstanding construction reservations. Delivery and installed material remain physical; visual animation never grants or teleports inventory.

Resource definitions choose bulk rock, ingot or crate presentation. Buildings explicitly declare indoor or outdoor stock. Outdoor piles reflect local inventory and disappear at zero; construction piles show the uninstalled fraction of material already delivered. The committed construction ledger includes material incorporated into the rising building until completion. Nothing appears on the ground for an undelivered bill. During initial descent, the deployment kit and crew remain visually aboard the shuttle before appearing on site.

`HasActiveWork` uses the simulation's actual enabled/finished/staffed state, inputs, output space, robot-assembly demand/support and local service supply. Stalled production or maintenance leaves tools idle. Courier and bug positions interpolate between ID-matched fixed-step snapshots; construction reveal and worker/service movement use interpolated simulation time. Motion updates at rendering frequency and freezes on pause, while rates, capacity and actual deliveries retain the unchanged fixed-step simulation.

Completed outdoor stockyards place only present resources and find clear positions against known living building footprints and earlier piles. Exterior workstations use the same known bounds and avoid visible piles. Hidden neighbor structures are excluded from these decisions, and a crowded yard may omit a representative pile without changing its inventory. Scaffolds, stockpieces and visible robots are illustrative; exact item and staffing counts come from the dossier rather than counting meshes. The visible construction sequence does not add part-by-part assembly or individual worker pathfinding.

The exact rule schema and passing regression coverage are recorded in [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md). The v0.6 editor compiled, all 34 native tests passed cleanly (zero warnings, failed or unrun), and the 79-stage editor interaction route passed with zero failures. Its motion check recorded 413 courier frames between simulation ticks at 1× speed. The Shipping route separately passed 79 stages with zero failures and exit 0, recording 831 courier frames between ticks at 1× (`Saved/packaged-v0.6.0-UiSmoke-verification.json`); the v0.5 evidence below remains historical.

## Historical v0.5 construction and service operation

**Confirmed new requirement:** Buildings need worker construction and physically delivered materials. Initial command-core deployment comes from the orbital shuttle; robot charging/maintenance facilities must support population expansion.

**Historical v0.5 implementation:** A new order reserved available core inventory after operating buffers, sends physical couriers and waits for the full bill before builders work. Sites contribute temporary builder jobs. External `staffing_priority` orders both construction and operating jobs; current priorities preserve command, services, defenses and sensors ahead of general industry, with stable order for ties. Sites do not produce, repair, sense or fire until complete. Disabling a site pauses work but holds its reservation; there is no cancellation/refund command. Individual builder navigation remains outside the aggregate workforce model.

The core's carried deployment kit and initial crew bootstrap the first service expansion. Completed, enabled and staffed service buildings add support capacity and use local delivered maintenance components. Assembly needs both open jobs and available support. Unsupported existing robots remain but work at reduced efficiency; automatic job-driven retirement is independent. These prototype algorithms use external data and are included in the verified local v0.5 package. The final 29-test native suite and 63-stage packaged route passed; [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records their evidence and boundaries.

## Scenario setup and player control

**Confirmed direction:** A main menu leads to single-player scenario setup, settings, credits, and local loading. Setup presents the full 3×3 neighborhood with empty, starting-AI, or developed-AI neighbors. The center can be a player colony or an AI colony for observation. For human play, time stays paused while the player surveys the larger map and chooses the command-core location. Play begins after a valid landing selection.

**Current implementation:** Only active, unpaused play advances the center and neighbor simulations. Main menu, scenario setup, landing, settings, and credits stop their clocks. Current menu/display controls are documented in [Interface and Controls](INTERFACE_AND_CONTROLS.md); its final verification is separate from the earlier four-quality-level v0.5 settings. The credits and construction categories are externally editable in [Interface/ui.json](../../Interface/ui.json); AI priorities and developed starting stock are separately editable in [AIFILES](../../AIFILES/README.md).

Observer mode allows watching AI-controlled colonies, camera movement, pause, and saving; it disables player construction and colony commands. A human scenario retains one central core. These local controls do not establish pause, offline progression, or save ownership for future persistent multiplayer. See [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md).

## Buildings and automatic work

**Confirmed direction:**

- Buildings require workers, and robots automatically fill available jobs.
- Once a building is constructed and enabled, it should operate automatically when it has enough workers and required resources.
- Production and movement of goods are automatic. Routine operation does not require issuing individual production or hauling orders.
- The player can turn a building off.
- Manual adjustment should be minimal. The main choices remain what to build, what capabilities to establish, and which activities to keep operating.
- All repairs are automatic, as established in the [core-loop design](CORE_LOOP_AND_FIRST_PLAYABLE.md).

Automatic operation does not remove physical goods or transport. Inputs and outputs remain world objects, and the [physical logistics rules](FLEETS_AND_LOGISTICS.md) still apply.

## Workforce information

**Confirmed direction:** The player should be able to see worker demand and unfilled jobs, through an open-jobs or required-workers metric. Staffing should remain understandable even though assignment is automatic.

**Current prototype:** The interface reports colony population/jobs and building staffing and operating status. The building dossier covers costs, construction duration/progress and builders, health, staffing priority, production, local inventories, incoming cargo, service capacity/upkeep, repairs, weapon damage/reload/DPS, and power. Required inputs stay visible at zero stock, and unarmed/zero-power values are explicit. The final v0.5 native suite passed 29 tests (28 clean and one editor background HTTP-warning success), and the packaged interaction route passed 63 stages with zero failures. The captured service bay was empty because it had no assigned supported robots; occupied-bay visuals were not verified. Native save coverage and packaged observations remain distinct in the [development report](../DEVELOPMENT_REPORT.md). Explanations of the full-game allocation policy under scarcity remain open.

## Population, throughput, and consumption

**Confirmed direction:** Core or dedicated-factory production uses materials and energy to fill open jobs. Surplus workers become inactive physical stock, with configurable colony and trading-port reserve targets. Surplus can be recycled manually, or automatically for missing parts or insufficient storage, at 1 kWh per worker. Automatic staffing remains the default; reserve targets do not require individual worker management. Exact rates, material returns and capacity remain editable prototype balance.

**Confirmed direction:** More population provides more potential labor and can support defense through numbers, but also creates more consumption. Population alone should not multiply resource extraction: extraction remains constrained by defined rates per time unit.

A populous colony and a small specialized colony should both be viable strategies. A low-population settlement may be an outpost or the main colony. This is a design goal, not a selected class system or a claim that balance is already established; see [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md).

## Current prototype policies

Enabled buildings require staffing and power, couriers physically deliver inputs between local inventories, and repairs consume local materials. The new population policy is `store_inactive`: job demand plus configured reserves drives worker manufacture, and a stored-worker item has one physical location. Reactivation consumes that item; disassembly returns the explicit parts bill. This replaces historical retirement without refunds. Local support/upkeep and allocation priorities remain external balance, not a complete happiness system.

Construction reserves eligible surplus inventories without removing goods at order time. Physical couriers deliver to the site; arrived crews consume available materials incrementally into installed stock. Local AI uses the same commands, worker requirements, services, recipes, logistics, repairs and threats. Its ordered plan waits for required buildings to finish and be staffed before later spending, with prerequisite recovery after losses. It receives no recurring free inventory. The current privateering bridge lets a player-owned fleet travel into an occupied neighbor, fight and capture physically accessible cargo, then carry surviving units and loot home. Each colony retains its own inventory and power grid. Ordinary neighbor-to-neighbor trade and general cross-sector supply convoys remain unimplemented; external trading-port shipments are a separate system.

Road-connected power and batteries have an explicit energy ledger. Idle use is kW over elapsed simulation seconds; recipes and actions charge kWh per transaction. Defense/fleet source uses separate combat catalogs, finite vehicle batteries and ammunition, paid fabrication/refitting and persisted mission state. Expanded release acceptance is pending; earlier building-versus-bug test results do not verify it.

## Decisions still needed for the full game

- How robots choose and switch jobs, and how scarce workers or inputs are allocated among enabled buildings.
- Whether an understaffed building produces partially or waits until its requirements are met.
- How hauling work is staffed and how destinations are selected, while keeping transport automatic and physical.
- Final throughput, consumption and outage balance for selectable recipes and dedicated production.
- The exact effects of disabling a building on its assigned workers, inputs, queued work, and stored output.
- How job demand is counted for population production and reduction, including treatment of disabled buildings, temporary resource shortages, hauling, repair work, and defense roles.
- Final inactive-worker capacity, response delays and recovered parts; storage and disassembly are now selected mechanisms.
- Final construction, automatic-repair and upkeep balance beyond the implemented external prototype values.
- Which capabilities make a specialized low-population colony competitive, and how labor relates to defensive participation.

None of these open details establishes manual worker assignment, required production queues, or new priority controls. No additional control should be assumed from the presence of an open question.

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md)

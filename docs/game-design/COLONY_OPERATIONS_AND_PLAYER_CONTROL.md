# seige2222 — Colony Operations and Player Control

[Design index](README.md) · [Status definitions](README.md#design-status)

How a constructed colony operates with minimal routine adjustment. Automatic behavior and the single-player setup flow are established; prototype algorithms and numerical values remain distinct from final design decisions.

## v0.5 construction and service operation

**Confirmed new requirement:** Buildings need worker construction and physically delivered materials. Initial command-core deployment comes from the orbital shuttle; robot charging/maintenance facilities must support population expansion.

**Current source implementation:** A new order reserves available core inventory after operating buffers, sends physical couriers and waits for the full bill before builders work. Sites contribute temporary builder jobs. External `staffing_priority` orders both construction and operating jobs; current priorities preserve command, services, defenses and sensors ahead of general industry, with stable order for ties. Sites do not produce, repair, sense or fire until complete. Disabling a site pauses work but holds its reservation; there is no cancellation/refund command. Individual builder navigation remains outside the aggregate workforce model.

The core's carried deployment kit and initial crew bootstrap the first service expansion. Completed, enabled and staffed service buildings add support capacity and use local delivered maintenance components. Assembly needs both open jobs and available support. Unsupported existing robots remain but work at reduced efficiency; automatic job-driven retirement is independent. These prototype algorithms use external data and are included in the verified local v0.5 package. The final 29-test native suite and 63-stage packaged route passed; [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) records their evidence and boundaries.

## Scenario setup and player control

**Confirmed direction:** A main menu leads to single-player scenario setup, settings, credits, and local loading. Setup presents the full 3×3 neighborhood with empty, starting-AI, or developed-AI neighbors. The center can be a player colony or an AI colony for observation. For human play, time stays paused while the player surveys the larger map and chooses the command-core location. Play begins after a valid landing selection.

**Current implementation:** Only active, unpaused play advances the center and neighbor simulations. Main menu, scenario setup, landing, settings, and credits stop their clocks. Settings provide four graphics-quality levels and windowed/fullscreen selection. The credits and construction categories are externally editable in [Interface/ui.json](../../Interface/ui.json); AI priorities and developed starting stock are separately editable in [AIFILES](../../AIFILES/README.md).

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

**Confirmed direction:** The core automatically produces robots to fill open jobs and automatically reduces population when job demand falls. The player does not need to maintain a population target. How surplus robots are removed, how quickly demand changes take effect, and which work counts toward demand remain open.

**Confirmed direction:** More population provides more potential labor and can support defense through numbers, but also creates more consumption. Population alone should not multiply resource extraction: extraction remains constrained by defined rates per time unit.

A populous colony and a small specialized colony should both be viable strategies. A low-population settlement may be an outpost or the main colony. This is a design goal, not a selected class system or a claim that balance is already established; see [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md).

## Current prototype policies

Enabled buildings require full staffing to operate, couriers physically deliver inputs between local inventories, and damage is repaired automatically using the implemented repair inputs. Job demand drives core robot assembly and later surplus retirement, with a configurable population minimum. The assembly-input buffer reserves materials for future robots; it does not add extra robots above job demand. Retirement currently gives no resource refund. Local core/service-bay upkeep, external automatic staffing priorities, and these population choices are editable prototype policies, not settled full-game needs or satisfaction systems.

Construction reserves core inventory without removing it at order time. Couriers deliver the complete bill to the site before builders advance assembly; completion consumes that site inventory into the structure. Local AI uses the same construction commands, workers, services, recipes, logistics, repairs and threats. Its ordered plan waits for required buildings to finish and be staffed before later spending. It receives no recurring free inventory. Independent neighbor colonies cannot yet exchange cargo or attack each other.

v0.4 building defenses execute data-defined shots and reloads; operating efficiency slows reload progress. The displayed nominal DPS is derived from shot damage and reload time. All buildings currently show 0 kW usage and generation because no separate grid is simulated. These explicit statistics explain the implemented slice without selecting a final energy system or full-game combat balance.

## Decisions still needed for the full game

- How robots choose and switch jobs, and how scarce workers or inputs are allocated among enabled buildings.
- Whether an understaffed building produces partially or waits until its requirements are met.
- How hauling work is staffed and how destinations are selected, while keeping transport automatic and physical.
- How recipes are selected when a building can make multiple products.
- The exact effects of disabling a building on its assigned workers, inputs, queued work, and stored output.
- How job demand is counted for population production and reduction, including treatment of disabled buildings, temporary resource shortages, hauling, repair work, and defense roles.
- How surplus robots are removed and whether any resources are recovered; the mechanism and rates have not been chosen.
- Final construction, automatic-repair and upkeep balance beyond the implemented external prototype values.
- Which capabilities make a specialized low-population colony competitive, and how labor relates to defensive participation.

None of these open details establishes manual worker assignment, required production queues, or new priority controls. No additional control should be assumed from the presence of an open question.

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md)

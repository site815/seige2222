# seige2222 — Colony Operations and Player Control

[Design index](README.md) · [Status definitions](README.md#design-status)

How a constructed colony operates with minimal routine adjustment. Automatic behavior and the single-player setup flow are established; prototype algorithms and numerical values remain distinct from final design decisions.

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

**Current prototype:** The interface reports colony population/jobs and building staffing and operating status. Final presentation and explanations of allocation under scarcity remain open; check readability in the running game rather than treating the presence of a metric as sufficient.

## Population, throughput, and consumption

**Confirmed direction:** The core automatically produces robots to fill open jobs and automatically reduces population when job demand falls. The player does not need to maintain a population target. How surplus robots are removed, how quickly demand changes take effect, and which work counts toward demand remain open.

**Confirmed direction:** More population provides more potential labor and can support defense through numbers, but also creates more consumption. Population alone should not multiply resource extraction: extraction remains constrained by defined rates per time unit.

A populous colony and a small specialized colony should both be viable strategies. A low-population settlement may be an outpost or the main colony. This is a design goal, not a selected class system or a claim that balance is already established; see [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md).

## Current prototype policies

Enabled buildings require full staffing to operate, couriers physically deliver inputs between local inventories, and damage is repaired automatically using the implemented repair inputs. Job demand drives core robot assembly and later surplus retirement, with a configurable population minimum. The assembly-input buffer reserves materials for future robots; it does not add extra robots above job demand. Retirement currently gives no resource refund. Centralized upkeep, the staffing algorithm, and these population choices are editable prototype policies, not settled robot-needs or satisfaction systems.

Construction immediately spends core inventory. Construction hauling is still missing even though production inputs and outputs use physical delivery. Local AI issues the same normal construction commands and then relies on the same workers, recipes, logistics, repairs, and threats. It does not receive recurring free inventory. Independent neighbor colonies cannot yet exchange cargo or attack each other.

## Decisions still needed for the full game

- How robots choose and switch jobs, and how scarce workers or inputs are allocated among enabled buildings.
- Whether an understaffed building produces partially or waits until its requirements are met.
- How hauling work is staffed and how destinations are selected, while keeping transport automatic and physical.
- How recipes are selected when a building can make multiple products.
- The exact effects of disabling a building on its assigned workers, inputs, queued work, and stored output.
- How job demand is counted for population production and reduction, including treatment of disabled buildings, temporary resource shortages, hauling, repair work, and defense roles.
- How surplus robots are removed and whether any resources are recovered; the mechanism and rates have not been chosen.
- Construction work, automatic-repair costs and rates, and robot upkeep requirements.
- Which capabilities make a specialized low-population colony competitive, and how labor relates to defensive participation.

None of these open details establishes manual worker assignment, required production queues, or new priority controls. No additional control should be assumed from the presence of an open question.

## Related documents

- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md)

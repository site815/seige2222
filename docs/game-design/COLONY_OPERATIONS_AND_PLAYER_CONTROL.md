# seige2222 — Colony Operations and Player Control

[Design index](README.md) · [Status definitions](README.md#design-status)

How a constructed colony operates with minimal routine adjustment. Automatic behavior is established; allocation algorithms, exact requirements, and interface layouts remain open.

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

**Open:** Exact presentation, whether metrics are colony-wide, per building, or both, and how shortages or blocked work are explained.

## Population, throughput, and consumption

**Confirmed direction:** The core automatically produces robots to fill open jobs and automatically reduces population when job demand falls. The player does not need to maintain a population target. How surplus robots are removed, how quickly demand changes take effect, and which work counts toward demand remain open.

**Confirmed direction:** More population provides more potential labor and can support defense through numbers, but also creates more consumption. Population alone should not multiply resource extraction: extraction remains constrained by defined rates per time unit.

A populous colony and a small specialized colony should both be viable strategies. A low-population settlement may be an outpost or the main colony. This is a design goal, not a selected class system or a claim that balance is already established; see [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md).

## Decisions still needed

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

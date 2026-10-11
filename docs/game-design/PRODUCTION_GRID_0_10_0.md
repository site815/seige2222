# Production grid — 0.10.0 candidate

Provisional costs are physical kilograms. Bills conserve each manufactured chassis/module mass exactly. Electricity is a separate transaction; battery charge starts empty on newly manufactured vehicles. The landed transport has an explicit precharged starting manifest. No production grant is created by a progress animation.

The command core manufactures workers only. Wheeled, tracked and mech chassis require their corresponding specialist factory. Factory levels 1/2/3 support chassis tiers 1–2 / 3 / 4. Armed vehicles are no longer part of the landing kit.

## Vehicle chassis

| Chassis | Mass kg | Alloy | Parts | Circuits | Batteries | AI chips | Crystal | Fusion assemblies | Base time min | Energy kWh |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Small wheeled | 200 | 140 | 30 | 10 | 20 | 0 | 0 | 0 | 10 | 20 |
| Medium wheeled | 1600 | 1088 | 224 | 96 | 160 | 32 | 0 | 0 | 30 | 100 |
| Large wheeled | 12800 | 7680 | 1792 | 640 | 1280 | 256 | 128 | 1024 | 60 | 500 |
| Behemoth wheeled | 102400 | 56320 | 15360 | 5120 | 10240 | 3072 | 2048 | 10240 | 180 | 2000 |
| Small tracked | 200 | 140 | 30 | 10 | 20 | 0 | 0 | 0 | 12 | 20 |
| Medium tracked | 1600 | 1088 | 224 | 96 | 160 | 32 | 0 | 0 | 36 | 100 |
| Large tracked | 12800 | 7680 | 1792 | 640 | 1280 | 256 | 128 | 1024 | 72 | 500 |
| Behemoth tracked | 102400 | 56320 | 15360 | 5120 | 10240 | 3072 | 2048 | 10240 | 216 | 2000 |
| Small mech | 200 | 140 | 30 | 10 | 20 | 0 | 0 | 0 | 14 | 20 |
| Medium mech | 1600 | 1088 | 224 | 96 | 160 | 32 | 0 | 0 | 42 | 100 |
| Large mech | 12800 | 7680 | 1792 | 640 | 1280 | 256 | 128 | 1024 | 84 | 500 |
| Behemoth mech | 102400 | 56320 | 15360 | 5120 | 10240 | 3072 | 2048 | 10240 | 252 | 2000 |

Transport hauler: 1,000 kg body; 800 kg alloy + 100 kg parts + 20 kg circuits + 80 kg batteries; 60 kWh fabrication transaction; 30 minutes before factory/crew multipliers. Capacity is 2,000 kg and 3,000 L simultaneously. No driver, weapon, worker job or build capability. Cargo loading and unloading each require an actual endpoint worker for 8 seconds per equivalent 40 kg / 60 L drone load.

## Weapon modules

| Module | Mass kg | Alloy | Parts | Circuits | Batteries | AI chips | Crystal | Fusion assemblies |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Small laser | 40 | 28.8 | 7.2 | 2.4 | 1.6 | 0 | 0 | 0 |
| Medium laser | 320 | 195.2 | 64 | 22.4 | 22.4 | 6.4 | 9.6 | 0 |
| Large laser | 2560 | 1228.8 | 512 | 179.2 | 204.8 | 76.8 | 102.4 | 256 |
| Small rail cannon | 40 | 32 | 6 | 2 | 0 | 0 | 0 | 0 |
| Medium rail cannon | 320 | 230.4 | 57.6 | 16 | 6.4 | 9.6 | 0 | 0 |
| Large rail cannon | 2560 | 1536 | 512 | 128 | 76.8 | 76.8 | 25.6 | 204.8 |
| Small guided missile launcher | 40 | 32 | 6 | 2 | 0 | 0 | 0 | 0 |
| Medium guided missile launcher | 320 | 230.4 | 57.6 | 16 | 6.4 | 9.6 | 0 | 0 |
| Large guided missile launcher | 2560 | 1536 | 512 | 128 | 76.8 | 76.8 | 25.6 | 204.8 |
| Small plasma projector | 40 | 28.8 | 7.2 | 2.4 | 1.6 | 0 | 0 | 0 |
| Medium plasma projector | 320 | 195.2 | 64 | 22.4 | 22.4 | 6.4 | 9.6 | 0 |
| Large plasma projector | 2560 | 1228.8 | 512 | 179.2 | 204.8 | 76.8 | 102.4 | 256 |

Module bills are added to the selected chassis bill. Ammunition remains separate physical cargo. Large weapon cost includes fusion assemblies in every family, including powered targeting/launch systems for kinetic and missile weapons. Removed modules are currently scrapped without a material refund.

## Integration status

Rules and source are integrated into the v0.10.0 Shipping build. Configuration validation, seven focused native tests and the bounded packaged smoke passed; see the [verification record](../verification/v0.10.0.json). The tables below establish material/dependency arithmetic, not long-session economic balance.

Building `cost` values describe the cumulative material inventory in a finished building at that level. `upgrade_cost` is the additional bill paid when advancing to the next level; the destination building's total cost equals the prior total plus that upgrade bill. Do not charge both totals again during an upgrade.

## Finite landing-kit bootstrap

This is a material and dependency proof against the current candidate JSON, not a completed pacing playtest. The loose kit is **9,570 kg / 3,869.5 L** and starts with **zero credits**. Five installed 80 kg workers, the 1,000 kg transport, ship equipment and 20 kg of shuttle-deployment consumables are separate, explicit landing hardware/stock. They are not free additions to the loose kit later in play.

| First-stage construction | Alloy kg | Conductors kg | Substrates kg | Circuits kg | Parts kg | Batteries kg |
|---|---:|---:|---:|---:|---:|---:|
| Solar farm | 400 | 40 | 100 | 10 | 0 | 25 |
| Trading port | 800 | 60 | 40 | 20 | 80 | 0 |
| Charging / maintenance bay | 600 | 40 | 20 | 10 | 80 | 80 |
| One extraction mine | 500 | 40 | 20 | 10 | 80 | 0 |
| 500 m of basic road / power grid | 200 | 100 | 0 | 0 | 0 | 0 |
| **Total: 3,355 kg** | **2,500** | **280** | **180** | **50** | **240** | **105** |

Every line is covered by the starting kit. This leaves **6,215 kg** of loose stock. These buildings plus the core need ten permanent workers; manufacturing five more workers converts another **400 kg** into owned bodies (300 kg parts, 60 kg alloy, 20 kg circuits and 20 kg batteries), leaving **5,815 kg** of loose stock. The core pays **4 kWh per worker** and a base ten minutes per worker before staffing/interruptions; its worker-only assembler creates no industrial materials.

1. Land near one accessible deposit. Use the five landed workers to deploy the shuttle and build the service bay, first powered road connections, mine, trading port and solar farm in stages. The 500 m allowance is a budgeted example: longer routes need extra paid road materials and time. The core produces replacement/additional crew from the finite kit as workers take permanent assignments.
2. Establish power before operation. The core provides 30 kW continuously and starts with 20 kWh in its battery; the solar farm adds up to 25 kW in daylight. Core, mine, port, service bay and 500 m of road have a combined **4.25 kW** passive load. Worker charging, manufacturing, trade and weapons draw additional real energy and can delay activity; neither the night nor a temporarily empty battery grants free production.
3. Export the mine's actual output through the staffed trading port. All four standard deposit types can be sold, so a region missing one of them is still able to trade. At the provisional prices, 1,000 kg of metal ore/biomass/silica or 1,000 L of water earns **0.1 credits**, enough to import 100 kg of parts or approximately 166.7 kg of alloy. The level-one port carries at most 1,000 kg per 120-second shipment and consumes 0.25 kWh per shipment; transport, loading, production and stock availability remain additional limits. No starting credits or core material synthesis are required to open this exchange.
4. Use export proceeds and the remaining kit to add appropriate local processing, one dependency at a time: ore to alloy/conductors, silica to substrates, biomass plus water to fuel/plastic, then circuits, parts and batteries. Import missing raw inputs or intermediate products rather than pretending every region has every deposit. The finite kit is not intended to purchase the entire industrial tree simultaneously.
5. Expand storage, generation and specialist factories before military manufacture. AI chips require rare inputs; large/behemoth chassis and large modules also require fusion assemblies. Transport fabrication, all weapon work and all armed chassis production remain at specialist factories. The core's only manufactured output is workers throughout this progression.

The arithmetic establishes an affordable path to renewable export income for any allowed three-of-four standard-resource draw. Travel distance, permanent-crew allocation, charging queues, threat pressure and shipment pacing still require sustained balance playtesting beyond the focused checks; this proof does not guarantee uninterrupted operation or a fixed time to industrial self-sufficiency.

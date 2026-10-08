# seige2222 — Resource Proposal

[Design index](README.md) · [Resource rules](RESOURCES_AND_INDUSTRY.md) · [Product and recipe proposal](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md)

**Confirmed direction plus provisional balancing.** The four-standard/four-rare pool below supersedes the earlier twelve-raw proposal and four-material starter guarantee. The current [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md) supplies the accepted provisional 80 kg worker, 9.57 t loose kit and tonne-scale bills while retaining the same 22 cargo types. All 24 no-threat starter compositions passed a bounded native run; broader runtime/release acceptance is incomplete. The [v0.8.1 verification record](../verification/v0.8.1.json) remains historical evidence for that earlier single-mine/grouped-HUD package. Numerical recipes, prices and rates remain provisional.

## Confirmed resource pool and placement

Each region contains **exactly three standard deposits and two rare deposits**, with **no duplicate resource type in the region**. For now, draw three different types from the four standard types and two different types from the four rare types below. Do not add more resource types to the current pool.

Deposits are randomized inside the **inner 75% of the region's area**. The current implementation uses a centered square with half-width `sector_half_width × sqrt(0.75)`, approximately 86.6% of the sector side length. A seeded, separated placement pass chooses distinct types; each occupied region receives a different derived seed. Saves retain the generated nodes and seed. Minimum separation is provisional external data; spatial and long-term economic fairness still need verification.

| Confirmed class | Confirmed resource | Proposed industrial purpose |
| --- | --- | --- |
| Standard | Water | Processing, cooling and organic feedstock treatment; not a worker drinking need. |
| Standard | Metal ore | A mixed industrial ore family supplying construction metals and conductors. |
| Standard | Silica | Industrial glass, insulating materials and electronic substrates. |
| Standard | Biomass | Fuel, bio-derived polymers, Rex's organic food and food exports. |
| Rare | Rare metals | Precision electronics, specialty alloys and advanced equipment. |
| Rare | Radioactive ore | A candidate nuclear-energy or isotope branch; not automatically fusion fuel. |
| Rare | Crystalline material | High-purity optical/electronic or advanced-power components. Exact composition remains fictional/abstract. |
| Rare | Hydrocarbons | A concentrated alternative feedstock for fuel and plastic pellets. |

These are **gameplay resource families**, not a mineralogical taxonomy or purity specification. Metal ore combines several useful metals so the design need not track iron, copper and every alloying element separately. Rare metals and crystalline material similarly cover specialist feedstocks. Rarity describes availability; it does not by itself determine loot tier, product quality or whether a material is mandatory for every advanced recipe.

## Confirmed trade, credits and energy

- **Galactic credits are used only for external trade.** Local construction and production consume physical goods, labor and energy rather than local credit wages or money fees.
- External trade uses a **trading port upgradeable through levels 1–3**. The current provisional capacities are **1,000 / 2,000 / 3,000 kg per shipment**, with a nominal 120-second flight and 0.25 kWh departure cost at each level. These replace the older 100/200/300 kg limits to test freight against tonne-scale building bills; actual staffing, power, storage, cargo delivery and credits still constrain trade. [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md#provisional-external-freight-capacity) records the reason and pending reruns.
- **1 Galactic Credit is anchored to the price/value of 1 kg of gold.** This is a fictional accounting anchor, not a real-world exchange-rate quote, a guaranteed conversion/redemption mechanism, or a requirement to add a gold deposit.
- **Energy is the main operating resource.** Active buildings have passive consumption per simulation second; production also has an energy requirement per transaction. A transaction can produce multiple outputs.
- **A connected road network shares an electricity grid, with battery storage.** The current simulation derives separate grids from completed road corridors; disconnected networks do not share generation or stored charge. More detailed conversion losses and charging/discharging limits remain design choices.

The colony remains robotic and the interface calls its population workers. Organic food feeds Rex, the morale companion, and can be exported externally. It is not worker food. Trade delivery must still respect physical inventory and transport; a credit balance is not a shared material inventory.

## Current prototype units and accounting

| Kind | Prototype unit | Accounting boundary |
| --- | --- | --- |
| Solid/raw/manufactured material | kg | Recipes and cargo count physical mass. A discrete machine may also have a count and declared mass. |
| Liquids | L | Declare density in kg/L when checking recipes, transport mass and storage. |
| Electricity | kWh | Grid generation/storage is separate from physical cargo. |
| Passive electrical draw | kW, equivalent to kWh per simulation second multiplied by 3,600 | Over `dt` seconds, energy used is `kW × dt / 3600`. |
| Recipe electricity | kWh per completed transaction | Do not charge independently for each co-product or also count the same process energy as passive draw. |
| External account | Galactic Credit | Used at the external trading port; not a material unit or local construction input. |

Battery equipment is a physical manufactured item; its stored electrical energy is a separate grid state. Manufacturing an empty battery must not manufacture charge. Battery mass, capacity, efficiency, throughput and lifetime are proposed balance fields, not implied by selecting storage.

## Deposit extraction and resource presentation

**Confirmed v0.8.1 direction:** One **Extraction Mine** blueprint handles all eight raw types. Its placement binds it to the underlying deposit; that resource determines output, rather than a manually chosen mining recipe. The prototype permits one live mine per deposit and stores the binding in save data. Per-resource rates, construction cost, workers, storage and energy remain editable definitions; staffing alone cannot bypass the extraction rate.

The resource HUD separates **Credits**, **Energy**, **Raw materials**, **Basic production** and **Adv production**. All eight raw types are visible even when the home region lacks a deposit. Basic and advanced products are separate from the worker-status row; stored workers remain physical counted cargo but are displayed with workforce controls. The group hover panel explains each item’s units and the difference between owned stock and locally spendable inventory. [Interface and Controls](INTERFACE_AND_CONTROLS.md) records the exact current grouping and shortcut **B, R, M**. The final v0.8.1 Shipping executable passed 117 interaction stages, boot/offline checks and four display states; reconciled native coverage contains 88 unique clean results. The [v0.8.1 verification record](../verification/v0.8.1.json) identifies the exact evidence and retained limitations.

## Proposed viability approach

There are **24 possible resource-type combinations** before considering positions: four choices of the missing standard type, multiplied by six rare pairs. No region has all four standard resources. A mandatory four-standard closed starter chain would contradict the confirmed distribution.

**Confirmed bootstrap:** Start with **zero credits**. Landed physical materials must suffice for a **road, solar generation and level-one trading port**. Export available raw goods to earn credits, then buy the missing standard resource. No free starting credit or import is granted. The current provisional baseline supplies 9.57 t of loose cargo and 20 kWh of initial core charge, with the separately installed bodies/equipment accounted for in [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md). Freight capacities now use the tonne-scale revision above; broad economic balance remains unverified.

**Recommendation:** Include the equipment/labor needed to extract and deliver a saleable local material in that finite bootstrap. Solar, road-grid connection and port operation must work before the first import. Then test whether trade and local specialization sustain worker support, repairs and expansion after the landed stock is consumed.

Check every one of the 24 type combinations with the chosen starter recipes, port cost, power demand, trade delay and physical hauling. All 24 passed full7 with the older freight limits; reruns with the new capacities and 1,000-native-unit AI batch ceiling are pending. A port must not need an import to become able to import; a generator must not depend on a factory that cannot start without that generator. Spatial randomness should be validated separately for core reservation clearance, reachability and useful extraction access. Final depletion, extraction yields and market availability remain open.

## What stays external and what remains open

External definitions hold resource IDs/display names, class, unit, `unit_mass_kg` and `litres_per_unit` in [resources.json](../../Rules/resources.json); recipe inputs, all outputs and `energy_kwh` in [recipes.json](../../Rules/recipes.json); building and extraction properties in [buildings.json](../../Rules/buildings.json); generation and finite landed stock in [scenario.json](../../Rules/scenario.json); connected grids/batteries in [energy.json](../../Rules/energy.json); and port levels, prices and shipment timing in [trade.json](../../Rules/trade.json). Storage is measured in litres, worker hauling is bounded by both kilograms and litres, shipment capacity is kilograms, battery state is kWh and credits occupy a separate account. Fuel generation consumes physical fuel. Solar follows the shared daylight curve and produces zero at night. Balance values are editable candidates, not settled design facts.

**Current pool is closed at four standard plus four rare types.** Earlier iron/copper/aluminum/titanium distinctions can live inside the ore-family abstraction. A future expansion could discuss sulfur/phosphate industrial minerals, lithium-bearing battery minerals, or platinum-group catalysts, but none is a current ninth deposit type or a hidden prerequisite. The earlier [provisional catalog](PROVISIONAL_CATALOG.md) remains historical proposal material.

The companion [product proposal](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md) preserves the confirmed product tiers and illustrative material-balanced alternatives. The executable candidate recipes are the JSON definitions, including multi-output refinery transactions. They use no separate waste resource yet. Runtime values and verification must be evaluated together before calling the economy balanced.

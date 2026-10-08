# seige2222 — Product Recipes and Starter Viability Proposal

[Design index](README.md) · [Resource proposal](RESOURCE_PROPOSAL.md) · [Building proposal](BUILDING_PROPOSAL.md)

**The product types below are user-confirmed; supporting items, recipes, quantities and energy values are provisional.** The current v0.9 catalog in [recipes.json](../../Rules/recipes.json) is summarized with its accepted worker/kit baseline in [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md). The larger example transactions below remain design alternatives, not a second runtime recipe table. Native checks provide bounded evidence; expanded scenario and packaged acceptance remain incomplete.

## Confirmed products and their role

| Confirmed stage | Products | Proposed use, not an extra confirmed mechanic |
| --- | --- | --- |
| First-stage products | Fuel; plastic pellets; organic food | Fuel for an input-consuming generator; polymers for equipment; organic food for Rex and external export. |
| Second-stage products | Control circuits; robotic parts | Shared controls, actuators and replaceable assemblies for worker production, buildings, defenses and logistics. The UI population remains workers. |
| Third-stage products | AI chips; fusion reactors | Advanced computation and power equipment at the ends of deeper chains. A manufactured fusion reactor is machinery, not consumable fuel or stored electricity. |

Organic food does **not** become worker food or worker hunger upkeep. Rex consumes it as the colony's morale companion, and it is also an export product. Production stage does not automatically select a loot priority or market price. Buildings and access to materials unlock capabilities; no research currency/tree is added.

## Recommended supporting products

| Assistant-proposed product | Why it earns a place |
| --- | --- |
| Construction alloys | A shared structural material for buildings, machinery, repairs and transport; avoids separate iron/steel/aluminum inventories in the initial catalog. |
| Industrial glass | Turns silica into a useful structural, insulating and optical input. |
| Conductors | A shared electrical input for grid equipment, circuits, worker parts and power machinery. |
| Battery units | Physical storage equipment for the confirmed electricity grid. Stored charge is tracked separately in kWh; newly manufactured units begin empty unless explicitly charged afterward. |

These suggestions support the named product chain without expanding the confirmed four-standard/four-rare deposit pool. Battery chemistry, discrete pack size and advanced battery variants are not selected. Radioactive ore can support a later proposed nuclear/isotope branch; it is not casually substituted for fusion fuel. The reactor's operating fuel cycle and generation rate remain open.

## Illustrative closed material transactions

**Assistant examples, not tuned recipes or real industrial process specifications.** Solids use kg; liquids use L. For these examples only, water is 1 kg/L, liquid hydrocarbons 0.85 kg/L and fuel 0.80 kg/L. These are game accounting densities, not specifications for every substance in a broad resource family. Every row balances total material mass. Electricity is a separate transaction charge and is not included in material mass.

| Suggested transaction | Physical inputs | Physical outputs, including residue | Example process electricity |
| --- | --- | --- | --- |
| Mixed-ore processing | 100 kg metal ore | 60 kg construction alloys + 10 kg conductors + 30 kg mineral residue | 200 kWh |
| Glass processing | 100 kg silica | 95 kg industrial glass + 5 kg mineral residue | 150 kWh |
| Biomass processing | 100 kg biomass + 20 L water | 25 L fuel (20 kg) + 20 kg plastic pellets + 80 kg wet residue | 60 kWh |
| Hydrocarbon processing | 100 L hydrocarbons (85 kg) | 70 L fuel (56 kg) + 20 kg plastic pellets + 9 kg process residue | 30 kWh |
| Organic-food processing | 100 kg biomass + 10 L water | 80 kg organic food + 30 kg wet residue | 20 kWh |
| Control-circuit assembly | 2 kg conductors + 1 kg industrial glass + 1 kg plastic pellets | 3.6 kg control circuits + 0.4 kg process residue | 30 kWh |
| Robotic-parts assembly | 6 kg construction alloys + 2 kg conductors + 1 kg plastic pellets + 1 kg control circuits | 9.5 kg robotic parts + 0.5 kg process residue | 20 kWh |
| Empty battery assembly | 12 kg construction alloys + 4 kg conductors + 2 kg plastic pellets + 1 kg industrial glass + 1 L water | 19 kg empty battery equipment + 1 kg process residue | 25 kWh |
| AI-chip fabrication | 2 kg control circuits + 0.3 kg industrial glass + 0.3 kg rare metals + 0.4 kg crystalline material | 2.4 kg AI chips + 0.6 kg process residue | 200 kWh |
| Fusion-reactor assembly | 700 kg construction alloys + 120 kg conductors + 60 kg industrial glass + 60 kg robotic parts + 10 kg AI chips + 30 kg rare metals + 20 kg crystalline material | 1 reactor with a declared 980 kg mass + 20 kg process residue | 2,000 kWh |

The broad ore/material families abstract beneficiation, alloy chemistry, semiconductors and battery active materials. The rows establish a coherent **game material ledger**, not validated metallurgy or a real battery/fusion design. Recipe yield, cycle duration, staffing and electrical costs need joint balancing. A smaller batch is a proportional transaction, not an opportunity to round fractional ingredients away.

Residue accounts for the material that does not enter the intended product. Whether it becomes stored waste, a recycling feedstock or an explicitly discarded/emitted loss is **open**. Do not silently create free saleable byproducts or unlimited waste storage. Likewise, do not treat all multi-output recipes as a single-output recipe whose other products vanish.

The two chemical paths are alternatives: biomass can supply fuel and polymers without hydrocarbons, while hydrocarbons offer another specialization. First and second stages do not require a rare deposit in these examples; advanced chips/reactors do. A region lacking a standard input can import it through the confirmed trade path rather than being silently granted a fourth standard deposit.

## Electricity, transactions and batteries

**Confirmed:** Building operation consumes energy passively per simulation second; production also consumes energy per transaction, and a transaction may create multiple products. Buildings on the same connected road network share generation and battery storage.

**Proposed transaction behavior:** Reserve the entire physical input batch and transaction energy, then debit/produce them through one explicit transaction state so a shortage, pause, save or interruption cannot duplicate outputs. Publish a separate passive draw in kW and recipe draw in kWh/transaction. These are different costs; do not count the same process energy twice. If electricity or output capacity is unavailable, the recipe waits. Charge all co-products once as one transaction.

For passive draw, `energy_kWh = power_kW × simulation_seconds / 3600`. Stored battery charge must stay between zero and capacity. The current source recomputes shared grids from completed connected road corridors, including upgraded corridors; generation, capacity, initial charge and idle draw come from [energy.json](../../Rules/energy.json). Those values are provisional balance settings. Charge/discharge limits, conversion losses and more elaborate outage priorities remain future design choices. Manufacturing battery equipment does not itself create stored charge, and selling equipment must not duplicate stored energy.

**UI requirement:** Every building dossier shows workers used/capacity, storage used/capacity and its weapons/statistics. A building with batteries also shows stored energy/capacity with a clear energy unit. Zero, unarmed and absent-capability states should be explicit rather than omitted or implied to exist.

## Confirmed bootstrap and proposed acceptance checks

The colony starts with **zero credits**. The landed physical materials must suffice to build a **road, solar generation and the level-one trading port**. The player exports available raw goods to earn external trade credits, then imports the missing standard input. There is no starting credit grant, local wage economy or free import. Port levels 1–3 now provisionally carry 1/2/3 tonnes per shipment, retaining a nominal 120-second flight and 0.25 kWh departure cost. The previous 100/200/300 kg limits passed the 24 no-threat opening tests but obstructed longer tonne-scale development; the revised freight policy and AI batch ceiling require reruns. [Economy Baseline 0.9](ECONOMY_BASELINE_0_9.md#provisional-external-freight-capacity) separates current settings from prior evidence.

**Proposed validation:** Test all 24 allowed resource subsets, not just one favorable map. The landing kit must support extraction/handling of a saleable local material as well as the named road/solar/port bootstrap. Check solar/grid connection and port operation before any sale, real export cargo/arrival before credit settlement, import delay/capacity, worker maintenance and repairs before the first full production loop, and battery charging against the explicit initial-charge ledger. The current candidate assigns the landed core finite initial charge through `energy.json`; later battery installations start empty. This is a balance choice to evaluate, not an unlimited startup-energy exception. Market demand, prices and fees must make at least one legal opening viable without a hidden inventory or credit award.

The 1-credit/1-kg-gold anchor does not by itself price any table row or guarantee a buyer. Use an explicit price table and trade settlement rule after discussion. Roads carry both physical transport and grid connectivity, but electricity and cargo remain separate systems. Construction, ordinary recipe production and external trade should not share one ambiguous resource balance.

## Scope and next choices

Agree on supporting intermediates and whether the co-product alternatives above produce useful choices. Then select battery capacity/limits, passive loads, process-energy costs, recipe timing and port prices/capacity together. Keep the resource pool at four standard and four rare types for now. The earlier twelve-material/steel/motor/sensor/fleet recipes remain historical suggestions in the [provisional catalog](PROVISIONAL_CATALOG.md), not additional current requirements.

The expanded candidate uses kilogram solids/manufactured goods and litre liquids, with mass-balanced physical-output batches and explicit transaction energy. Inactive workers are the discrete exception: assembly creates whole `stored_workers` items with configured mass and occupied litres; activation consumes an item. Core replication offers all recipes slowly, while the worker factory is faster and uses less process energy. The authored ammo recipes add kinetic shells and missiles as kilogram cargo. Supporting products do not expand the eight-type deposit pool. Recipes abstract away waste as a separate cargo system; the residue alternatives above are not implemented. This expanded revision needs native, rendered and packaged verification beyond the earlier economy/Rex checkpoint.

Current ammunition examples are provisional but mass-balanced: shells use 4 kg alloy + 1 kg robotic parts + 1.25 L fuel to make 6 kg ammunition; missiles use 4 kg alloy + 1 kg conductors + 1 kg circuits + 5 L fuel to make 10 kg ammunition. Fuel is 0.8 kg/L. Worker disassembly costs 1 kWh and returns at most its declared physical input mass. Recipe, factory multiplier, storage and pricing validators do not imply that combat effectiveness or economic balance is tuned.

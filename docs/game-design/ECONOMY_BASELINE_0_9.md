# seige2222 — Provisional Economy Baseline 0.9

[Design index](README.md) · [Resource proposal](RESOURCE_PROPOSAL.md) · [Building proposal](BUILDING_PROPOSAL.md) · [Production dependencies](PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md)

## Status

The user accepted **80 kg hovering utility workers, tonne-scale construction bills and a finite 9.57 t loose landing kit as a provisional test baseline** on October 6. These are editable fictional balance values, not engineering measurements. This revision keeps the existing 22 cargo types, four standard/four rare raw pool and zero starting Galactic credits. Rules began at `prototype-9.0` and are now `prototype-9.3` (9.2 added the material gates below; 9.3 removed the unused `category` field, checks the cumulative level bills and fingerprints the parsed JSON content, so whitespace, line endings and number spelling no longer invalidate a save while any changed value, key or key order still does); individual-worker and world-calendar saves use format 7. The completed v0.8.1 evidence is historical and does not verify these changes.

The economy mechanisms below are implemented, but v0.9 acceptance remains incomplete. The user has selected an authored established initial state for Developed AI; replaying hours of growth is no longer its loading requirement. Player and Starting AI retain the finite landing baseline. Earlier 18/36-hour growth failures remain historical diagnostics of autonomous economic and defensive behavior, not current established-setup gates. Native manifest validation, ordinary post-start continuation, final packaged acceptance and isolated performance remain pending. No new isotope-cell product or winter gameplay penalty is included.

Editor **WorldReview16 completed 38 stages with zero assertions**, advancing the colony **1,296.8 simulation seconds** (`Saved/WorldReview-v09-build16.json`). The reviewed captures show the actual finite-worker hatch sequence, readable daytime/night and winter snow, plus lake/stream scenery. The completed core showed **5 kW with one arrived operator** before pausing, consistent with its proportional 30 kW/six-job definition. Weather phases are isolated visual fixtures, not a 100-day simulation soak; the conspicuously circular lake outline remains a visual limitation. This is rendered candidate evidence, not Shipping or performance acceptance.

## Units and physical accounting

- A `kg` quantity weighs exactly one kilogram per unit. Water, fuel and hydrocarbon quantities are litres, using the authored 1.00 / 0.80 / 0.85 kg/L densities. Manufactured-worker quantities are whole bodies, each 80 kg and 160 L packed. Bulk packing volumes are provisional cargo-space coefficients, not solid-material density measurements.
- Hover-worker payload is capped by **both 40 kg and 60 L**; a packed 80 kg/160 L worker cannot fit that cargo bay. Individual worker relocation requires its physical body path rather than an oversized courier parcel.
- Grid electricity is kW and kWh, separate from cargo. Recipe/weapon energy is an actual per-transaction kWh cost; idle and worker consumption are kW. Credits are a trading account, not an inventory item.
- Every current recipe balances its physical input/output mass. Worker assembly uses 60 kg components + 12 kg alloy + 4 kg circuits + 4 kg batteries. Recycling returns 90% of each input: 72 kg recovered, 8 kg unrecovered, with the existing **1 kWh per body** disassembly cost. This loss is deliberate; there is no mass creation or free full recycling loop.

The numeric masses and packing volumes originate in this project’s provisional rule definitions and the accepted worker baseline. They are not specifications copied from a reference game or certified real equipment. The cargo abstraction omits detailed chemistry, heat, exhaust, smelting waste and tooling wear unless the rules explicitly account for them.

## Finite landing manifest

This manifest applies to **Player and Starting AI**. Developed AI has a separate, explicit established-state manifest in [AIFILES](../../AIFILES/README.md); its initial assets are not charged against this loose kit or presented as outputs produced during loading. After either initialization path, ordinary finite bills, physical logistics and threats apply.

The current established manifest separately authors 24 installations, 44 active workers plus four stored bodies, local stocks, 145 kWh stored across its batteries and 0.5 credits. Its local age of 60 seconds carries no claim of earned production or trade history. Those provisional scenario totals are not added to the Player/Starting-AI kit; [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) records the initialization and pending verification boundary.

| Loose cargo | Native quantity | Mass (kg) | Packed volume (L) |
|---|---:|---:|---:|
| Construction alloys | 6000 kg | 6000 | 1200 |
| Conductors | 400 kg | 400 | 80 |
| Industrial glass | 400 kg | 400 | 200 |
| Plastic pellets | 200 kg | 200 | 300 |
| Control circuits | 240 kg | 240 | 192 |
| Robotic parts | 960 kg | 960 | 576 |
| Battery modules | 240 kg | 240 | 144 |
| Metal ore | 250 kg | 250 | 100 |
| Silica | 250 kg | 250 | 162.5 |
| Biomass | 250 kg | 250 | 500 |
| Water | 250 L | 250 | 250 |
| Fuel | 150 L | 120 | 150 |
| Organic food | 10 kg | 10 | 15 |
| **Total** | | **9570** | **3869.5** |

Separately installed at landing: six 80 kg worker bodies (480 kg), the configured carried fleet, and the shuttle’s installed weapons. The new mixed laser loadout weighs 3,520 kg. The current two armed small wheeled vehicles add 480 kg, making **14,050 kg for these explicitly counted bodies, cargo and weapons**. The 20 kg preloaded deployment consumables are a separate consumed construction manifest. Neither subtotal includes the unmodeled ship hull, fusion reactor assembly, landing gear or all other installed equipment; neither is a claim about total shuttle mass or a validated launch payload.

Core level 1 arrives already built as an upright shuttle. Its `cost` is therefore the **20 kg deployment consumables**, not a claim that the ship is built from 20 kg of metal. Level 2/3 add paid surrounding infrastructure; the central shuttle itself stays the same visual size. All levels reserve the final level-3 plot.

## Raw geography and bootstrap

The standard pool is water, metal ore (`iron_ore`), silica and biomass (`carbon`). The rare pool is rare metals (`copper_ore`), radioactive ore, crystalline material and hydrocarbons. Each region contains three distinct standard types plus two distinct rare types. There are exactly **4 × C(4,2) = 24** resource compositions; deposit positions and dry-terrain accessibility vary separately by seed.

The loose kit funds the first road-linked solar array, service bay, trading port, one local mine and added workers. A local raw export earns credits; actual imported missing inputs arrive at the trading port and require local delivery. Starter viability must not require a particular rare pair or hidden shared inventory. Radioactive ore currently has export value; a local isotope/fuel-cell production branch remains future design.

The 24 parameterized `Seige.Economy.StarterCombinations` tests use separate generated seeds, the finite live manifest, normal paid AI placement/roads/workforce/trade and no stock/credit grants. Each must complete and staff solar, support, mine, port and sensor, earn export credits and physically receive one missing standard resource. Threats are explicitly disabled for these economic tests; combat survivability and full industrial development remain separate checks. **All 24 passed** in the retained full4, focused5 and `Saved/Automation/v09-full-7/index.json` runs, before the freight revision below. This is one generated seed per composition, not exhaustive terrain-seed coverage or proof of later industrial/combat survival. Those old-capacity passes remain historical regression evidence; they do not verify the new shipment values.

## Provisional external freight capacity

The current [trade definitions](../../Rules/trade.json) use capacities aligned with the tonne-scale construction bills:

| Port level | Maximum shipment mass | Nominal shipment time | Departure electricity |
|---|---:|---:|---:|
| 1 | 1,000 kg | 120 simulation seconds | 0.25 kWh |
| 2 | 2,000 kg | 120 simulation seconds | 0.25 kWh |
| 3 | 3,000 kg | 120 simulation seconds | 0.25 kWh |

These are provisional authored values. They replace the earlier 100/200/300 kg limits after the larger building bills exposed a freight bottleneck: historical paid-growth runs exhausted their 18-hour preparation budget while still waiting for expansion materials. Increasing shipment capacity is a targeted balance revision, not a stock or credit grant, and is not yet proof that every developed scenario succeeds.

The [AI policy](../../AIFILES/colony_ai.json) now requests export/import batches of up to **1,000 native resource units**. A solid kilogram resource therefore targets one tonne; liquids use litres and discrete workers use whole counts, with the request reduced to the port's actual mass limit, storage, available stock, demand and affordability. Each port handles one shipment at a time. Exports still require local physical delivery; imports spend credits and arrive into local storage. Flight progress still depends on actual staffing/power, so 120 seconds is the nominal duration rather than guaranteed elapsed delivery time. Existing prices, energy costs, local hauling caps and finite starter stock remain separate constraints.

## Physical delivery priority and refill policy

The current [worker policy](../../Rules/workers.json) derives desired logistics jobs as **min(8, 1 + floor(eligible completed facilities / 4))**. Eligibility requires a living, enabled, completed building outside the `core` and `wall` roles; an unpowered, unstaffed or supply-starved facility still counts. Sites and roads do not count. Thus zero to three eligible facilities require one desired logistics job, four to seven require two, and 28 or more reach the provisional eight-job cap. The six-body landing manifest is unchanged.

These jobs join total workforce demand and the scheduler's reserve of idle bodies plus actual working haulers. They require ordinary paid 80 kg worker manufacture or reactivation and service capacity; additional logistics bodies have the same maintenance and energy costs as other workers. Essential core/service staffing takes precedence during shortages. A packed worker relocating itself does not count as a working hauler. The cap applies to desired reserve, not to deliveries already active: no carrying task is preempted when buildings are disabled or lost. The implementation has compiled, but its new regressions and complete AI viability remain unverified.

[policies.json](../../Rules/policies.json) owns the candidate `delivery_priority_order`: fuel, maintenance, repair, defense, construction, production, trade, reserve, then storage. It selects the next available body's task; a load already being carried is not interrupted. Dispatch ordering grants no stock, energy or workers and does not increase trip speed or the 40 kg/60 L payload limits.

`delivery_refill_trigger_fraction` is provisionally **0.5**. Standing buffers replenish when their usable local/incoming stock is at or below half the target, then aim toward the full target. Higher-priority targets are accounted for before a lower class uses the same stock, without adding separate cargo buckets. This low-water mark suppresses repeated tiny top-ups without imposing a minimum parcel size. Finite construction bills, exports, fabrication/refit materials and worker reserve/export targets retain their final small deliveries.

`delivery_raw_input_buffer_loads` provisionally sets a **two-load minimum** for standard/rare, non-discrete recipe inputs. Their local-plus-incoming target is the larger of the existing recipe-cycle buffer and two worker loads, each limited by both 40 kg mass and 60 L volume. For metal ore this increases the target from 15 kg to 80 kg, allowing full 40 kg deliveries while retaining the same half-target refill trigger. A value of zero restores the recipe-cycle target. Manufactured inputs, construction bills, workforce and hauling speed remain unchanged; the buffer reserves no free goods or extra storage. Native acceptance of this new buffer policy is pending.

The new `construction_source_policy: surplus_then_largest_load` prefers useful source surplus above operating buffers, then the largest capacity-bounded load, then shorter access-port-to-site distance and stable source ID. Pickup claims are always deducted. Useful surplus must cover the smallest of the remaining bill, haul capacity and `courier_min_batch`; finite tails remain possible. Reserved stock inside operating buffers is a fallback only when no useful surplus source exists. This avoids repeatedly draining a nearby facility's small buffer while a bulk source can supply the accepted bill; it grants no material or extra carrying capacity.

**Bounded evidence; release pending:** the preceding cargo run passed 19 tests cleanly and failed two AI cases (`Saved/Automation/v09-cargo-14/index.json`). The latest source-selection checkpoint passed `ConstructionUsesBulkBeforeOperatingBuffers` and `EssentialDispatchAndFiniteTails` cleanly, but two AI viability cases still failed (`Saved/Automation/v09-dispatch-16/index.json`). Those growth-run failures remain historical strategy diagnostics after the established-start decision. The new manifest and its ordinary continuation require separate evidence; subsystem passes alone do not establish sustained AI operation.

## Candidate AI bootstrap policy

The [AI economy policy](../../AIFILES/colony_ai.json) uses `core_replication_policy: funded_shortage_first`. Within its ordered recipe list, the core prefers a needed goods batch that can pay unreserved local inputs, electricity and output-room reservation. Otherwise selection requests ordinary deliveries and paid imports. Committed batches are not replaced, normal automatic worker production still applies, and a staffed specialist is preferred over duplicating its recipe in the slower core. Useful producer-input purchases can precede ordinary expansion; immediate road, power, support and trading-port prerequisites retain priority.

`bulk_input_policy: remaining_output_bill` budgets raw feedstock for currently needed manufactured outputs using actual enabled, installed recipes. Coproducts satisfy remaining output needs once, rather than creating a second input bill. Each purchase is bounded by the outstanding useful input bill, normal import batch, port mass/storage limits and available credits. Manufactured inputs and worker-body inputs retain their existing four-cycle buffers. This avoids alternating long bulk-export trips with unnecessarily small ore purchases; no input or output is granted. The `recipe_buffers` alternative retains the earlier stocking behavior for controlled diagnostics.

Fuel purchasing remains separate: `fuel_import_buffer_cycles: 4` targets four authored operating buffers across colony stock, and `fuel_import_refill_fraction: 0.5` refills below half that target. Credit, port and storage limits still apply, and stock elsewhere still needs a physical trip to the generator. Generator consumption, local fuel buffers, worker refill thresholds and flight times are unchanged.

The live AI plan retains **23 target installations**, completing four perimeter towers before industry and adding bulk battery storage afterward. Starting AI follows this paid growth plan from the finite kit; established AI uses it for ordinary operation, missing-target recovery and expansion after loading its separate initial manifest. The earlier `developed_setup_seconds` limits of 64,800/129,600 seconds describe superseded history-generation experiments. Their failures remain diagnostics; the current developed option does not advance a fabricated construction history or claim that its assets were funded by the 9.57 t kit. Established-state validation and bounded live continuation still require fresh evidence.

The candidate `placement.defense_coverage_policy: prefer_covered_approaches` ranks legal civilian/industrial plots by known fixed-gun coverage before committing a road-feasible construction order. It samples 24 approaches, provisionally 8 m outside the actual square body, using equipped weapon ranges and friendly-building obstruction, including the proposed building itself. This avoids crediting an inward gun with shots through the building it protects. Enabled completed fixed platforms supply geometric coverage; the score does not promise staffing, electricity, accuracy or survival, and uses no hidden enemies or neighboring intelligence. Tied scores preserve authored search order; zero coverage still permits normal bootstrap/fallback. Defense and sensor roles are excluded so perimeter spread and sensor extension retain their existing order. Deposit-bound mines retain their node placement. These policy settings are external; focused geometry and full-scenario verification are pending.

## Production chain

Core replication accepts the same recipe inputs and pays its configured time/energy multipliers. Specialized factories improve the authored rate. Current numerical recipe times were retained while body mass and construction scale changed, avoiding a simultaneous unexplained speed rebalance. Worker assembly is **60 base seconds and 1 kWh**, giving **600 seconds (10 minutes) and 4 kWh at the core**, or **30 seconds and 0.5 kWh at the worker factory**, before staffing/power efficiency. The earlier 300-second draft was not adopted; no recipe timing was changed during verification.

| Product recipe | Inputs | Outputs | Base seconds | Base kWh |
|---|---|---|---:|---:|
| smelt_alloy | 5 kg Metal ore | 4 kg Construction alloys, 1 kg Conductors | 4 | 0.2 |
| draw_conductors | 5 kg Metal ore | 4 kg Conductors, 1 kg Construction alloys | 4 | 0.2 |
| make_substrates | 2 kg Silica | 2 kg Industrial glass | 4 | 0.12 |
| make_circuits | 1 kg Conductors, 1 kg Industrial glass, 1 kg Plastic pellets | 3 kg Control circuits | 5 | 0.3 |
| make_components | 2 kg Construction alloys, 1 kg Conductors, 1 kg Control circuits | 4 kg Robotic parts | 7 | 0.2 |
| refine_biofuel | 8 kg Biomass, 2 L Water | 5 L Fuel, 6 kg Plastic pellets | 8 | 0.3 |
| refine_hydrocarbons | 10 L Hydrocarbons | 5 L Fuel, 4.5 kg Plastic pellets | 8 | 0.2 |
| make_organic_food | 4 kg Biomass, 1 L Water | 5 kg Organic food | 10 | 0.15 |
| make_batteries | 4 kg Construction alloys, 2 kg Conductors, 1 kg Plastic pellets, 1 kg Industrial glass | 8 kg Battery modules | 12 | 0.4 |
| make_ai_chips | 2 kg Control circuits, 0.5 kg Rare metals, 0.5 kg Crystalline material | 3 kg AI chips | 20 | 1 |
| make_fusion_reactors | 10 kg Construction alloys, 3 kg Conductors, 2 kg Industrial glass, 3 kg Robotic parts, 1 kg AI chips, 1 kg Crystalline material | 20 kg Fusion reactor assemblies | 30 | 2 |
| assemble_robot | 60 kg Robotic parts, 12 kg Construction alloys, 4 kg Control circuits, 4 kg Battery modules | 1 worker | 60 | 1 |
| make_shells | 4 kg Construction alloys, 1 kg Robotic parts, 1.25 L Fuel | 6 kg Kinetic ammunition | 6 | 0.6 |
| make_missiles | 4 kg Construction alloys, 1 kg Conductors, 1 kg Control circuits, 5 L Fuel | 10 kg Missile ammunition | 12 | 1.2 |

These are production dependencies, not research unlocks or a research timer. Rare metals and crystalline material enter the AI-chip/fusion branch. Fuel has both biomass/water and hydrocarbon routes. The universal core and paid imports prevent absent local raw types from creating a permanent technology lock.

## Construction and upgrade accounting

**prototype-9.2 gates (2026-10-09).** The advanced tier is no longer affordable from the landing kit alone: the AI-chip works needs 300 kg circuits and 120 kg batteries, the battery works 100 kg plastic, the fusion works 40 kg AI chips, the mech factory 20 kg AI chips, the tracked-vehicle factory 300 kg batteries and the plasma tower 6 kg AI chips (22 of 28 blueprints remain day-one affordable, down from 26). Every level-3 upgrade (towers, solar, port, factories) needs AI chips and the three vehicle-factory level-2 upgrades need 150 kg batteries; the cumulative level-2/3 bills below include those additions. `make_fusion_reactors` now also consumes 2 kg radioactive ore per batch (22 kg out), so the deposit class has a consumer. These are the first progression gates after the v0.9 baseline and remain provisional.

The table reports current `cost` totals in kg and local inventory litres. For upgraded definitions, the total includes earlier installed stages; **the actual upgrade consumes only the separately authored incremental `upgrade_cost`**. Tower stage bills include the weapon installed at that stage and do not refund removed modules. Existing local inventory, repair/input reservations, worker berths, equipment bills and construction staging still compete for real storage.

| Building | Level | Cumulative bill (kg) | Next upgrade (kg) | Local storage (L) |
|---|---:|---:|---:|---:|
| Orbital command shuttle · Level 1 | 1 | 20 | 5080 | 40000 |
| Extraction Mine | 1 | 650 | 0 | 1000 |
| Alloy refinery | 1 | 1500 | 0 | 1500 |
| Conductor works | 1 | 1300 | 0 | 1500 |
| Industrial glass works | 1 | 1340 | 0 | 1500 |
| Circuit works | 1 | 1400 | 0 | 1500 |
| Robotic parts works | 1 | 1600 | 0 | 1500 |
| Sensor mast | 1 | 130 | 0 | 800 |
| Laser tower · Level 1 | 1 | 400 | 770 | 500 |
| Cargo depot | 1 | 685 | 0 | 10000 |
| Worker charging and service bay | 1 | 830 | 0 | 1500 |
| Solar array · Level 1 | 1 | 575 | 718.75 | 800 |
| Grid battery bank | 1 | 650 | 0 | 800 |
| Trading port · Level 1 | 1 | 1000 | 1250 | 3000 |
| Trading port · Level 2 | 2 | 2250 | 1780 | 6000 |
| Trading port · Level 3 | 3 | 4030 | 0 | 12000 |
| Biomass refinery | 1 | 1500 | 0 | 1500 |
| Hydrocarbon refinery | 1 | 1800 | 0 | 1500 |
| Organic food producer | 1 | 1200 | 0 | 1500 |
| Battery works | 1 | 1800 | 0 | 1500 |
| AI chip works | 1 | 2720 | 0 | 1500 |
| Fusion assembly works | 1 | 4040 | 0 | 3000 |
| Fuel generator | 1 | 925 | 0 | 800 |
| Command center · Level 2 | 2 | 5100 | 10160 | 60000 |
| Command center · Level 3 | 3 | 15260 | 0 | 80000 |
| Solar array · Level 2 | 2 | 1293.75 | 1026.25 | 800 |
| Solar array · Level 3 | 3 | 2320 | 0 | 800 |
| Wheeled vehicle factory · Level 1 | 1 | 4000 | 5150 | 5000 |
| Wheeled vehicle factory · Level 2 | 2 | 9150 | 7040 | 12000 |
| Wheeled vehicle factory · Level 3 | 3 | 16190 | 0 | 50000 |
| Tracked vehicle factory · Level 1 | 1 | 5500 | 6650 | 5000 |
| Tracked vehicle factory · Level 2 | 2 | 11850 | 9140 | 12000 |
| Tracked vehicle factory · Level 3 | 3 | 20990 | 0 | 50000 |
| Mech factory · Level 1 | 1 | 5020 | 6400 | 5000 |
| Mech factory · Level 2 | 2 | 11400 | 8800 | 12000 |
| Mech factory · Level 3 | 3 | 20200 | 0 | 50000 |
| Worker factory | 1 | 1340 | 0 | 3000 |
| Ammunition works | 1 | 1700 | 0 | 1500 |
| Laser tower · Level 2 | 2 | 1170 | 3198 | 1200 |
| Laser tower · Level 3 | 3 | 4368 | 0 | 4000 |
| Kinetic tower · Level 1 | 1 | 400 | 770 | 500 |
| Kinetic tower · Level 2 | 2 | 1170 | 3198 | 1200 |
| Kinetic tower · Level 3 | 3 | 4368 | 0 | 4000 |
| Missile tower · Level 1 | 1 | 400 | 770 | 500 |
| Missile tower · Level 2 | 2 | 1170 | 3198 | 1200 |
| Missile tower · Level 3 | 3 | 4368 | 0 | 4000 |
| Plasma tower · Level 1 | 1 | 406 | 770 | 500 |
| Plasma tower · Level 2 | 2 | 1170 | 3202 | 1200 |
| Plasma tower · Level 3 | 3 | 4372 | 0 | 4000 |
| Wall segment · Level 1 | 1 | 600 | 750 | 800 |
| Wall segment · Level 2 | 2 | 1350 | 1050 | 800 |
| Wall segment · Level 3 | 3 | 2400 | 0 | 800 |

Level-1 roads consume 40 kg alloy + 20 kg conductors per 100 m; rail and vacuum upgrades have separate heavier bills. This represents a graded track and installed utility equipment, with on-site earth treated as terrain rather than cargo. It does not claim to model a full concrete or asphalt road slab. Wall-segment base material is 600 kg alloy per 6 m segment; exact structure, hollow sections and strength remain provisional.

## Two large hardpoint banks

The confirmed starting loadout is **one large laser + two medium lasers + eight small lasers**, occupying 16 + 8 + 8 = **32 small front-area points**, exactly two large banks. The mount relation **1 large = 4 medium = 16 small** differs from the geometric envelope relation: each size step doubles every dimension and therefore multiplies volume by eight.

Current provisional weapon definitions give 3,520 kg installed mass, a replacement-material bill of 3,080 kg alloy + 352 kg components + 88 kg circuits, and 106.8 nominal aggregate DPS when all ranges/targeting/energy permit. Range makes that aggregate conditional: only the large laser reaches its longest range. A full volley costs 0.1068 kWh per two seconds (192.24 kW sustained), so the 30 kW level-1 core plus finite stored energy cannot fire continuously at that total rate. These values require playtesting and remain external balance choices.

Small/medium/large loaded ceilings of 500 / 4,000 / 32,000 kg imply 1,000 kg/m³ at the stated 0.5 / 4 / 32 m³ envelope. Those are provisional ceilings, not confirmed engineering capacities. Weapon envelope volume and front-area occupancy are separate budgets; cargo-space allocation must not be described as a physical packing solver. [Outfitting details](WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) owns the runtime boundary.

## Shared day, night and seasons

One authoritative scenario clock supplies every region: **1,800 simulation seconds daylight + 1,800 night**, **30 days per season**, ordered spring, summer, autumn, winter. An established manifest may state a local colony age; this is not the world date, elapsed loading work or credited production history. All colonies bind to the same live calendar at scenario commit. Pausing/menus do not advance that clock. 1×/5×/10× advance the same simulation calendar; a long rendering stall can still slow simulation under the existing frame-debt cap.

Solar uses a daylight half-sine and exactly zero night output. Its interval average is analytically integrated, including boundary crossings, so tick length does not invent energy. A 25 kW peak array yields **25/π ≈ 7.96 kWh per full one-hour day/night cycle** before staffing/connectivity effects. A 100 kWh battery uses 500 kg of authored battery modules (0.2 kWh/kg at module level), plus its support structure. Core fusion is **daylight-independent**, but completed-core output still scales with actual on-station staffing and workforce efficiency: 30 kW is the level-one rated output, not a guaranteed output from one operator. During the deployment-to-operation handover it can temporarily be zero until an operator physically reaches the job. No new radioactive consumable is silently added.

The external energy dispatch policy `pending_defense_before_optional_transactions` protects the authored electricity for building shots due in the current step against detected, unobstructed native threats. Passive operation remains first. Production, shield recovery, vehicle charging and other optional transactions may use only the remainder on their own connected grid; actual firing still pays its full bill. Blocked, out-of-range, not-yet-due and ammunition-starved weapons do not reserve power, and unrelated grids remain independent. The normal fixed step builds this derived reserve before production, refreshing again only for newly spawned threats; actual fire rechecks its target and line after vehicle movement. Save/load rebuilds the reserve from live state rather than inventing another stored-energy balance. Both focused energy regressions passed in `Saved/Automation/v09-integration-12`, including actual recipe/shot competition, shield and vehicle charging, disconnected grids and full save/load continuation. That integration run still has two AI viability failures, so this is subsystem evidence, not release acceptance. It cannot fix generation below passive demand or guarantee continuous firing.

The earlier 20-installation AI target layout exposed a substantial power deficit: its 37 operating jobs and building idle loads totalled **28.2 kW**, before road loads and additional logistics workers. A fully staffed 30 kW core plus one 25 kW solar array averages **37.96 kW** over day/night, leaving at most **9.76 kW** before those extra loads. Six industrial recipes running simultaneously at their full authored cadence would consume about **921.86 kW** in process energy alone. This is a theoretical duty-cycle comparison for that earlier layout, not measured continuous draw: physical inputs, output space, staffing and stored power throttle transactions.

The current paid AI plan retains **23 target installations**, including fuel generation, a four-tower perimeter and battery storage. Ordinary construction, staffing, road connection and imported or produced fuel use finite bills and physical deliveries. Developed setup instead declares its initial installed assets explicitly; it does not simulate that plan to completion during loading. Adaptive sizing of further generation, fuel and storage against actual industrial duty remains a proposal. Batteries shift generated energy in time and cannot cure a persistent generation deficit. The new established starting state does not by itself establish sustainable operation.

Winter adds smooth visible snow coverage and bounded local falling snow. Sun intensity/ambient follow the same calendar; shadow direction uses an external angular/cadence threshold to avoid invalidating all shadow pages every rendered frame. Snowflakes interpolate fixed-step presentation at 1× and stop with pause. No winter mobility, yield, heating or solar-season penalty is implemented. Those would need separate design decisions.

## Remaining acceptance

Retain earlier finite-kit and subsystem results with their original source/data scope. Verify all 24 paid starter compositions separately from established-manifest validation and ordinary post-start AI operation. Current established acceptance must check legal dry plots/roads, distinct finite worker identities, local stock and energy bounds, no fabricated production/trade counters, shared calendar, deterministic save continuation and atomic rejection. Preserve old paid-growth failures as historical strategy diagnostics rather than requiring hours of growth at load. Inspect day/night/winter, known-deposit visibility and final native-resolution rendering; validate staged definitions and the save boundary before delivery. This is the chosen test baseline, not a completed release or visual-parity claim.

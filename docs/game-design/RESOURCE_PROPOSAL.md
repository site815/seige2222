# seige2222 — Resource Proposal

[Design index](README.md) · [Status definitions](README.md#design-status)

**Assistant proposal for discussion.** This is a candidate revision of the resource list, not an approved catalog or a balanced specification. The existing [resource rules](RESOURCES_AND_INDUSTRY.md) remain authoritative. Building choices, complete production chains, and runtime data files are separate work.

## Recommendation

Use **12 raw materials**, with four common materials forming a proposed starter industrial base. Additional common and rare materials open branches or improve particular products. Give each material a recognizable industrial purpose; avoid a generic exotic ingredient that every advanced product requires.

The proposal preserves the established direction: physical batches, finite carrying capacity, extraction at defined rates, recipes enabled by buildings and material access, and deep manufactured chains without a research tree. Adding robots alone must not bypass extraction limits. All new catalog entries, classes, applications, and starter arrangements below are proposals.

## What counts as a resource

| Category | Proposed meaning | Examples and boundary |
| --- | --- | --- |
| Raw material | A material acquired from a world source before industrial processing. These are the entries counted toward the approximately 10–15 raw types. | Iron ore, water, silica. Source locations and extracted batches are distinct: a deposit is not inventory already available at the core. |
| Manufactured item | A physical output made from raw materials or other manufactured items. | Metal stock, circuits, motors, battery packs, sensor equipment. These do not increase the raw-material count. |
| Energy | A proposed operating input or output, distinct from a material stockpile. | Electricity is not automatically a cargo item. Fuel and battery packs are physical items; a pack's stored charge, if modeled, is a separate property. Power generation, distribution, and charging remain design questions. |

Water and industrial biomass are material inputs, not assumed robot food or drinking requirements. Robot needs, consumption, and happiness effects remain open in the [population design](POPULATION_AND_MORALE.md).

## Candidate raw-material catalog

Common and rare are proposed availability classes, not automatic quality levels or restrictions on what a material can unlock. Both classes can support new branches and specialized variants. Product loot tier must be assigned separately from deposit rarity.

| Proposed material | Availability | Industrial role | Reason to develop, defend, or trade it |
| --- | --- | --- | --- |
| Iron ore | Common | Structural metal stock, ordinary robot frames, machine housings, cargo containers, and basic armor. | A broadly useful construction and replacement-parts supply. |
| Copper ore | Common | Conductors, motor windings, electrical connections, and circuit inputs. | Competing demand from robot production, logistics equipment, sensors, and defenses. |
| Silica | Common | Glass, ceramic insulation, and simplified semiconductor substrates. | Makes electronics and optical equipment a material-dependent branch. |
| Carbon | Common | Alloying input, carbon-based industrial materials, filters, and processed fuel. | Connects structural production with a candidate starter fuel path and later composites. |
| Water | Common | Process fluids, coolant preparation, and chemical inputs. | Supports chemical and thermal-management branches without becoming a biological need. |
| Industrial biomass | Common | Feedstock for polymers, industrial lubricants, and processed fuels. | Adds materials for casings, seals, cables, and flexible parts; it is not a food supply. |
| Aluminum ore | Common | Lightweight metal stock for robot bodies, containers, and vehicle or equipment frames. | Offers lighter construction variants alongside iron-based structures; exact benefits need balancing. |
| Titanium ore | Rare | Advanced structural alloys, protective parts, and high-load components. | Supports durable specialized robots and defenses without becoming a requirement for ordinary repairs. |
| Rare-earth minerals | Rare | Precision magnetic components, specialized actuators, and sensor assemblies. | Opens precision equipment and stronger compact motors while leaving a basic motor path available. |
| Lithium-bearing mineral | Rare | Processed battery materials and higher-performance energy-storage assemblies. | Supports mobile equipment and remote installations; lithium batteries need not be a prerequisite for every robot or transport. |
| Platinum-group ore | Rare | Manufactured catalyst modules for specialized chemical processing and material recovery. | Creates an industrial specialization resource, rather than another armor ingredient. Catalyst consumption and recovery are undecided. |
| Fictional superconductive crystals | Rare | Manufactured superconductive assemblies for advanced power equipment and electromagnetic systems. | Retains one explicit science-fiction branch with a defined purpose; it should not become the universal late-game ingredient. |

These are game material families and fictional recipe abstractions, not validated chemistry or complete real-world supply chains. In particular, the proposed starter electronics simplify semiconductor manufacture.

### Changes from the earlier catalog

- Replace the **unnamed exotic mineral** with **lithium-bearing mineral**. Energy storage gives it a clear production and logistics role; the earlier possible shield use is not carried forward as a requirement.
- Refine **catalytic minerals** to **platinum-group ore**, making the extracted input distinct from the catalyst modules manufactured from it.
- Describe **biomass** as industrial feedstock and shift water's emphasis to processing and cooling. Human-oriented uses remain historical proposals, not robot needs.
- Retain superconductive crystals as explicitly fictional. Their role is narrow enough to evaluate or remove later without blocking the whole economy.

These are recommended revisions, not decisions to erase the [initial catalog](PROVISIONAL_CATALOG.md).

## Proposed starter subset and viability

**Recommended prototype subset:** iron ore, copper ore, silica, and carbon. Test these four before adding the other eight. This is a proposed first-playable slice, not a settled rule that every final sector contains these deposits.

For that test, provide reachable sources of all four within the starting sector. This uses a subset of the full catalog and avoids requiring trade with an uncertain neighbor merely to replace ordinary workers or repair the starter economy. The earlier example of three common and two rare materials was never a fixed distribution rule.

**Proposed bootstrap arrangement:**

1. The starting command center and robots arrive with finite, physically stored construction kits, components, and a startup fuel reserve. Specify their quantities when the candidate recipes are drafted; they are not a shared or unlimited inventory.
2. Those supplies must allow extraction, processing, transport, and the first replacement components to become operational before startup stocks run out. Starter construction cannot require a product that only the unfinished starter building can make.
3. A candidate power solution is a preinstalled, rate-limited core generator consuming delivered carbon fuel. This is an explicitly proposed simplification for the test, not an established power system or free energy. Its fuel preparation must be available during startup.
4. Ordinary robot production, basic hauling equipment, basic repairs, and a starter defensive capability should have recurring input paths using these four materials. Rare materials must not be hidden prerequisites for this test. This is a proposed viability constraint, not a completed recipe graph.
5. The loop must remain workable after the initial component stock is exhausted. Local extraction rates, hauling time, fuel consumption, and job-driven robot production must be checked together. Deposit depletion and replenishment remain undecided; this proposal does not make deposits infinite.

The final game's fair-subset rule needs a separate viability check. A starting sector should either support the chosen baseline production path or have an explicitly designed alternative; the existence of nearby deposits or a potential trade partner alone does not prove viability. Distribution fairness should consider reachable output and transport exposure as well as material counts. Exact site numbers, yields, and access guarantees are open.

### Manufactured examples for this discussion

| Proposed output | Illustrative dependency | Purpose |
| --- | --- | --- |
| Structural alloy stock | Processed iron + carbon | Frames, machinery, basic armor, and repair parts. |
| Conductor parts | Processed copper | Wiring and motor inputs. |
| Insulating substrate | Processed silica + carbon-based industrial material | A simplified starter alternative that does not require industrial biomass or a rare deposit. |
| Basic control circuit | Conductor parts + insulating substrate | Shared electronics for robots, equipment, and basic control systems. |
| Basic actuator | Structural parts + conductor parts + control circuit | A layered component serving robots and machinery. |
| Specialized component variants | Add appropriate processed aluminum, titanium, rare-earth, battery, catalyst, or superconductive inputs | Distinct product branches or variants, subject to separate recipe design. |

These examples demonstrate a possible four-material path. They are neither complete bills of materials nor production quantities. The earlier polymer-based circuit example remains another proposed path; it would add biomass dependencies and should not silently enter this starter test. Robot assembly, sensors, transport equipment, and defenses still need full recipe and building definitions. No recurring robot happiness good is selected here.

## Fields to keep in external editable definitions

**User direction:** Game rules should live in editable external files. The following resource-related fields are design recommendations for that requirement. The current prototype's JSON definitions and smaller implemented content target are recorded in [Rules and Simulation Architecture](RULES_AND_SIMULATION_ARCHITECTURE.md) and [First Playable Scope](FIRST_PLAYABLE_SCOPE.md).

| Definition area | Fields to externalize |
| --- | --- |
| Material identity | Stable ID, display name, description, category, tags, visual reference, and availability class. |
| Physical item behavior | Quantity unit, batch size, transport load per unit, storage compatibility, and destruction or salvage parameters once selected. |
| Automatic loot | Item tier and any eventual comparison-unit metadata needed for the confirmed highest-tier/least-held rule. Normalization and tie handling remain open; do not add player priority fields. |
| World sources | Material reference, source type, placement constraints, distribution weights, and any quantity, depletion, or renewal policy once selected. |
| Extraction | Resource/site and extracting-capability references, output quantity, duration, explicit time basis, worker requirements, operating inputs, and the chosen rate-limit scope. Avoid assuming whether the ceiling belongs to a deposit or facility. |
| Recipes and variants | Stable recipe references, explicit material/product inputs and outputs with quantities, duration, required building capabilities, and operating energy if adopted. Quantities and variant rules belong in recipe definitions rather than scattered resource descriptions. |
| Starting scenario | Accessible source set, physical starting stock, already installed equipment, and the proposed starter recipe subset. Keep these separate from the universal resource catalog. |
| Validation metadata | Units, permitted ranges, reference checks, and scenario viability checks. An unresolved parameter should remain visibly undecided rather than acquire a silent gameplay default. |

The prototype uses external JSON; the long-term schema, live reload, and migration behavior still need development. Stored world stockpiles must remain tied to actual locations and transports regardless of how definitions are loaded, with temporary construction simplifications recorded in the first-playable scope.

## Next decisions

Evaluate whether the four-material test can sustain its own construction, fuel, replacements, and repairs. Then assess whether each of the eight additional materials creates a useful choice or only an extra ingredient. Agree on the candidate catalog and starter path before tuning quantities, deciding final sector distribution, or treating these examples as implementation requirements.

## Related documents

- [Resources, Industry, and Progression](RESOURCES_AND_INDUSTRY.md)
- [Initial Provisional Catalog](PROVISIONAL_CATALOG.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)

# seige2222 — Workers, Construction and Transport 0.8

[Design index](README.md) · [Interface and Controls](INTERFACE_AND_CONTROLS.md) · [Rules architecture](RULES_AND_SIMULATION_ARCHITECTURE.md)

**Current v0.8 implementation direction; release verification pending.** This document owns the new construction, reserved-plot and road behavior. Earlier release reports remain historical evidence and do not verify this revision. Exact balances and supported fields come from the linked external definitions; broader fleet, population-needs and multiplayer proposals remain separate.

## Workers and time

The interface calls the robotic population **workers**. This changes player-facing terminology, not the robot-only population scope or existing stable IDs such as `robot_service_bay`. Workers still fill jobs automatically; the player does not issue individual worker orders.

**Confirmed controls:** Local playback cycles in the ordered ring **Paused → 1× → 5× → 10× → Paused**. Minus follows the reverse order. Space toggles pause and resumes the previous nonzero rate; the paused label says **Paused**. New scenarios begin at 1×; the automated construction/economy route selects 10× to test normal timed operations. These local controls do not define a persistent multiplayer server's clock.

Durations are simulation seconds, as declared by `rule_time_basis` in [policies.json](../../Rules/policies.json). Construction now takes meaningful simulated time rather than the earlier few-second prototype timings. At full staffing and efficiency the current core duration is 900 simulation seconds, or 90 real seconds at 10×; delivery, travel and shortages can extend ordinary building completion. Exact building durations remain in [buildings.json](../../Rules/buildings.json).

## Reserved plots and access

Buildings have a square current body half-width (`footprint`) and a square reserved half-width (`reserved_footprint`). Placement and transport must respect the reserved plot. The command core currently uses 240 and 720 logical units respectively: **three times the width and nine times the area**. The initial structure retains its level-one size inside the larger plot.

**Latest confirmed expansion:** Command cores have levels 1–3 with body half-widths 240/480/720 and reserved half-width 720 at every level. Level one is the parked shuttle. Solar arrays, trading ports, the three vehicle-factory families, four tower families and wall segments also have three levels; their body footprints stay fixed within each family. Upgrades pay the current level's additional bill and use physical construction. These catalog entries are authored; integration and release acceptance of this expanded revision are pending.

Vegetation clears across the **reserved largest-level plot** from the outset. Terrain grading, foundation geometry and foundation dirt use the **actual built footprint**, preserving natural relief in the empty expansion yard. Placement and routing still protect the full reserved area. Clearing vegetation does not imply a paved or flattened plot.

## Replication and inactive workers

**Confirmed:** Every core has a very slow selectable universal replicator, including worker assembly. A dedicated worker factory assembles workers faster with less electricity. Both require real materials and transaction energy; normal production does not create free population. Core fusion generation and storage support the initial colony. Production, storage and generation values are editable balance, not settled final rates.

Workers fill open jobs automatically. Surplus workers become inactive physical stock where space exists. The colony also has a configurable inactive-worker production target; trading ports have their own worker stock target for exports. Defaults are zero, a provisional opening configuration rather than a required play style. A dedicated operational worker factory takes the worker-production role while the core can run another selected product. If the core's automatic worker batch lacks local parts, charge or output room, it may make its selected goods to restore supply; explicitly selecting workers still waits for those inputs. Existing committed production must finish consistently through a selection change.

`stored_workers` is counted in whole **workers**, with external mass and occupied litres. It is one physical inventory item, not a second population ledger. Storage, courier cargo and trade must preserve indivisible counts; reactivation consumes a stored worker instead of duplicating it. The present 1 kg material bill and 50 L berth per worker are provisional accounting abstractions, not a final robot design.

Surplus workers automatically disassemble for parts when needed or when they cannot fit storage, and the worker HUD offers manual surplus disassembly. Each disassembly costs **1 kWh by default**, the selected interpretation of the user's one-energy requirement, and returns only the configured parts. The positive energy charge remains editable balance. Colony reserves and committed shipments are protected; a port's export stock target supplies goods for sale, so an explicit export may consume that stock while preserving the colony reserve. Store/reactivate/disassemble delays, returned parts, manufacture rates and target limits remain external prototype policy. Workers do not consume organic food; Rex does.

Current authored example: one assembly recipe consumes 0.5 kg robotic parts plus 0.5 kg circuits. Core multipliers yield 600 seconds and 0.4 kWh; the worker factory yields 30 seconds and 0.05 kWh before operating/worker power. These numbers are provisional and are not a measured balance claim. [Recipes](../../Rules/recipes.json), [buildings](../../Rules/buildings.json), [energy](../../Rules/energy.json) and [policies](../../Rules/policies.json) are authoritative.

Each building declares an edge access direction. Physical routes use an access point outside its reserved plot, with clearance from [transport.json](../../Rules/transport.json). Couriers and aggregate construction crews walk around occupied plots. Their path planner selects walking and completed transport links by travel cost; no individual worker micromanagement is added. This worker/courier model handles local plot avoidance. Combat vehicles use their separate terrain and cross-sector movement module; a complete traffic simulation remains beyond this model.

## Construction and deployment

Orders reserve available eligible stock after operating buffers. Every delivery still debits one actual source inventory; reservations are not a shared teleporting stockpile. Completed buildings with surplus material can supply a site. Couriers carry bounded batches to the physical destination.

Assigned construction crews must arrive before work advances. Delivered materials enter `ConstructionMaterials`; progress transfers the proportional bill into `InstalledMaterials`. A site can advance with a sufficient partial delivery and then stop when a required material runs out. Undelivered cargo and installed material are distinct. The former full-bill-before-any-work rule is superseded.

The current external phases are **Foundations** (to 20%), **Frame and utilities** (to 55%), **Enclosure and equipment** (to 85%), and **Commissioning** (to 100%). The renderer follows those phases with foundation, frame, equipment, workers and delivered stock. It does not create production or consume material merely because an animation finished. Visible workers remain representative of assigned staffing.

The command core deploys from its carried shuttle kit and initial crew. The shuttle remains docked at the completed core as the emergency escape vehicle. The core's explicit `deployment_defense` capability allows its defensive weapon and sensor coverage during deployment; other unfinished buildings do not operate as completed facilities. This is an armed core, not a timed invulnerability window. Ordinary production, repair and service behavior remains gated by completion.

Disabled building sites pause and retain reservations. Cancellation/refund rules are not added. Costs, required workers, durations, phase boundaries and deployment-defense eligibility remain validated external data.

## Roads and upgrades

**Confirmed progression:** Road → Road + rail → Road + rail + vacuum. The current tiers provide **2×, 4× and 8×** travel speed relative to walking. The configured walking reference is 5 km/h in simulated time. Names, widths, speed multipliers, construction labor, time per length, minimum duration and per-100-metre costs belong to [transport.json](../../Rules/transport.json).

Road construction chooses two endpoints; nearby building access ports and existing road endpoints snap into alignment. Roads must stay within the sector, meet visibility/length constraints and avoid reserved building plots. Crossing completed segments provide routing junctions. Connections are straight segments following rendered terrain, with no bridge, tunnel, terrain-slope feasibility or rail-traffic model.

Roads require physically delivered materials and on-site workers. Upgrades target an existing completed segment, pay that next tier's **additional** material bill and preserve its previous operational speed until the upgrade completes. The route's earlier installed infrastructure remains accounted for separately. Roads do not grant a second inventory or free cargo transfer.

Use **B, L, R** for Road and **B, L, U** for Upgrade road. Selecting a road shows its name, multiplier, length, construction progress, assigned/on-site workers and next-tier requirements. An upgrade can use the selected segment or enter a click-existing-road tool. Right click or Escape cancels placement/targeting. Human commands remain restricted to the home colony; observer views remain read-only.

## Walls and factory families

The authored catalog adds **B, L, W** for a wall polyline: click extends it, selects/moves a joint or inserts a point on an edge; E flips the inside/outside, Enter commits and Backspace removes the last point. Delete removes the selected **planning joint**, not a completed wall. Committed wall levels use ordinary physical construction and upgrades. Their road-grid electrical requirement does not make an unpowered wall cease to be a physical obstacle. These controls and obstruction integration remain subject to the expanded runtime tests.

Wheeled, tracked and mech factories have separate level-one blueprints and level-two/three upgrades. Defense offers laser, kinetic, missile and plasma tower families. Core armament is four large lasers. Ammunition works supplies kilogram cargo for kinetic shells and missiles; energy/plasma use their configured electrical costs. [Weapons, Defenses and Vehicle Outfitting](WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) owns hardpoints, vehicles and the fleet rules; the new catalog alone does not establish that combat has passed gameplay tests.

Closed storage must fit the permitted chassis bill and weapon outfit before paid assembly can begin. Current provisional capacities are 2,000 / 6,000 / 40,000 L for factory levels, 40,000 / 60,000 / 80,000 L for universal core levels, and 200 / 500 / 1,600 L for tower levels. Validation checks legal mount/mass combinations, ammunition and local operating buffers. These capacities supply no free materials. The current largest chassis costs 128 kWh per assembly; a level-one core's 30 kWh battery plus one charged 100 kWh bank can meet that transaction, while generation and operating demand determine recharge time.

## Commands, data and save boundary

The separate Colony dock menu is removed. General actions—save, load, settings, credits, main menu and exit—use **Menu / Escape / F10**. Manual **Launch shuttle** appears only in the selected live own command core's building panel. A stale ID, another building, observer view or neighbor view cannot issue that command. Existing automatic failure behavior is a separate simulation rule.

The current data schema is `prototype-8.0`; simulation snapshots and neighborhood metadata use **format 5** for the new resource seed/nodes, credits, grid/batteries, transactional production, shipments and Rex alongside routed cargo/crews, roads and construction ledgers. Earlier rules and snapshot formats are incompatible: start a new scenario. The neighborhood wrapper and camera metadata do not bypass this check.

`Rules` owns mechanics and balance; `AIFILES` owns AI choices and finite preparation bounds; `Interface/ui.json` owns category/shortcut presentation, including the reserved road and wall tools. `Graphics` owns presentation. The expanded catalog has 59 building definitions, 35 ordinary blueprints, 22 cargo types and 14 recipes. Static Rules validation passes 79 negative cases and full configuration validation passes at this checkpoint; independent combat-module checks are included. Native gameplay, rendered interaction, package integrity and performance still need evidence for this expanded revision. Earlier v0.8 economy/Rex results do not verify the added replication, tiers, walls or fleets.

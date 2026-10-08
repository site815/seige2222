# seige2222 — Weapons, Defenses and Vehicle Outfitting

[Design index](README.md) · [Fleet command](FLEETS_AND_LOGISTICS.md) · [Conflict scope](COMBAT_AND_RAIDING.md)

## Status and implementation boundary

The rules marked confirmed below come from the user's October 5 design additions. They supersede the earlier unspecified roster and tentative fleet limit. The latest authorized expansion implements projectiles, customizable vehicles, fleet commands and layered defenses through separate simulation modules. The [v0.8.1 record](../verification/v0.8.1.json) records bounded native and packaged checks of these systems. Current v0.9 corrections have separate focused evidence below; full native and packaged acceptance remain pending.

The [combat design data](../CombatDesign/README.md) retains the earlier design specification. New runtime combat catalogs in `Rules/` supply weapons, chassis, platforms and fleet policies, with separate schema/version checks and fingerprints. Numerical weapon balance and chassis loadouts remain provisional external values. The catalog is not a substitute for gameplay verification.

## Weapon families

| Family | Confirmed direction | Current prototype and remaining design |
|---|---|---|
| Energy | Requires energy and a targeting solution to fire; no ammunition consumable. | Instant swept shot with provisional range, energy and reload. Additional beam/pulse variants and heat remain open. |
| Kinetic | Includes railguns, cannons and related projectile weapons. | Moving projectile consumes kinetic ammunition cargo and configured energy. Caliber/penetration variants remain open. |
| Missile | A distinct weapon family. | Missile cargo, flight, homing and splash use external values. Countermeasures and a richer lock system remain open. |
| Plasma | A slower energy weapon, between conventional energy and projectile combat; available to both player forces and bugs. PPC and arachnid plasma are behavior references. | Slower projectile uses energy without cargo ammunition; speed, damage and splash are provisional external values. |

**Confirmed:** Weapons must model shots, misses, unintended hits and splash damage. Damage cannot simply be deducted from the selected target at fire time. The overall shield/armor/weapon interaction should support the kind of loadout choices the user associates with Stellaris. These references do not authorize importing another game's assets or copying numerical balance.

## Shields, armor and hull

**Confirmed:** Shields and armor have different effectiveness against different weapon types.

**Proposed resolution:** Each hit resolves against the struck combatant, applying a rule-defined shield/armor/hull response. A weapon definition explicitly states penetration or bypass; damage never bypasses a layer because its name contains a particular word. Shield capacity, regeneration, recharge delay, regeneration energy, armor mass and armor response are editable data.

No exact resistance matrix is confirmed yet. In particular, energy being strongest against armor, kinetics against shields, or missiles bypassing shields must not be inferred as settled rules merely from the Stellaris reference.

Splash requires a radius, falloff and cover policy. Direct impact and area damage must avoid accidental double counting. Friendly and neutral collateral policy remains open: unintended targets must be physically hittable, but the diplomatic consequences and full eligibility matrix are not settled.

## Twelve chassis classes

The four size classes apply across three movement families. Fleet point costs are confirmed and depend on size: small **1**, medium **2**, large **4**, behemoth **8**.

| Family | Small | Medium | Large | Behemoth |
|---|---|---|---|---|
| Wheeled | Wheel count open; 1 point | 6 wheels; 2 points | 8 wheels; 4 points | 12 wheels; 8 points |
| Tracked | 1 point | 2 points | 4 points | 8 points |
| Mech | Leg count open; 1 point | Leg count open; 2 points | 4 legs; 4 points | 8 legs; 8 points, heavy walking-vehicle silhouette |

**Wheeled:** Efficient on roads and easy terrain. Rough terrain can be prohibitively slow or impassable, depending on the terrain and chassis rules.

**Tracked:** Can traverse almost everywhere, moves very slowly, and damages infrastructure it crosses. The current prototype applies configured road wear over actual distance travelled on a road; it does not damage a road merely because a vehicle is stationary. Broader infrastructure eligibility and ownership consequences remain design work.

**Mechs:** Faster than tracked vehicles, somewhat weaker than comparable tracked vehicles, and capable across terrain. Their urban maneuverability should make them easier to use inside settlements. These are relative design requirements; exact speed, armor and clearance values remain open. Buildings, solid obstacles and map boundaries remain physical constraints rather than walk-through surfaces.

## Outfitting

**Confirmed:** Chassis provide weapon hardpoints and storage, with a configuration experience inspired by MechWarrior. A combat-focused loadout trades against cargo capacity and other equipment. Equipment does not currently add fleet points; the confirmed point schedule is chassis-based.

**Confirmed hardpoint dimensions and mounting ratio:** Small envelopes measure 0.5 × 0.5 × 2 m; medium 1 × 1 × 4 m; large 2 × 2 × 8 m. Mount capacity is **1 large = 4 medium = 16 small**. Physical volume scales eightfold between these dimensions, a separate constraint from mount capacity. The command core has two large hardpoint banks carrying one large, two medium and eight small lasers. Laser, kinetic, missile and plasma tower families have three levels on unchanged family footprints.

**Provisional loaded-mass ceilings:** Small / medium / large mounts currently allow 500 / 4,000 / 32,000 kg. Their 0.5 / 4 / 32 cubic-metre envelopes imply the same maximum average loaded density of 1,000 kg per cubic metre. These are consistent fictional balance ceilings, not confirmed engineering capacities; actual module masses remain provisional external values below them. Runtime outfit validation checks per-size module mass, total platform equipment mass and mount area. Ammunition occupies separate finite cargo storage. Installed weapons do not currently subtract mass or envelope volume from that cargo allowance: a shared internal-volume allocation between weapons and cargo remains future work. The prototype does not pack geometric ammunition cells inside each mount.

**Missile capacity:** A hardpoint envelope alone does not establish a missile count. The launcher structure, fictional ammunition size and mass, and reload arrangement must be specified first. Current missile ammunition is tracked in kg, with an externally configured kg-per-shot cost and finite cargo buffer. It should not be presented as a physically validated number of missiles inside a 50 cm mount.

**Proposed configurable constraints:** Each chassis defines hardpoint positions, accepted mount types and size, total equipment mass, internal volume, cargo capacity, reactor output, battery capacity, armor and shield equipment limits. Each module consumes explicit budgets. A valid configuration fits every relevant limit, not merely its fleet-point cost.

The intended interface covers unused and occupied mounts, equipment mass/volume, cargo kg/L, energy generation/storage, passive demand, per-shot cost, ammunition endurance, range, damage, reload and nominal DPS. The current controls show mount/mass budgets, weapon statistics and assembly requirements; invalid actions report their reason. **Request materials/parts** creates a local delivery plan without paying or creating the result. **Assemble vehicle** commits the actual chassis/weapon bill and energy to a timed job. **Install outfit** pays actual local parts and energy; vehicle refits require a nearby completed factory, and removed modules are not refunded. The prototype installs a valid refit immediately after payment; a separate timed field-refit process is not implemented.

For vehicle manufacture, select the core or a matching family factory, choose the chassis and draft modules, then **Request materials**. Couriers supply that building; **Assemble vehicle** succeeds only when its local bill and stored grid energy are available. Assembly progresses with the factory's actual work fraction. The finished vehicle begins unassigned with an empty battery and must charge and join a fleet. Fabrication does not automatically assign fleet capacity. For a vehicle refit, select an own service factory for the parts request and bring the vehicle within service range before installing. For a building refit, the selected own platform receives and pays its own bill. Requesting a new plan replaces that building's previous request; it is not an automatic repeating production queue.

## Fleets and the command center

**Confirmed:** Command-center levels 1, 2 and 3 grant **1, 2 and 3 total fleets**. Every fleet can take defense, escort or privateering assignments. These are shared slots, not separate limits for each mission type.

**Confirmed correction:** A fleet starts with **50 capacity points**, replacing the earlier spoken 100-point example. AI-control infrastructure and other factors may modify that capacity; their exact modifiers remain open. At the unmodified cap, examples include 50 small units, 25 medium units, 12 large units plus 2 small units, or 6 behemoths plus 2 small units.

**Confirmed level-1 direction:** The initial command center is the parked orbital shuttle, ready to leave, with one defense force carried aboard. If that force is deployed when the shuttle launches, it is left behind; it is not duplicated or teleported aboard. This revises the earlier level-1 building presentation. Levels 2 and 3 retain the confirmed 2× and 3× footprint dimensions relative to level 1.

**Current prototype:** The player uses **Board shuttle** when every surviving fleet vehicle is already within the command-core service range. Boarding is immediate, holds one fleet, and preserves each unit and its physical cargo. The button does not route remote units home. Only embarked vehicles evacuate on launch; deployed vehicles remain behind. Additional boarding duration/space constraints and the later simulation of abandoned forces remain open.

Rex's first-person roaming does not grant first-person combat or individual vehicle control. Fleets still fight autonomously after receiving fleet-level mission, movement and aggression orders.

**Retained guard behavior, covered by bounded native checks:** Defensive and escort fleets can reposition around friendly buildings to obtain a clear shot, constrained by their existing sensor coverage, weapon range and guard leash. This does not reveal unseen targets or grant unbounded pursuit. Passive fleets do not pursue; explicit fleet movement remains a separate order.

**v0.9 defensive arrival correction:** A flanking guard continues ordinary, energy-paid movement until its current firing line is clear or it reaches the selected firing point. Being within one metre of that point is no longer sufficient to stop while a friendly building still blocks the shot. Ordinary station orders retain their one-metre arrival tolerance. This changes neither movement speed nor targeting, sensor, range or leash rules; it grants no movement or shots for free. Physical spread, moving obstructions and splash can still cause unintended hits.

The focused `Seige.Combat.DefensiveCornerArrival` regression passed with zero warnings or errors after compile 21 (`Saved/Automation/v09-corner-21/index.json`). It exercises an occluded guard less than one metre from a clear corner, verifies actual movement and a paid shot, preserves the allied building, and checks the unchanged ordinary station tolerance. This establishes the specific arrival fix; full-suite results, developed-colony survival and final Shipping verification are still pending.

## Implementation and acceptance checklist

Use the following checks to assess the current modules. Final tests and rendered review must establish their accepted behavior; this checklist is not a completed verification report.

1. Validate external weapon, defense, chassis and fleet policies, references, hardpoint constraints and upgrade paths.
2. Check damage resolution for buildings, vehicles, bugs and unintended victims independently of rendered models.
3. Verify fixed-step shots with swept travel, first collision, targeting error, guidance and splash. Check energy/ammunition payment and reload state.
4. Exercise terrain-aware movement, formations and bounded guard repositioning. Road wear must use actual traversal, not a timer on a stationary tracked unit.
5. Exercise loadout editing, fleet capacity, missions, physical boarding, and save/load of units, shots, defenses, reloads and random state.
6. Verify at 10×, including thin-target collision, unintended impact, splash falloff, zero-energy firing, depleted ammunition, mixed defenses, route restrictions, exact capacity limits, fleet loss and shuttle launch while a force is away.

Visual projectile interpolation can run every rendered frame. Collision and resource consumption remain authoritative simulation operations; low frame rate must not let fast shots tunnel through targets or cause extra damage.

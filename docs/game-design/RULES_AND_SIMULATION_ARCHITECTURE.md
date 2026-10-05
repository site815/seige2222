# seige2222 — Rules and Simulation Architecture

[Design index](README.md) · [Status definitions](README.md#design-status)

The user has authorized native Unreal development, a playable single-player build, packaging, and a GitHub push. This document records implementation direction and architectural recommendations. The specific verified milestones and remaining checks are tracked in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md); architectural descriptions alone are not completion claims.

## Chosen direction and current limits

**Confirmed user requirement:** Gameplay content and numerical rules must live in editable files outside the main engine execution code. Resource/building values, recipe inputs and outputs, timing, costs, rates, and balance should be changeable without rewriting the simulation's ordinary execution paths.

**Selected development environment:** Native Unreal Engine 5.8.3 on Windows. The initial implementation is single-player. Persistent multiplayer remains the gameplay design baseline and a later implementation stage; there is no claim that networking, authoritative servers, offline progression, or cross-server relocation already work.

**Implementation approach:** Separate data definitions, mutable simulation state, and presentation. Use generic mechanisms to execute external definitions. Concrete prototype choices are reversible implementation assumptions until reviewed; they do not silently settle the full game's open mechanics.

## Responsibilities

| Layer | Responsibility | Keep outside it |
| --- | --- | --- |
| Editable definitions | Resource and item identities, building capabilities, recipes, rates, costs, scenario setup, and numerical tuning | Live colony inventories, current orders, and player-owned runtime state |
| Loading and validation | Parse definitions, resolve IDs, check units/ranges, and report invalid content before starting the scenario | Hidden replacement balance values that disguise broken definitions |
| Simulation state | Buildings, stock by location, robots/workforce, jobs, cargo in transit, health, threats, and elapsed simulation time as supported by the slice | Rendering objects as the only copy of economically important state |
| Generic simulation execution | Apply construction commands and advance the supported staffing, production, transport, repair, and combat rules | A separate hardcoded behavior branch for every resource, recipe, or building |
| Unreal presentation and input | Render the current state, display explanations, select/place buildings, and submit player commands | Awarding production or moving inventory merely because an animation finished |
| Persistence and future authority | Save/load versioned state locally; later validate and own shared outcomes in the selected server design | An assumption that client-local simulation is sufficient authority for multiplayer |

The table expresses the intended separation. Only systems included in [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) belong in the initial implementation. A named layer is not evidence that a full implementation exists.

## Definitions and supported behavior

**Current prototype files:**

| Folder | Definitions and responsibility |
| --- | --- |
| `Rules` | [Resources](../../Rules/resources.json), [recipes](../../Rules/recipes.json), [buildings](../../Rules/buildings.json), [policies](../../Rules/policies.json), and [scenario](../../Rules/scenario.json) define the shared economy, simulation tuning, threats, initial stock, and deposit template. |
| `AIFILES` | [Colony controller](../../AIFILES/colony_ai.json) priorities, timing, and placement search; a finite [developed-colony preset](../../AIFILES/developed_start.json). The separate folder is a confirmed user requirement. AI orders still pay ordinary costs and use the same simulation. |
| `Interface` | [ui.json](../../Interface/ui.json) defines construction groups and shortcuts, resource-overlay references, descriptions, and credits. These are presentation definitions, not a second economy. |
| `Graphics` | [scene.json](../../Graphics/scene.json) defines logical-to-rendered scale, camera pitch/zoom/clearance and orbit sensitivity, regional-view threshold, terrain relief/pads, terrain/cloud materials and lighting, bounded vegetation counts/scales, and ten nature-asset roles. It does not replace logical simulation values. |

All four use explicit versions. Catalog records have stable IDs; policies contain numerical tuning and selectors for supported behaviors. New types of behavior can require code; changing supported content and balance must not require rewriting engine execution. See [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md), [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md), and [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for represented behavior and verification boundaries.

**Definition contract:**

- Use stable IDs for resources, products, buildings, recipes, and scenarios. Display names can change without becoming save-file keys.
- A building declares capabilities and refers to recipes. A recipe declares its input/output quantities and duration. The simulation applies the same transaction rules to all definitions using that mechanism.
- Store rate ceilings, worker demand, health, repair inputs, ranges, movement speeds, and threat tuning in external data where those systems are implemented.
- Keep starting stocks and placed assets in scenario data, separate from universal item definitions.
- State units explicitly: quantity, transport capacity, distance, simulation time, and any real-time conversion. A number alone does not establish which clock a timer uses.
- Preserve actual locations for inventory and cargo. The prototype already keeps building inventories and courier cargo separately; a definition ID is not a colony-wide teleporting inventory.
- Define the permitted behavior vocabulary in code or a validated schema. Editing numbers or composing existing behaviors should not require engine changes; inventing a new simulation mechanism can require a code extension.

Do not place executable arbitrary scripts in balance files merely to satisfy the external-data requirement. A declarative format is sufficient for the initial resource, production, workforce, and threat systems.

### v0.5 construction and robot services

Current source Rules use `prototype-5.0`. Buildings declare `construction_seconds`, `construction_workers`, `robot_support_capacity` and `staffing_priority`. Policies select `reserved_core_physical_delivery` and `local_capacity_and_maintenance`. The scenario carries an exact `starting_deployment_materials` core kit, separate from operating inventory and escape-shuttle cargo. Native and JS validation check the kit, builders, storage and a viable first support expansion.

An order reserves uncommitted core stock after protecting operating buffers. Materials remain at their physical source until tagged couriers take them to a separate site inventory. Once the full bill arrives, assigned builders advance normalized progress using external time and worker requirements. Completion embodies those materials in the building. Destroyed sites lose their delivered materials; surviving inbound cargo returns to source/core. Lost cargo creates a replacement requirement that waits for real stock.

Construction sites contribute temporary jobs. Automatic staffing uses external `staffing_priority` for both supplied construction and operating jobs, with stable entity order for ties. Current numerical priorities are core 0, service 1, defense 2, sensor 3 and other workplaces 4; lower numbers receive workers first. This preserves service expansion and defenses before general industry without manual job assignment. Paused sites retain reservations. Sites cannot produce, repair, sense or fire before completion. The short core deployment has its carried crew and support; it grants no separate alien-protection period.

Robot growth remains driven by jobs and local assembly inputs, now also bounded by usable support capacity. Completed, enabled and staffed core/service buildings allocate berths and consume local maintenance supplies. Unsupported or unsupplied robots contribute the configured reduced efficiency. Losing capacity does not delete robots; job-driven retirement remains separate. Charging is abstract service capacity, not a simulated power grid or individual battery.

Simulation save format 2 stores construction flags/progress, site materials, courier purpose and maintenance results, together with operating inventories, health, enabled/paused state, production clocks, population/upkeep/dispatch timers, wave and roaming schedules, RNG state, IDs and outcome flags. Staffing and support assignments derive from the saved state. Reservations likewise derive from living site costs minus their delivered materials and tagged incoming cargo; paused sites retain their reservations. Loads parse and validate a temporary candidate before replacing the active colony, including construction consistency and destination payload limits. Outer neighborhood format 2 is independent. Rules/AI fingerprints reject incompatible saves. Developed scenarios run actual construction, logistics, services and threats from a finite seed within external preparation limits. Loading initializes only controller definitions before restoring the snapshot, without replaying preparation.

Static validation passes 29 negative Rules cases and the full configuration check. The final native suite passed 29 tests (28 clean, one with an Unreal editor background HTTP warning; zero failed or unrun), including construction reservation/delivery, deployment, service expansion/local upkeep, deterministic state continuation and developed setup. Its first-objective scenario succeeded at 435 simulation seconds. Evidence: `Saved/Automation/v05-final/index.json`. The final Shipping route passed 63 stages with zero failures and exit 0; all nine staged JSON files match source byte hashes, and 73 process-tree samples each showed zero TCP/UDP endpoints. These bounded observations and native persistence coverage do not imply a separate packaged save/load roundtrip. See the [development report](../DEVELOPMENT_REPORT.md).

### Weapon and information definitions retained from v0.4

The v0.4 revision introduced `prototype-4.0`. Current definitions retain `weapon_name`, `damage_per_shot`, and `reload_seconds`; nominal DPS is derived as damage divided by reload time. Armed definitions require a name, positive damage, positive attack range, and a reload interval no shorter than the simulation step. Unarmed definitions use an empty weapon name and zero damage, reload, and attack range. A separate `damage_per_second` building override is rejected so the displayed values cannot disagree with combat.

Combat now applies individual shots. A ready weapon fires at the nearest living visible bug in range, then reloads. Staffing/maintenance efficiency changes reload progress, rather than silently changing the displayed damage per shot. Idle time stores one ready shot, not accumulated damage. Building cooldown and its most recent shot event are persisted. Core and turret defaults retain their earlier nominal DPS, but discrete shots change damage timing and overkill; older continuous-combat balance results do not fully validate this revision.

`power_usage_kw` and `power_generation_kw` are explicitly zero for every current definition. Both validators reject nonzero power values because this prototype has no separate power-grid simulation. The information panel must say that plainly rather than imply free energy or invent consumption.

The shared simulation information function supplies six sections for blueprints and live instances: Overview, Weapons, Power, Production, Resources, and Maintenance. It reports construction costs, jobs, health, footprint/ranges, shot damage/reload/DPS, local inventory including required zero-stock inputs, physical inbound cargo, recipe quantities/timing, extraction, repairs, and local support capacity/upkeep. Construction dossiers show required builders, duration, progress and zero-stock cost items. Presentation supplies its distance scale when formatting metre values. The HUD consumes these rows rather than maintaining an independent table of balance values.

Historical v0.4 evidence: the `prototype-4.0` fingerprint intentionally rejected prior-rule saves: start a new scenario. Outer scenario format 2 and optional camera fields do not bypass simulation compatibility. Rules passed static validation and 22 deliberately invalid cases. Final native verification passed 27 tests (26 clean plus one with editor background HTTP warnings), with zero failures/unrun tests. The Shipping route passed 53 stages and exit 0; ten staged files matched source hashes, and 45 process-tree samples each showed zero TCP/UDP endpoints. The failed editor request/retry to `google.com/generate_204` is separate from gameplay assertions and those bounded Shipping observations. See the [development report](../DEVELOPMENT_REPORT.md).

## Validation and reload policy

**Implemented pre-build validation:** Run `node Tools/validate_configuration.mjs` from the project root. [The validator](../../Tools/validate_configuration.mjs) includes the existing Rules checks and validates AI building/resource references, timing and search bounds, developed stock capacity and necessary setup costs, UI building coverage, shortcut conflicts, summary resource IDs, and credits. Graphics validation checks version, numerical bounds, vegetation counts, ordered camera/pad limits, supported terrain resolution, ten nature roles, project asset references, and the supported bundled cloud material. It runs before Unreal in [build.ps1](../../Tools/build.ps1) and in the [GitHub workflow](../../.github/workflows/rules.yml). Invalid definitions fail with a field-specific error rather than silently substituting balance values. Runtime loaders also reject invalid supported definitions. A matching asset package still needs Unreal import/cook and visual checks.

**Verified configuration check:** The current complete definition set passes; focused mutations with unknown AI/UI/resource references, a duplicate shortcut, and invalid AI timing are rejected. These static checks do not prove every AI layout is placeable or an economy survives. Native tests exercise actual placement, production, save continuation, and a first-objective strategy; their results are recorded in the scope.

Not every production cycle is invalid: recycling could intentionally form a loop. The relevant starter check is whether useful production is reachable from actual starting assets and inputs. Do not claim viability from a graph alone without considering labor, throughput, consumption, and physical delivery.

**Current reload policy:** Load a coherent definition set at scenario startup or restart; restart the application after interface or graphics edits. Live hot reload is not implemented. Saves require matching Rules and AI fingerprints. Changing rules underneath existing cargo, queued production, and saved state needs a deliberate migration policy, especially before multiplayer.

**Packaging contract:** [The module build file](../../Source/Seige/Seige.Build.cs) stages `Rules`, `AIFILES`, `Interface`, and `Graphics` as loose files beside the target binary. The runtime first checks the project directory, then the executable directory. Keep these folders with the packaged executable. v0.5 verified all nine external JSON files byte-identical to source and loaded them in its packaged 63-stage interaction check. Historically, v0.4 verified ten loose files and passed 53 stages. The earlier v0.3 package separately passed its 28-stage route. v0.2 separately passed a controlled packaged balance edit; that edit experiment was not repeated for v0.3. Evidence is recorded in the [development report](../DEVELOPMENT_REPORT.md).

## State, saving, and future multiplayer

**Current local ownership:** Each occupied scenario cell owns one independent simulation instance. The center is human-controlled or AI-controlled for observation; up to eight neighbors run the same economy and local threat rules. Their inventories and combat are independent. Rendering them in a common 3×3 view does not implement cross-sector travel, trade, or shared authority.

The scenario clock advances only during active play while unpaused. Setup, human core placement, main menu, settings, and credits do not advance colony simulations. Core placement uses the simulation's placement validation before play starts. These confirmed single-player controls do not determine how a future persistent server behaves.

Local save/load writes the center and occupied neighbor snapshots with scenario slots, camera, speed, pause state, and AI fingerprints. A generation directory and atomic metadata replacement protect the active save from partial writes. Loading validates all candidate snapshots before replacing live state. Native continuation and neighborhood save/load tests have passed; long-term migration and recovery guarantees remain later work.

**Verified v0.3 extension:** Camera yaw and pitch are optional format-2 fields, with bounded validation and stable defaults for older saves. Logical simulation coordinates stay unchanged while presentation maps each unit to six Unreal centimeters. Native tests cover camera/terrain mathematics and saved orbit compatibility; the packaged rendered route covers actual perspective interactions. No separate v0.3 packaged save/load roundtrip was run. Native success and packaged interaction success establish those distinct boundaries, not a long-term migration guarantee.

**Recommended future boundary:** Reuse the definition model and command/state concepts when choosing authoritative multiplayer execution. Clients would submit intentions; the authoritative owner would resolve shared inventories, production, combat, and travel. A trusted service remains an architectural proposal, not a user-selected deployment or networking design.

Before persistent multiplayer, resolve ruleset versions, save migration, command validation, cross-sector transactions, reconnects, offline simulation, and relocation transfer ownership. These are future engineering tasks, not automatic benefits of using external JSON or a local subsystem. Local editable balance files do not imply that multiplayer clients can alter authoritative balance.

## Prototype deviations must remain explicit

Construction now uses reserved core stock, physical delivery and aggregate workers. Cancellation, individual builder travel and partially executable bills remain absent. Aggregate staffing, simplified courier movement, threat formulas, retirement, abstract charging capacity and the absent power/happiness systems remain explicit prototype boundaries.

Externalizing these choices makes them easier to tune; it does not make them approved final game rules. Keep observed implementation behavior, candidate design, and verified test results distinct.

## Related documents

- [First Playable Scope](FIRST_PLAYABLE_SCOPE.md)
- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Building Proposal](BUILDING_PROPOSAL.md)
- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)
- [Open Decisions and Design Evolution](DESIGN_DECISIONS.md)

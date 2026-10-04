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
| `Interface` | [ui.json](../../Interface/ui.json) defines construction groups and shortcuts, top-bar resource references, descriptions, and credits. These are presentation definitions, not a second economy. |

All three use explicit versions. Catalog records have stable IDs; policies contain numerical tuning and selectors for supported behaviors. New types of behavior can require code; changing supported content and balance must not require rewriting engine execution. See [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md) and [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for represented behavior and verification boundaries.

**Definition contract:**

- Use stable IDs for resources, products, buildings, recipes, and scenarios. Display names can change without becoming save-file keys.
- A building declares capabilities and refers to recipes. A recipe declares its input/output quantities and duration. The simulation applies the same transaction rules to all definitions using that mechanism.
- Store rate ceilings, worker demand, health, repair inputs, ranges, movement speeds, and threat tuning in external data where those systems are implemented.
- Keep starting stocks and placed assets in scenario data, separate from universal item definitions.
- State units explicitly: quantity, transport capacity, distance, simulation time, and any real-time conversion. A number alone does not establish which clock a timer uses.
- Preserve actual locations for inventory and cargo. The prototype already keeps building inventories and courier cargo separately; a definition ID is not a colony-wide teleporting inventory.
- Define the permitted behavior vocabulary in code or a validated schema. Editing numbers or composing existing behaviors should not require engine changes; inventing a new simulation mechanism can require a code extension.

Do not place executable arbitrary scripts in balance files merely to satisfy the external-data requirement. A declarative format is sufficient for the initial resource, production, workforce, and threat systems.

## Validation and reload policy

**Implemented pre-build validation:** Run `node Tools/validate_configuration.mjs` from the project root. [The validator](../../Tools/validate_configuration.mjs) includes the existing Rules checks and validates AI building/resource references, timing and search bounds, developed stock capacity and necessary setup costs, UI building coverage, shortcut conflicts, summary resource IDs, and credits. It runs before Unreal in [build.ps1](../../Tools/build.ps1) and in the [GitHub workflow](../../.github/workflows/rules.yml). Invalid definitions fail with a field-specific error rather than silently substituting balance values. Runtime loaders also reject invalid supported definitions.

**Verified configuration check:** The current complete definition set passes; focused mutations with unknown AI/UI/resource references, a duplicate shortcut, and invalid AI timing are rejected. These static checks do not prove every AI layout is placeable or an economy survives. Native tests exercise actual placement, production, save continuation, and a first-objective strategy; their results are recorded in the scope.

Not every production cycle is invalid: recycling could intentionally form a loop. The relevant starter check is whether useful production is reachable from actual starting assets and inputs. Do not claim viability from a graph alone without considering labor, throughput, consumption, and physical delivery.

**Current reload policy:** Load a coherent definition set at scenario startup or restart; restart the application after interface edits. Live hot reload is not implemented. Saves require matching Rules and AI fingerprints. Changing rules underneath existing cargo, queued production, and saved state needs a deliberate migration policy, especially before multiplayer.

**Packaging contract:** [The module build file](../../Source/Seige/Seige.Build.cs) stages `Rules`, `AIFILES`, and `Interface` as loose files beside the target binary. The runtime first checks the project directory, then the executable directory. Keep these folders with the packaged executable. A controlled packaged balance edit remains a separate acceptance check; staging declarations alone do not prove it works in a delivered build.

## State, saving, and future multiplayer

**Current local ownership:** Each occupied scenario cell owns one independent simulation instance. The center is human-controlled or AI-controlled for observation; up to eight neighbors run the same economy and local threat rules. Their inventories and combat are independent. Rendering them in a common 3×3 view does not implement cross-sector travel, trade, or shared authority.

The scenario clock advances only during active play while unpaused. Setup, human core placement, main menu, settings, and credits do not advance colony simulations. Core placement uses the simulation's placement validation before play starts. These confirmed single-player controls do not determine how a future persistent server behaves.

Local save/load writes the center and occupied neighbor snapshots with scenario slots, camera, speed, pause state, and AI fingerprints. A generation directory and atomic metadata replacement protect the active save from partial writes. Loading validates all candidate snapshots before replacing live state. Native continuation and neighborhood save/load tests have passed; long-term migration and recovery guarantees remain later work.

**Recommended future boundary:** Reuse the definition model and command/state concepts when choosing authoritative multiplayer execution. Clients would submit intentions; the authoritative owner would resolve shared inventories, production, combat, and travel. A trusted service remains an architectural proposal, not a user-selected deployment or networking design.

Before persistent multiplayer, resolve ruleset versions, save migration, command validation, cross-sector transactions, reconnects, offline simulation, and relocation transfer ownership. These are future engineering tasks, not automatic benefits of using external JSON or a local subsystem. Local editable balance files do not imply that multiplayer clients can alter authoritative balance.

## Prototype deviations must remain explicit

The initial construction command pays directly from the core inventory as a simplification. Record that in the scope rather than portraying it as complete physical construction delivery. The same applies to aggregate staffing, simplified movement, threat formulas, population reduction, and any missing power or robot-needs systems.

Externalizing these choices makes them easier to tune; it does not make them approved final game rules. Keep observed implementation behavior, candidate design, and verified test results distinct.

## Related documents

- [First Playable Scope](FIRST_PLAYABLE_SCOPE.md)
- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Building Proposal](BUILDING_PROPOSAL.md)
- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)
- [Open Decisions and Design Evolution](DESIGN_DECISIONS.md)

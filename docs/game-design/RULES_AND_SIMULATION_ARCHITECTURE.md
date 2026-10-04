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

**Current prototype files:** [resources.json](../../Rules/resources.json), [recipes.json](../../Rules/recipes.json), [buildings.json](../../Rules/buildings.json), [policies.json](../../Rules/policies.json), and [scenario.json](../../Rules/scenario.json). They currently use JSON with an explicit version string. Catalog records have stable IDs; policies contain both numerical tuning and selectors for supported behaviors. Their presence is verified, but runtime validation and packaging must be tested separately. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for the represented content.

**Proposed definition contract:**

- Use stable IDs for resources, products, buildings, recipes, and scenarios. Display names can change without becoming save-file keys.
- A building declares capabilities and refers to recipes. A recipe declares its input/output quantities and duration. The simulation applies the same transaction rules to all definitions using that mechanism.
- Store rate ceilings, worker demand, health, repair inputs, ranges, movement speeds, and threat tuning in external data where those systems are implemented.
- Keep starting stocks and placed assets in scenario data, separate from universal item definitions.
- State units explicitly: quantity, transport capacity, distance, simulation time, and any real-time conversion. A number alone does not establish which clock a timer uses.
- Preserve actual locations for inventory and cargo as physical logistics becomes implemented. A definition ID is not a colony-wide teleporting inventory.
- Define the permitted behavior vocabulary in code or a validated schema. Editing numbers or composing existing behaviors should not require engine changes; inventing a new simulation mechanism can require a code extension.

Do not place executable arbitrary scripts in balance files merely to satisfy the external-data requirement. A declarative format is sufficient for the initial resource, production, workforce, and threat systems.

## Validation and reload policy

**Recommended validation:** Reject duplicate IDs, unknown references, invalid quantities, nonpositive durations/rate intervals, malformed costs, and incompatible capability references. Report the definition and field responsible. Validate the chosen starting scenario for unavailable dependencies separately from parsing the catalog.

Not every production cycle is invalid: recycling could intentionally form a loop. The relevant starter check is whether useful production is reachable from actual starting assets and inputs. Do not claim viability from a graph alone without considering labor, throughput, consumption, and physical delivery.

**Initial policy recommendation:** Load a coherent definition set at scenario startup or restart. Live hot reload is not required. Changing rules underneath existing cargo, queued production, and saved state needs a deliberate migration policy, especially before multiplayer.

**Packaging requirement:** Preserve editable runtime rule files in the packaged build and document the load location. Files that are only present in the development checkout do not satisfy this requirement. Validate the packaged game with one controlled balance edit before claiming that this works.

## State, saving, and future multiplayer

**Prototype approach:** One local simulation owns the colony's state. Input requests changes and the visual world reflects outcomes. Local save/load, if included in the slice, is a development feature rather than a final decision about pause or persistence in the complete game.

**Recommended future boundary:** Reuse the definition model and command/state concepts when choosing authoritative multiplayer execution. Clients would submit intentions; the authoritative owner would resolve shared inventories, production, combat, and travel. A trusted service remains an architectural proposal, not a user-selected deployment or networking design.

Before persistent multiplayer, resolve ruleset versions, save migration, command validation, cross-sector transactions, reconnects, offline simulation, and relocation transfer ownership. These are future engineering tasks, not automatic benefits of using external JSON or a local subsystem. Local editable balance files do not imply that multiplayer clients can alter authoritative balance.

## Prototype deviations must remain explicit

The initial construction command may pay directly from the core inventory as a simplification. Record that in the scope rather than portraying it as complete physical construction delivery. The same applies to aggregate staffing, simplified movement, threat formulas, population reduction, and any missing power or robot-needs systems.

Externalizing these choices makes them easier to tune; it does not make them approved final game rules. Keep observed implementation behavior, candidate design, and verified test results distinct.

## Related documents

- [First Playable Scope](FIRST_PLAYABLE_SCOPE.md)
- [Resource Proposal](RESOURCE_PROPOSAL.md)
- [Building Proposal](BUILDING_PROPOSAL.md)
- [Multiplayer, Persistence, and Development](MULTIPLAYER_AND_DEVELOPMENT.md)
- [Open Decisions and Design Evolution](DESIGN_DECISIONS.md)

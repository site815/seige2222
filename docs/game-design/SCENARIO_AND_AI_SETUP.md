# seige2222 — Scenario and AI Setup

[Design index](README.md) · [Status definitions](README.md#design-status)

This document records the confirmed local setup flow and the current prototype implementation. It does not turn its finite map, AI presets, or numerical balance into final full-game rules.

## Starting a scenario

**Confirmed direction:** The game opens to a main menu with single-player setup, loading, settings, credits, and exit. The player configures a 3×3 neighborhood and can inspect it at a broad zoom. Human play starts with time paused while choosing the central command-core site. An AI-controlled center provides an observer mode.

| Cell | Current choices | Meaning |
| --- | --- | --- |
| Center | Player, starting AI, developed AI | Player enters paused landing selection. Either AI choice starts observation after setup. |
| Each of eight neighbors | Empty, starting AI, developed AI | Empty is the default. An occupied slot runs an independent local colony simulation. |

The default setup is one player center and eight empty neighbors. Before a human landing, the view surveys the resource deposits; placement uses the simulation's sector-edge and deposit-clearance validation. This creates the player's only core. It does not introduce a second core or move an established colony.

## Map and information

**Current prototype:** Sectors are 600 m square; the nine-sector view is 1.8 km across. Each colony uses the same template of 25 irregularly clustered resource nodes. These are editable scenario values and a repeated template, not randomized planetary geography. The broader wilderness and remote-outpost proportions remain design targets in [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md).

After human landing, live information uses finite sensor coverage. Seeing sector borders does not reveal its buildings or threats. Observer mode shows the simulated colonies so the player can watch AI behavior; this is an observation tool, not a scouting advantage in human play.

## AI behavior and editable files

**Confirmed requirement:** AI definitions live separately in `AIFILES`. Shared resource, building, production, and threat rules stay in `Rules`.

| File | Current role |
| --- | --- |
| [colony_ai.json](../../AIFILES/colony_ai.json) | Ordered building targets, decision interval, action budget, placement search, and sensor expansion. |
| [developed_start.json](../../AIFILES/developed_start.json) | Finite initial core stock and population for developed colonies; initial buildings pay ordinary costs from that stock. |
| [Rules/scenario.json](../../Rules/scenario.json) | Ordinary starting inventory and population, core, deposit template, world extent, and seed. |
| [Interface/ui.json](../../Interface/ui.json) | Construction groups and shortcut mappings, displayed summary resources, descriptions, and credits. |

A starting AI begins with the ordinary colony start. Both AI types seek nearby matching deposits, place industry and defenses, extend working sensor coverage when necessary, and replace missing targets when affordable. They use ordinary placement commands and construction costs. Staffing, physical couriers, input consumption, repair, and alien pressure follow the same simulation as the player. The developed preset grants setup assets once; there is no recurring free inventory.

This is a deterministic construction controller. It does not implement diplomacy, rival fleet planning, cross-sector combat, or trade. Each occupied slot has its own stock and local alien threats. A common rendered neighborhood does not create shared movement or economy.

## Time, settings, and saves

Only active, unpaused play advances the colony simulations. Main menu, setup, landing, settings, and credits stop their clocks. Settings currently offer Low/Medium/High/Ultra graphics quality and windowed/fullscreen selection. Observer mode supports camera movement, pause, and saving while disabling player construction and colony commands.

Local scenario saves include every occupied colony, scenario selections, camera, speed, pause state, and AI configuration fingerprints. Loading validates all snapshots before replacing the active scenario. AI cadence derives from saved simulation time; it has no separate hidden timer. Changed Rules or AI definitions invalidate incompatible saves. Restart scenarios to apply definition changes, and restart the application after interface edits; live reload and save migration are not implemented.

These single-player controls do not establish how a future persistent multiplayer world pauses, simulates offline colonies, or transfers a relocating core.

## Validation and remaining work

Run `node Tools/validate_configuration.mjs` before building. It checks Rules plus AI/UI references, bounds, construction-menu coverage, shortcut conflicts, and developed preset capacity and necessary cost bounds. [Tools/build.ps1](../../Tools/build.ps1) and the [GitHub workflow](../../.github/workflows/rules.yml) run it automatically. Geometric feasibility and long-term survival still require simulation tests and playtesting.

**Verified:** The current definition set passes static validation, and focused invalid-reference/shortcut/timing mutations are rejected. The sixteen-test native suite includes AI startup, deterministic continuation, core placement, frontend/observer controls, and neighborhood save/load. A separate rendered route completed twenty-three stages without failures. See [First Playable Scope](FIRST_PLAYABLE_SCOPE.md) for evidence and the remaining package checks.

**Later work:** Cross-sector travel and physical transfer, neighboring extraction, trade, privateering, relocation, abandoned territories, richer AI strategies, distinct sector generation, and persistent multiplayer. Changing occupied AI slots during play is not supported. No existing test result establishes those features or indefinite AI survival.

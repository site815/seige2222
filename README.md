# seige2222

A single-player Unreal Engine colony builder: establish an industry of robotic workers in an Earth-like wilderness, move physical materials, and defend the colony. Detailed futuristic industry contrasts with the natural landscape.

**v0.8.0 playable build.** The current source includes the resource economy, shared power, physical construction, Rex, tiered facilities, worker replication/storage, walls, outfittable vehicles and fleets. External definitions pass **79 negative Rules cases** and the full configuration/combat checks. The **117-stage Shipping** gameplay checkpoint passed without failures, including corrected command-shuttle selection and developed-neighbor preparation. Subsequent rendering and configurable-budget changes passed affected native tests; the final package passed startup, offline and four display checks. The [development report](docs/DEVELOPMENT_REPORT.md) and [verification record](docs/verification/v0.8.0.json) identify each tested revision.

Launch **v0.8.0** with `Play-seige2222.cmd`, which opens `Builds/v0.8.0/Windows/Seige.exe`. Start a new scenario; older saves are incompatible. The retained v0.7 package remains available for its older saves. Final v0.8 static views averaged **30.5–48.1 FPS** at 3840×1600, Medium, 100% render resolution and 10× simulation on the test machine; this is not a sustained-60-FPS claim or a crowded-colony stress test.

## Current v0.8 gameplay

**Scenario and landing.** Configure a 3×3 neighborhood with eight Empty, Starting AI or Developed AI neighbors. The center can be Player or AI observation. Background bugs and periodic invasions are independently configurable, both initially enabled. Player landing pauses the scenario while deposits are surveyed and the command site is chosen. New gameplay begins at **1×**; test orchestration uses 10× without granting materials or changing construction durations.

Developed choices prepare their paid construction history on a progress screen. Preparation advances in bounded frames, can be cancelled, and replaces the active scenario only when every chosen region is ready. Several developed regions can still take minutes to prepare; no precomputed cache is used. Empty and starting-only scenarios avoid this historical preparation.

Every region, including empty regions, contains **three distinct standard and two distinct rare deposits**, seeded within a centered square covering 75% of its area. The standard pool is water, metal ore, silica and biomass; the rare pool is rare metals, radioactive ore, crystalline material and hydrocarbons. Empty regions have no invented core or workforce. Each rendered sector is 3.6×3.6 km; later resource and hostile intelligence respects sensor coverage.

**Physical economy.** A finite landed kit starts with **zero Galactic credits**. Establish extraction near a real deposit, connect roads, solar generation, worker support and a trading port, then export available goods to buy missing inputs. Trading ports have three levels, prices and timed physical shipments; one Galactic credit is anchored to the value of one kilogram of gold. It is an external-trade account, not a shared inventory. Raw and manufactured goods use kg or L, storage uses litres, workers are indivisible cargo counts, and grid electricity uses kWh.

Connected completed roads share generation and batteries. Passive loads consume energy over time; production commits its actual local ingredients and transaction energy, including multi-output recipes. Chains produce construction alloys, conductors, industrial glass, fuel, plastic, organic food, circuits, robotic parts, batteries, AI chips and fusion assemblies. Quantities, prices, durations and efficiencies remain provisional external balance.

**Construction and upgrades.** Building orders reserve eligible physical stock. Couriers carry it to the site, crews walk to access ports, and construction progressively installs delivered materials through foundations, framing, enclosure and commissioning. Ordinary sites cannot operate before completion. The command shuttle has explicit deployment defense/sensing and begins with six workers; its current deployment duration is 900 simulation seconds at full efficiency.

Core level one is the parked shuttle. Levels two and three expand its width to 2× and 3×; all levels reserve the level-three plot. Vegetation clears across the **reserved expansion plot**, while foundations, grading and foundation dirt use the **actual built footprint**. Solar arrays, three vehicle-factory families, four tower families and walls also have level-one through level-three definitions. Their largest plot is reserved from placement; upgrades consume additional materials and work.

**Workers and production.** Workers fill jobs automatically, without individual unit orders. Select the core to choose its slow universal replicator output, including workers; a dedicated worker factory assembles workers faster with lower energy use. Every batch still consumes physical materials and energy. Core worker demand takes priority unless a staffed operational worker factory supplies it. Service bays expand supported capacity and consume local maintenance parts.

Surplus workers enter physical storage. The worker HUD controls the colony's spare-worker production target and manual disassembly; trading ports have separate worker export-stock targets. Whole stored workers can travel and trade. Automatic recycling handles parts shortages or insufficient storage, respecting required workers and protected reserves. Disassembly costs **1 kWh by default**, configurable with the other prototype rates. Workers do not eat organic food; Rex does.

**Transport and walls.** Road, road-plus-rail and road-plus-rail-plus-vacuum give powered 2×/4×/8× travel relative to walking. Construction and upgrades need delivered materials and on-site crews; earlier tiers remain available during an upgrade. Routes avoid reserved plots. Roads have physical durability, with wear/repair rules in the transport module. Wall plans use editable joints and a selected inside/outside; committed sections require ordinary paid construction. Unpowered walls remain physical obstacles.

**Defense and fleets.** Runtime catalogs define energy, kinetic, missile and plasma weapons, shield/armor behavior, and twelve wheeled/tracked/mech chassis across four sizes. Fleet capacity starts at **50 points**; small/medium/large/behemoth chassis use **1/2/4/8 points**. Core levels allow **1/2/3 total fleet slots**, shared among roles. Weapon mounting area follows **1 large = 4 medium = 16 small**, with separate mass limits.

Selected own factories and platforms expose chassis and hardpoint controls. Request materials creates a physical delivery plan; assembling or installing equipment pays the delivered bill and energy. Fleet commands include defense, movement, escort, aggression and privateering against populated neighbors. The current world bridge models transit, combat, stolen cargo and return; these paths passed the packaged route, while broader balance remains provisional. Boarding requires surviving fleet vehicles at the command service port before evacuation. Ground vehicles do not automatically escape with the shuttle.

**Rex and presentation.** Select the own core, choose **Find Rex**, then **Roam as Rex** for optional first-person walking at 1×. Rex otherwise walks autonomously, consumes local organic food and grants a capped nearby morale benefit while fed. His original Blender interpretation and generated coat texture contain no private reference photographs. The control and feeding paths passed native and packaged checks. His likeness and animation remain a stylized prototype, not a photorealistic reconstruction.

The floating HUD shows resources, alerts and building dossiers: workers, storage, power/batteries, weapons, production and maintenance. Construction and operating workers are representative visual agents; the population is not individually micromanaged. Menus pause the local scenario and restore its prior pause state; the construction overlay keeps time running.

## Controls and saves

| Control | Action |
| --- | --- |
| WASD / arrows | Pan relative to the camera; WASD walks in Rex view |
| Mouse wheel | Zoom; enter a region from the map |
| Q / E; middle-mouse drag | Orbit; drag also tilts |
| Home | Return to the colony view |
| B, then R / I / L / D | Construction: extraction / industry / logistics / defense |
| Category, displayed letter | Choose a blueprint |
| Left click / right click | Select or place / cancel construction targeting |
| B, L, R / B, L, U | Road / upgrade road |
| B, L, W | Wall plan: click joints, E flips inside, Enter commits, Backspace undoes, Delete removes a selected joint |
| + / − | Cycle Paused / 1× / 5× / 10× forward or backward |
| Space | Pause/resume the previous nonzero speed |
| Escape / F10 / Menu | Cancel active tools first / open the paused game menu; F10 opens it directly |
| F5 / F9 | Save / load |
| Selected own core | Production, spare-worker target, Find Rex and Launch shuttle |
| Selected own platform/factory | Fleets, chassis and hardpoints |
| Rex view | Mouse look; Escape returns to the colony camera; F10 opens Menu |

Current saves use **format 5** with matching `prototype-8.0` Rules and AI fingerprints. **Start a new v0.8 scenario; earlier saves are incompatible.** Loading validates all colony snapshots and metadata before replacing live state. Shipping saves live in `%LOCALAPPDATA%/seige2222/Saved/SaveGames`; editor saves use the project's `Saved/SaveGames`. Saving from Rex's view retains the colony camera for later loading. Keep the v0.7 package for its compatible older scenarios.

Settings offer one **Medium** profile, native-resolution borderless **Full window**, and selectable **Windowed** sizes. There is no exclusive fullscreen. Render resolution is independently adjustable from 50–100%; UI text remains native. Full-resolution rendering uses TAA and reduced resolution uses TSR. Display choices persist locally. Persistent multiplayer, remote player extraction, a general inter-colony trading economy and full relocation/scavenging remain later work.

## Data and development

`Rules/` owns resources, recipes, buildings, policies, scenario generation, transport, energy, external trade, companions, walls, combat, weapons and chassis. `AIFILES/` owns AI timing, construction priorities and finite developed-start preparation. `Interface/ui.json` owns categories, shortcuts, summaries and credits. `Graphics/scene.json` owns visual scale, camera behavior, scenery, lighting and the Medium profile. Supported definition edits require a restart/new compatible scenario; new mechanisms still need code.

Packaged loose definitions are staged beside the executable under `seige2222/Binaries/Win64`. No runtime account or server is required; HTTP, discovery and telemetry plugins are disabled for Shipping. The successful 117-stage gameplay checkpoint recorded zero TCP/UDP endpoints in **828 live process-tree samples**. Development/editor services and source publishing are separate from game runtime.

Windows build requirements: Unreal Engine 5.8, Visual Studio C++ tools, Windows SDK and Node.js. Install Git LFS before cloning and run `git lfs pull` for tracked large source assets. Open `seige2222.uproject` for editor development, or run:

```powershell
node Tools/validate_configuration.mjs
node Tools/validate_rules.mjs --self-test
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

The build script validates definitions and creates a versioned Shipping directory. Override its engine path with `-Engine` or output directory with `-ArchiveDirectory`. The project does not modify engine source. Versioned local packages, caches, downloaded tools and logs are excluded from the [source repository](https://github.com/site815/seige2222).

## Art and design records

The environment combines recorded CC0 Poly Haven/ambientCG assets with original Blender geometry, photographic ground layers and streamed vegetation. [Third-party provenance](Art/THIRD_PARTY_ASSETS.md), [v0.8 surface/canopy records](Art/EnvironmentV08/README.md), [industrial source](Art/Source/Seige_Industry_Architecture.blend), [construction assets](Art/Construction/README.md) and [Rex provenance](Art/CompanionDog/TEXTURE_PROVENANCE.md) retain sources and authorship. No reference-game assets or private photo pixels are included.

Visual quality does **not** claim Manor Lords parity. Terrain repetition, foliage transitions, animation contact and native-resolution performance remain review targets. Imports and test passes alone do not establish finished art. Historical runtime captures and measurements are linked from the [historical development archive](docs/DEVELOPMENT_HISTORY_0_2_TO_0_7.md).

Start with the [design index](docs/game-design/README.md), [construction and transport](docs/game-design/CONSTRUCTION_AND_TRANSPORT_0_8.md), [resources](docs/game-design/RESOURCE_PROPOSAL.md), [production dependencies](docs/game-design/PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md), [combat specification](docs/game-design/WEAPONS_DEFENSES_AND_VEHICLE_OUTFITTING.md) and [current verification report](docs/DEVELOPMENT_REPORT.md). They distinguish confirmed requirements, provisional balance and implemented scope.

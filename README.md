# seige2222

A single-player Unreal Engine colony builder about robotic industry in an Earth-like wilderness. Build a physical production network, support a finite workforce and defend the settlement.

**v0.9.2 source candidate — release verification is in progress.** The launcher continues to open the retained v0.8.1 package; the newest Shipping package is `Builds/v0.9.2/Windows`. The candidate uses save format **7** and `prototype-9.2` definitions (material gates on the advanced tier, radioactive-ore reactor fuel), so saves from 9.0 are rejected; start a fresh scenario. v0.9.2 adds the original building kit (24 family meshes, authored panel/bolt/weathering surfaces), build-card readiness, the production-chain dependency graph (**P**) and the scheduler, sensor-coverage and AI road-order fixes on top of the v0.9.1 rendering revision. Its [development report](docs/DEVELOPMENT_REPORT.md) and [verification record](docs/verification/v0.9.2.json) separate what has passed from what has not; the [to-do list](docs/TODO.md) is the ordered backlog. Do not substitute earlier package results for this candidate.

## Gameplay in the v0.9 source

**Choose a site.** Configure a 3×3 neighborhood with empty, Starting-AI or Developed-AI regions. The center can be a player colony or an AI observer. Background bugs and invasions have independent switches. Player landing pauses the shared world while you survey and place the core; play begins at 1×. The user-selected Developed-AI model loads an explicitly authored established colony instead of simulating hours of growth during loading. Its one-time initial assets are separate from the player/Starting-AI kit; ordinary simulation, paid AI orders and selected threats apply afterward. This new setup path still requires native and packaged acceptance.

Each 3.6×3.6 km region has three distinct standard and two distinct rare deposits, including empty regions without invented colonies. The standard pool is water, metal ore, silica and biomass; the rare pool is rare metals, radioactive ore, crystalline material and hydrocarbons. A single Extraction Mine binds to the real deposit beneath it. Resource knowledge and neighboring-colony details follow sensor visibility.

**Land with finite supplies.** Player and Starting-AI colonies use six identifiable hover workers and a **9,570 kg loose starter kit**, with zero Galactic credits. Installed shuttle systems and weapons are separate from that loose inventory. The two large hardpoint banks hold one large, two medium and eight small lasers. Initial workers remain aboard during descent, leave through the cargo hatch and perform the actual deployment work. The provisional deployment-work duration is 900 simulation seconds at full staffing and efficiency; travel and hauling also take time. Developed AI uses its separate established-state manifest, not a claim that this kit instantly constructs a mature colony.

The accepted [economy baseline](docs/game-design/ECONOMY_BASELINE_0_9.md) uses **80 kg / 160 L workers**, tonne-scale construction bills and explicitly priced material/energy recipes. These numbers are provisional playtest choices, not engineering specifications. The catalog has 22 cargo types and 14 recipes. All 24 choices of three standard and two rare types have passed bounded starter-bootstrap tests; that does not prove every placement or long-term AI economy.

**Move and transform real cargo.** Orders reserve eligible materials. A worker claims stock, travels to collect it, loads it, carries at most **40 kg and 60 L**, and unloads it at the destination. Builders and operators come from that same finite body ledger. Local recipe batches pay electricity and hold their inputs until all outputs emerge. Construction progressively installs delivered goods. One Galactic credit is anchored to a kilogram of gold; external ports trade timed shipments with provisional prices and level-one/two/three freight capacities of 1/2/3 tonnes. Credits are an account, not a shared cargo inventory.

**Grow the workforce.** The core's universal replicator can make goods and workers; a dedicated worker factory is faster and uses less electricity. Service bays provide support capacity and consume maintenance parts. Colony spare-worker targets and port export-stock targets are separate. Intact stored workers self-relocate under their own IDs rather than being carried as an 80 kg package by a 40 kg hauler. Storage, trade, recycling and destruction retain body identity. Recycling returns 90% of the current authored body mass, records the remainder as waste, and costs 1 kWh by default. Required workers and protected reserves are not free recycling stock.

Automatic hauling demand grows by one job per four completed eligible facilities, from a base of one up to eight. These are ordinary paid worker jobs within support capacity. Existing deliveries continue when job demand falls; assigning another task never creates another body.

**Power and time.** Completed road networks connect generation, consumers and shared batteries. The single world clock gives 30 simulation minutes of daylight, 30 minutes of night and 30 days per season. Pause and menus stop the clock; 1×/5×/10× advance the same date across every region, independently of any authored established-colony age. Solar follows daylight and produces zero at night. Core fusion is daylight-independent but its completed output depends on physically present operators and efficiency; 30 kW is the level-one rating. Passive loads remain first, then an external dispatch policy protects valid pending native defensive shots against optional transaction spending. Shared energy tests pass, but complete AI industrial viability remains unresolved.

Winter visibly adds snow coverage and snowfall. No winter heating, movement or resource-yield penalties are implemented. The river/lake/cliff profile is shared by rendered terrain and dry-land routing; discovered deposits have physical geology cues.

**Expand and defend.** Core upgrades add a surrounding campus while retaining the central upright shuttle. All levels reserve the largest expansion plot; actual foundations follow the built footprint. Solar arrays, vehicle factories, defense towers and walls have paid tier upgrades. Road/rail/vacuum tiers provide powered 2×/4×/8× movement, with physical durability and repair bills. Wall plans use editable joints and a chosen inside/outside.

Four weapon families — energy, kinetic, missile and plasma — interact with shields and armor. Projectiles can miss, hit unintended objects and cause splash damage. Twelve wheeled/tracked/mech chassis span four sizes. Fleets have a base 50-point budget, with 1/2/4/8 points per chassis size and 1/2/3 total fleet slots by core level. Mount front-area capacity is **1 large = 4 medium = 16 small**; physical envelopes instead grow eightfold in volume per size step, with separate provisional mass ceilings. Factories and platforms request local materials before paid assembly or outfitting. Fleets support movement, defense, escort and privateering against populated neighbors, including physical loot and return. Only vehicles actually boarded at the core service port evacuate with the shuttle.

**Rex and controls.** Rex consumes nearby organic food and grants a bounded local morale effect. Select the own core to find him, then choose first-person roaming at 1×. His original Blender interpretation remains stylized; no private photographs or reference-game assets are embedded. Workers are not individually micromanaged.

| Control | Action |
| --- | --- |
| WASD / arrows | Camera pan; WASD walks in Rex view |
| Wheel | Zoom and regional survey |
| Q / E; middle-button drag | Orbit; drag also tilts |
| Home | Return to the colony |
| B, then R / I / L / D | Build categories |
| B, R, M | Extraction Mine on a real deposit |
| B, L, R / U / W | Road / road upgrade / wall plan |
| Left / right click | Select or place / cancel targeting |
| + / −; Space | Cycle Paused/1×/5×/10×; pause/resume |
| Escape / F10 | Cancel active tools first / open paused menu |
| F5 / F9 | Save / load |

Resources use five top HUD groups with full hover inventories. Credits, stored/capacity electricity, generation/passive demand, workforce and couriers remain available. Building dossiers show local storage, production, power and complete weapon statistics, including unarmed/zero values. Rendered portraits come from actual meshes.

## Development and evidence

`Rules/` contains economy, worker, calendar, environment, transport and combat definitions. `AIFILES/` contains paid colony-planning rules and the separate established Developed-AI manifest; `Interface/` contains authored UI data. `Graphics/` contains presentation, weather, camera and the single Medium graphics profile. Most balance values are external; new mechanisms still require code. Changed fingerprints require a compatible fresh scenario. Failed historical growth runs remain diagnostic evidence; generating that history is no longer the Developed-AI load requirement.

Use Unreal Engine 5.8, Visual Studio C++ tools, Windows SDK, Node.js and Git LFS. Open `seige2222.uproject`, or run `node Tools/validate_configuration.mjs`, `node Tools/validate_rules.mjs --self-test`, then `Tools/build.ps1 -Package`. The launcher and final package identity should only change after release verification. Versioned local packages and saves remain separate; Shipping saves use `%LOCALAPPDATA%/seige2222/Saved/SaveGames`. A fresh checkout also needs the installed licensed engine's daylight cubemap. [Tools/prepare_engine_assets.py](Tools/prepare_engine_assets.py) recreates only the ignored local `T_AmbientDaylight` asset; `build.ps1` runs it when missing after compiling the Editor and before cooking. This step does not reauthor weather materials or redistribute the engine source asset in Git.

The single Medium profile supports native borderless or windowed display, with 50–100% render resolution. Native resolution uses TAA; reduced resolution uses TSR. The v0.9.1 rendering revision replaces the Nanite grass sward with non-Nanite impostor cards and measures **47–67 FPS** across the five authored static views at 3840×1600 and 100% render resolution, against 34–45 FPS for v0.9.0 on the same machine ([graphics report](docs/game-design/GRAPHICS_PERFORMANCE_0_9_1.md)); meadow and ground views stay below 60 FPS, and offline/interaction evidence for this candidate is pending. Terrain/material repetition, repeated grass-card silhouettes, foliage transitions, sector-crossing hitches, character likeness and crowded-colony performance remain limitations. There is no claim of Manor Lords parity. Persistent multiplayer, remote player extraction, general inter-colony trade and full relocation remain later work.

See the [design index](docs/game-design/README.md), [economy baseline](docs/game-design/ECONOMY_BASELINE_0_9.md), [third-party provenance](Art/THIRD_PARTY_ASSETS.md), [orbital workforce assets](Art/OrbitalV09/README.md), [environment](Art/EnvironmentV09/README.md), [grass cards](Art/EnvironmentV091/README.md), [Rex provenance](Art/CompanionDog/TEXTURE_PROVENANCE.md) and [candidate development report](docs/DEVELOPMENT_REPORT.md).

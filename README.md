# seige2222

**v0.10.10 — Windows single-player prototype.** The [development report](docs/DEVELOPMENT_REPORT.md) records verification and remaining limitations.

**Start a new scenario:** save format 9 remains, but environment, transport and AI fingerprints changed. Existing saves and earlier local builds are preserved.

[Download releases](https://github.com/site815/seige2222/releases)

## Gameplay

**Choose a site.** Configure a 3×3 neighborhood with empty, Starting-AI or Developed-AI regions. The center can be a player colony or an AI observer. Background bugs and invasions have independent switches. Player landing pauses the shared world while you survey and place the core; play begins at 1×. Developed AI loads an explicitly authored established colony. Its one-time initial assets are separate from the player/Starting-AI kit; ordinary simulation, paid AI orders and selected threats apply afterward.

Each 3.6×3.6 km region has three distinct standard and two distinct rare deposits, including empty regions without invented colonies. The standard pool is water, metal ore, silica and biomass; the rare pool is rare metals, radioactive ore, crystalline material and hydrocarbons. A single Extraction Mine binds to the real deposit beneath it. Each new scenario draws a fresh, saved layout. The player's home deposits stay charted after landing, with or without either bug toggle; neighboring deposits and colonies still follow sensor visibility.

**Land with finite supplies.** Player and Starting-AI colonies use five identifiable hover workers and a **9,570 kg loose starter kit**, with zero Galactic credits. An autonomous transport vehicle is a separate installed starting asset. Installed shuttle systems and weapons are separate from that loose inventory. The two large hardpoint banks hold one large, two medium and eight small lasers. Initial workers remain aboard during descent, leave through the cargo hatch and perform the actual deployment work. Initial deployment work takes 30 simulation seconds at full staffing and efficiency, following the 12-second descent and 3-second hatch sequence; physical worker exit and material delivery also take time. Later construction and upgrades retain their longer authored durations. Developed AI uses its separate established-state manifest, not a claim that this kit instantly constructs a mature colony.

The accepted [economy baseline](docs/game-design/ECONOMY_BASELINE_0_9.md) uses **80 kg / 160 L workers**, tonne-scale construction bills and explicitly priced material/energy recipes. These numbers are provisional playtest choices, not engineering specifications. The catalog has 22 cargo types and 14 recipes. All 24 bounded starter-bootstrap combinations passed for v0.10.10; broader economic balance and hostile progression remain work in progress.

**Move and transform real cargo.** Orders reserve eligible materials. A worker claims stock, travels to collect it, loads it, carries at most **40 kg and 60 L**, and unloads it at the destination. Builders and operators come from that same finite body ledger. Local recipe batches pay electricity and hold their inputs until all outputs emerge. Construction progressively installs delivered goods. One Galactic credit is anchored to a kilogram of gold; external ports trade timed shipments with provisional prices and level-one/two/three freight capacities of 1/2/3 tonnes. Credits are an account, not a shared cargo inventory.

**Own the workforce.** Only command centers manufacture workers. Their crew capacities are 5/10/15 across the three levels; a body's permanent crew owner is separate from its temporary hauling, construction, operating or charging task. The former worker-factory blueprint is now a Construction Hub. No automatic-disassembly or task-priority controls are presented. The automatic spare reserve matches command level: 5 / 10 / 15 stored workers. There is no adjustable colony spare target; independent trading-port stock targets add to that reserve.

Each new worker requires **60 kg robotic parts + 12 kg construction alloy + 4 kg control circuits + 4 kg battery modules**, plus **4 kWh** of assembly energy. The core takes **600 simulation seconds** at full operating efficiency: **10 real minutes at 1×, 1 minute at 10×, or 30 seconds at 20×**. Actual elapsed time increases when operators are away, inputs have not arrived, or power/support is insufficient. For example, one operator working in a five-person core gives roughly 50 minutes at 1× before other effects. These are assembly times, distinct from the accelerated calendar display; the five starting workers are already installed assets.

Workers move at **10 km/h empty or loaded** on easy ground; autonomous transports move at **30 km/h empty or loaded**. Rough-terrain penalties apply equally in both cargo states. Road construction, upgrades and road speed bonuses remain deferred. These are base rates at 1× simulation speed.

Workers carry a real 0.2 kWh battery and visit a physical charging berth instead of being teleported to a service job. The core and charging station each provide one berth; the charging station has its own crew. Battery and charging values are editable rules. Workers remain the same counted bodies during storage, delivery and trade.

Charging departure considers the traversable route, queue and docking travel. A derived per-worker route estimate is cached and refreshed when the world or charging destination changes, with an exact refresh near the departure threshold. Workers return to the actual workplace or cargo endpoint before resuming. A job beyond full-battery return range still needs a nearer powered charging station.

**Haul in bulk.** A separate autonomous vehicle carries up to 2,000 kg / 3,000 L using a 100 kWh battery. It has no driver population and never substitutes for construction workers: physical crews load and unload its sole cargo ledger. Vehicle factories assemble additional transports from local materials and energy. The shuttle has one shared vehicle bay; only a vehicle physically aboard can evacuate. Movement profiles for drones, transports, wheeled, tracked and mech units are authored in Rules/mobility.json.

**Power and time.** While roads are deferred, each colony shares generation and batteries across its own buildings. Neighboring colonies retain separate grids. The single world clock gives 30 simulation minutes of daylight, 30 minutes of night and 30 days per season. Pause and menus stop the clock; 1×/10×/20× advance the same date across every region, independently of any authored established-colony age. Solar follows daylight and produces zero at night. Core fusion is autonomous and daylight-independent, including during deployment; 30 kW is the level-one rating. Other staffed generators depend on their operators, efficiency and local fuel. Passive loads remain first, then an external dispatch policy protects valid pending native defensive shots against optional transaction spending. Separate level-1/2/3 fusion plants are available in the Energy build category. The focused v0.10.0 checks passed; longer development, varied seeds and sustained recovery still need balance playtesting.

Winter visibly adds snow coverage and snowfall. No winter heating, movement or resource-yield penalties are implemented. The river/lake/cliff profile is shared by rendered terrain and dry-land routing; discovered deposits have physical geology cues.

Tree foliage and grass use continuous GPU wind motion. Pause freezes it; cosmetic wind follows the square root of simulation speed, capped at 4× (about 3.16× wind during 10× play, 4× during 20× play), without restarting its phase. Height-weighted crown bend remains active on distant tree proxies at survey zoom; only fine leaf flutter and nearby grass fade with distance. Trunks and cached shadows remain stable. [Wind configuration and verification](Art/WeatherV09/WIND.md) explain the rendering limits; this change does not establish an FPS gain.

**Expand and defend.** Core upgrades add a surrounding campus while retaining the central upright shuttle. All levels reserve the largest expansion plot; actual foundations follow the built footprint. Solar arrays, vehicle factories, defense towers and walls have paid tier upgrades. The authored road/rail/vacuum tiers remain deferred and absent from the build catalog; no hidden roads or connector prerequisites are created by AI. Wall plans use editable joints and a chosen inside/outside.

Four weapon families — energy, kinetic, missile and plasma — interact with shields and armor. Projectiles can miss, hit unintended objects and cause splash damage. Twelve wheeled/tracked/mech chassis span four sizes. Fleets have a base 50-point budget, with 1/2/4/8 points per chassis size and 1/2/3 total fleet slots by core level. Mount front-area capacity is **1 large = 4 medium = 16 small**; physical envelopes instead grow eightfold in volume per size step, with separate provisional mass ceilings. Specialist vehicle, tank and mech factories request local materials before paid assembly or outfitting; the core is not a universal goods or weapon factory. Fleets support movement, defense, escort and privateering against populated neighbors, including physical loot and return. Only vehicles actually boarded at the core service port evacuate with the shuttle.

Command-center and tower mounts aim independently in all directions, subject to range, power, ammunition and cover. Open foundations and frames become opaque projectile cover when enclosure starts; deliberate hits and splash can still damage them. Threat-enabled AI starts prioritize a paid early tower and place later defenses to cover exposed approaches, while preserving normal construction costs and physical crews.

**Rex and controls.** Rex consumes nearby organic food and grants a bounded local morale effect. Use the bottom Rex button or select the own core to find him, then choose third-person roaming at 1× once safely outside. Mouse movement steers him, Shift runs, and Alt + mouse moves the camera independently. His detailed retriever mesh and photograph-derived coat are restored under the owner's explicit prototype exception. The unresolved Hunyuan3D-2.1 territorial licence restriction is recorded in Credits and the asset provenance. The restricted base is not classified as CC0 or original project geometry. Workers are not individually micromanaged.

| Control | Action |
| --- | --- |
| WASD / arrows | Camera pan; WASD moves in Rex view |
| Mouse / Shift / Alt + mouse in Rex view | Steer Rex / run / free look |
| Wheel | Zoom and regional survey |
| Q / E; middle-button drag | Orbit; drag also tilts |
| Home | Return to the colony |
| B, then R / I / E / L / D | Extraction / production / energy / logistics / defense |
| B, E, F | Fusion power plant |
| P | Production-chain overlay in the current view |
| B, R, M | Extraction Mine on a real deposit |
| B, L, W | Wall plan; roads and road upgrades are deferred |
| Left / right click | Select or place / cancel targeting |
| + / −; Space | Cycle Paused/1×/10×/20×; pause/resume |
| Escape / F10 | Cancel active tools first / open paused menu |
| F5 / F9 | Save / load |

Resources use six top HUD groups with nested material ledgers, usable while the build catalog remains open. Blank/building clicks preserve the local camera; region navigation uses its explicit controls. Navigation, placement and accepted/rejected actions have distinct sound feedback; hovering remains silent. Interface, combat and notification budgets are separated in real time, with damage warnings limited to new incidents. Worker totals distinguish owned, assigned, active, stored and idle bodies from vehicles. Date/time, colony age and pulse timing share a bottom strip. Building dossiers show local storage, production, power and complete weapon statistics, including unarmed/zero values. Rendered portraits come from actual meshes. Visible outdoor stockpiles use the same hover/click inspection: quantity, mass, volume, owner and reserved goods come from their actual inventory. Empty or hidden piles cannot remain selected.

## Development and evidence

`Rules/` contains economy, worker, calendar, environment, transport and combat definitions. `AIFILES/` contains paid colony-planning rules and the separate established Developed-AI manifest; `Interface/` contains authored UI data. `Graphics/` contains presentation, weather, camera and the single Medium graphics profile. Most balance values are external; new mechanisms still require code. Changed fingerprints require a compatible fresh scenario. Failed historical growth runs remain diagnostic evidence; generating that history is no longer the Developed-AI load requirement.

Use Unreal Engine 5.8, Visual Studio C++ tools, Windows SDK, Node.js and Git LFS. Open `seige2222.uproject`, or run `node Tools/validate_configuration.mjs`, `node Tools/validate_rules.mjs --self-test`, then `Tools/build.ps1 -Package`. The launcher and final package identity should only change after release verification. Versioned local packages and saves remain separate; Shipping saves use `%LOCALAPPDATA%/seige2222/Saved/SaveGames`. A fresh checkout also needs the installed licensed engine's daylight cubemap. [Tools/prepare_engine_assets.py](Tools/prepare_engine_assets.py) recreates only the ignored local `T_AmbientDaylight` asset; `build.ps1` runs it when missing after compiling the Editor and before cooking. This step does not reauthor weather materials or redistribute the engine source asset in Git.

The single Medium profile supports native borderless or windowed display, with 50–100% render resolution. Native resolution uses TAA; reduced resolution uses TSR. The historical v0.9.3 static benchmark measured **52.5–74.7 FPS** across five authored views at native **3840×1600, Medium, 10×**; see the [historical graphics report](docs/game-design/GRAPHICS_PERFORMANCE_0_9_3.md) for conditions, moving-camera measurements and their limits. Terrain/material repetition, repeated grass-card silhouettes, foliage transitions, sector-crossing hitches, character likeness and crowded-colony performance remain limitations. There is no minimum-FPS or Manor Lords parity claim. Persistent multiplayer, remote player extraction, general inter-colony trade and full relocation remain later work.

See the [design index](docs/game-design/README.md), [economy baseline](docs/game-design/ECONOMY_BASELINE_0_9.md), [third-party provenance](Art/THIRD_PARTY_ASSETS.md), [orbital workforce assets](Art/OrbitalV09/README.md), [environment](Art/EnvironmentV09/README.md), [grass cards](Art/EnvironmentV091/README.md), [Rex provenance](Art/CompanionDog/TEXTURE_PROVENANCE.md) and [development report](docs/DEVELOPMENT_REPORT.md).

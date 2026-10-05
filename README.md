# seige2222

A first playable Unreal Engine colony builder: establish a robot industry in an Earth-like wilderness, keep physical deliveries moving, and survive periodic bug attacks. Detailed futuristic industry contrasts with the natural landscape.

**v0.5.0 is built and verified locally.** Buildings require delivered materials and robot builders, the first core deploys from its orbital shuttle, and staffed service bays expand robot charging/maintenance capacity. The graphics pass adds continuous neighboring terrain/woodland and configurable foliage lighting. All **29 native tests passed: 28 clean and one with an Unreal editor background HTTP warning**. The Shipping package passed **63 interaction stages with zero failures and exit 0**; 73 process-tree samples recorded no TCP/UDP endpoints, and all nine staged JSON files match source byte hashes. See the [development report](docs/DEVELOPMENT_REPORT.md) for evidence and boundaries, and the [graphics investigation](docs/game-design/GRAPHICS_PERFORMANCE_0_5.md) for measured performance limits.

This is an early single-player prototype. The broader design uses a persistent multiplayer world as its baseline. Local neighboring AI colonies now run the same simulation; networking, controllable fleets, raiding, trade, and cross-sector travel remain later milestones. The [first-playable scope](docs/game-design/FIRST_PLAYABLE_SCOPE.md) separates implemented systems from the full design.

The distributed single-player build runs offline. It uses Unreal's Shipping configuration, with HTTP, UDP/TCP discovery, and telemetry plugins disabled. It needs no account, server, or internet connection. Development tools and GitHub publishing are separate from the game.

## Play

Run **`Play-seige2222.cmd`** for the verified **v0.5.0** package in `Builds/v0.5.0/Windows`. **Start a new scenario for v0.5:** construction/service state and the new rule fingerprint intentionally reject earlier saves. Open `seige2222.uproject` in Unreal Engine 5.8 to work on its source. Versioned directories retain earlier packages. The source repository is [site815/seige2222](https://github.com/site815/seige2222); local versioned binaries, caches, and generated logs are excluded from Git.

- The main menu offers Single player, Load single player, Multiplayer (coming soon), Credits, Settings, and Exit.
- Choose Single player to configure the 3×3 scenario. Each of the eight neighbors cycles between Empty (default), Starting AI, and Developed AI. The center cycles between Player, Starting AI, and Developed AI.
- With a human center, time remains paused while you survey deposits and choose a command-center landing site. Click a valid site to deploy the shuttle-carried core kit. Six starting robots assemble it over six simulation seconds; its systems become available on completion. The shuttle then remains docked as the emergency escape vehicle. The suggested location is optional.
- An AI center starts observation mode. Watch the same simulation, pan/zoom, pause, change local speed, and save/load without issuing construction orders.
- Each sector renders at **3.6×3.6 km** and the full neighborhood at **10.8×10.8 km**, using six rendered centimeters per logical simulation unit. Deposit coordinates, travel times, and gameplay balance are unchanged by this presentation scale. Deposits form uneven clusters, with remote groups for isolated outposts. Human play requires sensors to reveal live activity; observation mode can inspect the AI colonies.
- Open **Build [B]** in the floating bottom dock, choose a category and blueprint, then click terrain to queue a construction site. The order reserves available core materials; couriers deliver them physically before assigned builders assemble the structure. The catalog opens upward. Extractors belong on the matching labeled deposit. Hover a blueprint for its explanation and requirements.
- Version and FPS stay at the top left. Resource and colony summaries float at the top, with alerts below them; hover summaries for details. The bottom dock holds construction, colony commands, regions, and local time controls.
- Zoom out or choose **Regions** to open the simplified 3×3 survey map. Click a sector or scroll up over it to inspect its detailed terrain. The focused sector renders detailed woodland and buildings; coarse neighboring terrain and sparse woodland continue beyond its borders. Human neighbor views respect sensor intelligence; observation mode can inspect all AI colonies.
- Build sensors toward distant deposits to extend construction visibility. Protect exposed industrial sites with turrets.
- Set up iron and carbon extraction with an alloy refinery, copper extraction with conductor works, and silica extraction with substrate works. Circuit and component works complete the chain.
- Completed buildings fill jobs, manufacture, dispatch cargo, and repair automatically. Construction sites cannot produce, repair, sense or fire until finished. Robots carry real resource batches between local stockpiles. Select a building for Overview, Weapons, Power, Production, Resources, and Maintenance tabs. Damage, reload, DPS, range, local stock and zero values are explicit. Unarmed structures say **Unarmed**. Power displays **0 kW** because a separate power grid is not yet simulated. Owned non-core buildings can be switched off or on.
- The core manufactures robots for open jobs while resources and support berths are available. It starts with eight berths. Build a **Robot service bay [B, L, C]** for sixteen more; it needs one operating worker and components delivered to its own inventory for maintenance. Unsupported or poorly maintained robots lower colony efficiency. Charging is abstract support capacity, not a separate electrical grid or individual battery simulation.
- Staffing automatically favors the core, service facilities, defenses and sensors before ordinary industry. Construction also needs workers; disabled sites retain their material reservations.
- The objective panel tracks survival time and newly manufactured components. Starting supplies do not count as production.
- The core can defend itself, but distant extraction sites and cargo routes need defenses. Damage can accumulate if repair supplies run out.

| Control | Action |
| --- | --- |
| WASD / arrows | Pan relative to camera direction |
| Mouse wheel | Zoom; scroll up over a regional map cell to inspect it |
| Q / E | Orbit the perspective camera |
| Middle-mouse drag | Captured 360-degree orbit and bounded tilt; release restores pointer |
| Home | Return to colony |
| B | Open the build menu |
| B, R / I / L / D | Choose extraction / industry / logistics / defense |
| Category, displayed letter | Choose a blueprint; for example B, L, S selects a sensor |
| Left click | Select a building or place the selected blueprint |
| Right click / Escape | Back out of menus or cancel placement / selection |
| Space | Pause this local prototype |
| F5 / F9 | Save / load colony |
| Colony menu | Save/load, settings, credits, eject, and main menu |

Middle-mouse orbit uses raw mouse displacement with sensitivity defined in `Graphics/scene.json`. It no longer compounds that setting with Unreal's former 7% mouse-axis scaling. Close zoom lowers the camera angle while preserving the chosen orbit tilt for zooming back out. Camera clearance and terrain selection use the same surface as the visible ground, including hills above the camera horizon. Map view remains a stable overhead survey.

Pausing and accelerated playback are local prototype conveniences. They do not define time control for a persistent server. Main menu, scenario setup, settings, credits, and human landing setup suspend the entire local scenario; Build and Colony overlays keep time running. The bottom dock controls local speed. Saving records all colonies, scenario choices, construction progress and delivered materials, cargo in transit, service maintenance state, weapon reloads, camera, and time controls. Shipping saves live in `%LOCALAPPDATA%/seige2222/Saved/SaveGames`, with `Scenario.json` pointing to a complete snapshot folder. Editor builds use the project's `Saved/SaveGames`. Saves require matching rule/AI fingerprints. **Start a new scenario for v0.5:** `prototype-5.0` definitions and simulation save format 2 are incompatible with earlier saves.

Settings contain graphics quality and windowed/fullscreen display mode and persist locally. Credits list tools, original art, and the attributed CC0 texture creators. Neighbor colonies operate independently for this milestone: cross-sector extraction, trade, shared combat, and privateering are not implemented yet.

## Editable game rules

`Rules/resources.json`, `recipes.json`, `buildings.json`, `policies.json`, and `scenario.json` own content, costs, recipes, construction time/builders, staffing priorities, service capacity, local maintenance, rates, combat values, spawning, objectives, and the starting scenario. The engine loads and validates these files. No content rebake is required for supported rule edits: restart the game or colony after editing.

For a packaged build, the loose JSON files live under `seige2222/Binaries/Win64/Rules` inside that version's Windows directory. Existing saves require the exact rule fingerprint. Build-menu categories, keyboard shortcuts, resource summaries, and credits live separately in `Interface/ui.json`. All AI configuration lives in `AIFILES`: decision timing, construction priorities, placement search, sensor expansion, and the finite developed-start preset. AI queues the same construction and runs normal jobs, physical delivery, services, production, repairs and threats. A Developed AI start is prepared by actually simulating its finite supplied colony; its plan waits for earlier targets to complete and staff before progressing. New simulation mechanisms still require implementation; the documented supported rule vocabulary is not a general scripting language.

`Graphics/scene.json` separately controls rendered world scale, perspective FOV/orbit defaults and sensitivity, map transition, zoom limits, forest density, terrain material, nature assets, grass lighting/shadow distance and neighboring woodland. The package stages `Graphics`, `Interface`, and `AIFILES` beside `Rules`. Presentation definitions do not change logical simulation rates.

Run `node Tools/validate_configuration.mjs` before building to validate Rules, AIFILES, Interface, and Graphics together. The build script and CI also run it. See [Rules and Simulation Architecture](docs/game-design/RULES_AND_SIMULATION_ARCHITECTURE.md).

## Build

Requirements: Windows, Unreal Engine 5.8, Visual Studio C++ tools and Windows SDK, Node.js for rule validation.

Install Git LFS before cloning, then run `git lfs pull` in the checkout. The mature tree Unreal asset and v0.4 Blender source files use LFS to retain their full geometry.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

The script builds the editor module, creates the default map if missing, then compiles, cooks, and packages the offline Shipping game in a directory named for `ProjectVersion`. Override the engine installation with `-Engine 'D:\Epic\UE_5.8'` or choose `-ArchiveDirectory` if needed. The project contains no engine source changes. The Unreal Editor is a development tool and can still use its own diagnostic services; use the packaged executable for offline play.

## Art and design

The retained environment combines [CC0 Poly Haven and ambientCG assets](Art/EnvironmentV04/ATTRIBUTION.md), a mature woodland canopy, mixed short/tall meadow grass, original wildflowers, and photographic ground layers. Actual rolling terrain and a ridge are sampled at approximately 3.52 m spacing in the focused sector. Compact foundations flatten only the ground near known buildings. Locally streamed ground-cover cells retain their positions while the camera moves. The [third-party register](Art/THIRD_PARTY_ASSETS.md) records creators, source links, licenses, and file manifests. No reference game's assets are included; downloaded assets are permitted for commercial use under their recorded CC0 license.

The retained [industrial Blender source](Art/Source/Seige_Industry_Architecture.blend), [generator](Tools/create_industry_assets.py), [importer](Tools/import_industry_assets.py), and [verified import report](Art/industry_import_report.json) describe the six industrial structures and their nine original PBR maps. [Command-hub](Art/Previews/industry_command_closeup.png) and [factory](Art/Previews/industry_factory_closeup.png) previews are Blender renders, not screenshots of the game. Nature has separate [prepared sources and import evidence](Art/EnvironmentV04/ATTRIBUTION.md). Earlier original environment assets remain documented in the [historical asset register](Art/REALISTIC_ASSET_REGISTER.md). The new [original construction assets](Art/Construction/README.md) add a docked shuttle and recognizable charging/service bay, with retained Blender sources, generators and import reports. Construction shows delivered stacks, aggregate builders, scaffold and a progressive structure reveal; the translucent placement ghost does not alter the simulation. Downloaded development tools are excluded from Git.

**v0.5 Shipping captures:** [shuttle deployment](Art/Previews/v05_shuttle.png), [physical construction](Art/Previews/v05_construction.png), [completed service bay](Art/Previews/v05_service.png), [neighboring landscape](Art/Previews/v05_boundary.png), and [meadow](Art/Previews/v05_meadow.png). The meadow capture comes from the matched Epic/100% Shipping benchmark; the other four come from the packaged interaction route. Runtime review covered core materials, builders, shuttle, scaffold and completed structures. The captured service bay had no assigned supported robots; occupied-bay presentation was not verified by that capture. These are functional/visual checks, not a controlled frame-rate benchmark.

**Historical v0.4 Shipping captures:** [colony](Art/Previews/v04_gameplay.png), [hillside meadow](Art/Previews/v04_meadow.png), [ground detail](Art/Previews/v04_ground.png), [wide terrain](Art/Previews/v04_terrain.png), [regional map](Art/Previews/v04_regions.png), [build menu](Art/Previews/v04_build.png), and [weapon information](Art/Previews/v04_weapons.png). These are game screenshots, not Blender renders. This remains below the Manor Lords reference target: aerial ground still reads too smoothly, forest variety is limited, and close-view foliage needs performance work. Close captures show roughly 33 FPS on this machine; they are snapshots, not a timed benchmark. The existing industrial set and robot/bug character art are retained alongside the new v0.5 shuttle/service bay; no new character animation system is claimed. The [v0.5 graphics investigation](docs/game-design/GRAPHICS_PERFORMANCE_0_5.md) records mixed preliminary timings and the later fixed-quality benchmark protocol; no universal speedup or reference-game parity is claimed.

Start with the [design index](docs/game-design/README.md), [resource proposal](docs/game-design/RESOURCE_PROPOSAL.md), [building proposal](docs/game-design/BUILDING_PROPOSAL.md), and [production dependencies](docs/game-design/PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md). The resource/building proposals describe a wider future game than the compact first-playable rule set.

# seige2222

A first playable Unreal Engine colony builder: establish a robot industry in an Earth-like wilderness, keep physical deliveries moving, and survive periodic bug attacks. Detailed futuristic industry contrasts with the natural landscape.

**Current build: v0.3.0, verified 5 October 2026.** The Windows Shipping package is built and playable. Nineteen native tests pass cleanly, and the packaged 28-stage interaction route passes with zero failures. The rotatable perspective camera, physical world scale, licensed woodland assets, and detailed industrial architecture are included. Visual polish continues. See the [development report](docs/DEVELOPMENT_REPORT.md) for current evidence and the [graphics milestone](docs/game-design/GRAPHICS_MILESTONE_0_3.md) for scope.

This is an early single-player prototype. The broader design uses a persistent multiplayer world as its baseline. Local neighboring AI colonies now run the same simulation; networking, controllable fleets, raiding, trade, and cross-sector travel remain later milestones. The [first-playable scope](docs/game-design/FIRST_PLAYABLE_SCOPE.md) separates implemented systems from the full design.

The distributed single-player build runs offline. It uses Unreal's Shipping configuration, with HTTP, UDP/TCP discovery, and telemetry plugins disabled. It needs no account, server, or internet connection. Development tools and GitHub publishing are separate from the game.

## Play

Run **`Play-seige2222.cmd`** for the verified **v0.3.0** package in `Builds/v0.3.0/Windows`. Open `seige2222.uproject` in Unreal Engine 5.8 to work on its source. Versioned directories retain earlier packages. The source repository is [site815/seige2222](https://github.com/site815/seige2222); local versioned binaries, caches, and generated logs are excluded from Git.

- The main menu offers Single player, Load single player, Multiplayer (coming soon), Credits, Settings, and Exit.
- Choose Single player to configure the 3×3 scenario. Each of the eight neighbors cycles between Empty (default), Starting AI, and Developed AI. The center cycles between Player, Starting AI, and Developed AI.
- With a human center, time remains paused while you survey deposits and choose a command-center landing site. Click a valid site to begin. The suggested location is optional.
- An AI center starts observation mode. Watch the same simulation, pan/zoom, pause, change local speed, and save/load without issuing construction orders.
- In v0.3, each sector renders at **3.6×3.6 km** and the full neighborhood at **10.8×10.8 km**, using six rendered centimeters per logical simulation unit. Deposit coordinates, travel times, and gameplay balance are unchanged by this presentation scale. v0.2 used 600 m sectors and a 1.8 km neighborhood. Deposits form uneven clusters, with remote groups for isolated outposts. Human play requires sensors to reveal live activity; observation mode can inspect the AI colonies.
- Open **Build [B]** in the top menu, choose a category and blueprint, then click terrain to build. Extractors belong on the matching labeled deposit. Hover a blueprint for its explanation and cost.
- Version and FPS stay at the top left. Hover the top-bar resource, workforce, objective, or threat summaries for details. There is no permanent bottom construction bar.
- Build sensors toward distant deposits to extend construction visibility. Protect exposed industrial sites with turrets.
- Set up iron and carbon extraction with an alloy refinery, copper extraction with conductor works, and silica extraction with substrate works. Circuit and component works complete the chain.
- Buildings fill jobs, manufacture, dispatch cargo, and repair automatically. Robots carry real resource batches between local stockpiles. Select a building to see its status and switch it off or on.
- The core manufactures robots for open jobs. Maintenance uses components; more robots also consume more supplies.
- The objective panel tracks survival time and newly manufactured components. Starting supplies do not count as production.
- The core can defend itself, but distant extraction sites and cargo routes need defenses. Damage can accumulate if repair supplies run out.

| Control | Action |
| --- | --- |
| WASD / arrows | Pan relative to camera direction |
| Mouse wheel | Zoom |
| Q / E | Orbit the perspective camera |
| Middle-mouse drag | Rotate and tilt the perspective camera |
| Home | Return to colony |
| B | Open the build menu |
| B, R / I / L / D | Choose extraction / industry / logistics / defense |
| Category, displayed letter | Choose a blueprint; for example B, L, S selects a sensor |
| Left click | Select a building or place the selected blueprint |
| Right click / Escape | Back out of menus or cancel placement / selection |
| Space | Pause this local prototype |
| F5 / F9 | Save / load colony |
| Colony menu | Save/load, settings, credits, eject, and main menu |

The Q/E and middle-mouse controls are part of the current perspective-camera revision; the v0.2 executable retains its fixed orthographic view. Perspective placement and selection passed the revised rendered regression route, including rotated and low-angle views.

Pausing and accelerated playback are local prototype conveniences. They do not define time control for a persistent server. Main menu, scenario setup, settings, credits, and human landing setup suspend the entire local scenario; Build and Colony overlays keep time running. The top bar controls local speed. Saving records the center and every AI neighbor, scenario choices, camera, and time controls. v0.3 adds camera yaw/pitch while accepting format-2 saves that omit those fields. Shipping saves live in `%LOCALAPPDATA%/seige2222/Saved/SaveGames`, with `Scenario.json` pointing to a complete snapshot folder. Editor builds use the project's `Saved/SaveGames`. Saves reject changed rule or AI definitions; v0.1 colony saves do not match the expanded v0.2 rule set.

Settings contain graphics quality and windowed/fullscreen display mode and persist locally. Credits list tools, original art, and the attributed CC0 texture creators. Neighbor colonies operate independently for this milestone: cross-sector extraction, trade, shared combat, and privateering are not implemented yet.

## Editable game rules

`Rules/resources.json`, `recipes.json`, `buildings.json`, `policies.json`, and `scenario.json` own content, costs, recipes, staffing, rates, combat values, spawning, objectives, and the starting scenario. The engine loads and validates these files. No content rebake is required for supported rule edits: restart the game or colony after editing.

For a packaged build, the loose JSON files live under `seige2222/Binaries/Win64/Rules` inside that version's Windows directory. Existing saves require the exact rule fingerprint. Build-menu categories, keyboard shortcuts, resource summaries, and credits live separately in `Interface/ui.json`. All AI configuration lives in `AIFILES`: decision timing, construction priorities, placement search, sensor expansion, and the finite developed-start preset. AI buys construction and runs normal jobs, production, delivery, repairs, and threats after initialization. New simulation mechanisms still require implementation; the documented supported rule vocabulary is not a general scripting language.

`Graphics/scene.json` separately controls rendered world scale, perspective FOV/orbit defaults, zoom limits, forest density, and nature-asset references. The package stages `Graphics`, `Interface`, and `AIFILES` beside `Rules`; all ten loose files were verified byte-identical to their source versions. Presentation definitions do not change logical simulation rates.

Run `node Tools/validate_configuration.mjs` before building to validate Rules, AIFILES, Interface, and Graphics together. The build script and CI also run it. See [Rules and Simulation Architecture](docs/game-design/RULES_AND_SIMULATION_ARCHITECTURE.md).

## Build

Requirements: Windows, Unreal Engine 5.8, Visual Studio C++ tools and Windows SDK, Node.js for rule validation.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

The script builds the editor module, creates the default map if missing, then compiles, cooks, and packages the offline Shipping game in a directory named for `ProjectVersion`. Override the engine installation with `-Engine 'D:\Epic\UE_5.8'` or choose `-ArchiveDirectory` if needed. The project contains no engine source changes. The Unreal Editor is a development tool and can still use its own diagnostic services; use the packaged executable for offline play.

## Art and design

The v0.3 environment combines [CC0 Poly Haven nature models](Art/Nature/ATTRIBUTION.md), photographic ground surfaces including Leafy Grass, and six original industrial structures authored in Blender. The [third-party register](Art/THIRD_PARTY_ASSETS.md) records creators, source links, licenses, and file manifests. No purchased asset packs or copied reference characters are required; downloaded assets are permitted for commercial use under their recorded CC0 license.

The new [industrial Blender source](Art/Source/Seige_Industry_Architecture.blend), [generator](Tools/create_industry_assets.py), [importer](Tools/import_industry_assets.py), and [verified import report](Art/industry_import_report.json) retain the architecture and its nine original PBR maps. [Command-hub](Art/Previews/industry_command_closeup.png) and [factory](Art/Previews/industry_factory_closeup.png) previews are Blender renders, not screenshots of the game. Nature has separate [prepared sources and import evidence](Art/Nature/ATTRIBUTION.md). Earlier original environment assets remain documented in the [historical asset register](Art/REALISTIC_ASSET_REGISTER.md). Downloaded development tools are excluded from Git.

Actual v0.3 game captures: [colony view](Art/Previews/v03_gameplay.png), [command-hub close-up](Art/Previews/v03_closeup.png), and [neighborhood](Art/Previews/v03_neighborhood.png). This remains an early visual milestone, below the Manor Lords reference target: distant canopy thinning, visible terrain repetition, and regional presentation need further polish. Existing robot/bug art is retained; this pass adds no new character animation.

Start with the [design index](docs/game-design/README.md), [resource proposal](docs/game-design/RESOURCE_PROPOSAL.md), [building proposal](docs/game-design/BUILDING_PROPOSAL.md), and [production dependencies](docs/game-design/PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md). The resource/building proposals describe a wider future game than the compact first-playable rule set.

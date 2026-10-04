# seige2222

A first playable Unreal Engine colony builder: establish a robot industry on a colorful alien planet, keep physical deliveries moving, and survive periodic bug attacks.

This is an early single-player prototype. The broader design uses a persistent multiplayer world as its baseline; networking, neighboring AI colonies, controllable fleets, raiding, and cross-sector relocation are later milestones. The [first-playable scope](docs/game-design/FIRST_PLAYABLE_SCOPE.md) separates implemented systems from the full design.

The distributed single-player build runs offline. It uses Unreal's Shipping configuration, with HTTP, UDP/TCP discovery, and telemetry plugins disabled. It needs no account, server, or internet connection. Development tools and GitHub publishing are separate from the game.

## Play

The packaged Windows build is generated into `Builds/Windows`. Run `Builds/Windows/Seige.exe`, or double-click `Play-seige2222.cmd`. Alternatively, open `seige2222.uproject` in Unreal Engine 5.8 and press Play.

- Select a construction blueprint along the bottom, then click terrain to build. Extractors belong on the matching colored deposit.
- Build sensors toward distant deposits to extend construction visibility. Protect exposed industrial sites with turrets.
- Set up iron and carbon extraction with an alloy refinery, copper extraction with conductor works, and silica extraction with substrate works. Circuit and component works complete the chain.
- Buildings fill jobs, manufacture, dispatch cargo, and repair automatically. Robots carry real resource batches between local stockpiles. Select a building to see its status and switch it off or on.
- The core manufactures robots for open jobs. Maintenance uses components; more robots also consume more supplies.
- The objective panel tracks survival time and newly manufactured components. Starting supplies do not count as production.
- The core can defend itself, but distant extraction sites and cargo routes need defenses. Damage can accumulate if repair supplies run out.

| Control | Action |
| --- | --- |
| WASD / arrows | Pan camera |
| Mouse wheel | Zoom |
| Home | Return to colony |
| Left click | Select a building or place the selected blueprint |
| Right click / Escape | Cancel blueprint / clear selection |
| Space | Pause this local prototype |
| F5 / F9 | Save / load colony |
| 1x / 3x button | Local test speed |
| Eject | End the settlement with its preloaded shuttle cargo |
| Restart | Start a fresh colony |

Pausing and accelerated playback are local prototype conveniences. They do not define time control for a persistent server. Restart does not overwrite the saved colony; F5 does. Shipping saves are stored in `%LOCALAPPDATA%/seige2222/Saved/SaveGames/Colony.json`; editor builds use the project's `Saved/SaveGames`. Saves refuse mismatched rule versions/content.

## Editable game rules

`Rules/resources.json`, `recipes.json`, `buildings.json`, `policies.json`, and `scenario.json` own content, costs, recipes, staffing, rates, combat values, spawning, objectives, and the starting scenario. The engine loads and validates these files. No content rebake is required for supported rule edits: restart the game or colony after editing.

For packaged builds, edit the loose JSON files in `Builds/Windows/seige2222/Binaries/Win64/Rules`. This path was verified in the standalone build. Existing saves require the exact rule fingerprint. New simulation mechanisms still require implementation; the documented supported rule vocabulary is not a general scripting language.

Run `node Tools/validate_rules.mjs` before building. See [Rules and Simulation Architecture](docs/game-design/RULES_AND_SIMULATION_ARCHITECTURE.md).

## Build

Requirements: Windows, Unreal Engine 5.8, Visual Studio C++ tools and Windows SDK, Node.js for rule validation.

```powershell
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

The script builds the editor module, creates the default map if missing, then compiles, cooks, and packages the offline Shipping game. Override the engine installation with `-Engine 'D:\Epic\UE_5.8'` if needed. The project contains no engine source changes. The Unreal Editor is a development tool and can still use its own diagnostic services; use the packaged executable for offline play.

## Art and design

Original Blender models, editable `.blend` source, FBX export script, Unreal importer, and an asset register live under [Art](Art/ASSET_REGISTER.md). No purchased asset packs or copied reference characters are required. The downloadable Blender tool itself is excluded from Git.

Start with the [design index](docs/game-design/README.md), [resource proposal](docs/game-design/RESOURCE_PROPOSAL.md), [building proposal](docs/game-design/BUILDING_PROPOSAL.md), and [production dependencies](docs/game-design/PRODUCTION_DEPENDENCIES_AND_STARTER_VIABILITY.md). The resource/building proposals describe a wider future game than the compact first-playable rule set.

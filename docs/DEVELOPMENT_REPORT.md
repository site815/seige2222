# seige2222 v0.2.0 — Development and verification

Date: 2026-10-04. Engine: Unreal Engine 5.8.3. Platform: Windows x64.

## Current implementation

The single-player prototype now starts at a main menu. Scenario setup configures a human or AI center and eight empty, starting-AI, or developed-AI neighbors. A human scenario pauses the whole world until the command center is placed. An AI center enables observation, with the same colony rules and no player construction orders.

Each sector is 600×600 meters: six times the original side length and thirty-six times its area. The full 3×3 neighborhood is 1.8 kilometers across. The resource template contains twenty-five irregularly clustered deposits, including distant groups. Terrain is Earth-like, with photographic CC0 surfaces, original detailed futuristic buildings, and Blender vegetation. Individual neighboring sectors currently reuse the deposit template; the landscape varies across the larger world.

The interface has version/FPS at the upper left, top-bar menus and major summaries, hover details, and a floating B-key construction catalog. There is no permanent lower construction bar. Settings cover graphics quality and display mode. Credits are maintained in the external interface definition.

The simulation retains automatic jobs, manufactured robot population, local production, physical cargo, repairs, sensors, roaming bugs and scaled invasions, a first-playable objective, and emergency escape. Scenario save/load now includes all colonies, AI configuration fingerprints, time controls, and camera position. AI definitions live in `AIFILES`; content and balance live in `Rules`; menus, shortcuts, summaries, and credits live in `Interface`.

## Crash and presentation fixes

The reported left-click crashes were traced to `ASeigeHUD::Click` reading the transient Unreal drawing canvas during controller input. Input now uses cached hit rectangles and viewport dimensions. The regression test calls the real controller/HUD path with a null canvas, including world selection and construction.

Expanding the orthographic clipping range exposed an Unreal camera-origin correction that placed foliage outside its distance-culling range. The project now disables that correction for its explicit orthographic planes. Trees use authored canopy-preserving detail levels and remain visible at regional zoom; small ground details may still cull. The runtime sun is explicitly movable, and Lumen mesh-distance-field generation is enabled. Runtime screenshots are checked separately from Blender asset previews.

## Recorded verification

- The revised editor target compiled with the installed Visual Studio 2026 toolchain. UBT reports that this compiler family is newer than Epic's preferred version; no engine-source changes were required.
- All sixteen native tests passed with zero test failures or warnings: six simulation, four AI, three interaction, and three frontend tests. Report: `Saved/Automation/v02-final/index.json`.
- A normal-action strategy covering three resource approaches with sensors and turrets completed the objective at 360 simulation seconds with twenty-two manufactured components and no building losses. No stock or health edits were used in that strategy test. This validates one viable strategy, not indefinite balance.
- Starting AI manufactured components by 300 simulation seconds. Tests also covered the developed preset, rejected invalid AI files, relocated-core threat spawning, and deterministic AI save continuation.
- Frontend tests covered default empty neighbors, mode cycling, paused landing, rejected and accepted landing clicks, observer input restrictions, settings pause/return, exact center-and-neighbor save restoration, and rejection of corrupt scenario metadata without changing the running state.
- The rules validator passed the current rule set and fifteen deliberately invalid variants. The combined configuration validator also checks AI and interface references; five focused invalid-reference/key/timing mutations were rejected.
- The rendered `-UiSmoke` route completed twenty-three stages with zero assertions: main menu, setup, landing, real controller construction clicks, construction shortcuts, neighborhood overview, credits, and AI observation. It saves screenshots and a `PresentationSmoke.json` report without touching player saves. Native input-route coverage and programmatic rendered interaction are not a claim of a complete human mouse-driven play-through.
- Art import verified fourteen revised building/environment meshes, twelve PBR textures, five material masters, bounds, pivots, and material slots. Original robot and bug models remain in use. Texture source URLs, creators, licenses, and hashes are recorded in [third-party attribution](../Art/THIRD_PARTY_ASSETS.md).

## Packaged release checks

The offline Win64 Shipping package built successfully in `Builds/v0.2.0/Windows`. The root launcher selects this version. The packaged controller-driven check completed all twenty-three stages with zero failures, including the human landing/build path and an observer scenario with two AI neighbors. Cooked screenshots verified the main menu, top bar, build catalog, neighborhood labels, credited assets, and rendered forest/industry.

All nine loose files staged from `Rules`, `AIFILES`, and `Interface` matched their source hashes. A controlled edit to the packaged scenario changed starting population from six to eight without recompiling. After fifteen simulation seconds the report showed seven robots, reflecting the normal automatic retirement interval. The exact original rule bytes were restored and their hash rechecked. This verifies numerical definition loading in the executable; new simulation mechanisms still need code.

Local evidence: `Saved/package-v02-final.log`, `Saved/packaged-UiSmoke-verification.json`, and `Saved/packaged-PrototypeSmoke-verification.json`. Packaged screenshots and runtime reports are under `%LOCALAPPDATA%/seige2222/Saved`. Test routes do not modify player saves. Source publication uses the private `site815/seige2222` repository; executable archives and local verification logs remain excluded from Git.

Tracked cooked-game captures: [colony view](../Art/Previews/v02_gameplay.png), [scenario setup](../Art/Previews/v02_scenario.png), and [nine-sector overview](../Art/Previews/v02_neighborhood.png). These are runtime screenshots, not target-art mockups.

## Offline operation

The deliverable uses Shipping configuration, which avoids Unreal's development profiling listener. HTTP transport, UDP/TCP messaging, telemetry, and unused online-service plugins are disabled. The game requires no account or server. Its third-party textures are local packaged assets. Development downloads and GitHub publishing are separate from runtime behavior.

The v0.2 Shipping game was observed with zero TCP sockets and UDP endpoints in every live-process sample during the menu/gameplay/observer check and the separate rule-edit run. This is an observation of those executed paths, not a packet capture of every possible future session. Editor commandlets may initialize additional development plugins and are not the single-player deliverable.

Earlier sandboxed Unreal build attempts coincided with the reported dotnet dialogs. Subsequent engine/compiler calls use the required filesystem access. No .NET or engine reinstall was performed.

## Remaining scope

Some forest views still show an abrupt pale foliage shading band at a distance. Explicit movable sunlight and a separate far-shadow-culling diagnostic did not eliminate it; the diagnostic override is not included in the release. This remains visual polish to investigate, alongside broader terrain variety and art refinement. The presentation is an early art pass, not finished production graphics.

Neighbor colonies are independent instances of the same simulation, not a shared multiplayer economy or battlefield. Fleet control, privateering, cross-sector extraction/trade/travel, full robot needs and revolt, abandoned-region scavenging, shuttle loading/relocation, and persistent networking remain future work. Multiplayer is explicitly marked Coming soon in the menu. The full design remains the persistent-world baseline, with single-player developed first.

See [first-playable scope](game-design/FIRST_PLAYABLE_SCOPE.md), [scenario setup](game-design/SCENARIO_AND_AI_SETUP.md), and the [design index](game-design/README.md). The design documents distinguish confirmed intent, prototype choices, and unimplemented systems.

## Reproduce

```powershell
node Tools/validate_configuration.mjs
node Tools/validate_rules.mjs --self-test
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

Native tests use UnrealEditor-Cmd with `-NullRHI`, `-ExecCmds="Automation RunTests Seige"`, `-TestExit="Automation Test Queue Empty"`, and a report export directory. Inspect test result JSON; an editor process exit code alone does not report individual test failures. Rendered verification uses `-UiSmoke -RenderOffscreen -ForceRes -ResX=1600 -ResY=900`. The flag drives test-only interactions and exits after writing its report.

The release package is generated in `Builds/v0.2.0/Windows`. `Play-seige2222.cmd` launches that version and honors saved display settings. Older packages are retained. Build output, caches, downloaded tools, and test logs are excluded from Git; source assets and their provenance are tracked.

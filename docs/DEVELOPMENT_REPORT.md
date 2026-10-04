# seige2222 — First playable verification

Date: 2026-10-04. Engine: Unreal Engine 5.8.3. Platform: Windows x64.

## Delivered scope

Native Unreal C++ colony simulation, 4 raw resources and 5 manufactured items, 12 placeable blueprints and one command core; automatic jobs, robot manufacturing and retirement, local production, physical couriers, automatic repairs, sensor-dependent combat, roaming bugs and proportional invasion pulses, an objective, escape, and local save/load.

The game contains eight original Blender meshes with palette materials and editable source, a generated alien landscape, an overhead camera, construction UI, building inspection/toggles, and local time controls. It is a compact first playable, not the complete proposed single-player game. Neighbor AI, fleets, privateering, trade, revolt, abandonment simulation, and relocation remain unimplemented. See [scope](game-design/FIRST_PLAYABLE_SCOPE.md).

## Verified results

- Native editor module and Windows game compiled using the installed VS2026 toolchain; the compiler is newer than Epic's preferred family, so UBT reports a compatibility warning.
- Six native simulation automation tests passed: rule rejection, physical delivery/conservation, automatic workforce and repairs, deterministic save/load continuation, core-loss escape with only preloaded cargo, and a full playable objective.
- The objective test used normal placement and time advancement without altering inventory, health, or enemy state. It won at approximately 370 simulation seconds with 12 newly manufactured components; two exposed buildings were lost. This validates one viable strategy, not long-term balance.
- The separate JSON validator passes the shipped rule graph and 15 deliberately invalid variants.
- All eight Blender meshes imported with verified dimensions and material slots; import and map-generation commandlets reported zero errors or warnings.
- The standalone development package launched and rendered its original assets and HUD. A temporary edit of its loose starting-population rule changed the live population without recompilation; the original was then restored and its hash matched source.
- Runtime screenshots exposed camera-exposure and text-sizing issues; both were corrected. Dead visual actors, selection after rebuilding, load-time visual reconstruction, and objective dismissal were also corrected during review.

## Offline runtime

The first development build exposed Unreal's profiling-control listener on TCP port 1985. A socket snapshot found it listening with no outbound TCP connection. The deliverable was changed to Shipping configuration, where Unreal's default trace support is disabled. Unused HTTP transport, UDP/TCP messaging, telemetry, and Android file-server functionality are disabled in the project. This does not alter Windows firewall settings.

Two sandboxed build attempts exited before producing normal build output and coincided with reported dotnet exception dialogs. No Windows crash report was available to conclusively attribute them. Subsequent Unreal invocations use the required filesystem access and complete successfully; no runtime or engine reinstall was performed.

The final Shipping package compiled, cooked, and staged successfully and launched with the original assets and HUD. Its running game process had **zero TCP sockets and zero UDP endpoints** during the observed check. This is an observed runtime check, not a claim about every possible future code path. The earlier development package is preserved under `Builds/DevelopmentPreview`; use the current `Builds/Windows/Seige.exe` for offline play.

The six native tests also passed after the final simulation-rate and validation fixes. Visual screenshots were inspected. Automated desktop access for a mouse-driven play-through timed out twice, so that interaction check is **not claimed as completed**. Construction, staffing, transport, combat, and save/load behavior are covered by the native tests; a player's first hands-on session remains valuable for camera and interface usability.

## Reproduce

```powershell
node Tools/validate_rules.mjs --self-test
powershell -ExecutionPolicy Bypass -File Tools/build.ps1 -Package
```

Native tests run in the Unreal Editor commandlet with `-NullRHI`, `-ExecCmds="Automation RunTests Seige.Simulation;Quit"`, and `-TestExit="Automation Test Queue Empty"`. Test reports and build logs are generated under `Saved` and excluded from Git.

The source repository excludes engine binaries, build output, caches, the downloaded Blender tool, and generated logs. The Windows package remains local in `Builds/Windows`.

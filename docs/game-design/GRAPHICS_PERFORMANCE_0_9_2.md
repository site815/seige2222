# Graphics performance and presentation — v0.9.2

Updated: **2026-10-09** (Asia/Seoul). Unreal Engine 5.8.3, Windows x64, Ryzen 7 9800X3D / RTX 4070 Ti SUPER / 32 GB, **3840×1600, Medium profile, 100% render resolution, 10× simulation**, Shipping packages, `Tools/benchmark_packaged.ps1` (five authored views, five-second samples). Reports are in `Saved/`; the baseline for every comparison below is the v0.9.2 run of the same harness on the same machine earlier the same day (`GraphicsBenchmark-v092-static.json`, `-travel.json`).

This revision tried to find more GPU time with render settings, removed part of the sector-crossing stall, and spent the rest of the effort on building art. The short version: **no render-setting change measured better than run-to-run drift, so none was committed**; the stall at the sector boundary is about 43% shorter; mean frame rates are unchanged within noise.

## Render-setting variants (job 47)

Each variant edited the staged `Graphics/scene.json` of the same Shipping package (or added a console variable through it), ran the meadow and ground views, and was reverted. `base` ran first and `base2` (identical to `base`) ran last, so the difference between them is the drift of this session (thermal and background state), about **1.1 FPS / 0.4 ms**.

| Variant | Change | Meadow FPS | Meadow GPU ms | Ground FPS | Ground GPU ms |
| --- | --- | ---: | ---: | ---: | ---: |
| base | — | 54.10 | 17.81 | 53.99 | 17.89 |
| wind30 | `grass_wind_distance_m` 60 → 30 (card sway, velocity pass) | 53.76 | 17.93 | 53.68 | 18.00 |
| shadow25 | `grass_shadow_distance_m` 45 → 25 | 53.66 | 17.97 | 53.81 | 17.96 |
| both | wind30 + shadow25 | 53.47 | 18.05 | 53.54 | 18.01 |
| smrt1 | `r.Shadow.Virtual.SMRT.RayCountDirectional` 2 → 1 | 53.70 | 17.94 | 53.67 | 18.00 |
| detail80 | `grass_detail_distance_m` 95 → 80 | 53.45 | 18.04 | 53.35 | 18.11 |
| vsm1 | directional VSM resolution LOD bias (static and moving) +0.5 → +1 | 53.95 | 17.88 | 54.08 | 17.84 |
| clouds96 | `r.VolumetricCloud.ViewRaySampleMaxCount` 160 → 96 | 52.71 | 18.28 | 52.74 | 18.32 |
| base2 | — (repeat of base) | 52.92 | 18.23 | 52.99 | 18.21 |

Every variant lies between the two base runs. Read as a result, the frame is not bound by any one of these settings: an editor `-game` CSV profile of the meadow view (`Saved/Profiling/v092/meadow.csv`, 17.2 ms GPU) spreads the time over Basepass 2.65 ms, ShadowDepths 2.16, NaniteVisBuffer 1.99, ShadowProjection 1.35, NaniteBasePass 1.23, deferred lighting 1.03, velocities 0.91, TAA 0.77 and post-processing 0.67 ms, with nothing above 16%. (The ground-view CSV of the same run is an outlier — 38.7 ms GPU with 7.6 ms in TSR — and is not used.) Further GPU savings at native 3840×1600 therefore need content changes (card overdraw in the meadow, the 1.8 M-triangle masked near tree, shadow-casting density) or a lower internal resolution, which the existing 50–100% render-resolution setting already offers.

## Sector crossing

The boundary travel sample crosses into the neighbouring sector, which rebuilds the focused 1024² terrain grid (about a million vertices), the coarse grid it leaves behind and the forest of both sectors. All three preparation steps were serial on the game thread. Terrain heights (per row), chunk vertex preparation (per chunk) and the forest clearance/woodland tests (per candidate) now run in `ParallelFor`; the random stream is still consumed in the original order, so placements are identical. Component creation and mesh-section upload stay on the game thread.

## Shipping comparisons

Static, `GraphicsBenchmark-v092-static.json` → `GraphicsBenchmark-k47-static.json` (job 47; kit art, AI and rules changes in between, no render-setting change):

| View | FPS before / after | GPU ms | p95 ms | p99 ms | Max ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Colony | 75.55 / 73.52 | 12.55 / 13.02 | 14.80 / 14.55 | 17.11 / 15.06 | 20.9 / 15.4 |
| Meadow | 53.58 / 52.52 | 17.95 / 18.29 | 21.22 / 19.83 | 25.98 / 21.76 | 28.5 / 22.2 |
| Ground | 53.10 / 52.24 | 18.11 / 18.42 | 20.97 / 20.22 | 23.54 / 21.58 | 25.3 / 22.3 |
| Hills | 63.65 / 63.41 | 15.03 / 15.12 | 17.72 / 16.98 | 21.50 / 17.34 | 24.3 / 18.9 |
| Boundary | 69.26 / 69.80 | 13.67 / 13.67 | 16.12 / 15.41 | 19.95 / 15.88 | 22.3 / 16.1 |

Travel (36 m/s), `GraphicsBenchmark-v092-travel.json` → `GraphicsBenchmark-k47-travel.json`:

| View | FPS before / after | GPU ms | p99 ms | Max ms (wall) | Max ms (game thread) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Colony | 65.50 / 67.09 | 14.52 / 14.27 | 22.03 / 18.96 | 24.0 / 19.9 | 5.5 / 5.2 |
| Meadow | 51.37 / 52.06 | 18.68 / 18.51 | 28.90 / 22.85 | 29.8 / 23.2 | 5.0 / 5.4 |
| Ground | 51.67 / 51.87 | 18.64 / 18.62 | 28.63 / 22.53 | 30.0 / 23.3 | 6.8 / 5.1 |
| Hills | 60.77 / 60.80 | 15.64 / 15.78 | 21.97 / 19.98 | 26.0 / 24.1 | 5.1 / 5.0 |
| Boundary | 61.95 / 64.38 | 13.89 / 14.03 | 26.89 / 19.17 | **416.8 / 236.3** | **399.3 / 222.6** |

Mean FPS moved by −2.0 to +2.4, inside the drift measured above, so the honest reading is "unchanged". The boundary stall is **43% shorter** (one frame of 236 ms instead of 417 ms). The tails (p99, max) of every static view are also lower; the sector work cannot explain that (the static views do not cross a boundary) and this report does not attribute it. The boundary's longest visible-cell backlog fell from 0.75 s to 0.57 s; scenery readiness waits are unchanged.

## Building art and its cost

All 52 building definitions except the procedural walls and the command core now use kit meshes; this revision added the service bay, sensor mast and extraction rig, separate meshes for the five clean-industry works (conductor, glass, circuit, robotic parts, battery — previously one shared hall) and level-2/3 meshes for the three vehicle factories ([kit README](../../Art/BuildingKitV092/README.md)). The industry master material gained per-building tint variation, soil grime over the lowest 1.6 m, seasonal snow on upward faces and night window glow (`MPC_Weather.Night`); the night capture of job 47 blew the glass out to white at a glow of 1.6, so it is 0.45, and upward glass (roof skylights) glows at a third of a wall window. Kit meshes are 6–44 k triangles at LOD0 with generated LODs at 45% and 14%.

The art changes are inside the colony view's numbers above (75.55 → 73.52 FPS static, 65.50 → 67.09 travel): no cost is measurable at this sample length. The showcase view with all 28 blueprints ran at 127–169 FPS in the 2560×1080 Development editor captures (`Saved/Screenshots/Benchmark/k48-*`), which is not comparable with the Shipping numbers.

## Remaining gap

- The meadow and ground views stay at about 52–54 FPS at native 3840×1600; the cost is spread across passes (above), so the next gains are content changes, not switches.
- The boundary crossing still produces one long frame (236 ms in Shipping). Height and vertex preparation are parallel; component creation and section upload for the million-vertex focused grid and the forest rebuild are still one game-thread frame.
- Samples are five seconds of a fresh colony at midday with empty neighbours; loaded colonies, night, winter and long sessions are not covered by these numbers.

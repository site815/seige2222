# Graphics performance and presentation — v0.9.2

Updated: **2026-10-09** (Asia/Seoul). Unreal Engine 5.8.3, Windows x64, Ryzen 7 9800X3D / RTX 4070 Ti SUPER / 32 GB, **3840×1600, Medium profile, 100% render resolution, 10× simulation**, Shipping packages, `Tools/benchmark_packaged.ps1` (five authored views, five-second samples). Reports are in `Saved/`; the baseline for every comparison below is the v0.9.2 run of the same harness on the same machine earlier the same day (`GraphicsBenchmark-v092-static.json`, `-travel.json`).

This revision tried to find more GPU time with render settings, attacked the sector-crossing stall in three steps, and spent the rest of the effort on building art. The short version: **no render-setting change measured better than run-to-run drift, so none was committed**; the longest frame when the camera crosses into a neighbouring sector fell from **417 ms to 86 ms** in Shipping by job 50, at the price of more frames carrying part of the work (a fourth step, below, is pending measurement in job 51); mean frame rates are unchanged within noise. The meadow grass costs about 2.5 ms of an 18.9 ms GPU frame.

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

## GPU attribution: the meadow grass

Job 49 ran the meadow and ground views of the same Shipping package with the sward hidden (`-HideSward`, `GraphicsBenchmark-k49-nosward-*.json`) next to the normal static run:

| View | With sward FPS / GPU ms | Sward hidden FPS / GPU ms | Sward cost |
| --- | ---: | ---: | ---: |
| Meadow | 51.03 / 18.95 | 58.88 / 16.42 | 2.5 ms |
| Ground | 50.84 / 18.93 | 57.76 / 16.77 | 2.2 ms |

The card grass is therefore about 13% of the GPU frame in the views that show the most of it; a card LOD or lower density could recover perhaps half of that. The other 16.5 ms are terrain, trees, shadows, lighting and post-processing at 6.1 megapixels, which is why the remaining large lever is internal resolution (the existing 50–100% render-resolution setting), not grass.

## Sector crossing

The boundary travel sample crosses into the neighbouring sector, which rebuilds the focused 1024² terrain grid (about a million vertices, 64 chunk components), the coarse grid of the sector being left and the forest of both sectors. The editor travel diagnostic (`-BenchmarkTravel -BenchmarkView=boundary`, log lines `Terrain surface ready`, `SCENERY_FOREST_READY`, `SECTOR_TRANSITION`, `SECTOR_DEFERRED`) split the stall, and three changes followed:

1. **Parallel preparation (job 47).** Terrain heights (per row), chunk vertex preparation (per chunk) and the forest clearance/woodland tests (per candidate) run in `ParallelFor`; the random stream is consumed in the original order, so placements are identical. Shipping max frame 416.8 → 236.3 ms. The job 48 editor run then showed the remaining crossing frame as terrain 68 ms (heights 17 ms, chunks 51 ms) and **forest 133 ms** for the 113k trees of the two changed sectors.
2. **Forest build and a split crossing (job 49, 18e0de6).** The forest builder formatted an `FName` key per tree for its batch maps (about 1.2 µs per tree on the game thread); kinds, LOD bands and both transforms are now computed in the parallel pass and the serial pass only appends to per-set arrays. A crossing also rebuilds only the sector being entered; the sector being left keeps its detailed chunks and trees (a valid, finer surface behind the camera) and is rebuilt on the next two frames. Editor: crossing frame 91 ms (terrain 71 ms, forest 15 ms for 58k trees, ground cover and geology 5 ms), then 5 ms and 15 ms. A full nine-sector forest build (scenario start) fell from 0.58–0.65 s to 0.12–0.19 s. **Shipping max frame 234.8 → 116.6 ms** (game thread 222.8 → 103.6 ms); the boundary's longest visible-cell backlog 0.57 → 0.43 s.
3. **Five frames (job 50, 4258df9).** The crossing frame only computes the entered sector's heights and chunk data while its coarse surface stays on screen; the next frames install its chunks, then its forest and ground cover, then the left sector's coarse surface, then its forest. A second crossing before installation hands the rendered sector back cleanly. Editor: crossing frame 32.3 ms (64 chunks prepared in 14.2 ms of it), chunk upload **40.6 ms**, forest and ground cover 23.6 ms, left coarse tile 4.1 ms, left forest 14.7 ms. Shipping boundary travel: max frame 116.6 → **85.8 ms**, but p99 19.9 → **38.7 ms** (game thread p99 27.9 ms): the work now falls on several frames of 25–40 ms instead of one, and a five-second sample with several crossings puts those frames into its p99. Whether five 30–40 ms frames read better than one 117 ms frame is a judgement; the next step attacks the largest of them.
4. **Staggered chunk upload (job 51).** The crossing frame now computes only the entered sector's heights plus a 64-section copy of its current coarse surface (the stand-in); the next frame prepares the 64 detailed chunks; then `TerrainChunksPerFrame` = 16 detailed chunks are committed per frame, nearest the camera first, each hiding the stand-in section it replaces; then forest and ground cover, the left coarse tile and the left forest. A flush (pad edit, a new crossing) still completes everything at once. Pending measurement in job 51.

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

Mean FPS moved by −2.0 to +2.4, inside the drift measured above, so the honest reading is "unchanged". The tails (p99, max) of every static view are also lower; the sector work cannot explain that (the static views do not cross a boundary) and this report does not attribute it.

Later runs on the same machine (k48 = source 56b9036, k49 = 18e0de6 with the per-process works meshes and the forest change):

| View | Static FPS k47 / k48 / k49 / k50 | Static GPU ms k49 / k50 | Travel FPS k47 / k48 / k49 / k50 | Travel max ms k47 / k48 / k49 / k50 |
| --- | ---: | ---: | ---: | ---: |
| Colony | 73.52 / 71.52 / 71.29 / 71.28 | 13.37 / 13.36 | 67.09 / 64.83 / 65.11 / 64.78 | 19.9 / 20.2 / 20.1 / 21.0 |
| Meadow | 52.52 / 51.08 / 51.03 / 50.95 | 18.95 / 18.92 | 52.06 / 50.81 / 50.70 / 50.67 | 23.2 / 23.9 / 25.0 / 23.6 |
| Ground | 52.24 / 50.95 / 50.84 / 50.84 | 18.93 / 18.96 | 51.87 / 50.79 / 51.04 / 50.67 | 23.3 / 24.8 / 24.0 / 24.0 |
| Hills | 63.41 / 61.74 / 61.68 / 61.61 | 15.59 / 15.54 | 60.80 / 59.52 / 59.48 / 59.25 | 24.1 / 22.5 / 22.3 / 21.9 |
| Boundary | 69.80 / 67.79 / 67.62 / 67.75 | 14.15 / 14.02 | 64.38 / 62.94 / 64.63 / 64.67 | **236.3 / 234.8 / 116.6 / 85.8** |

Boundary travel p99: 26.9 (v0.9.2) → 19.2 (k47) → 19.9 (k49) → 38.7 ms (k50); see step 3 above.

k48–k50 are 1–2 FPS (0.3–0.5 ms GPU) below k47 in every view, including meadow and ground, which show no buildings; that is the size of the session drift measured in job 47, so it is not attributed to the art. k49 and k50 agree within 0.15 ms GPU in every static view. The boundary stall halves from k48 to k49 and loses another quarter in k50.

## Building art and its cost

All 52 building definitions now use kit meshes (the command core's levels 2–3 as a campus around the retained level-1 shuttle, added for job 51); this revision added the service bay, sensor mast and extraction rig, separate meshes for the five clean-industry works (conductor, glass, circuit, robotic parts, battery — previously one shared hall), level-2/3 meshes for the three vehicle factories, a biofuel refinery distinct from the oil refinery and three wall-section levels that replace the four-cube wall assembly ([kit README](../../Art/BuildingKitV092/README.md)). The industry master material gained per-building tint variation, soil grime over the lowest 1.6 m, seasonal snow on upward faces and night window glow (`MPC_Weather.Night`); the night capture of job 47 blew the glass out to white at a glow of 1.6, so it is 0.45, and upward glass (roof skylights) glows at a third of a wall window. Kit meshes are 6–44 k triangles at LOD0 with generated LODs at 45% and 14%.

The art changes are inside the colony view's numbers above (75.55 → 73.52 FPS static, 65.50 → 67.09 travel): no cost is measurable at this sample length. The showcase view with all 28 blueprints ran at 127–169 FPS in the 2560×1080 Development editor captures (`Saved/Screenshots/Benchmark/k48-*`), which is not comparable with the Shipping numbers.

## Remaining gap

- The meadow and ground views stay at about 52–54 FPS at native 3840×1600; the cost is spread across passes (above), so the next gains are content changes, not switches.
- A sector crossing still costs several frames of 25–40 ms in Shipping (job 50). Job 51 spreads the largest step, the 64-chunk upload, over four frames behind a coarse stand-in; precomputing heights and chunks on a worker thread before the camera reaches the boundary would remove the height and preparation frames, and the forest step (24 ms in the editor) is the next largest.
- Samples are five seconds of a fresh colony at midday with empty neighbours; loaded colonies, night, winter and long sessions are not covered by these numbers.

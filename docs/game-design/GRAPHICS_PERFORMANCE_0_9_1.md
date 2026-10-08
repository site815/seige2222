# Graphics performance and presentation — v0.9.1 candidate

Updated: **2026-10-08** (Asia/Seoul). Unreal Engine 5.8.3, Windows x64, Ryzen 7 9800X3D / RTX 4070 Ti SUPER / 32 GB, **3840×1600, Medium profile, 100% render resolution, 10× simulation**. All numbers below come from the existing `Tools/benchmark_packaged.ps1` harness on Shipping packages built from this source; the JSON reports named here are in `Saved/`.

The goal set for this revision was "around Manor Lords in quality and performance". This report records what was measured. It does not claim parity; the remaining gap is listed at the end.

## Where the frame time went (v0.9.0)

The retained v0.8.1 ground-view GPU profile (`Saved/v081-profile-ground-gpu.json`) and a fresh v0.9.0 baseline run (`GraphicsBenchmark-v090-baseline-static.json`, 34–45 FPS) agree on the shape of the problem:

| Pass (v0.8.1 ground view, mean) | ms | Cause |
| --- | ---: | --- |
| `GPU/NaniteVisBuffer` | 12.0 | Meadow sward: 33,632-triangle Nanite clumps whose blades use an **alpha-masked** material. Masked Nanite geometry is rasterized by the programmable (software) path, which is far slower than fixed-function Nanite. Hiding the sward alone recovered ~8 ms. |
| `GPU/ShadowProjection` + `GPU/ShadowDepths` | 7.2 | Virtual shadow maps at 4 rays × 2 samples, with the 1,845,603-triangle masked near Jacaranda and 100 m of masked grass casting into them, and a Movable procedural terrain redrawn into shadow pages every frame. |
| `GPU/NaniteBasePass` | 3.8 | Material evaluation for the Nanite grass and trees. |
| Volumetric cloud passes | 2.2 | Default sample counts. |
| Everything else | ~5 | Base pass, lighting, TAA, post-processing. |

Game and render threads were under 2 ms; the frame was entirely GPU-bound.

## What changed

**Grass (the largest single change).** The CC0 `grass_medium_02` tufts and the prepared Bermuda turf were rendered in Blender into an 8-cell impostor atlas, and each clump is now nine bent cards (36 triangles) imported **without Nanite**, with a material that fades instances out per-instance through a dithered mask, animates a small wind offset, and calibrates its colour to the terrain palette with the same macro photograph and meadow-vigor field as `M_TerrainV08` (per-instance custom data). The far Nanite proxy sward (~1.4 million instances within 540 m) is gone; the card layer fades between roughly 75 and 145 m and the stream radius is 200 m. See [Art/EnvironmentV091/README.md](../../Art/EnvironmentV091/README.md).

**Trees.** `forest_programmable_distance_m` (60 m) tells Nanite to rasterize the masked Jacaranda leaf cards as solid quads beyond 60 m, which keeps near trees on the authored masked path and moves the rest onto the fast one. Detail trees hand off to the opaque sprays at 150–250 m instead of 500–700 m. The Nanite edge target returns to 6 (survey 8).

**Shadows and clouds.** VSM 2 rays × 2 samples, directional resolution LOD bias +0.5, sun direction stepped by 0.35° at most every 1.5 s (fewer full page invalidations), grass shadows limited to 45 m, terrain chunks marked `Rigid` for the shadow cache, cloud ray samples 160 / shadow-map samples 32, cloud shadow map at half resolution, `r.HZBOcclusion=1`.

**Presentation.** Sun at 42° instead of 52° (longer shadows), 9.0 / 1.9 sun-to-sky ratio, 6000 K sun, 0.3 cloud shadows, exponential height fog (0.003, 150 m start, 70% max opacity) plus moderate aerial perspective, filmic contrast 1.08, vignette 0.2, bloom 0.1, a faint ground-bounce lower hemisphere on the sky light, slightly cooler meadow palette and a wider meadow-vigor range (`Art/EnvironmentV08/surface_palette.json`, `Tools/import_terrain_v08.py`). All values live in `Graphics/scene.json` and `Graphics/weather.json`; the new keys are validated by both `SeigeCamera.cpp` and `Tools/validate_configuration.mjs`.

**Build.** `Seige.Build.cs` now sets `bUseUnity = false`. Earlier builds only compiled because `git status` marked every file as modified and UBT excluded them all from unity blobs; without git on `PATH` the unity build fails on duplicate anonymous-namespace helpers across files. The project version is 0.9.1 so packages land in `Builds/v0.9.1`.

## Static measurements (Shipping, five authored views)

`GraphicsBenchmark-v090-baseline-static.json` versus `GraphicsBenchmark-v091d-static.json`, same harness, same machine, same session.

| View | v0.9.0 FPS | v0.9.1 FPS | Change | v0.9.0 GPU ms | v0.9.1 GPU ms | v0.9.0 p95 ms | v0.9.1 p95 ms | Full scenery residency s (old / new) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Colony | 44.87 | 67.11 | +50% | 20.64 | 13.67 | 27.17 | 17.05 | 6.32 / 1.11 |
| Meadow | 34.70 | 47.64 | +37% | 27.14 | 19.28 | 36.45 | 25.36 | 0.60 / 0.05 |
| Ground | 33.94 | 47.18 | +39% | 27.87 | 19.68 | 35.95 | 25.71 | 0.12 / 0.00 |
| Hills | 35.33 | 57.16 | +62% | 26.60 | 16.12 | 33.70 | 21.29 | 1.75 / 0.36 |
| Boundary | 39.82 | 62.61 | +57% | 22.80 | 14.20 | 29.81 | 18.26 | 10.43 / 1.81 |

Intermediate candidates are retained for attribution: `v091a` (cards, lighting, shadow/cloud settings: colony 54.6, ground 39.3 FPS), `v091b` (denser nine-card clumps, exposure rebalance: 57.9 / 42.3), `v091c` (tree programmable-raster distance and earlier spray handoff: 66.8 / 47.1), `v091d` (palette and tonal balance only: 67.1 / 47.2). The tree change is therefore worth roughly 2.5 ms per frame in the colony view on top of the grass change.

An editor `-game` GPU profile of the `v091a` ground view (`Saved/Profiling/Profile(20261008_173145).csv`) showed the remaining order: `NaniteVisBuffer` 5.8 ms (trees), `ShadowDepths` 3.4 ms with 10–12 ms spikes on each sun-direction step, `Basepass` 1.9, `ShadowProjection` 1.5, clouds ~2.0, `NaniteBasePass` 1.1. These counters lag and overlap; they identify the next targets rather than a budget.

## Moving-camera measurements

No v0.9.0 orbit or travel baseline was recorded, so the comparisons below use the most recent retained runs of each mode: the v0.9 candidate-7 orbit (`GraphicsBenchmark-v09-candidate7-orbit.json`, same engine and source line, grass shadows at 100 m and no height fog) and the v0.8.1 final Shipping travel run (`GraphicsBenchmark-v081-final-travel.json`). They are older builds, not matched baselines; read the differences as indicative.

Orbit (72°/s, ±8° pitch, live streaming included) — `GraphicsBenchmark-v091d-orbit.json`:

| View | Candidate-7 FPS | v0.9.1 FPS | Change | GPU ms (old / new) | p99 ms (old / new) | Max ms (old / new) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Colony | 52.52 | 64.86 | +23% | 18.43 / 13.96 | 24.56 / 24.51 | 26.2 / 34.2 |
| Meadow | 46.64 | 50.52 | +8% | 20.83 / 18.14 | 36.02 / 30.03 | 39.5 / 32.2 |
| Ground | 42.44 | 49.44 | +16% | 22.97 / 18.69 | 33.28 / 28.36 | 39.0 / 31.4 |
| Hills | 38.85 | 55.70 | +43% | 25.07 / 16.40 | 32.27 / 25.40 | 33.6 / 34.4 |
| Boundary | 41.75 | 60.04 | +44% | 23.26 / 15.26 | 29.67 / 22.20 | 30.2 / 30.4 |

Travel (36 m/s translation) — `GraphicsBenchmark-v091d-travel.json`:

| View | v0.8.1 FPS | v0.9.1 FPS | Change | GPU ms (old / new) | p99 ms (old / new) | Max ms (old / new) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Colony | 44.44 | 59.14 | +33% | 21.83 / 15.37 | 27.20 / 23.62 | 28.4 / 24.0 |
| Meadow | 34.67 | 46.00 | +33% | 28.15 / 19.96 | 33.94 / 30.82 | 35.3 / 35.0 |
| Ground | 37.24 | 45.95 | +23% | 26.21 / 20.18 | 31.44 / 32.88 | 31.6 / 37.5 |
| Hills | 36.96 | 53.67 | +45% | 26.45 / 17.05 | 31.38 / 26.08 | 31.6 / 29.2 |
| Boundary | 38.27 | 55.54 | +45% | 20.68 / 14.65 | 150.75 / 50.82 | 790.5 / 410.7 |

Streaming is lighter because there is no far proxy sward to upload: peak pending cells during travel fell from 193 to 31 at the boundary and from 24–34 to 11–14 elsewhere, the longest visible-cell backlog at the boundary from 1.72 s to 0.72 s, and the initial scenery wait before sampling from 5.2 s to 1.1 s (colony) and 8.7 s to 1.8 s (boundary). The sector-crossing stall is unchanged in kind: the boundary travel sample still contains one game-thread frame of **396 ms** (772 ms in v0.8.1) where terrain and forest are rebuilt synchronously. Less is rebuilt, so it is shorter; it is not fixed. Render-thread means are 1.1–1.3 ms; game-thread means are under 1 ms except at the boundary (3.4–3.9 ms). Every view remains GPU-bound.

The `Seige.Camera` native automation group was rerun against this source after the terrain, foliage and configuration changes: **15 / 15 passed** (`Saved/Automation/v091-camera`, log `Saved/claude-tests-camera.log`), including `SwardPlacementCoverage` and `SwardRootContact` with the card clumps. The rest of the native suite, packaged gameplay routes and display-state checks were not rerun for this candidate.

## Visual review

Captures: `Saved/Screenshots/Benchmark/v090-baseline-static/*.png` versus `v091d-static/*.png`. Observed in review, not measured: longer and darker shadows, visible meadow patchiness from the wider vigor range, distance haze, denser canopies from the opaque leaf cards beyond 60 m, and a card meadow that reads as cover rather than as individual photographed blades. Regressions to watch: the card silhouettes repeat (eight cells), the opaque-beyond-60 m canopies have harder outlines than the masked near trees, and the sun steps 0.35° at a time.

## Remaining gap to the reference

- Trees are still a 1.8M-triangle masked Nanite asset near the camera and a single species; a lighter authored LOD0 with opaque leaf geometry, or true impostors, would be the next largest GPU saving.
- The meadow cards have no LOD and no interaction; no flowers or ground-litter variety beyond the retained wildflower patches.
- Sector-boundary terrain/forest rebuilds remain synchronous.
- No upscaler besides TSR; at 100% render resolution the ground and meadow views stay under 60 FPS on this GPU. Manor Lords on comparable hardware is usually played with DLSS/FSR; the equivalent here is the existing 50–100% render-resolution setting.
- All results are five-second samples of a fresh colony with empty neighbours at midday; loaded colonies, night, winter and long sessions are not covered.

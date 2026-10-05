# Medium graphics and continuous camera survey — v0.6

Status: v0.6 Shipping verification completed with 34 clean native tests, 79 packaged interaction stages, and four actual display states passing. The selected Medium profile uses Nanite edge target 3, TSR history at 100%, and aerial-perspective scale 0.15. Shipping benchmarks measured **20.11–35.06 FPS at 3840×1600** and **79.55–145.45 FPS at 1600×900**, both at 100% render resolution on this machine. Native-resolution close foliage remains too expensive. Manor Lords remains a reference, not a claim of equivalent quality or performance.

## One Medium profile

Medium is a fixed custom profile in [Graphics/scene.json](../../Graphics/scene.json), rather than Unreal's built-in all-Medium setting. It combines High shadows, global illumination, reflections, view distance, post-processing, effects, shading, and landscape settings with Epic texture and foliage groups. Anti-aliasing is a hybrid: the Epic group supplies its quality controls, then the profile explicitly sets TSR history to 100% instead of Epic's 200%. Foliage-only thin-geometry detection and temporal anti-flicker handling remain enabled.

Render resolution is a separate display choice, defaulting to 100%; changing display options reapplies Medium without overwriting that choice. Game DPI awareness now uses physical monitor dimensions: this machine's 3840×1600 display at 200% Windows scaling previously appeared as 1920×800 to a DPI-unaware process. Automated tests and benchmarks do not save player display preferences.

Epic's Lumen guidance provides the reason for the lighting budget: High GI/reflections target a lower rendering budget than Epic. Those platform targets are guidance, not a frame-rate guarantee for this game. [Lumen performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine)

| Change | Purpose and limits |
| --- | --- |
| One ambient sky capture | The current sun/time is fixed, so the skylight captures once instead of continuously. Cloud shadows remain dynamic; ambient lighting does not track every cloud change. A future day/night cycle would need explicit recapture scheduling. |
| Cloud shadow resolution scale 1, previously 2 | Reduces the broad cloud-shadow budget without removing clouds or vegetation. Grass retains its existing 100 m near-camera shadow budget. |
| Exponential fog 0; Mie scale 0.2; aerial-perspective scale 0.15 | Removes the added fog layer and reduces distant surface haze. Sky Atmosphere still renders the sky. The retained Shipping survey shows greener, less blue/desaturated terrain than the earlier survey. |
| Sun source angle 0.6; bloom 0.03 | Uses tighter sunlight shadows and less bloom to improve contrast. These remain visual choices, not independent measured speedups. |
| Foliage-only TSR thin-geometry detection | Targets unstable sub-pixel foliage. It adds some compute work; it must earn its visual benefit in measured comparison. |
| TSR history 100%; moving-pixel history weight 2; sharpening 0.25 | Reduces history-update work and favors clearer motion. Static captures cannot establish whether temporal stability is sufficient. |
| Nanite edge target 3 for near views and surveys | Permits coarser screen-space geometry without removing instances. All Nanite meshes are affected, including trees and buildings. Near and survey values currently match, so the retained configurable survey interpolation introduces no further change. |

Epic documents thin-geometry detection specifically for foliage instability and recommends its foliage-only mode when broader line detection is unnecessary. It is not free and cannot fix normal-map aliasing on flat surfaces. [Thin geometry detection](https://dev.epicgames.com/documentation/unreal-engine/thin-geometry-detection-with-temporal-super-resolution)

The installed UE 5.8 scalability settings give Epic AA a 200% history. At 3840×1600 output this creates a 7680×3200 history, four times the texel count of 100% history. The history-update dispatch uses that size; this does not mean the whole frame costs four times as much. Lowering it can reduce sharpness or stability during reprojection, so moving review remains necessary. [Temporal Super Resolution](https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine)

The separate aerial-perspective distance scale controls atmospheric tint on distant surfaces even without Exponential Height Fog. This is a visual adjustment, not a measured performance improvement. [Sky Atmosphere controls](https://dev.epicgames.com/documentation/en-us/unreal-engine/sky-atmosphere-component-in-unreal-engine)

## Grass coverage and filtering

Near grass instance count, clump geometry, opacity cutoff, and alpha textures are unchanged. The failed v0.5 opaque-distance experiment remains disabled. Nanite's screen-space geometry budget changes without introducing a sparse density ring or removing the meadow at a fixed camera distance. The 350,000 candidates span 81 cells of 54×54 m before coherent meadow and woodland rejection. Each sward has 77,572 source triangles; these are authored counts, not measured triangles rendered by Nanite. Overlapping masked foliage remains a substantial GPU cost. [Nanite aggregate geometry guidance](https://dev.epicgames.com/documentation/unreal-engine/working-with-naniteenabled-content)

The existing grass color sampler used explicit mip bias −2. Installed UE 5.8 `EngineTypes.h` states that this mode disables anisotropic filtering. The imported candidate changes only BaseColor samples to implicit derivative-based mips, allowing anisotropic filtering while retaining the original masked silhouette sampling. Two shared color expressions were changed; all other sample states and the 0.33 opacity cutoff were verified unchanged. No texture source, geometry, tree material, or asset license changed.

The targeted [import script](../../Tools/import_grass_filtering_v06.py) saved `/Game/Art/NatureV04/M_V04Grass` successfully. Its [report](../../Art/EnvironmentV06/grass_filtering_report.json) records before/after sample states. The authoritative [material calibration](../../Tools/calibrate_meadow_materials.py) now preserves computed color mips and the profile applies 16× anisotropic filtering. Static native-resolution meadow and ground comparisons for history 100 and Nanite edge 3 retain blades, continuous cover, and the same authored bare-patch contours. Edge 3 produces coarser middle-distance geometry without the previously rejected opaque-grid or missing-clump appearance. These observations do not establish motion stability or visual parity with the reference game.

## Continuous zoom into a sector survey

The wheel changes desired zoom, and rendered zoom approaches it through frame-rate-aware logarithmic damping. Direct `UpdateCamera()` calls snap, preserving deterministic loading, focus actions, tests, and fixed benchmark cameras. Picking still uses the actual camera and the unchanged cached terrain triangles.

The 3D view remains available for surveying the entire 3.6 km sector before the regional grid appears. The regional overlay fades from zoom 160,000 to 200,000, accepts map interactions from its midpoint at 180,000, and allows further zoom to 360,000. Terrain remains rendered under the fade until its end. These values and damping response are validated external settings. The HUD must use displayed zoom for labels and transition opacity; target zoom is not a substitute during motion.

## Measurement method

The benchmark defaults to Medium at explicit 100% resolution. A fresh player core is deployed through normal construction, then paused with eight empty neighbors. Each fixed view receives at least four seconds of warmup and five seconds of sampled complete frames, with its screenshot taken afterward. Reports record the profile, lighting controls, all quality groups, and each view's actual Nanite edge target. VSync, the frame cap, dynamic resolution, and the Nanite primary-raster time budget are disabled. The expanded report also records effective TSR history scale and aerial-perspective scale.

- `-BenchmarkV05Epic` restores the old Epic groups and lighting controls for a same-build profile reference. Shared assets include the new color-filtering material; this is not an untouched v0.5 package.
- `-BenchmarkDisableThinGeometry` isolates the additional TSR foliage detection cost.
- `-BenchmarkRealtimeSky` isolates continuous skylight capture.
- `-BenchmarkNaniteBaseline` forces the original 1.0 Nanite target, overriding survey adaptation.

A Medium-versus-Epic comparison changes several quality and lighting controls. It must be labelled as a profile comparison, not a claim of equal-quality speedup. Keep full-resolution output fixed, inspect moving foliage and wide terrain, and report frame times together with images. The controlled v0.5 results remain historical evidence in [the v0.5 report](GRAPHICS_PERFORMANCE_0_5.md).

## Native-resolution experiments

These editor game runs use the same 3840×1600 output, explicit 100% rendering, custom Medium groups, 16× anisotropy, grass density, assets, and fixed cameras. The baseline uses history 200 and Nanite edge 1.5, reaching 1.6063 at the boundary camera through the then-active survey transition. One experiment changes only history to 100; the other retains history 200 and sets edge 3 at every view. They precede the new aerial-perspective setting. Console logs confirm the history overrides.

| View | Baseline mean FPS | History 100 mean FPS | History gain | Edge 3 mean FPS | Edge gain |
| --- | ---: | ---: | ---: | ---: | ---: |
| Colony | 21.54 | 24.63 | 14.4% | 23.85 | 10.7% |
| Meadow | 14.53 | 16.19 | 11.4% | 18.10 | 24.6% |
| Ground | 14.83 | 16.60 | 11.9% | 18.31 | 23.4% |
| Hills | 17.05 | 19.79 | 16.1% | 21.23 | 24.5% |
| Boundary | 28.09 | 33.48 | 19.2% | 30.31 | 7.9% |

| View | Baseline mean / p95 ms | History 100 mean / p95 ms | Edge 3 mean / p95 ms |
| --- | ---: | ---: | ---: |
| Colony | 46.43 / 48.77 | 40.60 / 42.28 | 41.92 / 44.62 |
| Meadow | 68.80 / 71.73 | 61.78 / 64.59 | 55.24 / 58.70 |
| Ground | 67.42 / 70.27 | 60.24 / 62.02 | 54.63 / 57.24 |
| Hills | 58.65 / 60.74 | 50.53 / 52.40 | 47.10 / 49.01 |
| Boundary | 35.61 / 37.75 | 29.87 / 32.00 | 32.99 / 35.01 |

Retained evidence: [initial native profile](../../Art/EnvironmentV06/benchmark-v06-medium-native-editor.json), [history 100](../../Art/EnvironmentV06/benchmark-v06-native-history100.json), and [edge 3](../../Art/EnvironmentV06/benchmark-v06-native-edge3.json), copied from their matching `Saved/GraphicsBenchmark-*.json` reports. Each comparison has one five-second sample per view, not repeated trials or a confidence interval. The isolated gains are 11.4–19.2% for history and 7.9–24.6% for edge target. They must not be added together or presented as the combined Shipping improvement. Native performance remains below the desired level in close foliage.

The first 1600×900 run reported 58.48–118.50 FPS and exposed an anisotropy-ordering issue, since corrected and confirmed as 16× in the native reports. Its smaller output and different aspect ratio make it unsuitable for a direct speedup comparison with 3840×1600. A repeated Nanite console lookup was also cached. An attempted combined run using an older DLL with the expanded external schema was invalid and is excluded from performance evidence.

### Combined editor profile

The rebuilt [combined report](../../Art/EnvironmentV06/benchmark-v06-native-combined.json) confirms 3840×1600 output, 100% scene resolution, edge 3 in all five views, history 100, aerial scale 0.15, and 16× anisotropy. It retains 350,000 ground-cover candidates, a zero opaque-grass cutoff, and 100 m grass shadows.

| View | Mean FPS | Mean frame ms | p95 frame ms | Mean FPS gain over initial native profile |
| --- | ---: | ---: | ---: | ---: |
| Colony | 26.98 | 37.06 | 38.93 | 25.3% |
| Meadow | 20.10 | 49.76 | 53.87 | 38.3% |
| Ground | 20.19 | 49.54 | 51.70 | 36.1% |
| Hills | 23.81 | 41.99 | 44.71 | 39.7% |
| Boundary | 35.24 | 28.38 | 30.80 | 25.5% |

These are measured combined results, not sums of the isolated gains. They compare two editor profile states with the same output and scene resolution; the combined run also includes the aerial-perspective change and rebuilt source. The 25.3–39.7% range describes this single sequence only, not a Shipping result or a universal speedup. Roughly 20 FPS in close native-resolution foliage remains a material limitation.

## Final Shipping measurements

The [native Shipping report](../../Art/EnvironmentV06/benchmark-shipping-native.json) and [1600×900 Shipping report](../../Art/EnvironmentV06/benchmark-shipping-medium.json) use the same packaged build, Medium settings, 100% rendering, and five-camera method on the NVIDIA GeForce RTX 4070 Ti SUPER. Reported quality dictionaries match, including history 100, edge 3, 16× anisotropy, and disabled dynamic resolution/VSync/frame cap. Grass is visible with the same candidate count in both.

| View | 3840×1600 mean FPS | 3840×1600 mean / p95 ms | 1600×900 mean FPS | 1600×900 mean / p95 ms |
| --- | ---: | ---: | ---: | ---: |
| Colony | 26.96 | 37.09 / 38.98 | 107.17 | 9.33 / 10.61 |
| Meadow | 20.11 | 49.73 / 54.24 | 80.06 | 12.49 / 13.61 |
| Ground | 20.25 | 49.38 / 52.08 | 79.55 | 12.57 / 13.72 |
| Hills | 23.78 | 42.05 / 44.65 | 80.89 | 12.36 / 13.46 |
| Boundary | 35.06 | 28.52 / 30.54 | 145.45 | 6.88 / 7.68 |

These are separate resolution measurements, not a quality-matched comparison against v0.5 Epic. The native output contains about 4.27 times as many pixels and has a different aspect ratio, changing visible scene coverage. Each view still has only one five-second sample per resolution. The results establish this package's observed cost at those settings; they do not establish minimum FPS, universal gains, or performance during a busy developed colony. Reducing render resolution remains a separate player option, not a second performance profile.

## Final verification and limitations

The rebuilt native suite passed **34 tests cleanly**, with **zero warnings, failures, or unrun cases**, in `Saved/Automation/v06-final/index.json`. The Shipping build completed with exit 0 in `Saved/package-v06.log`.

The packaged interaction route completed **79 stages with zero failures and exit 0**. It recorded **831 rendered frames** in which visible couriers moved between fixed simulation ticks at 1× speed, verifying presentation interpolation instead of merely counting simulation updates. All **84 process-tree socket samples** showed zero TCP and UDP endpoints. Evidence is `Saved/packaged-v0.6.0-UiSmoke-verification.json`. The earlier editor route's 413-frame result is superseded by this packaged functional check; neither count validates the temporal appearance of grass during camera movement.

The [Shipping display report](../../Art/EnvironmentV06/display-shipping.json) passed four states with zero failures: actual 3840×1600 borderless, 1280×720 windowed, 75% rendering at that window size, and restored 3840×1600 borderless at 100%. Custom Medium shadow quality remained active. This verifies real viewport changes and DPI handling, separately from the forced-resolution offscreen performance measurements.

The retained [sector survey](../../Art/Previews/v06_sector_survey.png) confirms reduced blue haze, with intentionally coarse neighboring vegetation still visible across marked boundaries. Grass density and static detail were retained in the inspected captures; camera-motion shimmer/blur and reference-level visual quality are not established by those stills. Native-resolution close grass remains near 20 FPS on this GPU and needs further work.

Configuration validation passes, including 14 earlier malformed graphics/profile cases, seven malformed optional UI cases, and six additional invalid aerial/history values. The completed checks validate this release's functional behavior and measured views; they do not close the remaining visual-quality or native-resolution performance work.

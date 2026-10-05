# Graphics performance and neighboring terrain — v0.5

Status: the matched v0.5 Shipping comparison and package checks completed. With Epic quality, 100% resolution, and unchanged grass density, the 1.5 Nanite edge target improved mean FPS by 5.14–15.10% across five fixed views in one controlled pair. Reviewed meadow and hills captures retain cover, and the forest-floor stripes are visibly reduced. These results do not establish universal performance or visual parity with Manor Lords.

The requested result is a responsive view with convincing nearby terrain and continuous surrounding sectors. The previous v0.4 captures demonstrate the appearance of that release, but their individual FPS overlays are not a controlled performance baseline.

## Final matched Shipping results

Both runs use the same packaged build and assets on an NVIDIA GeForce RTX 4070 Ti SUPER, 1600×900 output, all quality groups at Epic level 3, and explicit 100% resolution. Dynamic resolution, VSync, the frame cap, and the Nanite primary raster time budget are disabled. The only differing reported quality value is `r.Nanite.MaxPixelsPerEdge`: 1.0 baseline versus 1.5 selected default. Grass stays visible with 350,000 candidates, a zero programmable-raster cutoff, and identical lighting flags.

| View | Baseline FPS | Selected FPS | Mean frame ms, baseline → selected | p95 frame ms, baseline → selected | Mean FPS gain |
| --- | ---: | ---: | ---: | ---: | ---: |
| Colony | 61.85 | 66.37 | 16.17 → 15.07 | 17.92 → 16.55 | 7.32% |
| Meadow | 34.18 | 38.80 | 29.25 → 25.77 | 31.29 → 28.11 | 13.50% |
| Ground | 34.45 | 39.66 | 29.03 → 25.22 | 31.29 → 27.08 | 15.10% |
| Hills | 48.09 | 54.61 | 20.79 → 18.31 | 22.10 → 19.62 | 13.55% |
| Boundary | 66.88 | 70.32 | 14.95 → 14.22 | 16.58 → 15.72 | 5.14% |

Each view has at least four seconds of warmup and one five-second sample: 1,230 sampled frames across the baseline views and 1,352 across the selected views. This is one paired test, without repeated trials or a statistical confidence interval. It isolates the Nanite setting within v0.5; it is not a complete v0.4-versus-v0.5 comparison. Mean FPS and p95 frame time both improved in this pair, while close grass remains a material rendering cost.

The retained [baseline report](../../Art/EnvironmentV05/benchmarks/baseline.json) and [selected report](../../Art/EnvironmentV05/benchmarks/optimized.json) include frame counts, sample durations, effective settings, and the measurement method. Reproduce against the current local package from the repository root with [benchmark_packaged.ps1](../../Tools/benchmark_packaged.ps1):

```powershell
.\Tools\benchmark_packaged.ps1 -Name v05-epic-baseline -NaniteBaseline
.\Tools\benchmark_packaged.ps1 -Name v05-epic-optimized
```

The Shipping interaction route also completed 63 stages with zero failures and exit code 0. Its 73 process-socket samples observed zero TCP and UDP sockets. This records the tested offline run, not a claim about every possible network state. Neighbor transitions intentionally retain coarser geometry and sparse woodland; further terrain density, forest variety, and overall visual quality remain future work.

## Evidence guiding this pass

- Epic explains that dense, overlapping foliage remains difficult for Nanite: many small surfaces and masked layers weaken occlusion and increase overdraw. Nanite alone does not make the dense meadow inexpensive. This supports measuring foliage raster and shadow costs before changing coverage. [Working with Nanite-enabled content](https://dev.epicgames.com/documentation/unreal-engine/working-with-naniteenabled-content)
- Virtual Shadow Maps depend on caching. Epic documents `Rigid` as suppressing material-deformation invalidations while retaining transform invalidations; `Static` also suppresses transform invalidations. The current vegetation has no wind deformation, but foundation updates can move instances, so this pass uses `Rigid`. [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)
- Epic's HLOD documentation describes instanced low-detail representations for background objects such as trees. This game creates terrain at runtime and does not currently use World Partition HLOD. Its neighboring terrain uses a simpler manual representation: coarse terrain grids and sparse instances of already loaded tree assets. [World Partition HLOD](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---hierarchical-level-of-detail-in-unreal-engine)
- Epic recommends removing less important overlapping instances from distance-field lighting when they add little to indirect illumination. Its guidance specifically identifies dense grass as a candidate for reducing tracing cost. This pass makes grass participation independently configurable rather than reducing its visible geometry. [Lumen performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine)

Installed UE 5.8 source was checked alongside the documentation. `NaniteResources.cpp` now supplies instance end-cull distances through `GetInstanceDrawDistanceMinMax`; an older general foliage documentation statement about Nanite culling should not be used by itself to diagnose the current build.

## Implemented source changes

| Area | Current change | Preserved behavior |
| --- | --- | --- |
| Ground cover shadows | Grass and wildflower cells cast shadows within the configured camera distance, with a small hysteresis band to avoid repeated switching at the threshold. The default is 100 m. | Nearby grass mesh, candidate density, scale, materials, and placement are unchanged. Tree and building shadows retain their existing behavior. |
| Ground cover indirect lighting | Grass and wildflower components have distance-field and dynamic-indirect-lighting contribution disabled by default. One external boolean controls both flags. | They still receive scene lighting and retain nearby direct shadows. Trees/buildings remain indirect-lighting contributors. Grass occlusion and bounced color can change, so this requires a lighting comparison. |
| Vegetation deformation | World Position Offset evaluation is disabled for these non-deforming assets; their shadow-cache invalidation policy is `Rigid`. | Instance transforms can still update when construction changes a terrain pad. |
| Nanite geometry target | The externally configured `nanite_max_pixels_per_edge` uses 1.5 instead of the engine default 1.0, supported by the matched Shipping pair above. Higher values permit coarser screen-space geometry. | Instance density, texture detail, and nearby grass opacity remain unchanged. This affects all Nanite geometry; the reviewed meadow and hills captures retain cover. |
| Neighbor surfaces | All eight neighboring 128-subdivision grids use the same world-space terrain material as the detailed sector. | The focused grid remains 1024 subdivisions. Terrain heights and cached triangle picking retain the same authority. |
| Background woodland | Each neighbor samples 4,500 deterministic tree candidates, filtered by the existing woodland mask. Four shared instancing components reuse resident tree meshes. Background trees do not cast shadows or contribute distance-field/dynamic indirect lighting by default. | Background placement reads natural terrain rather than hidden colony information. The focused forest is unchanged. |
| Sector edges | Boundary normals use a common sampling distance, and incremental construction updates include that distance when refreshing an edge. | Shared boundary heights and the existing coarse-edge interpolation remain unchanged. |

The controls live in `Graphics/scene.json`: `grass_shadow_distance_m`, `grass_distance_field_lighting`, `neighboring_forest_candidates_per_sector`, `neighboring_forest_shadow`, and `nanite_max_pixels_per_edge`. Nanite edge values must be finite and between 0.5 and 4; command-line, console, or higher-priority platform settings retain precedence over the external project default. The separate `grass_programmable_distance_m` experiment defaults to zero, preserving masked grass at every distance. All are validated with the other presentation settings. The background candidate count is a sampling budget, not the final number of tree instances.

The visible region boundaries and label positioning are handled by the HUD. Neighboring scenery does not grant access to another colony's inventories, buildings, or simulation state.

## Measurement and acceptance

The matched benchmark uses a fresh player core at the origin, deployed through the normal construction simulation and then paused, with eight empty neighbors. This isolates landscape rendering from AI development and keeps both runs in the same state. It uses fixed colony, meadow, ground, hills, and sector-boundary cameras. Each view gets at least four seconds of warmup after synchronous setup, followed by at least five seconds of complete frame intervals. Screenshots occur on a later unsampled frame; the camera advances only after the screenshot request is processed. The benchmark now applies Epic quality level 3 and explicit 100% resolution quality before scenario setup, without saving player settings. Its report includes every quality group, screen-percentage controls, runtime resolution quality, and Nanite edge/budget settings.

The earlier lighting investigation compared the default 100 m shadows with grass indirect-lighting contribution disabled against `-BenchmarkFullGrassShadows -BenchmarkFullGrassLighting`, restoring 500 m shadows and both grass lighting flags. This measured their combined effect; it did not isolate either flag. The final matched Shipping pair instead holds both flags constant and changes only the Nanite edge target. Reports record mean frame time, p95 frame time, sample count, and settings; separate editor GPU profiling helps identify shadow, indirect-lighting, foliage rasterization, and other costs.

The existing Shipping configuration disables Unreal's CSV profiler at compile time, so a command-line CSV request against the v0.4 package did not produce a valid timed baseline. The dedicated timing report and editor profiling must supply the comparison. When CSV profiling is available and active, the benchmark calls the engine's `EndCapture()` API and continues ticking until its asynchronous file-write future resolves before exiting. Do not convert isolated FPS screenshots into averaged benchmark claims.

Acceptance requires intact nearby cover, no newly visible hidden-colony clearings, continuous sector edges, correct terrain picking, and measured performance evidence. A shadow-distance reduction may have limited benefit if masked foliage rasterization dominates. Further geometry or density changes should follow that evidence and retain a credible distant surface.

## Historical exploratory measurements

The initial five-second samples compare the same geometry and candidate density. Results are mixed: close-view frame time is modestly lower with the new lighting settings, while some wider views are slower. These single paired runs do not establish a consistent overall gain.

| View | 100 m shadows, grass indirect contribution off | 500 m shadows, contribution on |
| --- | ---: | ---: |
| Colony | 13.55 ms / 73.81 FPS | 13.43 ms / 74.45 FPS |
| Meadow | 24.56 ms / 40.72 FPS | 25.25 ms / 39.60 FPS |
| Ground | 24.41 ms / 40.97 FPS | 25.44 ms / 39.31 FPS |
| Hills | 18.52 ms / 53.99 FPS | 17.95 ms / 55.71 FPS |
| Boundary | 13.43 ms / 74.48 FPS | 12.68 ms / 78.85 FPS |

Raw timing reports are retained locally as `Saved/GraphicsBenchmark-optimized.json` and `Saved/GraphicsBenchmark-full-grass-lighting.json`. These historical runs preceded the explicit quality lock. They are not the baseline for the final locked 100% measurements above.

### Launch-quality mismatch found during investigation

Both the CSV and ordinary runs requested a 1600×900 output, but they loaded different quality settings. `Saved/benchmark-v05-gpu-profile.log` applies High level 2 after the initial Epic defaults; ordinary optimized, grass80, and verified hidden-sward logs retain Epic level 3. For example, the CSV run uses 100% TSR history rather than 200%, directional virtual-shadow bias 0 rather than −1.5, and Lumen radiance-cache probe resolution 16 rather than 32. The roughly 50 FPS CSV close views therefore cannot be compared directly with the roughly 40 FPS ordinary close views.

The CSV boot capture also resolves its Saved/config directory under the engine user directory, while ordinary launches use the project Saved directory. Installed engine code starts boot capture before normal startup, obtains the profiling directory through the cached `ProjectSavedDir`, and does not reset that cached path. This is consistent with the different saved settings, but the exact startup-order cause has not been runtime-instrumented. CSV's target-frame-rate override only describes timing metadata; no automatic rendering-quality override was found in the capture initialization examined.

The later ordinary reports show `r.ScreenPercentage=0`, which requests the engine's automatic screen-percentage policy; it does not establish native internal rendering resolution. Their 1600×900 output size alone is insufficient evidence. Future comparisons require matching reported quality, explicit resolution, binary, assets, camera, and profiler mode. The new Epic/100% lock establishes a new measurement series, not a retrospective correction of old frame times.

The separate editor GPU diagnostic identifies the dominant work under its High level 2 settings. The [analysis report](../../Art/EnvironmentV04/gpu_profile_analysis_v05.json) covers all 3,366 frames and five camera poses. Unreal appended columns during this CSV capture, so analysis uses its final 391-column header and fills unavailable early columns with zeros. Each reported steady window lasts at least five seconds and ends about half a second before screenshot/transition work. An initial analysis that used only the opening header omitted later views; the linked report replaces that incomplete interpretation.

| View | Game thread | GPU frame | Nanite visibility pass |
| --- | ---: | ---: | ---: |
| Colony | 0.96 ms | 11.18 ms | 3.73 ms |
| Meadow | 0.89 ms | 18.69 ms | 10.32 ms |
| Ground | 0.90 ms | 19.26 ms | 10.57 ms |
| Hills | 0.89 ms | 16.76 ms | 9.39 ms |
| Boundary | 2.72 ms | 8.20 ms | 1.38 ms |

Nanite visibility consumes roughly 55% of GPU time in the two close views. Their shadow-depth and shadow-projection scopes total roughly 2.3 ms, while cloud-shadow work is about 1 ms and temporal reconstruction about 0.9 ms. GPU scopes may overlap; these are diagnostic timings rather than an additive budget. Absolute frame rates from this separate profiling session are not an A/B comparison with the preceding table.

The verified ordinary hidden-sward run records `diagnostic_sward_hidden=true` and Epic groups at 1600×900 output. Meadow and ground improve to 16.77/16.86 ms (59.63/59.31 FPS), compared with the earlier ordinary visible-sward 24.56/24.41 ms. This supports grass as a substantial cost, but hiding grass is a diagnostic, not an acceptable visual optimization; repeat comparisons should use the newly locked settings. An earlier attempted no-sward run used a binary compiled before the hide flag existed; it is a normal repeat and provides no grass-isolation evidence. Texture packing alone cannot remove the dominant masked visibility cost.

## Reversible Nanite raster experiment

The final alpha-trim candidate found no wholly transparent triangles that could be safely removed. It reduced Bermuda card surface area by about 5.67%, while increasing the complete sward from 77,572 to 79,924 triangles (about 3.03%); tall-grass geometry was unchanged. This is a reduction in card area, not a measurement of opaque coverage or GPU work. The candidate remains unimported and is not selected for delivery. Earlier approximate 3.2%/2% figures described the first query rather than this final report.

Epic exposes `NanitePixelProgrammableDistance` on static-mesh components to disable pixel-programmable rasterization past a configured distance. The installed UE 5.8 culling shader switches each instance to its fallback raster bin when its bounds center exceeds that distance; zero disables the cutoff. [Static mesh component API](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UStaticMeshComponent?lang=en-US)

The benchmark flag `-BenchmarkGrassOpaqueBeyond80` sets an 80 m cutoff for only the Grass and GrassB meshes before their components register. Wildflowers and trees remain unchanged. Grass within that instance-distance threshold retains its opacity mask; farther grass can fill alpha holes and change silhouettes. The threshold applies per sward, not per 54 m streamed cell. The ordinary test produced 25.05/24.74 ms for meadow/ground (39.91/40.42 FPS), giving no benefit over the earlier visible-sward baseline. The normal configuration remains zero; this experiment is not selected as an optimization.

A separate ordinary diagnostic changed `r.Nanite.MaxPixelsPerEdge` from 1 to 1.5 with the primary raster time budget disabled. Meadow/ground measured 21.13/20.78 ms (47.32/48.13 FPS), versus the earlier approximately 41 FPS visible-sward measurements. This motivated the final matched Epic/100% comparison above. The earlier automatic-resolution figures are retained as investigation history and are not the release performance claim.

For that comparison, `-BenchmarkNaniteBaseline` explicitly restores 1.0 before scenario setup; an ordinary benchmark uses the configured 1.5. Both report the configured value, effective runtime CVar, and diagnostic flag. Keep `r.Nanite.PrimaryRaster.TimeBudgetMs=0` and the grass programmable cutoff at zero. This is a global screen-space geometry setting, including trees and buildings, and may reduce detail. The engine's experimental Nanite Foliage voxel pipeline would require an asset/project migration rather than this reversible parameter test. [Nanite foliage](https://dev.epicgames.com/documentation/unreal-engine/nanite-foliage)

## Grass-distance constraint

The streamed 9×9 ground-cover cells span about 486 m around the camera's ground target, not around the camera itself. A universal 300 m camera-distance cutoff can remove nearly all grass in the hills view, whose camera is about 403 m from its target and roughly 300 m above it. That would risk restoring the smooth wide meadow rejected during v0.4. The current material also has no instance-fade input, so merely setting a 180 m start and 300 m end does not guarantee a smooth visual transition. Keep close density intact; any conservative far cutoff or cheaper distant representation needs comparison against wide captures.

## Forest-floor aliasing follow-up

The first optimized boundary capture showed fine repeated brown stripes in woodland regions. The terrain shader blends two forest texture frequencies near a 3 m physical scale without reducing their contrast at long distances. The correspondence with the woodland mask supports this diagnosis, although the capture alone does not identify whether color or normal detail dominates the artifact.

A targeted importer change is recorded in `Art/EnvironmentV04/terrain_import_report.json`, and the reviewed Shipping captures show visibly reduced stripes. From 200 to 600 m camera distance, only the forest color, normal, roughness, and AO smoothly approach their measured means and a flat tangent normal. The [source measurement report](../../Art/EnvironmentV04/forest_far_detail_analysis.json) records the linear averages and defaults. Existing forest tint, broad continuous variation, tree coverage, and all near forest samples remain. Meadow, soil, rock, and terrain geometry are unchanged. This interpolation addresses aliasing; it does not remove texture samples and is not claimed as a GPU optimization. Neighboring sectors remain deliberately coarse, and broader density and visual-quality improvements remain open.

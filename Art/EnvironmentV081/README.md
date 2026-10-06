# v0.8.1 meadow and scenery streaming

Status: final Shipping static, orbit and travel reports have been refreshed for the 0A8AB4BC executable. Gameplay acceptance is recorded separately. Measured timing and residency counters retain traversal-hitch and unresolved prior-F9 GPU-startup-risk caveats. The [graphics report](../../docs/game-design/GRAPHICS_PERFORMANCE_0_8.md) retains the preceding v0.8 Shipping baseline; these are different revisions and builds.

## Selected assets and profile

Each new near-meadow patch contains **33,632 triangles**, versus 77,572 previously. Sixteen photographic tall tufts remain, while the old 920 masked low tufts are replaced by four original opaque blade beds containing **3,072 blades total**. The authored A/B bounds and ground pivots are preserved for deterministic placement and proxy alignment. The [export manifest](Exports/environment_manifest.json) records counts, dimensions and hashes; the [generator](../../Tools/prepare_meadow_v081.py) and [importer](../../Tools/import_meadow_v081.py) reproduce the assets.

Photographic geometry/materials reuse the existing local CC0 sources recorded in [EnvironmentV04/sources.json](../EnvironmentV04/sources.json). Opaque bed geometry derives from the project's original [v0.7 grass proxy](../EnvironmentV07/proxy_manifest.json). This pass downloads no assets and does not include Manor Lords assets. The existing detailed tree assets and their **500-700 m** transition to source-derived canopy proxies are retained.

The selected single Medium profile uses a **6-pixel Nanite edge target** and **four directional shadow rays with two samples per ray**. Grass transitions from photographic detail to the existing opaque far representation over **15-35 m**. These are rendering tradeoffs, not equal-quality performance claims. Lighter geometry and earlier handoff can expose ground or change the distant appearance; reduced shadow sampling can affect motion stability. Visual handoffs and residual ground patterns remain limitations; timing gains do not establish visual parity.

## Runtime scheduling

[SeigeTerrain.cpp](../../Source/Seige/SeigeTerrain.cpp) keeps deterministic 54 m cells and a 540 m residency radius around the focus and camera, with a one-cell retention margin. It prioritizes actual nearby detail and frustum-visible cover before offscreen work. Bounds use the cached terrain vertices; distance includes camera altitude. View direction, projection, camera travel and changed terrain invalidate the relevant priority data.

Detail prefetch extends half a cell beyond the 35 m handoff limit. Visible ground ahead receives priority. Scheduling does not change candidate count, locations, occupancy rules, terrain height or plot exclusions; the separate asset revision changes geometry within each retained patch. No opacity fading is introduced. Near and far representations retain matching bounds and source positions.

The external work target is **4 ms**, with a **32-cell** completion safety ceiling. Candidate batches and per-mesh/per-band uploads are resumable. An individual engine call can exceed the target; it is not a guaranteed frame-time cap. Root-plane contact is fitted once per accepted source transform and reused by its proxy. Changed foundation/road revisions trigger clearance and surface revalidation for pending groups. Partial commits remain tracked and cannot be retired midway.

## Final Shipping evidence

The refreshed final Shipping reports are scoped to executable SHA-256 `0A8AB4BC5772C3DC49434D68E8ABF138F76E2C670469EAC943B9D3E557E436B7`, Unreal 5.8.3, Ryzen 7 9800X3D / RTX 4070 Ti SUPER / 32 GB, **3840x1600, Medium, 100% rendering and 10x simulation**. Runs are sequential and isolated. Each view uses a fresh deployed core with eight empty neighbors, waits for initial scenery, settles at least four seconds and samples at least five seconds. Static samples require full residency; orbit turns at **72 degrees/s** with **8-degree pitch amplitude**, and travel moves at **36 m/s**, retaining normal streaming work. Setup/wait and screenshot costs are separate. No diagnostic rendering mode is active. Benchmark JSON does not embed an executable hash; the release runner establishes the launch scope, while this documentation helper checks the packaged hash and report freshness.

Reports: [static](benchmark-shipping-native.json), [orbit](benchmark-shipping-orbit.json) and [travel](benchmark-shipping-travel.json). Historical comparison: [v0.8 static](../EnvironmentV08/benchmark-shipping-native.json) and [v0.8 orbit](../EnvironmentV08/benchmark-shipping-orbit-native.json).

| View | Static FPS | Static p95, ms | FPS vs v0.8 | Orbit FPS | Orbit p95, ms | FPS vs v0.8 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Colony | 48.56 | 21.87 | +0.94% | 48.99 | 24.23 | +0.23% |
| Meadow | 37.81 | 28.82 | +15.79% | 44.37 | 31.40 | +4.00% |
| Ground | 37.49 | 28.71 | +22.80% | 40.60 | 30.29 | +23.83% |
| Hills | 37.75 | 28.25 | -1.00% | 37.13 | 32.01 | +1.64% |
| Boundary | 43.39 | 24.44 | -1.81% | 40.49 | 27.20 | -2.00% |

The deltas compare the retained v0.8 and v0.8.1 Shipping releases at the same resolution and named views. Geometry, handoff distances, shadow sampling and Nanite targets changed together; this is neither equal-quality evidence nor an isolated optimization measurement. Static meadow changes from **32.66 to 37.81 FPS**, static ground from **30.53 to 37.49 FPS**, and orbit ground from **32.79 to 40.60 FPS**. Short samples do not establish sustained 60 FPS or long-session/endgame performance.

| View | Travel FPS | p95 / p99 / max frame, ms | Max visible cells pending | Longest visible backlog, s |
| --- | ---: | ---: | ---: | ---: |
| Colony | 44.44 | 25.31 / 27.20 / 28.36 | 0 | 0.000 |
| Meadow | 34.67 | 32.67 / 33.94 / 35.27 | 0 | 0.000 |
| Ground | 37.24 | 29.25 / 31.44 / 31.56 | 5 | 0.085 |
| Hills | 36.96 | 29.91 / 31.38 / 31.57 | 0 | 0.000 |
| Boundary | 38.27 | 24.97 / 150.75 / 790.53 | 97 | 1.719 |

**Boundary travel remains the critical traversal check:** maximum frame **790.53 ms**, p99 **150.75 ms**, longest visible-cell backlog **1.719 seconds**, and **97 visible cells** pending at peak. Sector-focus changes still rebuild terrain/forest and reset grass residency. Ground travel reaches 5 visible pending cells for a longest continuous **0.085-second** interval. Peak near-cell backlog across travel is 0; an elevated camera can have no near detail demand while visible cover is incomplete. Orbit peaks are 0 visible / 0 near / 124 total pending cells. These counters do not establish eliminated pop-in. The prior F9 checkpoint measured a 773.85 ms boundary frame and 1.702 seconds of visible backlog; the final executable's intervening change was test-harness pointer/capture handling, not a terrain-rebuild fix.

| Static view | Initial visible wait, s | Initial near wait, s | Full residency wait, s |
| --- | ---: | ---: | ---: |
| Colony | 0.538 | 0.000 | 5.015 |
| Meadow | 0.023 | 0.023 | 0.407 |
| Ground | 0.000 | 0.000 | 0.095 |
| Hills | 0.000 | 0.000 | 1.690 |
| Boundary | 1.026 | 0.000 | 9.627 |

Initial waits exclude synchronous setup. A zero wait can mean earlier views already populated the visible cells, not instant fresh loading. Full residency takes up to 9.627 seconds in the static route; orbit boundary setup waits 12.771 seconds. Orbit ends with 118 total pending cells at hills and 116 at boundary. Static samples have zero pending cells.

Visible/near counters are conservative CPU cell-residency measures, not pixel coverage or GPU fences, and can lag camera movement by one frame. Initial timestamps guard against uninitialized zeros. Native `Seige.Camera.SceneryStreamingPriority` covers altitude/frustum/priority logic. Static GPU means range from **19.92 to 25.99 ms**; these lagged counters are bottleneck evidence, not synchronized frame traces.

The earlier **F9 executable** (`F9A8056182903BAC7D9E0F9BCA57B3BB9F680DD994F84182A4710D026A160CEA`) had one **GPU startup crash at 0 seconds** (device-removed reason `-2005270522`, no DRED breadcrumbs). An unchanged-F9 retry passed all four display states. This is separate history from final 0A8AB4BC checks. The cause remains unresolved and may recur; the retry is not a fix. Only selected fields and local evidence hashes belong in public verification; the raw XML contains private machine fields.

These are graphics measurements only; full-route gameplay acceptance is recorded separately in the development/verification records. Residual terrain patterns, visible vegetation handoffs, distant silhouettes and grazing-angle cost remain limitations. No complete pop-in removal, sustained 60 FPS or Manor Lords visual parity is claimed.

## Historical Editor candidate evidence

The [retained candidate report](benchmark-editor-candidate.json) used 3840x1600, Medium, 100% rendering and 10x simulation, with a fresh core and eight empty neighbors. Each static view waited for full residency, settled four seconds and sampled at least five seconds. No grass hiding or simplified-terrain diagnostic was active. Actual runtime Nanite was 6; the recorded configured value of 4 reflects that experiment's command-line override, subsequently authored as 6 in the current scene configuration.

| View | Mean FPS | p95 frame, ms | Initial visible wait, s | Full residency wait, s |
| --- | ---: | ---: | ---: | ---: |
| Colony | 50.48 | 20.71 | 0.785 | 5.617 |
| Meadow | 38.95 | 29.18 | 0.024 | 0.454 |
| Ground | 38.64 | 28.87 | 0.000 | 0.116 |
| Hills | 38.38 | 27.69 | 0.000 | 1.921 |
| Boundary | 43.40 | 26.25 | 1.182 | 9.612 |

Visible wait excludes synchronous setup. Ground and hills began with zero visible backlog after earlier views had populated those areas; zero does not mean instant fresh loading. Full residency still takes longer than visible cover, notably at the boundary. These are short Editor candidate samples, not Shipping acceptance, a sustained frame-rate promise or an isolated gain attributable to one change.

## Coverage metrics and remaining limits

[SeigeSceneryStreaming.h](../../Source/Seige/SeigeSceneryStreaming.h) supplies the frustum/distance and priority helpers. Native `Seige.Camera.SceneryStreamingPriority` covers altitude, frustum edges and work ordering; existing contact, placement, seam and privacy checks remain relevant.

`PendingVisibleSceneryCells()` counts incomplete frustum-intersecting cells; `PendingNearSceneryCells()` counts incomplete cells within the actual 3D detail radius. They are conservative CPU cell-residency counters, not pixel coverage or GPU upload fences. Initial first-visible/first-near timestamps reject false zeros from uninitialized state. `-BenchmarkTravel` samples 36 m/s movement and records visible/near backlog and its longest consecutive duration without removing normal streaming work. The final travel report above includes the unresolved boundary hitch.

Changing the focused sector still rebuilds terrain/forest and resets grass residency; the measured boundary hitch remains unresolved. Visible detail, handoffs, residual ground patterns and grazing-angle cost remain review concerns. No complete pop-in removal, sustained 60 FPS or Manor Lords visual parity is claimed.

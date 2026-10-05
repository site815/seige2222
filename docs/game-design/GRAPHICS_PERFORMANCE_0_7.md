# Scenery detail and performance — v0.7

The v0.7 Shipping build passes its functional and offline checks and improves the measured native-resolution frame rates. Close foliage remains about 30 FPS at 3840×1600. Source-derived distant trees and the darker palette are accepted improvements, with recognizable low-detail silhouettes remaining. This is not a claim of Manor Lords visual parity.

## Current implementation

The game exposes one **Medium** profile, with output resolution and render percentage controlled separately. At 100% rendering it uses TAA; below 100% it uses TSR for reconstruction. Native TAA avoids TSR's reconstruction cost at full resolution, with a tradeoff in fine foliage stability and sharpness. The moving route was exercised, but does not prove that ghosting or shimmer has disappeared.

The externally editable profile uses View Distance 2, Anti-Aliasing 2, Shadows 2, Global Illumination 1, Reflections 1, Post Process 2, Textures 3, Effects 2, Foliage 3, Shading 2, and Landscape 2. Nanite's edge target is 4 pixels and anisotropic filtering is 16. Native TAA uses quality 2, filter size 0.8, and current-frame weight 0.08; TSR history resolution remains 100%.

In installed UE 5.8, GI level 1 retains Lumen using its irradiance-volume final gather and a smaller, slower-updating surface cache. Reflections level 1 uses half-resolution screen-space reflections and disables Lumen reflections. Screen-space reflections cannot show information absent from the current view. [Epic's SSR documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/screen-space-reflections-in-unreal-engine)

| Change | Current behavior |
| --- | --- |
| Grass detail | Existing photographic swards nearby; an original opaque 1,280-triangle geometric sward farther away. Eight fixed distance bands spread the handoff across 25–50 m. No density cut, filled ground rectangle, or alpha-card-to-opaque substitution. |
| Tree detail | Authored Jacaranda LOD1 nearby, preserving photographic materials; source-derived opaque crowns farther away. Handoffs span 500–700 m. Broadleaf/fir proxies use 23,422/18,000 source triangles and retain reduced real trunks and branches. |
| Stable placement | All nine sectors use the same full deterministic forest sequence regardless of focus. Near/far representations share placements and aligned world bounds centers, including source-pivot offsets. Hidden colony data does not alter public scenery. |
| Streaming | Grass is prepared under a 2 ms soft budget, at most two completed cells per frame, with coverage extending 540 m around camera and focus. Near detail is preloaded around the actual camera. |
| Spatial batches | Nanite ISMs group two-by-two cells per component. Small additions no longer invalidate a global foliage batch. Construction clearance also updates pending cells and removes retired instance handles. |
| Terrain and lighting | The terrain material skips unused photographic layers and replaces expensive procedural macro work. Daylight, cloud shadow, exposure, and saturation are externally editable. |

The 350,000 ground-cover setting defines density over the original 81-cell reference area; it does not cap all resident instances in the larger streamed region. The wider region uses more logical placements. Full-cell uploads and removals may still exceed the soft CPU budget.

The [asset record](../../Art/EnvironmentV07/README.md) retains editable sources, counts, import validation, and CC0 provenance. Near Jacaranda LOD1 has 1,845,603 source triangles versus the retained LOD0's 3,863,832. Source triangle counts are not per-frame GPU savings. Epic's guidance identifies aggregate foliage and alpha masking as costly for Nanite, and recommends ISM rather than redundant HISM trees when using Nanite. The project uses ordinary geometry, not Epic's experimental Nanite foliage voxel pipeline. [Nanite foliage](https://dev.epicgames.com/documentation/unreal-engine/nanite-foliage), [ISM guidance](https://dev.epicgames.com/documentation/unreal-engine/instanced-static-mesh-component-in-unreal-engine)

## Shipping measurements

Both releases were sampled at **3840×1600, 100% render resolution**, on the tested RTX 4070 Ti SUPER / Ryzen 7 9800X3D system. Each static view records at least five seconds after warmup. The comparison measures whole releases: geometry, shaders, lighting, anti-aliasing, and Medium settings changed. It is not an equal-quality comparison or an isolated optimization result. [v0.6 raw report](../../Art/EnvironmentV06/benchmark-shipping-native.json), [v0.7 raw report](../../Art/EnvironmentV07/benchmark-shipping-native.json)

| Static view | v0.6 FPS | v0.7 FPS | v0.7 mean ms | v0.7 p95 ms | FPS change |
| --- | ---: | ---: | ---: | ---: | ---: |
| Colony | 26.96 | 46.23 | 21.63 | 23.06 | +71.4% |
| Meadow | 20.11 | 31.84 | 31.40 | 34.04 | +58.3% |
| Ground | 20.25 | 30.35 | 32.95 | 35.39 | +49.8% |
| Hills | 23.78 | 36.81 | 27.17 | 28.93 | +54.8% |
| Boundary | 35.06 | 43.98 | 22.74 | 24.18 | +25.5% |

These are short samples on one machine, not guaranteed frame rates. The current static samples are GPU limited: reported GPU means are approximately 21–32 ms, versus game-thread means of 0.46–1.75 ms. Thread/GPU counters can lag or repeat; they identify likely bottlenecks, not synchronized per-frame traces.

The [separate Shipping orbit run](../../Art/EnvironmentV07/benchmark-shipping-orbit-native.json) includes camera-driven streaming during movement at the same native resolution:

| Moving view | Mean FPS | p95 frame ms | Mean game-thread ms |
| --- | ---: | ---: | ---: |
| Colony | 46.85 | 24.33 | 1.72 |
| Meadow | 42.19 | 35.66 | 0.58 |
| Ground | 32.95 | 37.27 | 0.59 |
| Hills | 36.40 | 31.59 | 2.79 |
| Boundary | 41.49 | 26.44 | 3.43 |

The retained [earlier editor orbit baseline](../../Art/EnvironmentV07/benchmark-editor-orbit-baseline.json) is diagnostic context, not a same-binary Shipping comparison. The current benchmark waits for `IsSceneryStreamingReady()`, then allows four seconds to settle before sampling. The older baseline started warmup after synchronous setup. Screenshot cost and initial residency waits are excluded from measured frame times; live streaming during orbit remains included.

At **1600×900 and 100% rendering**, the same current Medium profile is much faster. This is a separate output-resolution comparison, not temporal upscaling. [v0.6 report](../../Art/EnvironmentV06/benchmark-shipping-medium.json), [v0.7 report](../../Art/EnvironmentV07/benchmark-shipping-medium.json)

| Static view | v0.6 FPS | v0.7 FPS | v0.7 p95 ms |
| --- | ---: | ---: | ---: |
| Colony | 107.17 | 153.74 | 7.62 |
| Meadow | 80.06 | 103.30 | 10.93 |
| Ground | 79.55 | 102.86 | 10.90 |
| Hills | 80.89 | 94.18 | 11.57 |
| Boundary | 145.45 | 107.25 | 10.38 |

**The 1600×900 boundary view regresses by 26.3%.** More consistent neighboring forest coverage brings additional scenery work, but no isolated experiment attributes this regression to that change alone. The release does not improve every view at every resolution.

## Verification

- **38 native automation tests passed**, with no warnings, failures, or unrun tests (`Saved/Automation/v07-final/index.json`).
- The Shipping interaction route completed **79 stages with zero failures** and exit code 0. All **86 sampled process-network checks** reported no TCP or UDP endpoints; this verifies the observed run, not every possible future session.
- [Four display states passed](../../Art/EnvironmentV07/display-shipping.json): native borderless, windowed, 75% rendering, and restored native. They verified TAA / TAA / TSR / TAA respectively. All nine staged JSON configuration files matched source hashes.
- Mesh imports validated source colors, bounds, and material slots. Retained Shipping captures include [colony](../../Art/Previews/v07_colony.png), [ground](../../Art/Previews/v07_ground.png), [hills](../../Art/Previews/v07_hills.png), and [boundary](../../Art/Previews/v07_boundary.png).

## Research applied

PUBG's historical official patch notes describe model caching, more efficient creation/deletion, preloading to reduce hitches, prediction-based streaming, and avoiding unnecessary updates or invisible effects. These are useful general engineering patterns, not published internal settings to copy or evidence that this project has PUBG's performance. They informed the emphasis on stable instances, spatial batches, and separate CPU/GPU measurements. [Console Update 10.3](https://www.pubg.com/en-asia/news/1372), [archived console patch notes](https://www.pubg.com/en/news/1733)

Epic's Lumen guide recommends measuring individual GPU passes and describes the cost/quality tradeoff of lower internal resolution. Its TSR documentation identifies history update as a major cost and explains how history resolution and accumulation settings affect sharpness, stability, and motion blur. This supports measuring native rendering separately from upscaling and retaining motion review alongside timing. It does not prescribe this game's exact Medium profile. [Lumen performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-performance-guide-for-unreal-engine), [TSR documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/temporal-super-resolution-in-unreal-engine)

## Remaining limits and rejected approaches

Changing the focused sector still rebuilds terrain, all nine forests, and streamed ground cover. An intermediate editor log measured about 0.24–0.29 seconds for terrain and 0.56–0.78 seconds for forest. The final native Shipping static run waited 10.07 seconds for first-view scenery readiness. These are initialization observations, not a sector-crossing benchmark. The five orbit views keep their focus in one sector and exclude this rebuild from timing. Orbit streaming remained active, reaching 16 / 0 / 0 / 72 / 128 pending cells across the five views. Sector-local forest batches and visibility-aware height updates remain needed. A duplicate per-frame shadow scan was removed; its separate benefit is unmeasured.

Early candidates failed review: unchecked material links made proxies black; import settings then produced white vertex colors; corrected colors still left artificial lobed crowns. The final geometry instead follows the actual source leaf distribution. The importer checks every material link, explicitly updates both task and stored reimport settings, and verifies Unreal's source colors quantitatively. Material base albedo is explicit so missing runtime vertex attributes cannot bleach the result. A first global-batch implementation also caused large CPU stalls; spatial pages replaced it. These failed candidates are not release evidence.

Remaining visual risks include recognizable silhouette/color changes at LOD handoffs, foliage shimmer during motion, and expensive nearby masked vegetation at native ultrawide resolution. Memory use and focus-change stalls also remain. The reviewed captures are accepted as an incremental revision, not visual parity with the reference game; higher FPS alone is insufficient acceptance.

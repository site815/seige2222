# Ground and grass surfaces — v0.8

The retained photographed forest-fade shader reimported cleanly with zero errors/warnings (`Saved/import-terrain-v08-final.log`). Targeted native runs pass: **23 terrain/combat tests** and the later **17 placement tests**; they overlap and must not be added as unique coverage. **Residual soil banding and some bare grass patches remain.** The matching placement view shows markedly fewer long gaps, but not complete removal. Final local boot/offline, display and native-resolution static/orbit checks are complete, with these visual limits retained.

The [importer](../../Tools/import_terrain_v08.py) writes `/Game/Art/NatureV08/M_TerrainV08`, `M_GrassProxyV08`, `MI_GrassProxyV08`, and `SM_GrassProxyV08`. The proxy retains the original 1,280-triangle v0.7 geometry; earlier assets remain available. The [import report](surface_import_report.json) records the current graph, palette and source textures.

## Current material

Existing photographs are calibrated by their measured linear means to the [editable palette](surface_palette.json), with bounded local color variation and one shared **22 m photographic macro sample**. Terrain vertex RGB selects soil, rock and woodland; alpha carries meadow vigor.

Ordinary layers, including Dirt, retain two photographic color scales with nearby normal/roughness detail. Forest keeps `forest_leaves_02` nearby and blends over **40–120 m** camera distance to one rotated `leafy_grass` photograph using the unchanged Forest palette. Coordinates and explicit gradients rotate together; original normal/roughness detail fades across the transition.

| Active layer path | Maximum photographic/normal/roughness samples |
| --- | ---: |
| Ordinary layer, near | 4 |
| Ordinary layer, far | 2 |
| Forest, near/blending | 4 |
| Forest beyond transition | 1 |

There is **one additional shared 22 m macro sample** per terrain pixel. Multiple active layers add their costs; these counts are graph budgets, not measured GPU timings. This retained shader is imported; the [analysis record](forest_pattern_analysis.json) records its log and hash.

The photographed source pixels are unchanged, and no textures were downloaded for this correction. It adds no opacity masks, dithering, emissive light or flat-color terrain fill. [Source measurements](surface_measurements.json) and [pattern analysis](forest_pattern_analysis.json) retain provenance and diagnostic limits. Directional source statistics alone do not prove the cause of the remaining rendered bands.

Far grass retains opaque leaf geometry and two-sided foliage shading. Its lighting normals blend toward each instance's terrain-aligned up vector. The material pass preserves mesh geometry and distance bands. The separate placement revision below changes candidate distribution.

## Grass contact and terrain color

The [root-contact helper](../../Source/Seige/SeigeSceneryContact.h) samples a 3-by-3 support grid across each authored root plane. It raises the clump only by measured penetration into the authoritative triangle surface; planar contact receives no blanket lift. XY position, rotation, scale, density and terrain geometry remain unchanged, and proxy transforms follow the corrected source.

The latest placement revision uses one lightly jittered candidate per staggered stratum, with the same requested count, deterministic per-cell seed, density filters and plot exclusions. Candidate XY positions change, so the post-mask instance set may differ. Near/proxy regeneration remains aligned. Native coverage checks pass. The matching `v08-placement-ground/ground.png` capture shows markedly fewer long gaps than `v08-contact-ground/ground.png`, with some bare patches remaining. This is visual evidence, not a performance result.

Separately, terrain vertex Dirt no longer includes the fine, thresholded sward-density field. Broad soil variation and local building/deposit patches remain; grass placement still uses its original occupancy field. **The contact-only view retains strips; stratified placement reduces long gaps, while some bare patches and soil banding remain.** Native contact checks prove sampled geometric support, not that the visual artifact is resolved.

## Plots and canopy assets

Vegetation clears **ReservedFootprint**, the largest future upgrade plot. Grading, foundations and dirt use the actual **Footprint**, preserving relief in the expansion yard. Core levels use 240/480/720 logical-unit body half-widths and reserve 720 at every level. Clearing the reservation does not flatten it.

The imported [open-canopy sprays](Canopies/README.md) retain source wood, licensed photographic UVs, previous bounds and ground pivots. Broadleaf triangles are 22,822 versus 23,422 previously; conifer triangles are 17,420 versus 18,000. They are present in the final Shipping measurements; canopy handoffs and distant silhouettes remain polish limitations. Lower triangle counts alone do not prove improved frame time.

## Evidence and limits

`Saved/Automation/v08-final-rules-terrain/index.json` records **23 clean passes in 3.013 s**; `v08-final-placement/index.json` records **17 clean passes in 2.884 s**. Both have zero failures, warnings and unrun tests. The [pattern analysis](forest_pattern_analysis.json) records its hash and diagnostic captures. The earlier 117-stage Shipping gameplay checkpoint passed before these latest rendering changes. The final package separately passed boot/offline and all four display states. Exact-byte copies of its [static](benchmark-shipping-native.json) and [orbit](benchmark-shipping-orbit-native.json) reports record **30.5–48.9 mean FPS** across views at **3840×1600, Medium, 100% rendering and 10× simulation**; this is GPU-bound and does not sustain 60 FPS. See the [v0.8 graphics report](../../docs/game-design/GRAPHICS_PERFORMANCE_0_8.md) for p95 frame times, hardware, method and limitations.

Existing source licenses remain unchanged: ambientCG Grass004 and retained Poly Haven meadow, forest, dirt and rock maps. The [v0.7 record](../EnvironmentV07/README.md) describes original grass and source-derived tree geometry. This pass changes material graphs and placement fitting; it does not copy Manor Lords assets or claim visual parity.

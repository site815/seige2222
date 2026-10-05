# Ground surfaces and transport integration — v0.8

**Final local checks are complete with known visual and performance limits.** The retained forest-fade shader imported with zero errors/warnings. Targeted native runs have **23 clean terrain/combat passes** and **17 clean placement passes**; these overlap. Stratified placement substantially reduces long exposed grass gaps, while some bare patches and residual brown soil bands remain. The final package passed boot/offline and all four display states and completed isolated static/orbit benchmarks.

The [completed gameplay checkpoint](../verification/v0.8.0.json) passed **117 Shipping stages**, on an executable preceding the final rendering and configurable-validation changes. The final package's affected native, boot, display and performance checks are separate evidence. Version 0.8 is the locally verified prototype; [v0.7](GRAPHICS_PERFORMANCE_0_7.md) remains retained historical evidence.

## Final Shipping performance

The [static report](../../Art/EnvironmentV08/benchmark-shipping-native.json) and [orbit report](../../Art/EnvironmentV08/benchmark-shipping-orbit-native.json) measure the final executable `784B4CFA05A8C980C3F7A9BE37DEBFCF51B590A6BFFB3DAE2D7BA655073CD877`. They ran sequentially in isolation on a Ryzen 7 9800X3D / RTX 4070 Ti SUPER / 32 GB Windows system, using Unreal 5.8.3, **3840×1600, Medium, 100% rendering and 10× simulation**. No diagnostic grass hiding or simplified terrain was enabled.

| View | Static mean FPS | Static p95 frame, ms | Orbit mean FPS | Orbit p95 frame, ms |
| --- | ---: | ---: | ---: | ---: |
| Colony | 48.11 | 22.28 | 48.87 | 23.59 |
| Meadow | 32.66 | 33.32 | 42.66 | 36.31 |
| Ground | 30.53 | 35.85 | 32.79 | 38.43 |
| Hills | 38.13 | 28.03 | 36.53 | 30.90 |
| Boundary | 44.19 | 23.95 | 41.32 | 27.00 |

**These samples do not sustain 60 FPS.** Static GPU means are 20.3–32.2 ms; game-thread means are 0.58–0.64 ms except boundary at 1.86 ms. Orbit GPU means are 19.9–30.0 ms, with game-thread means up to 4.07 ms. The worst sampled wall p95 is **38.43 ms**, with a **39.83 ms** maximum frame. These lagged CPU/GPU counters identify the likely bottleneck; they are not synchronized frame traces.

Each view uses a freshly deployed core and eight empty neighbors, waits for initial scenery, settles for at least four seconds and samples at least five seconds. This is not an endgame stress test. Orbit turns at **72°/s** with **8° pitch amplitude**, retaining streaming work in its samples. Static pending-cell counts were zero; orbit reached 135 pending cells at boundary. Setup and screenshot costs are excluded. Mean FPS and p95 wall-frame time describe different aspects of each short sample; neither guarantees long-session responsiveness.

## Current shader and cost contract

The [asset record](../../Art/EnvironmentV08/README.md), [palette](../../Art/EnvironmentV08/surface_palette.json), [importer](../../Tools/import_terrain_v08.py), and [import report](../../Art/EnvironmentV08/surface_import_report.json) describe the current implementation. Existing photographs are normalized by measured linear source colors, retain bounded local variation, and share one **22 m photographic macro sample**. Far grass lighting normals follow each terrain-aligned instance. No source-photo pixels were changed and no new assets were downloaded.

Ordinary layers, including Dirt, retain two photographic color scales. Forest blends from `forest_leaves_02` to one rotated `leafy_grass` lookup over **40–120 m**, using matching explicit gradients and the same Forest palette. Original normal/roughness detail fades across the transition.

| Active layer path | Maximum samples |
| --- | ---: |
| Ordinary near / far | 4 / 2 |
| Forest near/blending / far | 4 / 1 |
| Shared 22 m macro | +1 per terrain pixel |

More than one active layer adds cost; graph counts are not measured frame times. This retained shader is imported; [the analysis record](../../Art/EnvironmentV08/forest_pattern_analysis.json) records the clean import and evidence hashes. The pass adds no opacity dither, flat-color replacement, emissive fill or new masks.

[Directional measurements](../../Art/EnvironmentV08/forest_pattern_analysis.json) describe the retained photographs, but do **not** establish the remaining artifact's cause. Terrain vertex Dirt no longer includes the small sward-derived contribution; broad soil variation and local plot/deposit patches remain. This change still leaves residual visible bands. The fine occupancy field continues to govern grass placement.

## Grass root contact and placement

The [contact helper](../../Source/Seige/SeigeSceneryContact.h) evaluates a 3-by-3 grid over the authored root plane against the same cached triangles used by terrain rendering. Each clump receives only the measured upward penetration correction. XY, rotation, scale, density and terrain heights do not change; proxies are rebuilt from the corrected source transform. Planar surfaces do not receive a blanket lift.

The latest placement helper changes independent uniform candidates to lightly jittered, staggered strata while retaining the exact requested count, deterministic cell seeds, size ranges, occupancy rules and exclusions. XY candidates change; accepted post-mask instances need not be identical. Near/proxy regeneration uses the same candidates. `Seige.Camera.SwardPlacementCoverage` checks deterministic correspondence, bounds and measured spatial coverage. The matching `v08-placement-ground/ground.png` view shows markedly fewer long exposed gaps than `v08-contact-ground/ground.png`, while some bare patches remain. This is a limited visual improvement, not full artifact removal.

`Seige.Camera.SwardRootContact` checks actual shipped sward bounds, deterministic fitting, unchanged placement/terrain, and support across triangle joins. These native checks pass. The contact-only view still showed pale strips; later stratified placement reduced those long gaps. Sampled contact is therefore a limited geometric correction rather than a verified explanation or complete repair. Nanite-quality and material-isolation diagnostics have not established removal of the strips; the remaining patches are a known prototype limitation.

## Building plots and roads

**Footprint** is the built body's square half-width; **ReservedFootprint** protects its largest future upgrade plot. Vegetation clears the full reservation. Grading, foundations and dirt use the built footprint with edge feathering, leaving relief in the expansion yard. Core levels use 240/480/720 logical-unit body half-widths and reserve 720 throughout. Placement and routing respect the reservation.

Transport corridors use externally configured physical width. Narrow road grading follows existing hills with short longitudinal smoothing; foundations retain priority at entrances. The cached triangle surface supplies roads, picking and scenery height, with common coarse-grid authority on sector seams.

Placing or widening roads removes overlapping resident foliage and pending grass using transformed geometry bounds. Nearby nonoverlapping vegetation retains deterministic placement. Neighbor routes can grade or clear only when their complete sampled path is revealed; visible endpoints do not expose a hidden middle. Observer mode can display known neighbor transport.

## Verification and remaining limits

`Saved/Automation/v08-final-rules-terrain/index.json` records **23 clean tests in 3.013 s**; the later `v08-final-placement/index.json` records **17 clean tests in 2.884 s**. Both have zero failures, warnings and unrun tests. They overlap; exact hashes are in the [pattern analysis](../../Art/EnvironmentV08/forest_pattern_analysis.json). This covers native terrain/combat behavior; import success and native geometry assertions do not establish rendered quality or performance.

The soil captures at `Saved/Screenshots/Benchmark/v08-soil-final/colony.png` and `v08-soil-antialias/colony.png` retain residual-band evidence. The matched `v08-contact-ground/ground.png` and `v08-placement-ground/ground.png` views record the limited grass improvement. Earlier colony/HUD previews established layout at 1600×900 but are not matched performance benchmarks; concurrent work makes their displayed FPS unsuitable for acceptance.

The final executable passed native borderless 3840×1600, windowed 1280×720, 75% rendering and restored borderless display states. Its short boot test reached ready and exited 0, with eight process-tree observations showing zero TCP/UDP endpoints. Exact scope is in the [verification record](../verification/v0.8.0.json).

Known limits include residual soil bands and grass patches, masked near-foliage cost, distant tree silhouettes and animation contact. Final static/orbit timings above measure this version; loaded-colony and long-session performance remain unproven. **No Manor Lords parity or sustained frame-rate claim is made.**

## Imported canopy variants

The [canopy record](../../Art/EnvironmentV08/Canopies/README.md) describes open folded foliage sprays replacing closed distance shells while retaining wood, photographic UVs, bounds and ground pivots. Broadleaf triangles decrease from 23,422 to 22,822; conifer triangles decrease from 18,000 to 17,420. Leaves remain opaque and two-sided.

These variants are imported and present in the final Shipping measurements. Canopy handoffs and distant silhouettes remain visual limitations; there is no isolated before/after canopy GPU comparison. Added edges and two-sided shading can alter cost independently of triangle count; prior assets remain available for comparison.

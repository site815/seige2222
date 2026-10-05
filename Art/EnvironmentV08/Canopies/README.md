# Open canopy distance candidates — v0.8

Status: generated and inspected in Blender on 2026-10-05. Unreal import, actual near/far handoff, and matched GPU measurements are pending. The existing v0.7 meshes, materials and runtime configuration are unchanged. These candidates address the visibly smooth, closed green crowns in the v0.8 hills benchmark; they do not establish reference-game quality or equal frame time.

| Candidate | Triangles | Previous proxy | Dimensions, cm |
| --- | ---: | ---: | --- |
| `SM_BroadleafSpraysV08` | 22,822 | 23,422 | 2441.70 × 1915.46 × 1946.89 |
| `SM_ConiferSpraysV08` | 17,420 | 18,000 | 658.33 × 646.70 × 1895.46 |

The earlier canopy used a smoothed, closed occupancy shell. These replacements sample occupied cells from the actual authored foliage distribution and place open, pointed, folded foliage sprays there. The sprays represent small groups of leaves at distance. Their gaps, edges, and differing surface directions provide geometric silhouette and lighting variation. The reduced source trunks and branches retain their photographic UVs. Bounds and ground pivots exactly match the previous proxies for the existing placement and size-matching system.

The new importer writes only `/Game/Art/NatureV08`. Leaf surfaces are opaque and two-sided, with no opacity mask, dither or wind deformation. They reuse the existing licensed leaf color/normal maps for bounded detail, add bounded per-instance brightness, and retain explicit mean albedo in case a rendering path omits vertex colors. This avoids the old closed-surface silhouette, but overlapping sprays and two-sided foliage shading may have different GPU cost despite the similar source triangle count. Both distant shape stability and the near/far transition require in-game review.

Reproduce with Blender `--background --python Tools/prepare_canopies_v08.py`. Editable scenes are in [Source](Source), FBX exports in [Exports](Exports), and the measured geometry/hash record is [canopy_manifest.json](canopy_manifest.json). The preparation script validates the triangle budget. [import_canopies_v08.py](../../../Tools/import_canopies_v08.py) imports, validates pivot/dimensions/material slots and actual imported vertex colors, and records `canopy_import_report.json` only on success. The color/normal material links are checked individually. Changing runtime asset paths is a separate integration decision.

The individual [broadleaf](Previews/SM_BroadleafSpraysV08.png) and [conifer](Previews/SM_ConiferSpraysV08.png) images are Blender geometry previews, not game captures or performance evidence. They do not include the final Unreal photographic surface graph.

[Matched old distance shapes](Previews/distance_shape_v07.png) and [new distance shapes](Previews/distance_shape_v08.png) use the same tree placements, camera, light and plain mean albedo to isolate the silhouette change. Reproduce them with `Tools/preview_canopies_v08.py`. The new sprays are more porous; whether this coverage is appropriate across the runtime handoff remains an explicit review item.

The tree derivatives retain **CC0-1.0** provenance from the existing source registers: [Jacaranda Tree](https://polyhaven.com/a/jacaranda_tree), Rico Cilliers with Rob Tuytel credited for guidance; [Fir Tree 01](https://polyhaven.com/a/fir_tree_01), modeling by Rico Cilliers and photography by Rob Tuytel. The original downloads and license evidence remain in the [v0.4 register](../../EnvironmentV04/ATTRIBUTION.md) and [nature register](../../Nature/ATTRIBUTION.md). No reference-game assets are used.

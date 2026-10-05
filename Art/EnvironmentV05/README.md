# Graphics investigation records

The matched v0.5 Shipping runs are retained in [benchmarks/baseline.json](benchmarks/baseline.json) and [benchmarks/optimized.json](benchmarks/optimized.json). Both use Epic quality, native 1600×900 output, and identical five-view scenarios; the Nanite geometry target changes from 1 to 1.5 pixels per edge. See the [performance analysis](../../docs/game-design/GRAPHICS_PERFORMANCE_0_5.md) for interpretation and reproduction.

## Rejected grass alpha-trim experiment

This CPU-only candidate was **not imported into Unreal**. It did not change the active grass meshes, textures, materials, density, or runtime settings. No FPS or visual-quality improvement is claimed for alpha trimming.

The experiment conservatively clipped transparent UV margins from source templates before assembling the same 16 tall tufts and 920 low Bermuda tufts. It retained source nonzero alpha with an eight-texel filter margin, interpolated UVs and normals, and protected horizontal hull/ground extrema. Both assembled patches retained their exact recorded dimensions within 0.002 cm. The original licensed sources were unchanged.

The initial rectangle-based atlas query saved 3.20% of Bermuda card surface area and increased each sward from 77,572 to 79,320 triangles. A refined query excluding unrelated atlas islands outside each triangle's filter footprint saved **5.67% of Bermuda card surface area**, but increased each complete sward to **79,924 triangles (+3.03%)**. Tall-source geometry was unchanged; no whole source triangles were safely removable with the conservative margin. Card surface area is not screen-space overdraw or measured GPU time.

These small area savings do not justify importing a more complex candidate. Source UV sampling also shows that the atlas's large empty background cannot be treated as unused mesh area: the source geometry selects individual leaf islands. Less conservative trimming would require a separate cooked-mip/silhouette comparison and is not approved by this experiment.

[alpha_trim_report.json](alpha_trim_report.json) records the final geometry measurements. [source_uv_coverage.json](source_uv_coverage.json) records a seven-point-per-triangle diagnostic, which is not a proof of complete triangle opacity. Unused generated candidate FBXs, source blend, and export manifest were removed after review.

To reproduce in a separate candidate folder, run the bundled Blender with `--background --python Tools/prepare_meadow_swards.py -- --alpha-trim-candidate`. The optional path uses [grass_alpha_trim.py](../../Tools/grass_alpha_trim.py) and writes only this folder; ordinary meadow regeneration keeps its existing destination and behavior. [inspect_grass_alpha_coverage.py](../../Tools/inspect_grass_alpha_coverage.py) performs the read-only UV diagnostic. Neither script launches Unreal or alters runtime assets.

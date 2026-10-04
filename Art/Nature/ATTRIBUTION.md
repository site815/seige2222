# Licensed woodland assets

These assets come from Poly Haven and are covered by **CC0 1.0**. Poly Haven explicitly permits commercial use and redistribution of the asset files, including in a sold product. Attribution is voluntary; the creators are credited here for provenance. License checked on 4 October 2026 against the [official asset license](https://polyhaven.com/license) and [CC0 legal code](https://creativecommons.org/publicdomain/zero/1.0/).

| Source | Creators credited by Poly Haven | Prepared meshes |
| --- | --- | --- |
| [Fir Tree 01](https://polyhaven.com/a/fir_tree_01) | Rob Tuytel — photography; Rico Cilliers — modeling | `SM_FirA`, `SM_FirB`, `SM_FirC` |
| [Tree Small 02](https://polyhaven.com/a/tree_small_02) | Rico Cilliers | `SM_BroadleafA` |
| [Fern 02](https://polyhaven.com/a/fern_02) | Rob Tuytel — scanning; Rico Cilliers — modeling | `SM_FernA`, `SM_FernB` |
| [Rock Moss Set 01](https://polyhaven.com/a/rock_moss_set_01) | Kless Gyzen | `SM_MossRockA`, `SM_MossRockB` |

The actual model and texture files were downloaded through Poly Haven's public API. Website example renders, branding, and promotional content are not included. The review images in this directory were rendered locally from the prepared meshes.

## Files and reproducibility

- `sources.json` records each exact download URL, byte count, official MD5 and independently calculated SHA-256, plus source pages and authors.
- `Textures/` contains the original downloaded color, OpenGL normal, roughness and alpha maps. Color, normal and alpha maps use 2K resolution; roughness uses 1K.
- `Source/` contains compact, editable Blender derivatives and a linked review scene. Required image files are referenced relative to this directory. The much larger, unmodified original blends are cached in ignored `.tools/nature-downloads/` and can be downloaded again from the recorded URLs.
- `Exports/nature_manifest.json` records source mesh names, material groups, dimensions, triangle counts and the corresponding `/Game/Art/Nature/` paths.
- `import_report.json` is produced only after successful Unreal import and verification.
- `prepared_hashes.json` records hashes for the prepared source, exports and local review images.

Run `Tools/download_natural_assets.py` with Python, then `Tools/prepare_natural_assets.py` with Blender, then `Tools/import_natural_assets.py` in Unreal's editor Python environment. Downloads are a development step. The packaged game uses local cooked assets and needs no Poly Haven connection.

## Changes from the originals

FirA, FirB and the broadleaf tree use the authors' LOD1 geometry. FirC uses the fuller authored LOD0 (505,494 triangles), promoted after runtime reviews found that its LOD1 canopy was too thin at gameplay distance. Source leaves and branches are retained; no random leaf sampling or generic destructive tree LOD reduction is applied. Models are converted from metres to centimetres and centred horizontally with the ground at Z=0. Fir vector UV attributes are converted to ordinary UV0; branch UV scales are baked. The original fir trunk's painted transition to box-projected bark is converted into a face material selection with baked UVs. Vertex color data is then removed so foliage shading cannot vary through missing vertex colors.

Materials are rebuilt from photographic color, normal, roughness and alpha maps. Unreal converts OpenGL normal maps by reversing the green channel. Bark and rocks use opaque materials; leaves and ferns use masked, two-sided foliage shading with modest subsurface transmission. Alpha mipmaps preserve coverage at the material's 0.33 mask threshold. Runtime distance tests motivated a foliage-only sample mip bias of -2 for alpha and -1 for color; normal and roughness samples retain default mip selection. This does not impose a global quality override. `Tools/patch_nature_foliage_mips.py` applies the same settings to existing materials without reimporting meshes or textures. Trees and rocks use Nanite, with PreserveArea on trees. Ferns retain their full, small meshes. No wind deformation is included in this pass.

The fir variants are approximately 19.0, 14.1 and 14.5 metres tall; the broadleaf is approximately 4.6 metres. These are physical centimetre meshes and should be placed at normal scale rather than scaled by the simulation's logical-unit conversion.

# SEIGE — Third-party Asset Attribution

The project's downloaded nature models and photographic surfaces come from Poly Haven. Its asset license was checked again on **2026-10-05**: these assets are **CC0 1.0**, permitting modification, redistribution, and commercial use. Attribution is voluntary and is retained here for provenance. No paid assets are used. The license applies to the asset files; website branding and example renders are not included.

[Poly Haven asset license](https://polyhaven.com/license) · [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)

All models and texture files are local and imported into Unreal content. The game does not download from Poly Haven. The v0.3 additions are imported and included in the verified Windows Shipping package; its rendered interaction check completed twenty-eight stages with zero failures. This verifies integration, not finished visual quality.

## v0.3 licensed nature models

| Source asset | Creator credits | Prepared project meshes |
| --- | --- | --- |
| [Fir Tree 01](https://polyhaven.com/a/fir_tree_01) | Rico Cilliers — modeling; Rob Tuytel — photography | `SM_FirA`, `SM_FirB`, `SM_FirC` |
| [Tree Small 02](https://polyhaven.com/a/tree_small_02) | Rico Cilliers | `SM_BroadleafA` |
| [Fern 02](https://polyhaven.com/a/fern_02) | Rico Cilliers — modeling; Rob Tuytel — scanning | `SM_FernA`, `SM_FernB` |
| [Rock Moss Set 01](https://polyhaven.com/a/rock_moss_set_01) | Kless Gyzen | `SM_MossRockA`, `SM_MossRockB` |

The [nature register](Nature/ATTRIBUTION.md) documents the preparation workflow and changes: selecting authored model detail levels, converting meters to centimeters and ground pivots, preparing UVs, rebuilding materials, and preserving canopy coverage. Source geometry is licensed third-party work; it is not claimed as original project modeling. Trees and rocks use Nanite; masked two-sided leaf/fern materials use local photographic maps. No wind animation is supplied by this pass. The current graphics definition selects seven of these eight prepared models; `SM_FernB` remains available in the imported set.

Exact downloads, source authors, byte counts, official MD5 values, and independently calculated SHA-256 values are recorded in [Nature/sources.json](Nature/sources.json). [Prepared-file hashes](Nature/prepared_hashes.json), the [export manifest](Nature/Exports/nature_manifest.json), and the [Unreal import report](Nature/import_report.json) track the derivatives. Required maps are retained in `Nature/Textures`; compact editable Blender derivatives are in `Nature/Source`. Large unmodified downloads remain in the ignored tool cache and can be recovered from their recorded URLs. Review images were rendered locally, not copied from the provider's website.

## Photographic surface maps

| Surface | Creator credited by Poly Haven | License | Included maps |
| --- | --- | --- | --- |
| [Bark Brown 02](https://polyhaven.com/a/bark_brown_02) | Rob Tuytel | CC0-1.0 | 2K JPEG color, DirectX normal, roughness |
| [Brown Mud Dry](https://polyhaven.com/a/brown_mud_dry) | Rob Tuytel | CC0-1.0 | 2K JPEG color, DirectX normal, roughness |
| [Grass Ground](https://polyhaven.com/a/grass_ground) | Charlotte Baglioni | CC0-1.0 | 2K JPEG color, DirectX normal, roughness |
| [Rock Boulder Dry](https://polyhaven.com/a/rock_boulder_dry) | Dimitrios Savva, Rico Cilliers | CC0-1.0 | 2K JPEG color, DirectX normal, roughness |
| [Leafy Grass](https://polyhaven.com/a/leafy_grass) | Charlotte Baglioni | CC0-1.0 | 2K JPEG color, DirectX normal, roughness, ambient occlusion; added for v0.3 |

## Exact file provenance

[Original four-surface source/SHA-256 manifest](Textures/PolyHaven/sources.json) · [Leafy Grass source/SHA-256 manifest](Textures/PolyHaven/ground_v03_sources.json)

| Local file | Official download |
| --- | --- |
| [ grass_ground_diff_2k.jpg](Textures/PolyHaven/grass_ground_diff_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/grass_ground/grass_ground_diff_2k.jpg) |
| [ grass_ground_nor_dx_2k.jpg](Textures/PolyHaven/grass_ground_nor_dx_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/grass_ground/grass_ground_nor_dx_2k.jpg) |
| [ grass_ground_rough_2k.jpg](Textures/PolyHaven/grass_ground_rough_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/grass_ground/grass_ground_rough_2k.jpg) |
| [ brown_mud_dry_diff_2k.jpg](Textures/PolyHaven/brown_mud_dry_diff_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/brown_mud_dry/brown_mud_dry_diff_2k.jpg) |
| [ brown_mud_dry_nor_dx_2k.jpg](Textures/PolyHaven/brown_mud_dry_nor_dx_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/brown_mud_dry/brown_mud_dry_nor_dx_2k.jpg) |
| [ brown_mud_dry_rough_2k.jpg](Textures/PolyHaven/brown_mud_dry_rough_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/brown_mud_dry/brown_mud_dry_rough_2k.jpg) |
| [ rock_boulder_dry_diff_2k.jpg](Textures/PolyHaven/rock_boulder_dry_diff_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/rock_boulder_dry/rock_boulder_dry_diff_2k.jpg) |
| [ rock_boulder_dry_nor_dx_2k.jpg](Textures/PolyHaven/rock_boulder_dry_nor_dx_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/rock_boulder_dry/rock_boulder_dry_nor_dx_2k.jpg) |
| [ rock_boulder_dry_rough_2k.jpg](Textures/PolyHaven/rock_boulder_dry_rough_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/rock_boulder_dry/rock_boulder_dry_rough_2k.jpg) |
| [ bark_brown_02_diff_2k.jpg](Textures/PolyHaven/bark_brown_02_diff_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/bark_brown_02/bark_brown_02_diff_2k.jpg) |
| [ bark_brown_02_nor_dx_2k.jpg](Textures/PolyHaven/bark_brown_02_nor_dx_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/bark_brown_02/bark_brown_02_nor_dx_2k.jpg) |
| [ bark_brown_02_rough_2k.jpg](Textures/PolyHaven/bark_brown_02_rough_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/bark_brown_02/bark_brown_02_rough_2k.jpg) |
| [leafy_grass_diff_2k.jpg](Textures/PolyHaven/leafy_grass_diff_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/leafy_grass/leafy_grass_diff_2k.jpg) |
| [leafy_grass_nor_dx_2k.jpg](Textures/PolyHaven/leafy_grass_nor_dx_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/leafy_grass/leafy_grass_nor_dx_2k.jpg) |
| [leafy_grass_rough_2k.jpg](Textures/PolyHaven/leafy_grass_rough_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/leafy_grass/leafy_grass_rough_2k.jpg) |
| [leafy_grass_ao_2k.jpg](Textures/PolyHaven/leafy_grass_ao_2k.jpg) | [Download source](https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/leafy_grass/leafy_grass_ao_2k.jpg) |

## Original project assets and earlier versions

The **v0.3 industrial architecture and its nine procedural PBR maps are original project work**, authored with [create_industry_assets.py](../Tools/create_industry_assets.py). The [Blender source](Source/Seige_Industry_Architecture.blend), [export manifest](IndustryExports/industry_manifest.json), and [import report](industry_import_report.json) distinguish those assets from licensed nature models. Original robot, alien, and grass-blade geometry also remain project-authored. These original works are not automatically assigned Poly Haven's CC0 license; project licensing decisions apply to them.

The earlier v0.2 trees, leaves, shrubs, and rocks were original procedural meshes, as recorded in [REALISTIC_ASSET_REGISTER.md](REALISTIC_ASSET_REGISTER.md). They remain available as historical source but the v0.3 nature roles now use the licensed models above, except for the original grass-blade mesh. Historical claims that all environment geometry was original do not apply to this revision.

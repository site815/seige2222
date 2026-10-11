# Rex texture provenance

## Current prototype exception - 2026-10-11

The owner explicitly instructed that the earlier detailed model be used for now and the license issue noted in the credits. Revision 7 is restored from local commit `5aed6e9` and its retained exports; no new hosted generation or reference-photo upload is performed. This records an unresolved prototype exception, **not** a declaration that attribution resolves the license or that these assets have unrestricted commercial permission.

The official [Hunyuan3D-2.1 license](https://huggingface.co/tencent/Hunyuan3D-2.1/raw/main/LICENSE) was checked on 2026-10-11. Section 1(g) distinguishes outputs from model derivatives and section 6(d) disclaims Tencent ownership of outputs, but section 5(c) separately and expressly restricts use, modification, distribution and display of outputs outside its territory. Section 1(l) excludes the EU, United Kingdom and South Korea. Therefore the territory issue applies to this generated base mesh, not only to redistributed model weights. No separate Tencent authorization has been established.

## `Textures/T_Dog_Albedo.png` (revision 6+, 2026-10-08)

Baked in Blender Cycles by `Tools/build_rex_from_generated.py` from two sources: an orthographic projection of the owner's standing photograph of Rex (background removed locally with `rembg`; the photograph and its matte stay untracked in `Saved/rex_photos/`) and authored colour zoning (gold back, cream chest, legs and belly, pale muzzle and brow). The projection is weighted by how directly each surface faces the photographed side; the far side is mirrored; the tail and dark off-face samples fall back to the zoning. The map therefore contains pixels derived from the owner's photograph and was restored at the owner's explicit request for this prototype; it is not represented as a CC0 source.

## `Source/generated/hy21shape_generation_0.glb`

Shape-only output of Tencent Hunyuan3D-2.1 (`tencent/Hunyuan3D-2.1` Hugging Face Space, `/shape_generation`, seed 1234, octree resolution 320) from the same photograph, requested by `Tools/generate_rex_image_to_3d.py`; no texture was requested from the service. The generated base remains subject to the unresolved territory restriction described above. Attribution alone does not resolve it. The retained pipeline is offline and uses the already available GLB.

## `Textures/T_Dog_CoatDetail.png` (2026-10-05)

Generated using the built-in OpenAI imagegen tool in text-to-image mode. No reference images were supplied to that generation. Retained for reference; revision 6+ materials no longer sample it.

Exact generation prompt:

> Create a production-ready seamless tileable PBR diffuse COLOR texture for the short dense fur of an adult golden retriever. This is a flat 2D texture swatch for wrapping an existing 3D dog, NOT a picture of a whole dog. Fill the entire square image edge to edge with extremely fine overlapping cream/off-white fur fibers, combed predominantly vertically top to bottom with slight natural wavy flow and tiny groups of strands. Close macro detail but each strand extremely fine; dense silky realistic coat as on a retriever muzzle and torso, no skin showing. Very even neutral diffuse illumination, NO cast shadows, NO ambient vignette, NO directional highlights, NO depth of field, NO borders, NO text. Restrained low-contrast ivory to neutral light gray palette, with mid-light mean value: preserve fine visible fiber detail without black gaps. Equal brightness at all edges; seamless on both axes. The final texture will be multiplied by separately authored gold/cream body colors in a game shader; do not bake anatomy, eyes, collar or objects.

## `FurNormal`, `FurDetail`, `FurAtlas`, `FurAlpha`

Generated numerically by the Blender build scripts (streak height field, its normal map, a 4Ã-2 lock atlas with alpha, and the legacy alpha mask). No photographs are involved.

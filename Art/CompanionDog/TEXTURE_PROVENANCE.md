# Rex texture provenance

## Current prototype exception - 2026-10-11

The owner explicitly instructed that the earlier detailed model be used for now and the license issue noted in the credits. The detailed revision-7 base was restored from local commit `5aed6e9`; revision 8 repairs that retained source locally; no new hosted generation or reference-photo upload is performed. This records an unresolved prototype exception, **not** a declaration that attribution resolves the license or that these assets have unrestricted commercial permission.

The official [Hunyuan3D-2.1 license](https://huggingface.co/tencent/Hunyuan3D-2.1/raw/main/LICENSE) was checked on 2026-10-11. Section 1(g) distinguishes outputs from model derivatives and section 6(d) disclaims Tencent ownership of outputs, but section 5(c) separately and expressly restricts use, modification, distribution and display of outputs outside its territory. Section 1(l) excludes the EU, United Kingdom and South Korea. Therefore the territory issue applies to this generated base mesh, not only to redistributed model weights. No separate Tencent authorization has been established.

## Current revision 8 photographic repair (2026-10-11)

The owner supplied five photographs and requested Rex's likeness. All five were inspected. The current offline bake reuses the valid original face/photo coat and adds cropped photographic detail from the frontal standing photograph (`KakaoTalk_20241105_104908617_06.jpg`), the close face/nose photograph (`KakaoTalk_20241105_104643116_04.jpg`), the lying photograph (`KakaoTalk_20241105_104908617.jpg`), the outdoor front-side photograph (`KakaoTalk_20241105_104643116_02.jpg`), and the outdoor standing photograph (`KakaoTalk_20241105_104643116_05.jpg`). No new upload or hosted generation was performed.

`Source/PhotoPatches/FrontFacePhoto.png` contains only a cropped face area, used for forehead and mouth alignment. `NosePhoto.png` is a lighting-normalized nose crop. `tile-body`, `tile-leg`, `tile-chest` and `tile-paw` retain fine hair variation after broad lighting/color normalization. The body and leg tiles come from the outdoor standing image, the chest tile from the outdoor front-side image, and the retained paw study from the lying image. The current bake blends body, leg and chest tiles; the paw tile remains an input study and is not currently sampled. Full original photographs are not committed.

`Textures/T_Dog_Albedo.png` is a new 4096-square sRGB atlas produced by `Tools/bake_rex_photos.py`: selected-to-active emission ray transfer from the mirrored sound side of the retained donor, local frontal-photo blending, and normalized photographic fur on repaired limbs/body. `Textures/T_Dog_Nose.png` is a 512-square photographic crop on the new closed nose/nostrils. These maps contain owner-photo pixels and are not claimed to be CC0.

`Source/RepairDonor.blend` and `Source/PhotoPatches/ReferenceCoat.png` preserve the old geometry/UV/albedo pair for rebuilding. The reference coat is the historical 2048-square single-view photo projection plus authored color zoning from `Tools/build_rex_from_generated.py`. It must not be substituted with the repaired atlas because their UVs differ.


The final coat polish uses retained `raw-body.png`, `raw-leg.png` and `raw-chest.png` crops from those same outdoor references. `Tools/prepare_rex_coat_tiles.py` removes broad lighting gradients while retaining clump-scale hair contrast, then prepares `coat-body`, `coat-leg` and `coat-chest`. Unlike the earlier low-contrast tiles, these preserve visible variation at game-camera mip levels. `Tools/polish_rex_coat.py` blends two differently scaled, non-mirrored samples, aligns leg/chest flow downward, and bakes both albedo and `T_Dog_CoatNormal.png` on the unchanged UV atlas. The normal is derived from photographic luminance; no new external image generation was used. The original facial projection and anatomy/weights are retained and checked by a geometry hash.

## `Source/generated/hy21shape_generation_0.glb`

Shape-only output of Tencent Hunyuan3D-2.1 (`tencent/Hunyuan3D-2.1` Hugging Face Space, `/shape_generation`, seed 1234, octree resolution 320) from the same photograph, requested by `Tools/generate_rex_image_to_3d.py`; no texture was requested from the service. The generated base remains subject to the unresolved territory restriction described above. Attribution alone does not resolve it. The retained pipeline is offline and uses the already available GLB.

## `Textures/T_Dog_CoatDetail.png` (2026-10-05)

Generated using the built-in OpenAI imagegen tool in text-to-image mode. No reference images were supplied to that generation. Retained for reference; revision 6+ materials no longer sample it.

Exact generation prompt:

> Create a production-ready seamless tileable PBR diffuse COLOR texture for the short dense fur of an adult golden retriever. This is a flat 2D texture swatch for wrapping an existing 3D dog, NOT a picture of a whole dog. Fill the entire square image edge to edge with extremely fine overlapping cream/off-white fur fibers, combed predominantly vertically top to bottom with slight natural wavy flow and tiny groups of strands. Close macro detail but each strand extremely fine; dense silky realistic coat as on a retriever muzzle and torso, no skin showing. Very even neutral diffuse illumination, NO cast shadows, NO ambient vignette, NO directional highlights, NO depth of field, NO borders, NO text. Restrained low-contrast ivory to neutral light gray palette, with mid-light mean value: preserve fine visible fiber detail without black gaps. Equal brightness at all edges; seamless on both axes. The final texture will be multiplied by separately authored gold/cream body colors in a game shader; do not bake anatomy, eyes, collar or objects.

## `FurNormal`, `FurDetail`, `FurAtlas`, `FurAlpha`

Generated numerically by the Blender build scripts (streak height field, its normal map, a 4Ã-2 lock atlas with alpha, and the legacy alpha mask). No photographs are involved.

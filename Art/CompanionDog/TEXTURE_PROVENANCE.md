# Rex coat texture provenance

`Textures/T_Dog_CoatDetail.png` was generated on 2026-10-05 using the built-in OpenAI imagegen tool in text-to-image mode. No reference images were supplied to that generation. The private Rex photos guided the authored geometry and gold/cream vertex markings, and were not copied or sampled into this texture.

The swatch is multiplied by separate coat vertex colors and also supplies small surface relief. It contains no baked dog anatomy, eyes, collar or identifying photograph. Blender previews are renders of the authored mesh using this swatch, not generated depictions of the completed dog.

Exact generation prompt:

> Create a production-ready seamless tileable PBR diffuse COLOR texture for the short dense fur of an adult golden retriever. This is a flat 2D texture swatch for wrapping an existing 3D dog, NOT a picture of a whole dog. Fill the entire square image edge to edge with extremely fine overlapping cream/off-white fur fibers, combed predominantly vertically top to bottom with slight natural wavy flow and tiny groups of strands. Close macro detail but each strand extremely fine; dense silky realistic coat as on a retriever muzzle and torso, no skin showing. Very even neutral diffuse illumination, NO cast shadows, NO ambient vignette, NO directional highlights, NO depth of field, NO borders, NO text. Restrained low-contrast ivory to neutral light gray palette, with mid-light mean value: preserve fine visible fiber detail without black gaps. Equal brightness at all edges; seamless on both axes. The final texture will be multiplied by separately authored gold/cream body colors in a game shader; do not bake anatomy, eyes, collar or objects.

The other retained `FurNormal`, `FurDetail` and `FurAlpha` maps are generated numerically by the Blender source generator. The final mesh does not use the old long masked fur-card geometry.

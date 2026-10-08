# SEIGE — Original Prototype Asset Register

The v0.8 Rex companion is an original authored interpretation guided by the user's private photographs: Blender mesh, rig, animation and procedural surface work, plus an original generated coat-detail bitmap. No photograph pixels are copied into the coat map. The photographs are not repository assets and must not be embedded or redistributed. See the [Rex asset record](CompanionDog/README.md) for provenance and the [companion design](../docs/game-design/COMPANIONS_AND_REX.md) for behavior and pending likeness/runtime acceptance. These original assets follow the project's licensing decisions; the private references are not assigned a public license.

The refined Rex revision-4 source has **118,331 triangles and 24 bones**, matching the current [source validation](CompanionDog/source_validation.json) and import report. Current FBX round-trip checks pass, and [Unreal reimport](CompanionDog/import_report.json) verifies three skeletal LODs, both animation clips and ten material slots including the brown iris material. **Runtime appearance and animation review remain pending**.

The v0.5 original shuttle, automated robot-service facility, and construction materials are documented in [Construction/README.md](Construction/README.md).

The v0.4 background assets are documented separately in [EnvironmentV04/ATTRIBUTION.md](EnvironmentV04/ATTRIBUTION.md): a licensed full-size mature tree, photographic grass clump, and meadow/forest PBR material. This original prototype register remains historical provenance.

**Current environment update:** The six building meshes below are superseded in the game by the detailed industrial set recorded in [REALISTIC_ASSET_REGISTER.md](REALISTIC_ASSET_REGISTER.md). That set also adds eight temperate vegetation/rock meshes and locally packaged CC0 photographic surfaces. The original prototype source remains for provenance. The current v0.9 upright shuttle, surrounding core campus and individually represented hover worker replace their earlier counterparts; see [OrbitalV09](OrbitalV09/README.md). The original bug mesh remains in use. See [third-party texture attribution](THIRD_PARTY_ASSETS.md) for the new maps.

These eight meshes and their flat-color materials were created for this project with the reproducible Blender script in [create_assets.py](../Tools/create_assets.py). They are original procedural designs, assembled from modeled primitives and authored proportions. No downloaded character, building, texture, or commercial game asset is included. The robot uses its own box-pebble body, binocular dot face, mitten arms, and coral backpack; it does not reproduce a referenced character.

## Provenance and licensing

- **Asset source:** Original project work generated with the included script; no third-party asset license dependencies identified.
- **Project licensing:** These files follow the project's licensing decisions. This register does not assign a separate public license or import an external model license.
- **Creation tool:** Official Blender 4.5.14 LTS, obtained from [Blender's official release directory](https://download.blender.org/release/Blender4.5/). The portable tool is kept separately under ignored `.tools`; it is not game content.
- **Editable source:** [Seige_Original_Assets.blend](Source/Seige_Original_Assets.blend).
- **Export inventory:** [asset_manifest.json](Exports/asset_manifest.json), including geometry bounds, triangle counts, palette values, and material-slot order.
- **Unreal import:** [import_assets.py](../Tools/import_assets.py) restores authored colors through a shared material and per-color instances. The importer records verified Unreal bounds and material assignments in `Art/import_report.json` after a successful run.

## Meshes

Dimensions are centimeters at unit scale. All exported meshes have centered XY pivots at ground Z=0. The authored front is +X. Building meshes are static assemblies; robot and bug are unrigged prototype meshes intended for runtime object movement, not supplied skeletal animation.

| Mesh | Design | Dimensions X × Y × Z | Triangles | Unreal destination |
| --- | --- | --- | ---: | --- |
| SM_Core | Four-pod rounded command hub with beacon and luminous orbital collar | 218 × 210 × 192 | 3,976 | `/Game/Art/SM_Core` |
| SM_Extractor | Twin gantry drill with collection bin | 161 × 137 × 161 | 4,116 | `/Game/Art/SM_Extractor` |
| SM_Factory | Rounded assembly workshop with twin ceramic stacks | 209.5 × 168 × 173 | 3,460 | `/Game/Art/SM_Factory` |
| SM_Depot | Open loading portal with stacked cargo pods | 182 × 161 × 124 | 3,196 | `/Game/Art/SM_Depot` |
| SM_Sensor | Braced mast, directional lens, and signal aerial | 112 × 112 × 199 | 2,612 | `/Game/Art/SM_Sensor` |
| SM_Turret | Compact twin-emitter defensive platform | 143.5 × 124 × 111 | 2,504 | `/Game/Art/SM_Turret` |
| SM_Robot | Small expressive worker with backpack, hands, and feet | 33.75 × 44 × 58 | 2,616 | `/Game/Art/SM_Robot` |
| SM_Bug | Six-legged armored alien with layered chitin and hooked mandibles | 117.79 × 116.92 × 58.96 | 5,456 | `/Game/Art/SM_Bug` |

The alien uses an organic segmented form, dark red armor, pointed legs, and exposed mandibles to contrast with the friendly colony. This is a readable stylized prototype asset; detailed realistic surface work and animation are not included.

## Materials and import behavior

`/Game/Art/M_Colony` exposes `Tint`, `Roughness`, `Metallic`, and `Emission`. Default roughness is 0.65, metallic is 0.16, and emission is zero. The palette uses named `MI_*` instances with authored colors; only lens and signal instances emit light. No image textures are needed.

FBX material slots preserve the separate shell, trim, visor, lens, and alien chitin colors. The importer assigns these slots from the manifest instead of depending on FBX shader translation. It checks mesh sizes and ground pivots, imports normals, and requests generated simple collision. Gameplay can scale meshes as needed while retaining their local pivots.

## Visual review

The generated [contact sheet](Previews/asset_contact_sheet.png) was rendered in Blender and inspected for silhouettes, palette, scale, and grounding. [Robot detail](Previews/robot_detail.png) and [bug detail](Previews/bug_detail.png) are generated by the same script. Preview ground, camera, and lights are retained in the source scene but excluded from FBX exports.

## Regeneration

Run Blender in background mode with `Tools/create_assets.py`, then run `Tools/import_assets.py` using Unreal's Python commandlet once the project editor module is built. Both scripts derive the project directory from their own locations. Generated outputs are confined to the art/export paths and the Unreal art folder; no map or runtime simulation is authored by these scripts.

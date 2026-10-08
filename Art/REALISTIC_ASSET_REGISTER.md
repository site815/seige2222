> **Historical.** The meshes described here were superseded by the industrial set (`Art/IndustryExports`, `docs/game-design/GRAPHICS_MILESTONE_0_3.md`), the orbital v0.9 set (`Art/OrbitalV09`) and the v0.9.2 building kit (`Art/BuildingKitV092/README.md`). Kept for provenance only; nothing here is loaded by the game.

# SEIGE — Industrial and Temperate Environment Assets

The current environment set replaces the six initial simplified buildings with original industrial architecture and adds temperate trees, understory, grass, and rocks. The meshes were authored procedurally in Blender, with no downloaded building or tree models. Photographic terrain and bark maps are separately licensed CC0 assets; exact sources, authors, download URLs, and hashes are recorded in [THIRD_PARTY_ASSETS.md](THIRD_PARTY_ASSETS.md) and its linked source manifest.

## Editable sources and outputs

- [Blender source with packed preview textures](Source/Seige_Realistic_Assets.blend)
- [Reproducible Blender generator](../Tools/create_realistic_assets.py)
- [Reproducible Unreal importer](../Tools/import_realistic_assets.py)
- [Canopy-preserving Blender LOD generator](../Tools/create_foliage_lods.py) and [targeted Unreal LOD importer](../Tools/import_foliage_lods.py)
- [Editable tree LOD source](Source/Seige_Foliage_LODs.blend) and [all reduced canopies review](Previews/foliage_lod_canopies.png)
- [FBX geometry and palette manifest](RealisticExports/realistic_manifest.json)
- [Forest and colony review render](Previews/realistic_forest_colony.png)
- [Industrial architecture detail](Previews/industrial_core_detail.png)

## Mesh contract

All meshes use centimeters, centered XY origins, ground Z=0, and the same `/Game/Art/` destination. Buildings preserve approximately the initial footprints. The tree and understory assets are suitable for the runtime's hierarchical instancing; mesh dimensions are source dimensions before instance scaling.

| Mesh | Source dimensions X × Y × Z, cm | Source triangles | Details |
| --- | --- | ---: | --- |
| SM_Core | 232.5 × 200 × 204 | 10,888 | Concrete footing, armored panel seams and fasteners, service deck, pipework, airlock, HVAC, communications radome and aerial. |
| SM_Extractor | 180 × 160.85 × 179.5 | 11,620 | Braced steel drill rig, hydraulic feed, cutting collars, louvered control cabinets, grated ore hopper. |
| SM_Factory | 228.5 × 184 × 184 | 11,048 | Modular factory hall, loading shutter, canopy, raised roof seams, flanged exhaust stacks, HVAC, exposed process lines. |
| SM_Depot | 212 × 180 × 132 | 8,744 | Steel-column warehouse, trusses, standing-seam roof, ribbed cargo containers, loading platform and bollards. |
| SM_Sensor | 111 × 105 × 251 | 9,168 | Braced lattice tower, bolted footing, service cabinet, phased array elements, mast and navigation light. |
| SM_Turret | 169 × 134 × 118.5 | 4,160 | Segmented pedestal armor, bearing, receiver, cooling collars, muzzle brakes, ventilation, and optical sensors. |
| SM_OakA | 756.58 × 825.42 × 857.69 | 22,104 | Irregular branching structure and individually curved lobed leaves. |
| SM_OakB | 646.96 × 639.70 × 759.92 | 22,104 | Different deterministic branch and foliage distribution. |
| SM_PineA | 519.30 × 525.26 × 990 | 32,692 | Tapered trunk, tiered woody branches, and narrow geometric needle sprays. |
| SM_PineB | 407.04 × 416.01 × 780 | 32,692 | Alternate height and branch distribution. |
| SM_Shrub | 134.75 × 152.35 × 115.42 | 3,000 | Woody multi-stem understory with individual leaves. |
| SM_Grass | 72.38 × 63.58 × 41.97 | 144 | Forty-eight bent, tapered grass blades with varying lengths and vertex colors. |
| SM_RockA | 146.81 × 114.09 × 72.90 | 320 | Irregular weathered boulder with flattened ground contact and UVs. |
| SM_RockB | 190.92 × 132.27 × 98.03 | 320 | Different rock proportions and surface deformation. |

The tree crowns use leaf geometry, not spherical or conical canopy placeholders. Foliage colors are authored per vertex and imported explicitly. Trees are static meshes; skeletal animation and wind deformation are not included in this set. The pine geometry was reduced from an earlier approximately 82,000-triangle version for denser instancing.

Trees have four LODs at screen sizes 1.0, 0.45, 0.18, and 0.06. Generic whole-tree triangle reduction initially removed too much disconnected leaf geometry, leaving bare branches in the runtime. The final three reduced tiers are authored separately: woody geometry is simplified independently, while distributed leaf samples are retained and enlarged to preserve canopy coverage. Oak LOD1/2/3 contain 4,846 / 2,902 / 1,410 triangles; pine LOD1/2/3 contain 9,474 / 5,180 / 2,724. The original LOD0 is retained. Exact leaf counts and generation settings are in [foliage_lods.json](RealisticExports/foliage_lods.json). Shrubs and grass receive three automatically reduced LODs; runtime HISM distance culling complements these reductions. Nanite is not required by this asset set.

For complete regeneration, run `create_realistic_assets.py` in Blender, then `create_foliage_lods.py`, then `import_realistic_assets.py` in Unreal. The main importer automatically applies the authored tree LODs when their manifest is present. To update only tree LODs, run the two dedicated LOD scripts. Run [verify_foliage_lods.py](../Tools/verify_foliage_lods.py) in a fresh Unreal commandlet after import: Interchange can expose temporary zero screen sizes in its in-process render data even when the saved source thresholds are correct. The final [verification report](foliage_lod_import_report.json) confirms the persisted thresholds, nonempty geometry, and material slots for all four trees.

The orthographic runtime also needs an appropriate HISM LOD distance scale: the engine's classic HISM thresholds otherwise select the lowest tree tier even at close screen sizes. Avoid a large negative near plane combined with automatic orthographic origin correction, which can move the effective view origin beyond every instance cull distance.

## Material contract

`/Game/Art/M_Terrain` uses vertex **R for dirt**, **G for rock**, and `1 − clamp(R + G)` for grass. Runtime weights should satisfy R + G ≤ 1. Input UV0 is multiplied by `TextureScale`, default **3.5**; runtime XY/700 UVs therefore repeat each map approximately every 200 cm. The master exposes these texture parameters:

- `GrassColor`, `GrassNormal`, `GrassRoughness`
- `DirtColor`, `DirtNormal`, `DirtRoughness`
- `RockColor`, `RockNormal`, `RockRoughness`

The material blends albedo, tangent-space normals, and roughness, with restrained world-position macro variation to reduce uniform coloration. It also exposes `TerrainTint` and `MacroScale`. Color maps use sRGB. Normal maps use DirectX convention, normal compression, and no green-channel flip. Roughness is linear.

`M_Industrial` and its palette instances provide painted metal, steel, concrete, glass-like dark windows, rust, and restrained signal lights, with roughness/metallic differences and procedural color weathering. The meshes retain their individual material slots.

`M_Foliage` uses two-sided foliage shading, vertex colors, and backlit subsurface color. `M_BarkPBR` and `M_RockPBR` use the CC0 color/normal/roughness maps. Imported textures live in `/Game/Art/Textures`. No game-time network access is needed.

## Scope and verification

The source and FBX assets are original project work; project licensing applies to their geometry. The included photographic maps retain their CC0 provenance. Blender source, exports, surface files, and scripts remain available for edits and regeneration.

The Blender renders were inspected for geometry, grounding, surface detail, and the temperate/industrial direction. Unreal import separately checks mesh dimensions, ground pivots, and material-slot counts and writes `Art/realistic_import_report.json` after a successful run. The v0.2 cooked-game screenshots were inspected at 1600×900: [colony view](Previews/v02_gameplay.png) and [neighborhood overview](Previews/v02_neighborhood.png). Trees remain visible across sectors while small ground details use distance culling. A distance-dependent pale foliage shading band remains known visual polish; see the [development report](../docs/DEVELOPMENT_REPORT.md).

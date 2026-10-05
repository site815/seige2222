# Construction and automatic robot service presentation

These assets and source tools are original project work. No third-party model, character, image, or reference-game asset is included. They use the project's existing original industrial material palette and PBR maps. Project licensing applies; this folder does not assign a separate public license.

| Mesh | Dimensions at unit scale (cm) | Source triangles | Game path |
| --- | --- | ---: | --- |
| Shuttle | 614 × 759 × 448 | 10,940 | `/Game/Art/SM_Shuttle` |
| Robot service bay | 1600 × 1250 × 604 | 18,392 | `/Game/Art/SM_RobotService` |

Both meshes have centered horizontal bounds, ground Z=0 pivots, photographic-free original geometry, named material slots matching the industrial palette, and authored UVs. The shuttle includes its cabin, cargo hull, lift ducts, landing shoes, and boarding ramp. The open service facility contains three charging plinths, charging leads, diagnostic arms, visible status indicators, and cooling plant.

The editable source is [Seige_Construction_Assets.blend](Source/Seige_Construction_Assets.blend). [create_construction_assets.py](../../Tools/create_construction_assets.py) regenerates it and the ignored FBXs; [construction_manifest.json](Exports/construction_manifest.json) records their bounds, slots, and hashes. [import_construction_assets.py](../../Tools/import_construction_assets.py) imports only these two meshes, assigns existing industrial PBR instances by slot name, creates three mesh LODs, and builds the construction materials. It does not change terrain, vegetation, maps, or simulation. [render_construction_assets.py](../../Tools/render_construction_assets.py) produces the [asset review](Previews/construction_assets.png); that image is a Blender asset inspection, not a runtime screenshot.

Runtime presentation is implemented in [SeigeConstructionVisuals.cpp](../../Source/Seige/SeigeConstructionVisuals.cpp):

- A placement ghost uses the selected building's actual mesh, tinted translucent mint for a valid position or red for an invalid position. It is restricted to local player placement and hidden over interface controls and during regional/observer views. The ghost does not clear scenery or create simulation entities.
- Confirmed core construction brings a visible shuttle down to its site. The core reveals from its foundation upward as construction advances. The shuttle settles on the authored rear docking collar and stays there as the colony's emergency vehicle; it does not fly away when construction finishes.
- Construction sites show a translucent plan, scaffold rails, delivered material stacks, and up to four aggregate builder robots. Materials and builders come from simulation construction state, and robot movement uses simulation time so pausing freezes the activity. This display does not add independently controlled units or consume extra resources.
- The reveal material clones the industrial PBR graph. Each building slot keeps its texture, tint, roughness, metallic, and emission parameters. A world-height mask progressively exposes the building, then the original appearance is restored. Invisible upper construction geometry has no picking collision; the existing ground-footprint selection remains available.
- Completed automatic service bays show a small representation of the robots assigned to their support capacity. Service does not require fictitious worker jobs. A mint/amber status marker follows local maintenance-supply state.
- Fog checks run before creating a colony's construction or service visuals. Optional missing-art paths use engine primitives and colored materials; NullRHI tests skip presentation work entirely.

Materials are `/Game/Art/Construction/M_ConstructionHologram` and `M_ConstructionReveal`. The hologram exposes `Tint` and `Opacity`; the reveal exposes `RevealHeight` in world centimeters while preserving all industrial PBR parameters. The v0.5 Shipping route passed 63 stages with zero failures. Rendered review confirmed the landing ghost, visible shuttle/core builders, valid construction materials, scaffolds and their removal on completion. The completed service capture had zero assigned supported robots, so it does not validate occupied charging-berth presentation. See the [development report](../../docs/DEVELOPMENT_REPORT.md) for evidence and limits.

# seige2222 — Graphics Milestone 0.3

[Design index](README.md) · [Status definitions](README.md#design-status)

**Status: packaged and verified, 2026-10-05.** The perspective camera, physical world scale, and revised industry/nature assets are included in the v0.3.0 Windows Shipping build. Nineteen native tests passed cleanly, and the packaged 28-stage interaction route completed with zero failures. Source is maintained in [site815/seige2222](https://github.com/site815/seige2222); versioned local binaries are excluded from Git. This is an early graphics milestone, not a claim of Manor Lords visual parity or finished art.

## Direction and boundaries

The selected direction is a rotatable, tiltable perspective camera, realistic Earth-like terrain and nature, and detailed futuristic industrial buildings at credible physical scale. Empathetic robots and threatening aliens remain part of the art brief. References guide visual quality and readability; the game should use original or appropriately licensed assets.

The current work changes presentation and interaction with that presentation. Colony costs, production, jobs, inventory, courier travel times, AI priorities, and threats remain defined by the existing logical simulation. It does not add fleet control, shared cross-sector economies, or multiplayer.

## Logical coordinates and physical scale

[Graphics/scene.json](../../Graphics/scene.json) selects **6 rendered centimeters per simulation unit**. The existing scenario has a logical sector half-width of 30,000 units.

| Measurement | Logical simulation | v0.3 rendered size |
| --- | --- | --- |
| Sector side | 60,000 units | 3.6 km |
| Nine-sector neighborhood side | 180,000 units | 10.8 km |
| Deposit layout | 25 clustered nodes per repeated sector template | Same coordinates, mapped through the new scale |

The earlier 600 m sector and 1.8 km neighborhood descriptions apply to v0.2's one-centimeter mapping. Enlarging rendered distances also scales the presentation of movement, buildings, terrain, and sensor ranges; it does not increase the logical travel time or change economic balance. Final world-size balance remains open.

## Current source changes

| Area | Verified implementation | Remaining boundary |
| --- | --- | --- |
| Camera | Perspective FOV, orbit yaw/pitch, yaw-relative pan, close-to-region zoom, and ground clearance; native and packaged interaction checks pass. | Broader hardware/performance and visual review. |
| Terrain picking | Terrain ray intersection and corrected live projection/deprojection; landing, construction, rotated views, and low-angle building selection pass. | Programmatic coverage is not an exhaustive human play-through. |
| Persistence | Native tests cover optional yaw/pitch, older format-2 defaults, invalid-angle rejection, and neighborhood continuation. | No separate v0.3 packaged save/load roundtrip was run. |
| Graphics definitions | External camera/scale/density and eight nature roles load in the package; all ten loose definition/document files match source hashes. | Live hot reload and arbitrary new behavior are not implemented. |
| Industrial art | Six original buildings, nine original PBR maps, three LODs each, and refined curved command roof are imported and packaged. | Further material and visual-feedback polish. |
| Nature and lighting | Licensed CC0 nature, full-source Fir C, local foliage mip tuning, dual-frequency ground, and clustered rocks appear in packaged captures. | Distant canopy thinning, terrain repetition, and regional presentation still need polish. |

Delivered controls use **Q/E** for orbit rotation and **middle-mouse drag** for rotation/tilt; WASD/arrows pan relative to the camera, the wheel zooms, and Home returns to the core view. Construction and modal UI consume their own input. These controls are in the v0.3 package; retained v0.2 builds use a fixed orthographic camera.

## Asset implementation and provenance

The industrial generator creates a command campus, drill/conveyor, sawtooth-roof factory, logistics canopy and charging bays, phased-array sensor tower, and armored turret. The command hub is authored at roughly 25.6×28.5 m, with doors around human height and distinct service details. The command roof uses a smoothly shaded 48-sided shell, radial standing seams, and flange joints. Its 71,656 triangles bring the six source meshes to 215,700 triangles. Their [Blender source](../../Art/Source/Seige_Industry_Architecture.blend), [generator](../../Tools/create_industry_assets.py), [importer](../../Tools/import_industry_assets.py), and [Unreal report](../../Art/industry_import_report.json) are retained. No third-party building models were used.

The nature set derives from Poly Haven's Fir Tree 01, Tree Small 02, Fern 02, and Rock Moss Set 01. The retained prepared sources preserve the authors' leaves and branches, convert units/UVs, and rebuild photographic materials for Unreal. Trees/rocks use Nanite; trees enable area preservation. Fir C retains its full 505,494-triangle source LOD0, and ferns use their full small meshes. Local foliage sampling biases alpha mips by −2 and color by −1 without global texture overrides. This pass has no foliage wind animation. The [nature attribution](../../Art/Nature/ATTRIBUTION.md) records source creators and exact changes; its linked manifests preserve download URLs and hashes.

Leafy Grass adds color, DirectX normal, roughness, and ambient occlusion to the local terrain material. The imported terrain revision blends grass and meadow surfaces at two texture frequencies, with clustered rock placement in the scene. Existing robot/bug art is retained, without new character animation. All downloaded nature and ground assets are CC0 and permitted for commercial use and redistribution under [Poly Haven's asset license](https://polyhaven.com/license). See the [complete third-party register](../../Art/THIRD_PARTY_ASSETS.md). Original buildings and their procedural PBR surfaces remain project-authored assets, separate from those licenses.

## Configuration checks and release evidence

Run `node Tools/validate_configuration.mjs`. It validates Rules, AIFILES, Interface, and Graphics before Unreal builds and in CI. Graphics checks cover version 1, finite numerical bounds, integer forest candidate counts, zoom ordering, all eight nature roles, valid `/Game` references, and matching Content packages. Finding a `.uasset` proves the referenced package exists; it does not prove its asset class, imported appearance, materials, or cooking behavior.

**Verification:** Nineteen native tests passed with zero failures or test warnings in `Saved/Automation/v03-final/index.json`; source code did not change after that run. Focused invalid graphics variants were rejected. The first rendered route exposed eleven picking-related assertions; the corrected editor and final packaged routes each completed all 28 stages with zero failures. The Shipping build completed with exit 0 in `Saved/package-v03.log`.

`Saved/packaged-v0.3.0-UiSmoke-verification.json` records packaged exit 0 and eighteen process-tree socket samples, all with zero TCP/UDP endpoints. `Saved/staged-v03-files.json` records ten source-identical loose files across Rules, AIFILES, Interface, and Graphics. These are bounded checks: no separate packaged save/load roundtrip was run, sampled endpoints are not a packet capture, and the on-screen FPS is not a cross-hardware benchmark.

Actual game captures are retained for the [colony](../../Art/Previews/v03_gameplay.png), [close-up](../../Art/Previews/v03_closeup.png), and [neighborhood](../../Art/Previews/v03_neighborhood.png). Canopy thinning at distance, terrain repetition, and region-level presentation remain polish targets. The [development report](../DEVELOPMENT_REPORT.md) records detailed evidence and the separation between tracked source and local versioned binaries. v0.2 results remain historical.

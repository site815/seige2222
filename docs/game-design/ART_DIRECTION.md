# seige2222 — Art Direction

[Design index](README.md) · [Status definitions](README.md#design-status)

Visual direction for the robotic colony, landscape, and alien threat. The user's latest direction supersedes the earlier bright Pandora landscape and cute-building direction wherever they conflict. The production assets and lighting still require development and visual review.

## Current graphics priority

**Confirmed direction:** Rebuild graphics first around a full 3D, realistic Unreal presentation, with Manor Lords as the landscape quality target. Use a perspective camera that can rotate and tilt, credible architectural scale, detailed futuristic industry, and natural terrain, vegetation, materials, and lighting. This is a target for the work, not a claim that the prototype already matches a finished commercial game's quality.

**Retained environment priority (v0.4):** Improve the natural background and landscape first; further building-art work can wait. Surrounding sectors should have a less detailed map presentation, while the area the player zooms into receives the detailed landscape. Responsive middle-button orbiting and a clearer settlement-style overlay are part of judging the environment in play. Existing industrial assets remain in use during this pass. [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md) records this work; its runtime and Shipping paths are verified while further visual polish and user acceptance remain open.

**Reference inspection and response, 2026-10-05:** The installed Manor Lords game's latest Autosave was loaded and paused for wide landscape, close-ground, and map-zoom inspection, then exited without saving. The user rejected seige2222's flat-ground/texture treatment despite passing UI checks. The resulting design priorities are actual relief, continuous varied grass, soil that fits its surrounding vegetation, readable medium-distance surfaces, and cloud/shadow depth. These guide original implementation rather than establish copied assets or quality parity.

**Terrain foundation and historical v0.4 verification:** The focused 1024-subdivision grid (approximately 3.52 m spacing) supplies real rolling/ridge relief and compact foundations. Each of the two grass patches retains its bounds and now contains 77,572 source triangles, including 920 low Bermuda tufts beneath taller swards, at scale 1.0–1.3. Calibrated alpha/transmission, original wildflowers, 500–900 m vegetation culling, and an 8 m photographic tonal/normal/roughness layer improve surface detail without geometric displacement. Measured filtered luminance sets the medium-layer center to 0.25 and contrast to 4, retaining the 0.86–1.14 clamp. The final runtime passed 27 native tests (one with editor HTTP warnings) and 53 rendered stages. Close/middle views are fuller and hills read clearly; distant ground remains smooth/olive and forests uniform. Shipping interaction/staging checks pass; final user acceptance and further visual polish remain open, with no Manor Lords parity claim. See the [v0.4 milestone](GRAPHICS_MILESTONE_0_4.md) and [retained meadow capture](../../Art/Previews/v04_meadow.png).

**v0.3 implementation delivered:** The verified Windows Shipping package includes perspective camera/terrain picking, revised physical scale, six original detailed industrial buildings, licensed CC0 nature, and revised grass/meadow surfaces. The approved rendering conversion is six Unreal centimeters per logical simulation unit. The logical economy, costs, movement timing, and resource layout remain unchanged. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md) for implementation evidence and verification boundaries. Distant canopy thinning, visible terrain repetition, and regional presentation remain polish work; this build does not yet match Manor Lords. Existing robot/bug art is retained with no new character animation.

**v0.6 clarity and performance direction:** Remove the foggy appearance, preserve readable grass and ground filtering, and improve frame time without emptying the landscape. The Medium profile, reduced atmospheric scattering, anisotropic grass color sampling, eased sector/map zoom and frame-interpolated couriers are recorded in [Graphics Performance 0.6](GRAPHICS_PERFORMANCE_0_6.md). Native-resolution performance and moving foliage still require further improvement; the reference quality target remains open.

**v0.7 distance-detail implementation:** Reduce distant geometry cost while keeping the landscape continuous. Neighbor sectors use the same deterministic tree population as a focused sector; they no longer stand in for it with a much sparser forest. Detailed and distant tree representations stay at aligned positions and bounds, and closer inspection changes their geometry rather than relocating the trees. Grass also uses cheaper distant geometry and budgeted streaming. The current broadleaf near asset uses the provider's authored LOD while retaining its photographic materials and established silhouette. These are implementation choices, not a claim of invisible transitions or finished performance. See [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md) for evolving appearance/performance evidence and remaining acceptance checks. The v0.7 Shipping package passed 79 interaction stages and four actual display states with zero failures; see the [verification record](../verification/v0.7.0.json).

The current near broadleaf mesh is `SM_JacarandaNearV07`; nearby grass retains the detailed `SM_MeadowSwardA/B` meshes. Separate opaque geometric proxies take over across stable distance bands of 25–50 m for grass and 500–700 m for trees. The terrain uses `M_TerrainV07`, so the v0.4 material-layer and culling figures above are historical. The single Medium profile keeps level-2 shadows and selects TAA at native 100% rendering or TSR below 100%; the current graphics report records the complete profile and its measured tradeoffs.

## Robot appearance

**Confirmed direction:** Robots should be cute and appealing enough for the player to empathize with them. The colony population should invite attachment even though it is mechanical.

**Retained reference:** The earlier “Eva-style” description was clarified with WALL-E / EVE as a reference for appealing futuristic robots. This supports robot empathy; it no longer establishes the architecture of colony buildings. Specific silhouettes, palettes, locomotion, and animation remain open.

The current scope is a single robotic population type. v0.6 adds representative exterior work cycles and carried cargo tied to real simulation state. Individual robot pathfinding and a complete character-animation set remain later work.

## Landscape and colony buildings

**Confirmed current direction:**

- Use a realistic, Earth-like landscape with Manor Lords as the environmental reference. Terrain, vegetation, materials, and lighting should support a believable place.
- Colony buildings should be futuristic, detailed, and realistic. Their industrial function should be legible through their shape and visible equipment.
- Alien bugs should appear menacing and realistic. Keep robots appealing and capable of inviting attachment within this more realistic environment.

**Superseded direction:** The earlier brighter-than-Pandora landscape and cute WALL-E / EVE-inspired colony architecture are historical exploration, not the current art brief. Existing colorful prototype assets do not override this change.

These references establish visual intent, not copied asset designs or a selected rendering technique. The current industrial/nature asset set is an initial implementation; broader biomes, additional buildings, lighting polish, and longer-term asset budgets still need development. The game remains science fiction; an Earth-like environment does not change its setting or economic rules.

## Working name

**Current working name:** **seige2222**, selected by the user. “Robot Manor Lords” was an earlier informal suggestion, and SEIGE is the older project label still present in the workspace path.

## Open visual decisions

- How to combine a believable Earth-like environment, realistic futuristic industry, and empathetic robots in an original visual language.
- Robot shape, scale, movement, expression, and ways of distinguishing jobs or state.
- Concrete designs for futuristic colony buildings, visible industrial processes, natural terrain, fleets, and menacing aliens.
- Camera framing, zoom limits, and presentation refinements needed to keep workers and physical goods readable within the selected rotatable perspective view.

These are areas to develop. Original procedural geometry can serve as an implementation tool, but its presence alone does not satisfy the requested realism and detail. Judge the result in the running game before claiming that this visual direction is achieved.

## Related documents

- [Vision and Setting](VISION_AND_SETTING.md)
- [Population, Necessities, and Morale](POPULATION_AND_MORALE.md)
- [Core Loop and First Playable](CORE_LOOP_AND_FIRST_PLAYABLE.md)

# seige2222 — Companions and Rex

[Design index](README.md) · [Population and morale](POPULATION_AND_MORALE.md) · [Interface and controls](INTERFACE_AND_CONTROLS.md)

## Status and scope

**Confirmed direction, current gameplay baseline `prototype-10.0`:** Rex is the colony's named dog companion. He walks autonomously within the command core's territory, consumes actual local organic food, and provides a capped local morale benefit while fed. The player can optionally view the world through Rex and walk him directly at 1×. Nearby building damage sends him physically back into the shuttle. This does not add individual worker or combat-unit control. Other animals are deferred.

**v0.10.2 imported revision 8:** the detailed retriever uses the retained Hunyuan3D-derived base and photograph-derived coat restored in v0.10.1 under the user's explicit temporary prototype exception. The repair replaces fused lower limbs and malformed paws, rounds the split tail, reconstructs the occluded face from the sound side, rebakes the photographic atlas and gives limb/tail attachments continuous weights. A final coat pass restores body-photo variation and a UV-matched detail normal after the first runtime candidate looked too flat. Deformation/export validation, Unreal import, Editor front/side review and final Shipping packaging passed; final Shipping front, side, rear and three-quarter Rex captures were reviewed and accepted for this correction. The [import report](../../Art/CompanionDog/import_report.json) describes the imported asset. Textured fur, joint shapes and feathering remain simplified; successful import or gameplay checks do not establish finished photorealism.

## Food and local morale

Rex takes each meal from one living, completed building with sufficient organic food within feeding range. That building's physical inventory is debited. Remote colony totals, neighboring stocks and food merely in transit cannot supply the meal. Feeding is automatic; it does not require a player order.

A fed Rex grants the configured morale multiplier to eligible building work within his local radius. The benefit does not stack from additional companions. It does not remove staffing, power, construction or material requirements. When the fed period expires without another meal, the bonus ends. This small companion benefit does not settle the broader happiness, shortage or revolt systems.

Organic food is also a trade product. Workers remain robotic and do not acquire a food requirement from Rex's diet. The finite landed kit currently includes organic food; further meals must use actual colony supplies.

The following are **editable prototype balance values**, not final simulation balance. [companions.json](../../Rules/companions.json) is authoritative.

| Field | Current prototype |
| --- | --- |
| Meal | 0.1 kg organic food every 1,800 simulation seconds when locally available |
| Feeding range | 100 m from an eligible food stock |
| Fed duration | 2,400 simulation seconds after a meal |
| Morale benefit | +0.05 work multiplier, capped rather than additive stacking |
| Morale range | 60 m from Rex |
| Autonomous walking | 5 km/h; destinations within 100 m of the core, with 8-second rests |
| Territory | 100 m from the core for autonomous and player-controlled movement |
| Damage alert | Actual building damage within 100 m of the core |
| Retreat | 14 km/h along a physical route to the shuttle hatch |
| Shelter | Remain inside until 60 simulation seconds after the latest qualifying damage |

## Territory and shelter

Rex's home follows the command core. Autonomous destinations and their routes must remain inside the configured territory; controlled movement also stops at that boundary. Building collision and route checks prevent him walking through the settlement. This is a bounded local companion area, rather than unrestricted travel through all nine regions.

Actual damage to a building within the core's damage-alert radius triggers retreat. The presence of an enemy alone does not. Retreat immediately releases first-person control and routes Rex to the real core access point and hatch at the configured running speed; it does not teleport him indoors. He is hidden and grants no morale bonus while sheltered. Further qualifying damage extends the quiet-period deadline. Once it expires, he walks back out through the access point before resuming normal movement. Control is unavailable during retreat, shelter and the exit sequence.

## Selection and first-person roaming

1. Select the player's own command core and choose **Find Rex**.
2. In Rex's panel, choose **Roam as Rex**.
3. Use **WASD** to walk and the **mouse** to look. **Escape** returns to the saved colony camera; **F10** opens the normal game menu. Space can pause.

Entering this view selects **1×** playback and unpauses. Speed controls cannot accelerate it. Returning restores the colony camera, not the prior accelerated speed; ordinary colony speed controls are then available again. The game menu pauses the local simulation normally.

Movement changes Rex's actual simulation position. It follows the 100 m core territory, physical building and world constraints, with a terrain-slope check in the world adapter. It is not a free camera, spectator teleport or first-person combat mode. Autonomous walking resumes after control is released unless damage has triggered retreat. Observer scenarios cannot take control of Rex. First-person rendering hides his body from the camera inside his head; his counted simulation state continues normally.

## Persistence and departure

Current **format-8** saves retain Rex's position, home, target/route, random state, feeding timers, consumed-food ledger, walking distance, evacuation state, shelter deadline and retreat/shelter/exit flags. Saves must match the `prototype-10.0` Rules/AI fingerprints; the v0.10.1/v0.10.2 presentation changes retain that compatibility. Earlier save formats remain incompatible. Loading resumes colony control rather than restoring held movement input. Saving during Rex's first-person view records the saved colony camera so the game can load into a valid strategy view. Starting or loading another scenario releases companion control before replacing the simulation.

Shuttle departure marks Rex evacuated, ends control and removes the local companion presentation and benefit. The current prototype ends that colony session; emergency evacuation does not yet require a separate physical boarding sequence or simulate a companion's arrival on another planet. The damage-triggered shelter route described above is physical. This implementation boundary does not resolve broader fleet embarkation or relocation rules.

## Artwork and credits

The current detailed mesh incorporates an existing Hunyuan3D-2.1 generated base. Blender processing supplies the project rig, animation, geometry repairs and texture baking. The albedo atlas contains pixels derived from the owner's photographs, with authored coat-color zoning; it is not accurately described as wholly original geometry or as free of photograph pixels. The full original photographs remain untracked; selected cropped and processed likeness derivatives are retained with the reproducible source, as documented in provenance. The user's request authorizes use of those references and their derived likeness in this prototype; it does not assign a public license to the original photographs.

The user explicitly instructed that the detailed model be used for now and the licence issue recorded in Credits. The unresolved Hunyuan3D-2.1 territorial restriction is documented in [texture and model provenance](../../Art/CompanionDog/TEXTURE_PROVENANCE.md); the generated base is not CC0, and attribution is not evidence of unrestricted commercial or public distribution permission. No new hosted generation or photograph upload is required for the current offline repair.

Credit the game concept and Rex reference direction to the user, the Hunyuan3D-derived base according to the provenance record, and the project implementation/repair work developed with Codex. The [Rex asset record](../../Art/CompanionDog/README.md), provenance, import report and source-validation report own the exact current asset identities. The legacy generated coat-detail bitmap is retained for reference; the restored photo-atlas materials do not sample it.

## Verification and remaining acceptance

The [v0.8.1 verification record](../verification/v0.8.1.json) retains the earlier physical-food/morale, walking/persistence and first-person-control checks, plus that version's packaged interactions. Those historical passes are not new v0.10.2 gameplay results. The [v0.10.0 focused record](../verification/v0.10.0.json) includes `Seige.Companions.DamageTriggeredShelter`, covering actual nearby damage, physical retreat, release of control, shelter persistence and return to the territory. The [v0.10.1 record](../verification/v0.10.1.json) records the restored detailed asset's import/rig review and rendered views; it also retains close-angle appearance limitations.

For v0.10.2, repaired front, both-side, rear and walking-pose Blender previews were reviewed; deformation/export validation and actual Unreal import passed. The body-coat correction was reviewed in actual Editor front/side captures before final cooking. The final packaged focused UI route passed 106 assertions / 25 stages at each of two resolutions; those are UI checks, not a rerun of companion gameplay coverage. Final Shipping front, side, rear and three-quarter Rex captures were reviewed and accepted for this correction. Geometry/texture validation must accompany visual review: valid weights, UVs and successful cooking alone cannot establish Rex's photographic likeness. The repair retains runtime rig paths, scale, animation stride and companion behavior; recheck affected gameplay if those contracts change.

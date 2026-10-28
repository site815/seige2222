# seige2222 — Companions and Rex

[Design index](README.md) · [Population and morale](POPULATION_AND_MORALE.md) · [Interface and controls](INTERFACE_AND_CONTROLS.md)

## Status and scope

**Confirmed direction, current gameplay baseline `prototype-10.0`:** Rex is the colony's named dog companion. He walks autonomously within the command core's territory, consumes actual local organic food, and provides a capped local morale benefit while fed. The player can optionally walk him directly at 1× with a third-person camera showing his body and walking animation. After landing he walks out of the shuttle. Injury, nearby threats and nearby building damage send him physically back into the shuttle; he heals safely inside. His death is permanent and slows every worker to half movement speed for thirty calendar days. This does not add individual worker or combat-unit control. Other animals are deferred.

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
| Player-controlled movement | Walk at 5 km/h; hold Shift to run at 14 km/h |
| Territory | 100 m from the core for autonomous and player-controlled movement |
| Damage alert | Actual building damage within 100 m of the core |
| Retreat | 14 km/h along a physical route to the shuttle hatch |
| Health | 25 HP; no shield or armor |
| Proximity danger | Living bugs, hostile projectiles or visiting privateers within 35 m |
| Shelter | Heal at 5 HP per simulation second; exit only at full health after 60 quiet simulation seconds |
| Death consequence | All worker movement ×0.5 for exactly 30 complete calendar days; roads still apply their usual multiplier |

## Territory and shelter

Rex's home follows the command core. Autonomous destinations and their routes must remain inside the configured territory; controlled movement also stops at that boundary. Building collision and route checks prevent him walking through the settlement. This is a bounded local companion area, rather than unrestricted travel through all nine regions.

Rex starts aboard at the physical shuttle hatch and remains hidden until the landing descent and hatch opening finish (12 + 3 simulation seconds under current worker rules). He then walks through the same access corridor used by workers. Choosing a landing location does not create a roaming dog on the ground before touchdown.

Actual damage to a building within the core's damage-alert radius, an injury to Rex, or a hostile within the danger radius triggers retreat. Retreat immediately releases companion control and routes Rex to the real core access point and hatch at the configured running speed; it does not teleport him indoors. Direct projectiles, unintended friendly hits, splash damage and bug contact attacks use his actual exposed body and health. Friendly fire control avoids shooting directly through him, while projectile movement and splash remain physical.

He is hidden, protected from exterior shots and grants no morale bonus while aboard or sheltered. Healing only starts after he reaches the hatch alive. Further qualifying danger extends the quiet-period deadline. Once the deadline expires and his health is full, he walks back out through the access point before resuming normal movement. Control is unavailable during landing, retreat, shelter, the exit sequence or after death.

A lethal hit sets a permanent death state. No later tick, healing or save/load can resurrect him. Every worker's actual route movement becomes 50% of its normal speed for exactly thirty authoritative calendar days; production throughput itself is not directly halved. The current 30-minute day plus 30-minute night makes this thirty real hours at 1× or three at 10×, excluding pauses. This effect is colony-wide and includes workers produced or activated during the mourning period. At the exact deadline normal movement returns automatically.

## Selection and third-person roaming

1. Select **Rex** in the bottom menu (or choose **Find Rex** from the player's own command core). The camera focuses Rex outdoors, or the orbital module while he is aboard, sheltered or dead.
2. In Rex's panel, choose **Roam as Rex**.
3. Move the **mouse** to steer Rex, including turning him while stationary. **WASD** moves relative to that steering heading; hold **Shift** to run. The third-person camera follows him, keeping his body and movement animation visible.
4. Hold **Alt + mouse** to look around without redirecting Rex. Releasing Alt smoothly returns the camera behind his steering heading. **Escape** returns to the saved colony camera; **F10** opens the normal game menu. Space can pause.

Walking is 5 km/h and running is 14 km/h under the current editable rules. Shift is reserved for running, so Alt is the independent camera modifier. This uses [PUBG's documented separation between Shift sprint and Alt + mouse look](https://support.pubg.com/hc/en-us/articles/360002074913-What-are-the-basic-game-commands).

Entering this view selects **1×** playback and unpauses. Speed controls cannot accelerate it. Returning restores the colony camera, not the prior accelerated speed; ordinary colony speed controls are then available again. The game menu pauses the local simulation normally.

Movement changes Rex's actual simulation position. It follows the 100 m core territory, physical building and world constraints, with a terrain-slope check in the world adapter. Autonomous walking resumes after control is released unless damage has triggered retreat. Since v0.10.7, the center colony's Rex can also be controlled in AI-observation scenarios: the colony remains AI-run, colony commands remain unavailable, and roaming still locks playback to 1×. In v0.10.8, the follow camera replaces the former view from inside his head; Rex remains visible while being controlled and his counted simulation state continues normally.

## Persistence and departure

Current **format-9** saves retain Rex's position, home, target/route, random state, feeding timers, consumed-food ledger, walking distance, health, permanent death, aboard/evacuation state, shelter deadline, retreat/shelter/exit flags and the calendar mourning deadline. Worker health is persisted alongside each physical body. Saves must match the current Rules/AI fingerprints. The v0.10.3 gameplay changes intentionally require a new colony: format-8 and earlier saves are rejected with an explanation and left untouched. Loading resumes colony control rather than restoring held movement input. Saving during Rex's roaming view records the saved colony camera so the game can load into a valid strategy view. Starting or loading another scenario releases companion control before replacing the simulation.

Shuttle departure marks Rex evacuated, ends control and removes the local companion presentation and benefit. The current prototype ends that colony session; emergency evacuation does not yet require a separate physical boarding sequence or simulate a companion's arrival on another planet. The damage-triggered shelter route described above is physical. This implementation boundary does not resolve broader fleet embarkation or relocation rules.

## Artwork and credits

The current detailed mesh incorporates an existing Hunyuan3D-2.1 generated base. Blender processing supplies the project rig, animation, geometry repairs and texture baking. The albedo atlas contains pixels derived from the owner's photographs, with authored coat-color zoning; it is not accurately described as wholly original geometry or as free of photograph pixels. The full original photographs remain untracked; selected cropped and processed likeness derivatives are retained with the reproducible source, as documented in provenance. The user's request authorizes use of those references and their derived likeness in this prototype; it does not assign a public license to the original photographs.

The user explicitly instructed that the detailed model be used for now and the licence issue recorded in Credits. The unresolved Hunyuan3D-2.1 territorial restriction is documented in [texture and model provenance](../../Art/CompanionDog/TEXTURE_PROVENANCE.md); the generated base is not CC0, and attribution is not evidence of unrestricted commercial or public distribution permission. No new hosted generation or photograph upload is required for the current offline repair.

Credit the game concept and Rex reference direction to the user, the Hunyuan3D-derived base according to the provenance record, and the project implementation/repair work developed with Codex. The [Rex asset record](../../Art/CompanionDog/README.md), provenance, import report and source-validation report own the exact current asset identities. The legacy generated coat-detail bitmap is retained for reference; the restored photo-atlas materials do not sample it.

## Verification and remaining acceptance

The [v0.8.1 verification record](../verification/v0.8.1.json) retains the earlier physical-food/morale, walking/persistence and first-person-control checks, plus that version's packaged interactions. Those historical passes are not new v0.10.2 gameplay results. The [v0.10.0 focused record](../verification/v0.10.0.json) includes `Seige.Companions.DamageTriggeredShelter`, covering actual nearby damage, physical retreat, release of control, shelter persistence and return to the territory. The [v0.10.1 record](../verification/v0.10.1.json) records the restored detailed asset's import/rig review and rendered views; it also retains close-angle appearance limitations.

For v0.10.2, repaired front, both-side, rear and walking-pose Blender previews were reviewed; deformation/export validation and actual Unreal import passed. The body-coat correction was reviewed in actual Editor front/side captures before final cooking. The final packaged focused UI route passed 106 assertions / 25 stages at each of two resolutions; those are UI checks, not a rerun of companion gameplay coverage. Final Shipping front, side, rear and three-quarter Rex captures were reviewed and accepted for this correction. Geometry/texture validation must accompany visual review: valid weights, UVs and successful cooking alone cannot establish Rex's photographic likeness. The repair retains runtime rig paths, scale, animation stride and companion behavior; recheck affected gameplay if those contracts change.


### v0.10.3 focused validation scope

New native tests cover aboard/physical landing exit, injury and danger retreat, shelter healing, permanent death, exact thirty-day expiry, actual half-speed worker displacement, companion/worker save state, physical projectile and splash damage, and dead courier/loader ownership. First-person tests retain 1× control restrictions. The final selected native suite passed all 46 tests, including all seven `Seige.Companions` and all three `Seige.Units` cases. The final packaged 10× route passed 143 assertions at each of two resolutions, including Rex focus and release of prior targeting modes. Actual Rex-panel captures and package identities are retained in the [v0.10.3 verification record](../verification/v0.10.3.json); these checks do not claim new character-art improvements.

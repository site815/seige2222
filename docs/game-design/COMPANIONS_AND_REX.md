# seige2222 — Companions and Rex

[Design index](README.md) · [Population and morale](POPULATION_AND_MORALE.md) · [Interface and controls](INTERFACE_AND_CONTROLS.md)

## Status and scope

**Confirmed direction, implemented in v0.8 source:** Rex is the colony's named dog companion. He walks autonomously, consumes actual local organic food, and provides a capped local morale benefit while fed. The player can optionally view the world through Rex and walk him directly. This does not add individual worker or combat-unit control. Other animals are deferred.

The revised likeness is an original Blender interpretation guided by the user's private reference photographs. The retained [import report](../../Art/CompanionDog/import_report.json) records the mesh, skeleton and clips. The v0.8.1 packaged route passed feeding/control interactions. Runtime captures still show a stylized interpretation with likeness/contact limits; successful import and those gameplay checks do not establish photorealistic appearance.

## Food and local morale

Rex takes each meal from one living, completed building with sufficient organic food within feeding range. That building's physical inventory is debited. Remote colony totals, neighboring stocks and food merely in transit cannot supply the meal. Feeding is automatic; it does not require a player order.

A fed Rex grants the configured morale multiplier to eligible building work within his local radius. The benefit does not stack from additional companions. It does not remove staffing, power, construction or material requirements. When the fed period expires without another meal, the bonus ends. This small companion benefit does not settle the broader happiness, shortage or revolt systems.

Organic food is also a trade product. Workers remain robotic and do not acquire a food requirement from Rex's diet. The finite landed kit currently includes organic food; further meals must use actual colony supplies.

The following are **editable prototype balance values**, not final simulation balance. [companions.json](../../Rules/companions.json) is authoritative.

| Field | Current candidate |
| --- | --- |
| Meal | 0.1 kg organic food every 1,800 simulation seconds when locally available |
| Feeding range | 100 m from an eligible food stock |
| Fed duration | 2,400 simulation seconds after a meal |
| Morale benefit | +0.05 work multiplier, capped rather than additive stacking |
| Morale range | 60 m from Rex |
| Autonomous walking | 5 km/h; destinations within a 35 m home radius, with pauses |

## Selection and first-person roaming

1. Select the player's own command core and choose **Find Rex**.
2. In Rex's panel, choose **Roam as Rex**.
3. Use **WASD** to walk and the **mouse** to look. **Escape** returns to the saved colony camera; **F10** opens the normal game menu. Space can pause.

Entering this view selects **1×** playback and unpauses. Speed controls cannot accelerate it. Returning restores the colony camera, not the prior accelerated speed; ordinary colony speed controls are then available again. The game menu pauses the local simulation normally.

Movement changes Rex's actual simulation position. It follows physical building and world constraints, with a terrain-slope check in the world adapter. It is not a free camera, spectator teleport or first-person combat mode. Autonomous walking resumes after control is released. Observer scenarios cannot take control of Rex.

## Persistence and departure

Current **format-5** saves retain Rex's position, home, target/route, random state, feeding timers, consumed-food ledger, walking distance and evacuation state. Earlier saves are incompatible with the expanded v0.8 revision. Loading resumes colony control rather than restoring held movement input. Saving during Rex's first-person view records the saved colony camera so the game can load into a valid strategy view. Starting or loading another scenario releases companion control before replacing the simulation.

Shuttle departure marks Rex evacuated, ends control and removes the local companion presentation and benefit. The current prototype ends that colony session; it does not yet simulate a separate physical boarding sequence or a companion's arrival on another planet. This implementation boundary does not resolve broader fleet embarkation or relocation rules.

## Artwork and credits

Rex is an original authored interpretation guided by user-provided private photographs; likeness and in-game review remain pending. The mesh, rig, animations and procedural surface work are authored in Blender. An original coat-detail bitmap was generated for this project with image generation, without copying user photograph pixels. The photographs must remain outside the repository and must not be packed into models, textures, previews or exports. No third-party dog model, rig or photograph is being redistributed.

Credit the game concept and Rex reference direction to the user, and the original asset implementation to the project work developed with Codex. This does not assign a new public license to either the photographs or the generated project assets. Editable source, asset provenance and preview limitations are recorded in the [Rex asset record](../../Art/CompanionDog/README.md) and [original asset register](../../Art/ASSET_REGISTER.md).

## Acceptance still required

Verify physical food debit and bonus expiry, capped range behavior, autonomous and controlled walking, pause/1× restrictions, camera restoration, save/load and evacuation. Separately inspect the imported likeness, animation, ground contact and first-person camera in the actual game. The expanded presentation route includes Rex selection and walking; its pending run must supply evidence before these documents claim packaged completion.

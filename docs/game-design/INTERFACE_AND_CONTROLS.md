# seige2222 — Interface and Controls

[Design index](README.md) · [Status definitions](README.md#design-status)

Interface direction and the boundary between player decisions and automatic simulation. The latest v0.4 request supersedes the earlier top-only/no-bottom construction layout. The final terrain/close-camera route completed 53 stages with zero failures and corrected the earlier screenshot timing. The Shipping route also passed 53 stages; broader human usability review remains separate.

## Interface references

**Confirmed direction:** Manor Lords is the closer primary interface reference because the game is mainly a settlement-building simulation. StarCraft II is a secondary reference for polish and clarity.

The user wants to improve on aspects of Manor Lords using the perceived polish of StarCraft II. The specific requirements below now refine that direction. These references do not adopt individual-unit combat micromanagement.

## Current layout and construction requirements

**Confirmed user requirements:**

- Show the current game version and FPS at the top left.
- Present resources as an overlay, with alerts underneath and useful explanations available on hover.
- Use a **floating bottom construction overlay**, retaining the **B** shortcut. This is the user's latest explicit layout change; the earlier prohibition on a bottom menu no longer applies.
- Represent construction choices with icons and reveal their building names on hover.
- Organize construction logically and provide understandable keyboard shortcuts. Exact category and item keys are implementation choices until verified in play.
- Show every building's weapon damage, reload, DPS, resource information, power, and other relevant statistics, including zero usage and unarmed buildings.

**Implementation guidance:** Keep the resource-related, logistics, and defense building categories recognizable; a more detailed industrial group can help organize production without changing building capabilities. A sectioned building panel may separate Overview, Weapons, Power, Production, Resources, and Maintenance. All sections must remain accessible, with pagination where necessary. Use the shared simulation information rather than copying numeric rules into the HUD. Explicitly explain that current power values are zero because the prototype has no separate grid. The interface should let the player understand a blocked action without reading source files.

Hover and keyboard behavior must preserve ordinary world controls. Interface clicks must not place a building behind a menu or dialog, and menu shortcuts must not simultaneously move the camera. Input handling must work between rendering passes; drawing state cannot be required to process a click.

The displayed version must match the delivered build's version. The v0.3 package passed its 28-stage rendered interaction route, including menus, construction shortcuts, and perspective world clicks. This does not establish exhaustive human usability or completed visual polish.

## Perspective camera revision

**Confirmed v0.3 direction:** The world uses a rotatable, tiltable perspective camera. Panning follows the camera's yaw; clicks and construction previews must intersect the visible terrain in front of the camera. Camera motion must not turn a UI action into a world command or accept a point behind the camera.

Delivered v0.3 bindings use Q/E to rotate, middle-mouse drag to rotate/tilt, WASD/arrows to pan, the wheel to zoom, and Home to return to the core view. Saved camera yaw and pitch are added without requiring them in older format-2 saves. Native camera tests and the packaged rendered route pass; save-format compatibility was tested natively, without a separate packaged save/load roundtrip. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md).

**Latest v0.4 request:** The middle/third mouse button should orbit responsively like the reference; the prior sensitivity was too low. The implementation captures pointer motion, uses editable sensitivity, and restores the pointer on release. Current configured limits are 8–80° pitch, minimum zoom 120 logical units, and 160 cm camera-to-ground clearance. At close zoom, the view smoothly lowers while retaining the user's orbit angle for zoom-out. Native tests cover clearance and angle restoration; final feel, gesture behavior, and appearance remain under rendered review.

Zooming out should lead to a less detailed regional map. Zooming/focusing into a sector should reveal that area's detailed environment. The current source uses a 3×3 cartographic survey with selectable sectors; it suppresses world construction while on the map, and human deployment remains limited to the home sector. This is a viewing transition, not cross-sector movement or colony ownership. See [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md).

## Colony controls

**Confirmed direction:**

- Buildings automatically fill jobs and operate when workers and resources are available.
- Production, goods movement, repairs, and adjustment of robot population to job demand are automatic.
- The player can turn buildings off and should be able to see required workers or open jobs.
- Routine manual adjustment should be minimal.

The implemented v0.4 building dossier explains supported construction, production, local stock, weapon, power, and maintenance behavior. The completed 53-stage route captured the intended armed, unarmed, and explicit zero-power panels. The earlier 41-stage action path passed, but its weapons/unarmed/regional-AI screenshot timing was incorrect and does not establish those captured states. Controls for future upgrades and the full robot-needs system remain open. See [Colony Operations and Player Control](COLONY_OPERATIONS_AND_PLAYER_CONTROL.md).

## Fleet controls

**Confirmed direction:**

- The player directs fleets, not individual combat units.
- A fleet can receive mission orders for autonomous operation.
- Under direct control, the player can direct where the fleet moves as a group.
- During an engagement, combat executes automatically; individual units cannot be micromanaged.
- Fleet aggressiveness can be configured for autonomous behavior, including behavior left in place while the player is offline.

**Illustrative aggression choices:** Actively seek engagements, take a middle approach, or fight only when necessary. Exact names, number of settings, target criteria, and actions are not yet defined.

**Open:** How mission orders and direct fleet movement interact, what fleet-level changes can be made during combat, retreat and disengagement, order persistence, and presentation of the risks associated with different aggression settings.

## Information boundaries

The interface must respect finite sensor coverage: a sector map is not a live view of all activity. The world includes remote outposts and distributed sensors that may be stolen. Exact last-known information and terrain-memory behavior remain open.

Observer mode may inspect AI colonies, but it must not issue building commands. In human play, neighbor views must not expose hidden live colony statistics or let a neighbor's local entity ID operate on a home building. Configured occupancy can appear on the regional survey without revealing current enemy positions. Ground and vegetation changes must follow the same visibility boundary as building models.

## Related documents

- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)
- [Art Direction](ART_DIRECTION.md)

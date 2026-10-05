# seige2222 — Interface and Controls

[Design index](README.md) · [Status definitions](README.md#design-status)

Interface direction and the boundary between player decisions and automatic simulation. The floating-bottom construction layout supersedes the earlier top-only requirement. The v0.6 menu, display, and speed revision passed **34 clean native tests**, the **79-stage Shipping interaction route with zero failures and exit code 0**, and all **four Shipping native-display states with zero failures**. The earlier v0.5 Shipping route completed 63 stages with zero failures, including construction, camera, terrain selection and building information. Automated checks do not establish exhaustive human usability.

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

## Main menu and game menu — v0.6

**Confirmed user requirements:** Use a polished main menu over the actual landscaped 3D scene, with restrained typography, clear hierarchy, and consistent navigation. The main menu offers Begin a colony, Load colony, Settings, Credits, and Exit game. Multiplayer is labelled as coming later. Menu title, eyebrow, and tagline are editable in `Interface/ui.json`.

The current title is **SEIGE 2222**. Its heading uses a runtime font rendered at the actual display size. A continuous vertex-gradient scrim replaces the overlapping translucent strips that produced visible vertical seams. Actual 1280×720 and 3840×1600 development and final Shipping captures confirm the sharp title, continuous gradient, and unclipped navigation. Ordinary labels also render at their final pixel size, with matching font measurements for wrapping and deposit-label widths. Final Shipping captures of the main menu, scenario, construction catalog, credits, game menu, and settings show crisp text contained within its panels. The 1600×900 forced interaction capture is layout evidence; its Settings "Native" label does not establish the monitor's native resolution.

The floating gameplay dock and landing survey expose a direct **Menu** button. **F10** opens or closes the game menu directly, including from the construction catalog or a game-menu subpage. **Escape** first cancels a blueprint, backs out of a construction category or overlay, or clears selection; with those dismissed, it toggles the game menu. Right click cancels/backtracks local placement and overlays without opening the game menu. Escape from Settings or Credits returns to the menu that opened that page.

The game menu offers Resume, Save colony, Load colony, Settings, Credits, Return to main menu, and Exit game. Saving is unavailable before landing. Opening the game menu pauses both the local colony and its independently simulated neighbors. Closing it restores the earlier paused/running state and selected speed. Its Settings and Credits pages retain that pause. Saving from the game menu records the pause state from before the menu opened; loading restores the saved state and closes the menu.

Main menu, scenario setup, and human landing survey also suspend the local simulation. Construction and Colony overlays continue to run time. These are local prototype controls; they do not define time control on a persistent server.

## Display and local speed — v0.6

**Confirmed user requirements and current implementation:**

- A normal first launch uses **borderless**, filling the monitor at its native resolution. Existing exclusive-fullscreen preferences are converted to borderless; the interface provides no exclusive-fullscreen mode.
- **Windowed** is selectable. Its resolution selector offers 1280×720, 1600×900, 1920×1080, 2560×1440, and 3840×2160 when they fit the desktop. The window choice is retained separately from the borderless monitor size.
- **3D render resolution** is independent of window/display resolution. It defaults to 100%, has 50–100% controls in ten-percentage-point steps, and shows the effective width and height in pixels. Menus and text remain at full display resolution.
- **Medium** is the sole graphics quality label. It is a custom, externally configured preset in `Graphics/scene.json`, with selected lighting, landscape, texture, and antialiasing settings. It is not an exposed choice among Unreal's generic Low/High/Epic presets. Applying a display or render-resolution change preserves this custom profile.
- Local playback speeds are **1×, 5×, and 10×**. The speed button and **+** cycle forward; **−** cycles backward. Main-keyboard and numeric-keypad variants work. **Space** pauses/resumes without changing the selected speed. The supported list is stored in `Interface/ui.json`; legacy saved 3× playback migrates to 5×.

Normal display preferences persist locally. Automated `-UiSmoke`, `-GraphicsBenchmark`, and `-ForceRes` runs leave the player's display preferences untouched and retain their explicitly requested capture size. The separate `-DisplaySmoke -NoSaveDisplay` route applies actual borderless/windowed and render-resolution changes, but the `NoSaveDisplay` guard suppresses preference saves and display config writes. It is a verification path, not a player-facing option.

**Verified development and final Shipping display routes:** On a 3840×1600 monitor using Windows 200% display scaling, both routes passed all four actual viewport checks with zero failures:

| State | Display pixels | 3D render scale | Effective render pixels |
| --- | --- | --- | --- |
| Native borderless | 3840×1600 | 100% | 3840×1600 |
| Windowed | 1280×720 | 100% | 1280×720 |
| Reduced rendering in the same window | 1280×720 | 75% | 960×540 |
| Restored native borderless | 3840×1600 | 100% | 3840×1600 |

The custom Medium shadow quality remained active in every state. High-DPI game mode is enabled in `DefaultEngine.ini`, including unattended verification. This corrected the earlier DPI-unaware desktop report of 1920×800 at 200% scaling. The final packaged result is retained in the [Shipping display report](../../Art/EnvironmentV06/display-shipping.json), with a local copy at `Saved/DisplaySmoke-v0.6.0.json`. Its fresh display captures are under `%LOCALAPPDATA%/seige2222/Saved/Screenshots/Review-v06/display_*.png`. Earlier development evidence remains in `Saved/DisplaySmoke.json` and `Saved/Screenshots/Review-v06`.

| Control | Action |
| --- | --- |
| Menu button / F10 | Open or close the game menu |
| Escape | Cancel/backtrack local UI first, then toggle the game menu |
| Space | Pause/resume local simulation |
| + / − | Cycle 1×, 5×, and 10× forward/backward |
| F5 / F9 | Save/load a running colony; also available through the game menu |
| B | Open construction; category and blueprint shortcuts follow |
| WASD / arrows | Pan relative to camera yaw |
| Q / E; middle drag | Rotate; rotate and tilt |
| Wheel / Home | Zoom; return to the command core view |

Native regression coverage includes direct menu access, pausing center and neighbor simulations, returning from Settings/Credits, saving the pre-menu pause state, loading from the menu, pre-landing resume and F9 load, speed keys, restoring the main-menu backdrop, and rejection of stale blueprint actions through the menu. The latest rebuilt headless run passed all **34 tests cleanly**, with **zero warnings, failures, or unrun tests**. This supersedes the earlier run containing an engine HTTP connectivity-probe warning. The report is `Saved/Automation/v06-final`. The final Shipping interaction route completed all **79 stages with zero failures**, including game-menu pause/restoration, settings, speeds, map transitions, and service activity; its report also counted 831 courier-motion frames between simulation ticks at 1×. Packaged captures and the report are under `%LOCALAPPDATA%/seige2222/Saved`. The separate Shipping native-display check also passed all four states, as recorded above.

## Perspective camera revision

**Confirmed v0.3 direction:** The world uses a rotatable, tiltable perspective camera. Panning follows the camera's yaw; clicks and construction previews must intersect the visible terrain in front of the camera. Camera motion must not turn a UI action into a world command or accept a point behind the camera.

Delivered v0.3 bindings use Q/E to rotate, middle-mouse drag to rotate/tilt, WASD/arrows to pan, the wheel to zoom, and Home to return to the core view. Saved camera yaw and pitch are added without requiring them in older format-2 saves. Native camera tests and the packaged rendered route pass; save-format compatibility was tested natively, without a separate packaged save/load roundtrip. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md).

**Latest v0.4 request:** The middle/third mouse button should orbit responsively like the reference; the prior sensitivity was too low. The implementation captures pointer motion, uses editable sensitivity, and restores the pointer on release. Current configured limits are 8–80° pitch, minimum zoom 120 logical units, and 160 cm camera-to-ground clearance. At close zoom, the view smoothly lowers while retaining the user's orbit angle for zoom-out. Native tests cover clearance and angle restoration; final feel, gesture behavior, and appearance remain under rendered review.

Zooming out should lead to a less detailed regional map. Zooming/focusing into a sector should reveal that area's detailed environment. The current source uses a 3×3 cartographic survey with selectable sectors; it suppresses world construction while on the map, and human deployment remains limited to the home sector. This is a viewing transition, not cross-sector movement or colony ownership. See [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md).

**v0.6 revision:** Wheel zoom, Home, the Regions button, and player-triggered sector focus ease toward their requested zoom. The cartographic overlay fades in across the configured transition band rather than replacing the world abruptly; a whole-sector survey remains a 3D view below that band. Input and map visibility use the displayed zoom, and projected world labels follow the actual camera. Scenario initialization, loaded-camera restoration, and direct setup/test calls can establish a view immediately; the `FocusSector` API uses its smooth-transition option for UI and controller actions. Maximum zoom and transition limits remain editable graphics parameters. Regional viewing continues to respect ownership and sensor boundaries.

## Colony controls

**v0.5 delivered revision:** Raw middle-drag sensitivity is 0.22 degrees per horizontal pixel and 0.18 per vertical pixel, between the previously rejected slow and fast settings. Resource badges retain their per-deposit offsets while the camera moves and resolve overlaps once it settles. Neighbor terrain remains textured in the local 3D view, with terrain-following sector borders; the cartographic zoom threshold remains separate. Native gesture checks and the 63-stage rendered Shipping route passed. Physical mouse feel still depends on the player's device and preferences.

Placement displays a translucent version of the selected building, green for valid ground and red for a blocked order. The initial placement previews the command core footprint; confirming starts the shuttle landing and deployment. Later construction delivers reserved materials and shows assembly progress. The logistics shortcut **B, L, C** selects charging and maintenance; the workforce hover explains service capacity and supply efficiency.

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

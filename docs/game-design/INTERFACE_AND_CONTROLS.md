# seige2222 — Interface and Controls

[Design index](README.md) · [Status definitions](README.md#design-status)

**Current v0.8.1 revision:** The five-group resource HUD and single Extraction Mine are implemented in source and external definitions. Configuration validation passes, including seven invalid grouping cases. The revision has compiled and been staged; package boot and four display states passed. The final v0.8.1 Shipping executable passed 117 interaction stages, boot/offline checks and four display states; reconciled native coverage contains 88 unique clean results. The [v0.8.1 verification record](../verification/v0.8.1.json) identifies the exact evidence and retained limitations. The [v0.8 verification record](../verification/v0.8.0.json) covers the preceding package and keeps its 117-stage gameplay checkpoint separate from its final rendered executable; it does not verify these new changes.

## Five resource groups and one mine — v0.8.1

**Confirmed direction:** The top resource overlay has five separate floating groups, in this order:

| Group | Persistent readout | Hover detail |
| --- | --- | --- |
| Credits | Galactic-credit balance, with four decimal places | More precise balance, external-trade purpose and the 1 kg gold price anchor |
| Energy | Stored/capacity kWh; generation and passive demand in kW | Precise totals, separate-grid limits and additional per-action consumption |
| Raw materials | Water, metal ore, silica, biomass, rare metals, radioactive ore, crystalline material and hydrocarbons | Full resource names, individual quantities and kg/L units |
| Basic production | Construction alloys, conductors, industrial glass, fuel, plastic pellets and organic food | Full names, quantities, units and physical mass/storage conversion |
| Adv production | Control circuits, robotic parts, battery modules, AI chips, fusion reactor assemblies, kinetic ammunition and missile ammunition | The same physical accounting details for advanced products |

Material quantities remain visible without opening a combined inventory panel. A compact name identifies each item; hovering it reveals the full name and unit. A group's heading reveals its complete list. Owned totals include building inventories, physical transit, committed construction/production inputs, fleet cargo and outgoing shipment escrow. They are not a promise that all goods are spendable at the selected building. Energy totals similarly include distinct grids without connecting them.

The worker/status row sits underneath the five groups: active workers/jobs and stored bodies, couriers, alien-pulse timing and objectives. Stored workers belong here, not in manufactured-material totals. The workforce hover retains colony reserve adjustment and **Disassemble surplus for parts**; crossing onto its child buttons must keep that panel open. Observer/neighbor ownership rules remain unchanged, and unreadable colony statistics must not appear through an old hover panel. Alerts appear below the resource area where space permits.

**Confirmed mine direction:** **B, R, M** selects **Extraction Mine**, the single ordinary extraction blueprint. Place it on a valid raw deposit; the selected site's resource fixes output. The player does not select a different resource recipe for that mine. The prototype binds the site to its deposit ID, allows at most one live mine per deposit, and reads per-resource extraction rates from external definitions. The blueprint hover describes the deposit beneath the placement cursor. Paid construction, workers, road-grid power, local storage and physical collection continue to apply.

[ui.json](../../Interface/ui.json) owns the ordered groups, compact item names, categories and shortcuts. The five cards share a compact band, with controls/dossiers below it. Version and FPS remain at the top left in a short row above the resource cards, preserving the earlier explicit requirement. Authored regression coverage checks viewport bounds/scaled hit tests at 1280×720, 1600×900 and 3840×1600, hover-button retention, hidden-state clearing and the single-mine shortcut. Those native checks passed; geometry assertions alone do not prove readable rendered text.

## Current v0.8 command and transport revision

**Retained command behavior:** The population is labelled **workers** throughout the player interface. It remains robotic; stable IDs are unchanged. The bottom dock contains Build, Regions, time controls and Menu. The separate Colony button/menu is removed. Save/load, settings, credits, main menu and exit live in the game menu. **Launch shuttle** is confined to the selected live own command core's building dossier; observer, neighbor, missing/stale selection, other buildings and active placement tools cannot issue it.

**B, L, R** starts Road construction; **B, L, U** upgrades a selected existing road or enters road targeting. Road / Road + rail / Road + rail + vacuum give 2× / 4× / 8× transport speed. The selected-road panel uses simulation data for tier names, speed, length, next-tier material requirements, progress and assigned/on-site workers. Placement, targeting and selection cancel before Escape opens Menu; F10 opens Menu directly and cancels active placement. Starting a new scenario clears road state.

The speed button and plus/minus now cycle **Paused, 1×, 5×, 10×**, with wraparound in either direction. Space resumes the previous nonzero speed, and the stopped label is **Paused**. New scenarios begin at 1×; the automated construction/economy route selects 10× to test normal timed operations. [Workers, Construction and Transport 0.8](CONSTRUCTION_AND_TRANSPORT_0_8.md) owns the mechanics; historical verification below does not verify this revision.

**Retained economy and companion controls:** The construction catalog contains extraction, production, logistics and defense groups, with a compact two-row grid for the current catalog. A selected own trading port exposes its Galactic-credit balance, resource picker, shipment quantity, import/export quote, validation reason, shipment progress and next-level upgrade. Bulk quantities use kg/L; stored workers use whole counts. Each port has a separate worker export-stock target. Goods remain local cargo; import orders reserve credits and exports settle after their physical shipment. Storage displays litres, shipment capacity kilograms and batteries kWh. The current price anchor is the value of one kilogram of gold per Galactic credit, not a redeemable gold inventory.

The former combined economy hover is superseded by the five groups above. A selected building's Power tab still describes its own grid; production, weapon shots and trade draw additional energy per action beyond passive demand.

The selected own core exposes selectable replication and the colony's spare-worker target. The workforce summary also provides target adjustment and **Disassemble surplus for parts**. Recycling uses eligible stored bodies and configured energy (1 kWh by default), preserving active jobs and protected reserves. Target changes request production; they do not create workers or materials immediately.

**B, L, W** opens a wall plan. Click to add, select or move joints, or insert a point on an edge. **E** flips inside/outside, **Backspace** removes the latest joint, **Delete** removes the selected planning joint and **Enter** submits ordinary paid construction. Escape/right click cancels the uncommitted preview. Delete is not a general completed-building demolition command.

Selected own combat-capable buildings expose **Fleets / chassis / hardpoints**, including maximum-level towers and factories. **Request materials/parts** sets a local delivery plan; **Assemble vehicle/Install outfit** pays actual stock and energy. Fleet-level movement, defense, escort, aggression and privateering remain separate from building ownership controls. **Board shuttle** requires all surviving fleet vehicles already at the command service port; it does not issue a return route or teleport them. Escape closes fleet targeting or its panel before opening Menu. Observer mode cannot issue these orders.

The chassis tab applies the selected factory's family and level limits. Its draft remains a request until paid assembly begins; completed vehicles need charging and fleet assignment. The hardpoint tab switches between a selected building and vehicle. A vehicle's **Request parts** uses the selected own factory, so installation requires the vehicle to reach that service location. Local **Escort** follows courier traffic for a selected home building; **Privateer neighbor** requires viewing an occupied neighboring sector. [Fleet command](FLEETS_AND_LOGISTICS.md) records the current mission boundaries.

Select the own command core and choose **Find Rex**, then select **Roam as Rex**. The named companion eats organic food and supplies a local morale bonus under [companions.json](../../Rules/companions.json). In Rex's first-person view, **WASD** walks and the mouse looks; playback is constrained to **1×**. **Escape** restores the colony camera. This is an optional companion view, not worker micromanagement. Menus and pause remain available; loading or starting a scenario exits companion control before replacing the simulation. [Companions and Rex](COMPANIONS_AND_REX.md) owns feeding, morale, save/evacuation behavior and pending visual acceptance.

## Interface references

**Confirmed direction:** Manor Lords is the closer primary interface reference because the game is mainly a settlement-building simulation. StarCraft II is a secondary reference for polish and clarity.

The user wants to improve on aspects of Manor Lords using the perceived polish of StarCraft II. The specific requirements below now refine that direction. These references do not adopt individual-unit combat micromanagement.

## Current layout and construction requirements

**Confirmed user requirements:**

- Show the current game version and FPS at the top left.
- Present Credits, Energy, Raw materials, Basic production and Adv production as separate floating groups, with alerts underneath and per-item explanations on hover.
- Use a **floating bottom construction overlay**, retaining the **B** shortcut. This is the user's latest explicit layout change; the earlier prohibition on a bottom menu no longer applies.
- Represent construction choices with icons and reveal their building names on hover.
- Organize construction logically and provide understandable keyboard shortcuts. Exact category and item keys are implementation choices until verified in play.
- Show every building's weapon damage, reload, DPS, resource information, power, and other relevant statistics, including zero usage and unarmed buildings.
- Every building dossier shows workers used/capacity and physical storage used/capacity; buildings with batteries additionally show stored energy/capacity. Battery capability must not be implied for buildings without it. Grid supply, passive draw and transaction energy are distinct from worker-support capacity.

**Implementation guidance:** Keep the resource-related, logistics, and defense building categories recognizable; a more detailed industrial group can help organize production without changing building capabilities. A sectioned building panel separates Overview, Weapons, Power, Production, Resources, and Maintenance. All sections remain accessible, with pagination where necessary. Use the shared simulation information rather than copying numeric rules into the HUD. The road-connected grid supplies real operating power; disconnected or short-supplied buildings must explain their blocked operation.

Hover and keyboard behavior must preserve ordinary world controls. Interface clicks must not place a building behind a menu or dialog, and menu shortcuts must not simultaneously move the camera. Input handling must work between rendering passes; drawing state cannot be required to process a click.

The displayed version must match the delivered build's version. The v0.3 package passed its 28-stage rendered interaction route, including menus, construction shortcuts, and perspective world clicks. This does not establish exhaustive human usability or completed visual polish.

## Scenario setup — v0.7

**Confirmed and implemented:** Keep **Background bugs** and **Periodic attacks** as two independent ON/OFF controls beneath the 3×3 neighborhood selector. Both default ON. The choices apply before initialization to every occupied colony and to developed AI preparation. The player can choose either source, both, or neither; controls from setup cannot alter a running scenario.

In play, the pulse summary reads **Disabled** when periodic attacks are OFF. Pressure details state the status of both sources. Loading restores the saved choices rather than using whatever is currently selected in setup. Compatible v0.6 saves without these fields default to ON/ON; malformed or inconsistent settings fail without replacing the active game. See [Scenario and AI Setup](SCENARIO_AND_AI_SETUP.md).

All three threat-setting native tests passed in the final 38-test suite (`Saved/Automation/v07-final/index.json`), including real controller/HUD routing without a drawing canvas, all four setups, AI propagation, and save/load. At 1366×768 the proportional layout's largest panel, including four notice lines, fits within 24-pixel top/bottom margins; this is a source-bounds review, not a rendered usability claim. The passing 79-stage Shipping route includes real OFF and ON button clicks; see the [verification record](../verification/v0.7.0.json).

## Main menu and game menu — v0.6

**Confirmed user requirements:** Use a polished main menu over the actual landscaped 3D scene, with restrained typography, clear hierarchy, and consistent navigation. The main menu offers Begin a colony, Load colony, Settings, Credits, and Exit game. Multiplayer is labelled as coming later. Menu title, eyebrow, and tagline are editable in `Interface/ui.json`.

The current title is **SEIGE 2222**. Its heading uses a runtime font rendered at the actual display size. A continuous vertex-gradient scrim replaces the overlapping translucent strips that produced visible vertical seams. Actual 1280×720 and 3840×1600 development and final Shipping captures confirm the sharp title, continuous gradient, and unclipped navigation. Ordinary labels also render at their final pixel size, with matching font measurements for wrapping and deposit-label widths. Final Shipping captures of the main menu, scenario, construction catalog, credits, game menu, and settings show crisp text contained within its panels. The 1600×900 forced interaction capture is layout evidence; its Settings "Native" label does not establish the monitor's native resolution.

The floating gameplay dock and landing survey expose a direct **Menu** button. **F10** opens or closes the game menu directly, including from the construction catalog or a game-menu subpage. **Escape** first cancels a blueprint, backs out of a construction category or overlay, or clears selection; with those dismissed, it toggles the game menu. Right click cancels/backtracks local placement and overlays without opening the game menu. Escape from Settings or Credits returns to the menu that opened that page.

The game menu offers Resume, Save colony, Load colony, Settings, Credits, Return to main menu, and Exit game. Saving is unavailable before landing. Opening the game menu pauses both the local colony and its independently simulated neighbors. Closing it restores the earlier paused/running state and selected speed. Its Settings and Credits pages retain that pause. Saving from the game menu records the pause state from before the menu opened; loading restores the saved state and closes the menu.

Main menu, scenario setup, and human landing survey also suspend the local simulation. The construction overlay continues to run time; the separate Colony overlay has been removed. These are local prototype controls; they do not define time control on a persistent server.

## Display and local speed — v0.6

**Confirmed user requirements and current implementation:**

- A normal first launch uses **borderless**, filling the monitor at its native resolution. Existing exclusive-fullscreen preferences are converted to borderless; the interface provides no exclusive-fullscreen mode.
- **Windowed** is selectable. Its resolution selector offers 1280×720, 1600×900, 1920×1080, 2560×1440, and 3840×2160 when they fit the desktop. The window choice is retained separately from the borderless monitor size.
- **3D render resolution** is independent of window/display resolution. It defaults to 100%, has 50–100% controls in ten-percentage-point steps, and shows the effective width and height in pixels. Menus and text remain at full display resolution.
- **Medium** is the sole graphics quality label. It is a custom, externally configured preset in `Graphics/scene.json`, with selected lighting, landscape, texture, and antialiasing settings. It is not an exposed choice among Unreal's generic Low/High/Epic presets. Applying a display or render-resolution change preserves this custom profile.
- **v0.7 antialiasing:** The current Medium preset uses TAA at 100% render resolution and TSR below 100%. Changing render resolution selects the corresponding method automatically. Shadows remain at the custom profile's level 2; no additional quality selector is introduced. Native regression covers 100% → 75% → 100% and preserves explicit diagnostic overrides. Actual Shipping display checks passed native borderless, windowed 100%, windowed 75%, and restored native borderless, with engine AA methods **2 / 2 / 4 / 2** (TAA / TAA / TSR / TAA) and zero failures; see the [verification record](../verification/v0.7.0.json).
- Current local playback cycles **Paused, 1×, 5×, 10×**. The speed button and **+** cycle forward; **−** cycles backward with wraparound. Main-keyboard and numeric-keypad variants work. **Space** resumes the previous nonzero speed. The nonzero list is stored in `Interface/ui.json`; current format-6 saves reject unsupported 3× playback rather than migrating it. Rex's first-person roaming locks playback to 1×.

Normal display preferences persist locally. Automated `-UiSmoke`, `-GraphicsBenchmark`, and `-ForceRes` runs leave the player's display preferences untouched and retain their explicitly requested capture size. The separate `-DisplaySmoke -NoSaveDisplay` route applies actual borderless/windowed and render-resolution changes, but the `NoSaveDisplay` guard suppresses preference saves and display config writes. It is a verification path, not a player-facing option.

**Historical v0.6 development and final Shipping display routes:** On a 3840×1600 monitor using Windows 200% display scaling, both routes passed all four actual viewport checks with zero failures:

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
| + / − | Cycle Paused, 1×, 5×, 10× forward/backward with wraparound |
| F5 / F9 | Save/load a running colony; also available through the game menu |
| B | Open construction; category and blueprint shortcuts follow |
| B, R, M | Extraction Mine; deposit determines its output |
| B, L, R / B, L, U | Road construction / existing-road upgrade |
| Selected own command core | Production, spare-worker target, Find Rex and manual Launch shuttle |
| Workforce summary | Spare-worker target and disassemble eligible stored surplus |
| Selected trading port | Import/export and independent worker export-stock target |
| B, L, W | Edit a wall plan; E flips inside, Enter commits, Backspace/Delete edit joints |
| Selected own combat platform/factory | Fleet, chassis and hardpoint controls; request parts before paid assembly/refit |
| WASD / arrows | Pan relative to camera yaw |
| Q / E; middle drag | Rotate; rotate and tilt |
| Wheel / Home | Zoom; return to the command core view |

Native regression coverage includes direct menu access, pausing center and neighbor simulations, returning from Settings/Credits, saving the pre-menu pause state, loading from the menu, pre-landing resume and F9 load, speed keys, restoring the main-menu backdrop, and rejection of stale blueprint actions through the menu. The final v0.6 headless run passed all **34 tests cleanly**, with **zero warnings, failures, or unrun tests**. This supersedes the earlier run containing an engine HTTP connectivity-probe warning. The report is `Saved/Automation/v06-final`. The final Shipping interaction route completed all **79 stages with zero failures**, including game-menu pause/restoration, settings, speeds, map transitions, and service activity; its report also counted 831 courier-motion frames between simulation ticks at 1×. Packaged captures and the report are under `%LOCALAPPDATA%/seige2222/Saved`. The separate Shipping native-display check also passed all four states, as recorded above.

## Perspective camera revision

**Confirmed v0.3 direction:** The world uses a rotatable, tiltable perspective camera. Panning follows the camera's yaw; clicks and construction previews must intersect the visible terrain in front of the camera. Camera motion must not turn a UI action into a world command or accept a point behind the camera.

Delivered v0.3 bindings use Q/E to rotate, middle-mouse drag to rotate/tilt, WASD/arrows to pan, the wheel to zoom, and Home to return to the core view. Saved camera yaw and pitch are added without requiring them in older format-2 saves. Native camera tests and the packaged rendered route pass; save-format compatibility was tested natively, without a separate packaged save/load roundtrip. See [Graphics Milestone 0.3](GRAPHICS_MILESTONE_0_3.md).

**Retained v0.4 request:** The middle/third mouse button should orbit responsively like the reference; the prior sensitivity was too low. The implementation captures pointer motion, uses editable sensitivity, and restores the pointer on release. Current configured limits are 8–80° pitch, minimum zoom 120 logical units, and 160 cm camera-to-ground clearance. At close zoom, the view smoothly lowers while retaining the user's orbit angle for zoom-out. Native tests cover clearance and angle restoration; final feel, gesture behavior, and appearance remain under rendered review.

Zooming out should lead to a less detailed regional map. Zooming/focusing into a sector should reveal that area's detailed environment. The current source uses a 3×3 cartographic survey with selectable sectors; it suppresses world construction while on the map, and human deployment remains limited to the home sector. This is a viewing transition, not cross-sector movement or colony ownership. See [Graphics and Interface Milestone 0.4](GRAPHICS_MILESTONE_0_4.md).

**v0.6 revision:** Wheel zoom, Home, the Regions button, and player-triggered sector focus ease toward their requested zoom. The cartographic overlay fades in across the configured transition band rather than replacing the world abruptly; a whole-sector survey remains a 3D view below that band. Input and map visibility use the displayed zoom, and projected world labels follow the actual camera. Scenario initialization, loaded-camera restoration, and direct setup/test calls can establish a view immediately; the `FocusSector` API uses its smooth-transition option for UI and controller actions. Maximum zoom and transition limits remain editable graphics parameters. Regional viewing continues to respect ownership and sensor boundaries.

**v0.7 scenery continuity:** Focusing a neighboring sector retains the same deterministic tree population. Distance selects detailed or simpler geometry at those positions; focusing does not replace a sparse placeholder forest with different trees. This changes presentation only and does not reveal hidden colony state or expand player control. Implementation, visual checks and performance evidence are tracked in [Graphics Performance 0.7](GRAPHICS_PERFORMANCE_0_7.md).

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

**Current prototype aggression choices:** Passive, defensive and aggressive. Passive units can receive movement orders but hold fire; defensive guards engage nearby known threats and may reposition for a clear shot; aggressive units can pursue known contacts. Exact targeting and long-term balance remain provisional.

**Current boundary and open design:** An explicit group order replaces the fleet's previous mission, and local saves retain orders, routes, ownership and cargo. More developed retreat/disengagement behavior, persistent multiplayer orders and the presentation of mission risks remain open.

## Information boundaries

The interface must respect finite sensor coverage: a sector map is not a live view of all activity. The world includes remote outposts and distributed sensors that may be stolen. Exact last-known information and terrain-memory behavior remain open.

Observer mode may inspect AI colonies, but it must not issue building commands. In human play, neighbor views must not expose hidden live colony statistics or let a neighbor's local entity ID operate on a home building. Configured occupancy can appear on the regional survey without revealing current enemy positions. Ground and vegetation changes must follow the same visibility boundary as building models.

## Related documents

- [Fleets, Physical Logistics, and Loot](FLEETS_AND_LOGISTICS.md)
- [Aliens, Combat, and Raiding](COMBAT_AND_RAIDING.md)
- [World, Visibility, and Relocation](WORLD_AND_RELOCATION.md)
- [Art Direction](ART_DIRECTION.md)

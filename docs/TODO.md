# seige2222 — recommended to-do list

Compiled 2026-10-08 from a review of the source, rules, art pipeline, tests and design documents; updated 2026-10-09 after the v0.9.2 kit, dependency graph, rules 9.2 and the verification runs. Ordered by expected value to a player, then by cost. Each item says what is wrong today and what "done" looks like; none of it is scheduled or promised.

## 1. Release verification (blocks everything else being called "done")

- **v0.9.2 has run the full gate three times on 2026-10-09** (`docs/verification/v0.9.2.json` holds the latest record): validators pass, the full native suite runs, the Shipping package builds and the packaged 117-stage route completes. Native failures went from 4 to 2 (`Seige.Workers.SpareOperatorsReachPaidConstruction` fixed in the scheduler, `Workforce.ReplicatorAndFactory` by its fixture), and the packaged route's assertion failures from 13 to 2 (core sensor coverage no longer depends on staffing; the two left were the stage-11 shuttle selection at a low tilt, fixed in the smoke route) to **0** in the third run. The launcher still opens v0.8.1 until the native failures below are closed. Both are simulation/AI tests that were already failing in the last full v0.9 run (`Saved/Automation/v09-full-25`, 2026-10-06); none touches rendering or the HUD. Each now prints a diagnostic so the next run says *why*:
  - `Seige.Simulation.FirstPlayableSolvable` — the colony AI survives 36 waves but never builds the robotic-parts works. Diagnostics from `v092-fix-1`: wave 19 (t≈34,300 s) destroys the service bay and the fuel generator north and west of the core with **a single enemy** on site, wave 24 takes the sensor, the rebuilt bay, the worker factory, the port and the solar array, wave 25 the mine; population falls 23 → 16 and the colony ends in "Waiting for materials to restore worker support". Three laser towers stand south and east; nothing covers the north and west approaches, and the core's lasers cannot shoot through its own buildings (`ClearFriendlyFire` is a designed, tested rule). Done: either the AI places its perimeter towers around every approach before industry (and rebuilds lost support first), or the owner changes the wave/roamer balance; then the test passes on the shipped rules.
  - `Seige.AI.WorkerSupportRecovery` — the bay was complete and staffed but unpowered because the AI connected the two sensor roads first; `NextPowerRoad` now wires service bays and generators before sensors and factories (f405a12); the third gate then showed the bay placed across the colony from the roads with its connection 12% short at the endpoint, so equal-coverage plots now prefer the shortest new road (b9abdc3, targeted run pending).
- **Save compatibility is byte-exact on every rules file** (`RulesFingerprint` = MD5 of the raw text of all 12 files). Rules 9.2 invalidated all 9.0 saves, accepted by the owner. Decide whether that stays policy for a shipped build; if not, fingerprint a canonical JSON serialisation and add a migration table.

## 2. Progression legibility (the "tech tree" that fits the design decisions)

The design documents decide against research trees, timers and currencies ("material access replaces research"); progression is building and upgrade access.

- **Build cards show readiness** (short materials / ready / no feedstock yet), the blueprint panel lists outputs and the upgrade step, and the **Production chain** panel (P) draws the real building–resource dependency tree with live states, hover-lit chains and click-to-place (v0.9.2). Remaining polish: long edges (construction alloys feed almost everything) still cross the middle of the graph — route them along the top/bottom margin or collapse them behind a "common materials" port; a "recommended next" line from the first unmet input of the first unbuilt capability; a keyboard focus for the panel.
- **Rules 9.2 added the first material gates**: 22 of 28 blueprints are affordable from the landing kit on day one (was 26); the AI-chip works, fusion works, mech factory, tracked-vehicle factory, plasma tower and battery works need circuits, batteries, plastic or AI chips the kit does not hold, every level-3 upgrade needs AI chips and the vehicle-factory level-2 upgrades need batteries ([economy baseline](game-design/ECONOMY_BASELINE_0_9.md)). Whether the arc feels right is a playtest question; the gates are provisional.
- The build palette fits 14 cards per category (seven narrower columns above 12 entries); a 15th still overflows.
- `category` in `Rules/buildings.json` is parsed and unused; the HUD groups come from `Interface/ui.json`. Remove one or make them agree.

## 3. Building art and construction presentation

- **v0.9.2 kit (in-engine reviewed):** 24 meshes — solar arrays L1–3, battery bank, trading ports L1–3, alloy refinery, fuel/biofuel refinery, greenhouse, clean works, chip fab, fusion works, ammunition works, fuel generator, three hangar kinds, four tower bases (laser, kinetic, missile, plasma; deck at the 250 cm mount height), depot yard and worker factory — mapped through `Graphics/building_visuals.json`. The sensor mast, service bay, extractor and command shuttle keep their earlier dedicated meshes. Towers still do not grow with level (same base, more mounts); ports grow by crane and container stacks only.
- **Industry surfaces v3 (2026-10-09):** the three palette surfaces (painted cladding, brushed steel, formed concrete) now carry 80 cm panels, bolts, welds, tie holes, grime, drips and scratches at 1024 px per 160 cm tile with 0.85 normal strength (`Tools/create_industry_textures_v092.py`; capture `Art/BuildingKitV092/Previews/textures_v092_showcase.jpg`). At the player zoom the gain is panel structure and a less uniform white; close-ups still lack decals, edge wear and ambient occlusion. Next steps in order of visible return: a per-building tint variation so identical kinds do not match exactly, world-aligned grime on the lower metre of every wall, and emissive windows at night.
- The weapon-mount height stays a constant in `SeigeCombatVisuals.cpp`; the tower bases were authored to it rather than the other way round. Make it part of the visual definition if a tower ever needs a different deck height.
- `Art/ASSET_REGISTER.md` and `Art/REALISTIC_ASSET_REGISTER.md` are marked historical and `Art/Construction/README.md` states that `SyncServiceVisuals` is an empty stub; the stub itself (service-bay robots) is still unimplemented.
- Residual terrain patterns, bare grass strips on slopes, terrain clipping through the edge of large plots and the 1.8 M-triangle near tree remain the largest scene-quality limits (v0.9.1 report).

## 4. Rex

- Smooth coat (no strand fur), a harsher face-to-body transition than the photographs, mirrored far side, eyes and nose only in the bake. Cheapest visible gains: a hand-painted mask that limits the pale face to the muzzle and brow, and a second photograph projected from the other side instead of mirroring.
- The companion walks through the sward; grass is not flattened and the dog gets no shadowed contact. Low priority at the player zoom.
- Licence: the base mesh came from Hunyuan3D-2.1, whose Community License excludes South Korea from its territory; the owner must decide whether to keep it or regenerate with an MIT-licensed model (TripoSG, TRELLIS) locally. Recorded in `Art/CompanionDog/TEXTURE_PROVENANCE.md`.

## 5. Economy and rules consistency

- Done 2026-10-09 (rules 9.2): `make_fusion_reactors` consumes radioactive ore; "structural 6 m" wall text; cumulative level-2/3 bills include the new gate materials; the native rules check verifies catalogue reachability without the trade shortcut; `Tools/extend_colony_catalog.mjs` deleted; storage figures in `CONSTRUCTION_AND_TRANSPORT_0_8.md` corrected.
- Open: the level-2/3 `cost` fields are documentation of the cumulative bill and are not what an upgrade charges (`upgrade_cost` is); either derive them in the loader or drop them from the rules so they cannot drift again.

## 6. AI

- The colony AI never upgrades buildings and never builds the advanced chain (`colony_ai.json` lists 21 targets, none of them AI-chip or fusion works, so under rules 9.2 it can never afford a level-3 upgrade). With the readiness information it could at least upgrade solar and ports when the bill is in stock, and it should add the AI-chip works once circuits flow. Done: `Seige.Simulation.FirstPlayableSolvable` still passes and the developed neighbours show level-2 structures.
- Perimeter coverage (item 1): towers are placed at `defense_distance` 1100 on four compass bearings from the export deposit, which leaves whole approaches open when fewer than four are built; losses in the first-playable run came through those gaps and from single roamers that no fixed gun could reach.

## 7. Tooling

- Cloud sessions can reach github.com but not GitHub's LFS object store, so the dog binaries moved to plain Git objects (commit 5aed6e9) and every later binary (kit meshes, portraits, textures, material instances) is a plain object too. Either accept the history growth or push LFS objects from the PC with a credential helper installed.
- The PC job runner is a double-clicked batch file; a persistent runner was rejected by policy. Keep jobs small and logged under `Saved/claude-jobs`.
- `Tools/record_verification.py` writes `docs/verification/v<version>.json` from the run artefacts; its exit code is the release gate (`locally_verified` only when every part passes).

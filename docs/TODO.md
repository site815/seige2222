# seige2222 — recommended to-do list

Compiled 2026-10-08 from a review of the source, rules, art pipeline, tests and design documents after the v0.9.1 graphics work, Rex revision 7 and the v0.9.2 building kit. Ordered by expected value to a player, then by cost. Each item says what is wrong today and what "done" looks like; none of it is scheduled or promised.

## 1. Release verification of the current source (blocks everything else being called "done")

- **v0.9 / v0.9.1 / v0.9.2 are not release-verified.** The launcher still opens v0.8.1. The remaining native test groups, the 117-stage packaged gameplay route, display states and offline observation have not been rerun on the current source ([development report](DEVELOPMENT_REPORT.md)). Done: `Tools/build.ps1 -Package` plus the full native suite and `Tools/verify_packaged.ps1` pass on one commit, recorded in `docs/verification/v0.9.2.json`.
- **`SeigeFrontendTests.cpp:196` asserts neighborhood metadata format 6 while `SeigePersistence.cpp:27` writes format 7.** Either the test or the writer is stale; it will fail when the frontend group runs.
- **Save compatibility is byte-exact on every rules file** (`RulesFingerprint` = MD5 of the raw text of all 12 files). Any whitespace edit invalidates all saves and there is no migration. Decide whether that stays policy for a shipped build; if not, fingerprint a canonical JSON serialisation and add a migration table.

## 2. Progression legibility (the "tech tree" that fits the design decisions)

The design documents decide against research trees, timers and currencies ("material access replaces research"); progression is building and upgrade access. Today that progression is invisible:

- **Build cards show readiness** (short materials / ready / no feedstock yet), the blueprint panel lists outputs and the upgrade step, and the **Production chain** panel (P) lists deposits, processing, advanced/defence and levels with live states (v0.9.2). Missing from that panel: the links between rows (which processor feeds which), a "recommended next" line derived from the first unmet input of the first unbuilt capability, and opening the build card from a row. Done: one screen that answers "what should I build next and why" without reading four columns.
- **26 of 28 blueprints are affordable from the landing kit on day one**, including the AI-chip works, fusion works and vehicle factories (`validate_rules.mjs` bootstrap check), so the only gates are two bills (battery bank, mech factory). If the intent is a real early/mid/late arc, make the starter kit smaller or make advanced bills require products the kit does not contain; this is a rules change and therefore a save-format event.
- **The Production category is full**: the 6×2 card grid overflows at a 13th entry (`SeigeHUD.cpp` card layout). Needed before any new blueprint.
- `category` in `Rules/buildings.json` is parsed and unused; the HUD groups come from `Interface/ui.json`. Remove one or make them agree.

## 3. Building art and construction presentation

- **v0.9.2 kit (done, pending in-engine review):** solar arrays L1–3, battery bank, trading ports L1–3, alloy refinery, fuel/biofuel refinery, greenhouse, clean works, chip fab, fusion works, ammunition works, fuel generator and three hangar kinds, mapped through `Graphics/building_visuals.json` without touching Rules. Remaining shared meshes: **worker_factory** (factory hall), **depot** (warehouse), **sensor**, **robot service bay**, and **all 12 tower definitions**, which share one turret base and differ only by procedural weapon mounts. Done: a base per weapon family (laser mast, kinetic bunker, missile box, plasma coils) and visible level growth for towers and ports.
- **Weapon mounts sit at a fixed 250 cm** (`SeigeCombatVisuals.cpp:126-128`) regardless of mesh scale, so they float or sink on bases of other heights. Make the mount height part of the visual definition.
- **Build-card portraits** are rendered per mesh kind; the 12 Production cards no longer share one image, but Logistics still shows the warehouse for the depot and the same portrait for every tower. Covered by the tower item above.
- `Art/ASSET_REGISTER.md` and `Art/REALISTIC_ASSET_REGISTER.md` describe superseded meshes; `Art/Construction/README.md` describes service-bay robots that `SyncServiceVisuals` (an empty stub) never draws. Rewrite or delete.
- Residual terrain patterns, bare grass strips on slopes and the 1.8 M-triangle near tree remain the largest scene-quality limits (v0.9.1 report).

## 4. Rex

- Smooth coat (no strand fur), a harsher face-to-body transition than the photographs, mirrored far side, eyes and nose only in the bake. Cheapest visible gains: a hand-painted mask that limits the pale face to the muzzle and brow, and a second photograph projected from the other side instead of mirroring.
- The companion walks through the sward; grass is not flattened and the dog gets no shadowed contact. Low priority at the player zoom.
- Licence: the base mesh came from Hunyuan3D-2.1, whose Community License excludes South Korea from its territory; the owner must decide whether to keep it or regenerate with an MIT-licensed model (TripoSG, TRELLIS) locally. Recorded in `Art/CompanionDog/TEXTURE_PROVENANCE.md`.

## 5. Economy and rules consistency

- `radioactive_ore` is extractable and tradable but no recipe consumes it. Either a reactor-fuel recipe or remove the deposit class.
- The C++ renewable-path check (`SeigeSimulation.cpp:290-308`) is vacuous once any deposit is mineable; only the JavaScript validator's version is meaningful. Make the native check equivalent or delete it so it does not imply coverage.
- `Tools/extend_colony_catalog.mjs` writes v0.8 values into the live Rules, `ui.json` and `colony_ai.json`; running it would overwrite the v0.9 balance. Delete it or make it read the current data.
- `CONSTRUCTION_AND_TRANSPORT_0_8.md` storage figures are stale against the JSON (2k/6k/40k L vs 5k/12k/50k L).
- Typo "structural6m" in the wall descriptions in `Rules/buildings.json` (a save-format event when fixed; batch it with the next rules change).

## 6. AI

- The colony AI never upgrades buildings and never builds the advanced chain (`colony_ai.json` lists 16 definitions). With the new readiness information it could at least upgrade solar and ports when the bill is in stock. Done: `Seige.Simulation.FirstPlayableSolvable` still passes and the developed neighbours show level-2 structures.

## 7. Tooling

- Cloud sessions can reach github.com but not GitHub's LFS object store, so the dog binaries moved to plain Git objects (commit 5aed6e9). Either accept ~25 MB of history per dog revision or push LFS objects from the PC with a credential helper installed.
- The PC job runner is a double-clicked batch file; a persistent runner was rejected by policy. Keep jobs small and logged under `Saved/claude-jobs`.

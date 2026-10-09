# seige2222 — recommended to-do list

Compiled 2026-10-08 from a review of the source, rules, art pipeline, tests and design documents; revised 2026-10-09 (evening) after v0.9.2 passed the full local gate, the building kit was completed and the sector-crossing work. Open items are ordered by expected value to a player, then by cost. Each says what is wrong today and what "done" looks like; none of it is scheduled or promised.

## Closed on 2026-10-09

- **Release gate.** v0.9.2 is `locally_verified` ([record](verification/v0.9.2.json), job 48, source 56b9036): both validators, the full `Seige` native suite **154 / 154**, the Shipping package and the packaged 117-stage route with **0** failures. `Play-seige2222.cmd` opens `Builds/v0.9.2`. The two native failures of the previous record are fixed:
  - `Seige.Simulation.FirstPlayableSolvable`: the AI plan (`AIFILES/colony_ai.json`) now builds the robotic-parts works right after the first tower, before the mine, port and generator. `Diagnostics.FirstPlayableTimeline` (outside the gate) shows the scenario won at t = 18,770 s with 12 / 12 robotic parts.
  - `Seige.AI.WorkerSupportRecovery`: equal-coverage plots prefer the shortest new road, so the recovered service bay gets power.
- **Save compatibility.** Rules prototype-9.3 fingerprint the parsed JSON content, so whitespace, line endings and number spelling no longer invalidate a save; values, keys and key order still do. Breaking saves on a rules change remains the owner's accepted policy.
- **Rules hygiene.** `category` is gone from `Rules/buildings.json`; the cumulative level-2/3 `cost` is checked against the previous level's bill plus `upgrade_cost` both natively and in `Tools/validate_rules.mjs`, so the two can no longer drift silently.
- **Progression legibility.** The production-chain panel collapses the most common inputs into ports, prints a "Next:" line (the first unmet input of the first unbuilt capability) and supports the arrow keys and Enter. The build palette wraps into as many rows as a category needs.
- **Building art.** Every blueprint uses an original kit mesh ([kit README](../Art/BuildingKitV092/README.md)): towers, trading ports, solar arrays and vehicle factories grow by level; the five clean-industry works and the biofuel refinery no longer share a hall; the service bay, sensor mast and extraction rig were replaced; the service bay draws its real stored worker bodies on its berths (`SyncServiceVisuals`). The industry material adds per-building tint variation, soil grime on the lowest 1.6 m, seasonal snow on upward faces and lit windows at night.
- **AI.** The colony AI upgrades solar arrays, trading ports and laser towers when it can afford the bill above its reserves and no enemy is within 180 m of the core (`defense_quiet_radius` 3000 units), one upgrade at a time, and builds the AI-chip works as an optional late target.

## 1. Verification of the latest source

- The per-process works and vehicle-factory meshes, the split sector crossing and the forest-build change (commit 18e0de6) are being re-verified by job 49; the wall sections and the biofuel refinery after it need one more full gate. Done: `docs/verification/v0.9.2.json` written from a run of the final source with `status: locally_verified`.

## 2. Performance

- **Sector crossing.** Crossing into a neighbouring sector produced one frame of 236 ms in Shipping (job 47/48: terrain 68 ms, forest 133 ms for 113k trees, then ground cover). Since 18e0de6 the sector being left is rebuilt on the following frames and the forest no longer formats a key per tree; the measured effect is pending (job 49). Remaining options, in order: precompute the next sector's heights and chunk data on a worker thread while the camera approaches a boundary; spread the entered sector's 64 chunk uploads over frames with a per-chunk coarse/detail swap.
- **GPU at native 3840×1600.** Meadow and ground views run at about 52 FPS (18 ms GPU), spread across base pass, shadow depths, Nanite visibility, shadow projection and lighting; eight render-setting variants changed nothing beyond session drift ([report](game-design/GRAPHICS_PERFORMANCE_0_9_2.md)). The remaining levers are content: card overdraw in the meadow (attribution run pending in job 49), the 1.8 M-triangle masked near tree, shadow-casting density — or the existing 50–100% render-resolution setting.

## 3. Scene and building art

- Residual terrain patterns, bare grass strips on slopes, terrain clipping through the edge of large plots and the single near tree species remain the largest scene-quality limits.
- The command core keeps the v0.9 orbital shuttle and campus meshes; it has its own review coverage and was not part of the kit.
- The weapon-mount height stays a constant in `SeigeCombatVisuals.cpp`; the tower bases are authored to it. Make it part of the visual definition if a tower ever needs a different deck height.
- Close-ups still lack decals, edge wear and ambient occlusion on the kit; at the player zoom the gain from further surface work is small.

## 4. Rex

- Smooth coat (no strand fur), a harsher face-to-body transition than the photographs, mirrored far side, eyes and nose only in the bake. Cheapest visible gains: a hand-painted mask that limits the pale face to the muzzle and brow, and a second photograph projected from the other side instead of mirroring.
- The companion walks through the sward; grass is not flattened and the dog gets no shadowed contact. Low priority at the player zoom.
- Licence: the base mesh came from Hunyuan3D-2.1, whose Community License excludes South Korea from its territory; the owner must decide whether to keep it or regenerate with an MIT-licensed model (TripoSG, TRELLIS) locally. Recorded in `Art/CompanionDog/TEXTURE_PROVENANCE.md`.

## 5. AI and balance

- **Perimeter coverage.** Towers are placed at `defense_distance` on four compass bearings from the export deposit. The first-playable run now wins, but the earlier loss analysis (single roamers destroying the bay and generator on the uncovered north and west approaches) still describes how the AI defends; covering approaches by where buildings actually are would be the robust fix.
- **Gates.** The rules 9.2 material gates (22 of 28 blueprints affordable on day one; AI chips for every level-3 upgrade) are provisional and untested by play.

## 6. Tooling

- Cloud sessions can reach github.com but not GitHub's LFS object store, so every binary since commit 5aed6e9 (kit meshes, portraits, textures, material instances) is a plain Git object. Either accept the history growth or push LFS objects from the PC with a credential helper installed.
- The PC job runner is a double-clicked batch file; a persistent runner was rejected by policy. Keep jobs small and logged under `Saved/claude-jobs`.
- `Tools/record_verification.py` writes `docs/verification/v<version>.json` from the run artefacts; its exit code is the release gate (`locally_verified` only when every part passes). The record does not yet store the source commit; add it.

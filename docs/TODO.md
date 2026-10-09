# seige2222 — recommended to-do list

Compiled 2026-10-08 from a review of the source, rules, art pipeline, tests and design documents; revised 2026-10-09 (night) after v0.9.2 passed the full local gate three more times (jobs 48–50), the building kit was completed and the sector-crossing work. Open items are ordered by expected value to a player, then by cost. Each says what is wrong today and what "done" looks like; none of it is scheduled or promised.

## Closed on 2026-10-09

- **Release gate.** v0.9.2 is `locally_verified` ([record](verification/v0.9.2.json), job 48, source 56b9036): both validators, the full `Seige` native suite **154 / 154**, the Shipping package and the packaged 117-stage route with **0** failures. `Play-seige2222.cmd` opens `Builds/v0.9.2`. The two native failures of the previous record are fixed:
  - `Seige.Simulation.FirstPlayableSolvable`: the AI plan (`AIFILES/colony_ai.json`) now builds the robotic-parts works right after the first tower, before the mine, port and generator. `Diagnostics.FirstPlayableTimeline` (outside the gate) shows the scenario won at t = 18,770 s with 12 / 12 robotic parts.
  - `Seige.AI.WorkerSupportRecovery`: equal-coverage plots prefer the shortest new road, so the recovered service bay gets power.
- **Save compatibility.** Rules prototype-9.3 fingerprint the parsed JSON content, so whitespace, line endings and number spelling no longer invalidate a save; values, keys and key order still do. Breaking saves on a rules change remains the owner's accepted policy.
- **Rules hygiene.** `category` is gone from `Rules/buildings.json`; the cumulative level-2/3 `cost` is checked against the previous level's bill plus `upgrade_cost` both natively and in `Tools/validate_rules.mjs`, so the two can no longer drift silently.
- **Progression legibility.** The production-chain panel collapses the most common inputs into ports, prints a "Next:" line (the first unmet input of the first unbuilt capability) and supports the arrow keys and Enter. The build palette wraps into as many rows as a category needs.
- **Building art.** Every blueprint except the command core uses an original kit mesh ([kit README](../Art/BuildingKitV092/README.md)): towers, trading ports, solar arrays and vehicle factories grow by level; the five clean-industry works and the biofuel refinery no longer share a hall; the service bay, sensor mast and extraction rig were replaced; walls are kit sections at three levels instead of four cubes; the service bay draws its real stored worker bodies on its berths (`SyncServiceVisuals`). The industry material adds per-building tint variation, soil grime on the lowest 1.6 m, seasonal snow on upward faces and lit windows at night.
- **AI.** The colony AI upgrades solar arrays, trading ports and laser towers when it can afford the bill above its reserves and no enemy is within 180 m of the core (`defense_quiet_radius` 3000 units), one upgrade at a time, and builds the AI-chip works as an optional late target.

## 1. Verification of the latest source

- Source 4258df9 (wall sections, biofuel refinery, five-frame crossing) passed the full gate in job 50: 154 / 154 native, route 117 / 0, record `locally_verified` with the source commit. The next commit — staggered chunk upload, command campus meshes, exposure-driven tower bearings, wall tree clearance — is in job 51. Done: the record of the final source says `locally_verified`.

## 2. Performance

- **Sector crossing.** The longest frame when the camera crosses into a neighbouring sector fell from 417 ms (v0.9.2 baseline) to 236 ms (parallel preparation) to 117 ms (job 49) to **86 ms** in Shipping (job 50, five frames) — but the boundary p99 rose from 20 to 39 ms because several frames now carry 25–40 ms each. Job 51 splits the largest step (the 64-chunk upload, 41 ms in the editor) into four frames of 16 chunks behind a sectioned coarse stand-in and moves chunk preparation off the crossing frame. Remaining options: precompute heights and chunks on a worker thread before the camera reaches the boundary; shorten the entered sector's forest step (24 ms in the editor).
- **GPU at native 3840×1600.** Meadow and ground views run at about 51 FPS (19 ms GPU), spread across base pass, shadow depths, Nanite visibility, shadow projection and lighting; eight render-setting variants changed nothing beyond session drift ([report](game-design/GRAPHICS_PERFORMANCE_0_9_2.md)). Hiding the meadow grass recovers only 2.2–2.5 ms (job 49), so a grass LOD would buy about 1 ms; the rest is terrain, trees, shadows, lighting and post at 6.1 megapixels. The large lever is the existing 50–100% render-resolution setting; content levers are the 1.8 M-triangle masked near tree and shadow-casting density.

## 3. Scene and building art

- Residual terrain patterns, bare grass strips on slopes, terrain clipping through the edge of large plots and the single near tree species remain the largest scene-quality limits.
- The command core's levels 2–3 now have kit campus meshes around the retained level-1 shuttle (`SM_CommandCampus2/3`, replacing the single scaled v0.9 `SM_Core`); `-BenchmarkLevel=2|3 -BenchmarkFocus=command_core_2|3` reviews them. In-engine captures pending in job 51.
- Wall sections now clear trees along their whole length (forest, ground cover and the build-time clearance); the k50 outside-face capture showed canopies over most sections because only the midpoint was cleared. Capture pending in job 51.
- Close-ups still lack decals, edge wear and ambient occlusion on the kit; at the player zoom the gain from further surface work is small.

## 4. Rex

- Smooth coat (no strand fur), a harsher face-to-body transition than the photographs, mirrored far side, eyes and nose only in the bake. Cheapest visible gains: a hand-painted mask that limits the pale face to the muzzle and brow, and a second photograph projected from the other side instead of mirroring.
- The companion walks through the sward; grass is not flattened and the dog gets no shadowed contact. Low priority at the player zoom.
- Licence: the base mesh came from Hunyuan3D-2.1, whose Community License excludes South Korea from its territory; the owner must decide whether to keep it or regenerate with an MIT-licensed model (TripoSG, TRELLIS) locally. Recorded in `Art/CompanionDog/TEXTURE_PROVENANCE.md`.

## 5. AI and balance

- **Perimeter coverage.** Towers stay at `defense_distance` from the core, but each armed plot now takes the ring bearing that reaches the most building approaches no fixed gun (finished or under construction) can hit with a clear line, counting the unoccupied export deposit as a future building; ties keep the authored spread (`FSeigeScenarioAI::DefenseBearing`, regression in `Seige.AI.CoveredCivilianPlacement`). This targets the earlier loss (single roamers destroying the bay and generator north and west of the core, where the core's lasers cannot shoot through its own buildings). Whether `Seige.Simulation.FirstPlayableSolvable` still wins, and how the timeline changes, is measured in job 51.
- **Gates.** The rules 9.2 material gates (22 of 28 blueprints affordable on day one; AI chips for every level-3 upgrade) are provisional and untested by play.

## 6. Tooling

- Cloud sessions can reach github.com but not GitHub's LFS object store, so every binary since commit 5aed6e9 (kit meshes, portraits, textures, material instances) is a plain Git object. Either accept the history growth or push LFS objects from the PC with a credential helper installed.
- The PC job runner is a double-clicked batch file; a persistent runner was rejected by policy. Keep jobs small and logged under `Saved/claude-jobs`.
- `Tools/record_verification.py` writes `docs/verification/v<version>.json` from the run artefacts; its exit code is the release gate (`locally_verified` only when every part passes). Since 4258df9 it also stores the source commit and the tracked files that differed from it (the job's regenerated assets).

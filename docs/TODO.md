# seige2222 — remaining work

Updated **2026-10-11**: **v0.10.1 local build complete; 95 packaged checks passed at 10×**.

- Detailed Rex and photo coat restored; lower-neck contours smoothed. The temporary licence exception is explicit in Credits/provenance. Remaining art work: fused inner-leg geometry, coat transition, and close-angle surface detail. Commercial/public asset clearance remains unresolved.
- Completed: quiet UI/gameplay audio, integrated bottom-dock timer, brighter nights, asset import/rig validation and 95 packaged assertions. Five UI renders and two Rex views reviewed.
- Completed: Shipping package, verified ZIP/checksum, updated launcher and [v0.10.1 evidence](verification/v0.10.1.json). GitHub's account-suspension restriction blocks remote delivery as last checked this session.

## Previous v0.10.0 checkpoint

Updated **2026-10-11**: **v0.10.0 integrated and locally packaged; focused checks passed**. The user requested focused testing for the next update. The old 117-stage route and broad benchmark loop are not current release requirements for this pass. Historical evidence below remains tied to its original source.

## Current v0.10.0 delivery

- Completed: editor/Shipping builds, configuration/rule validation, seven focused native tests, all 86 packaged smoke assertions at 10×, and review of four actual UI/night captures.
- Completed: content-bound [verification record](verification/v0.10.0.json), tested launcher and CRC/SHA-256-verified Windows ZIP. Save format 8 requires a new colony; legacy saves remain untouched.
- Delivery blocked externally: both `git push origin main` and the release API returned account-suspension HTTP 403. Code/assets are saved as local commit `31f8a84`, with the local `v0.10.0` tag, ZIP and checksum. Once GitHub restores access, push main and the tag and upload the existing archive. CI is unconfirmed.

## Ongoing design and quality work

- Permanent crews and real charging need balance playtesting across larger colonies. The zero-crew core recovery exception, storage/port surplus targets and Construction Hub labor must remain explicit rules, without phantom workers or automatic recycling.
- Autonomous transport needs varied routes, unloading/charging queues, destruction, evacuation and longer-session economy coverage beyond the focused regression. Keep one cargo ledger and reciprocal body/handler claims.
- The core is now worker-only. Review specialist production dependencies, the finite five-worker/9.57-tonne player start, fusion-plant operation and high-tier factories over sustained gameplay. Existing 14 recipe balances do not prove the revised economy is fun or self-sustaining.
- Current-source crowded-neighbor performance has not been measured. Retain the eight-developed-neighbor failure as a diagnostic target; do not reuse older frame-rate gains as evidence for v0.10.0.
- Rex's original asset remains stylized despite refinement. Continue reference-based face, coat and gait work. Terrain repetition, close-up ground quality, foliage transitions and sector-crossing stalls remain open; no Manor Lords parity claim.
- Persistent multiplayer, general inter-colony trade, remote extraction and full relocation/scavenging remain future work.

## Historical v0.9.3 checkpoint and retained backlog

The following records what was completed and outstanding at that earlier checkpoint. Its full-acceptance/stress-test plan has been superseded for this pass by the user's focused v0.10.0 scope above; its evidence is retained, not relabeled.

Updated **2026-10-11**: **v0.9.3 candidate held for a developed-neighbor performance fix**. The winter stress case with eight Developed-AI neighbors measured **1.74 FPS / approximately 570 ms game-thread time**. The completed [candidate checks](verification/v0.9.3.json) remain valid for `c2c5f31`, but the release is not ready for final delivery.

## Completed checks on candidate c2c5f31

- Source `c2c5f31` integrates the Codex fixes, Claude's previously unrun job-52 work and the complete building-kit meshes, portraits and material dependencies. It was pushed to `main`.
- **156 / 156 native tests**, both validators, Shipping packaging, **117 / 117 packaged stages with zero failures**, and **4 / 4 display states** passed. All **791** sampled process-tree observations had zero network endpoints. The main route ran at 10× and its separate 1× worker probe measured 4.985 km/h.
- The verification record binds the executable, cooked containers, **513 source/runtime inputs** and **22 matching staged JSON files**. The local ZIP has a recorded size and SHA-256. Release-evidence regressions pass and reject incomplete or mismatched reports.
- `Play-seige2222.cmd` now launches v0.9.3. Four actual renders were inspected: level-3 campus, daylight/night damage and Rex; controlled measurements and captures are in the [graphics report](game-design/GRAPHICS_PERFORMANCE_0_9_3.md).
- The restricted generated Rex asset is replaced by an original procedural fallback; requested photographic likeness remains open.

## 1. Resolve the developed-neighbor performance blocker

- Diagnose and fix the eight-Developed-AI winter workload without weakening the simulation or hiding populated neighbors. Recheck this exact stress case at 10×.
- Rebuild the fixed source and rerun affected tests, the complete acceptance gate and graphics stress checks. Replace the candidate's executable/archive hashes and provenance; preserve the earlier results as earlier evidence.
- Hold release publication until the fix is verified. The present launcher/archive refer to the pre-fix candidate.

## 2. Hosted release delivery

- After the successful candidate-source push, GitHub API calls returned HTTP 403, "Sorry. Your account was suspended". Restore account access before publishing the eventual fixed archive or checking CI; neither publication nor a green CI result is confirmed.
- After the performance fix passes, commit/push its source, evidence and launcher as remote access permits. Keep `c2c5f31` identified as the pre-fix candidate; new binary evidence must name the actual fixed source. See the [candidate artifact record](verification/v0.9.3-local-release.json).

## 3. Verify the integrated presentation changes

- **Sector transitions:** terrain uploads and both forests are spread across frames. Readiness now includes the entire geometry pipeline before screenshot settling; benchmark coverage cannot accept the old sector's finished ground cover. Review rapid crossings, reversals and map-to-world returns for gaps, pop-in or stalls.
- **Foundations:** adjacent plots on slopes retain their own level interiors instead of averaging overlapping pads. The native regression passed; continue checking rendered edges, narrow gaps and terrain interpolation across varied placements.
- **Battle damage:** health drives scorch, smoke, fire and collapsed ruins. Damage also extinguishes lamps/windows; review daylight and night views after regenerating materials.
- **Kit delivery:** level-2/3 command campuses retain the original upright shuttle. The kit, native readiness fixes and original Rex fallback are included in the tested build. A clean-machine reconstruction remains distinct from this workstation's validated package; preserve reproducible generation/import instructions and asset provenance.

## 4. Performance and scene quality

- The retained job-51 Shipping boundary crossing peaked at **43.3 ms**, down from **416.8 ms** in the early v0.9.2 baseline; boundary p99 was **28.5 ms**. Use the [v0.9.3 measurements](game-design/GRAPHICS_PERFORMANCE_0_9_3.md) for the new forest batching and spike attribution; pursue remaining transition stalls without removing distant terrain or increasing pop-in.
- The v0.9.3 static route at native **3840×1600, Medium, 10×** measured **52.5 FPS** in meadow/ground and **74.7 FPS** in the colony view. The GPU cost is spread across trees, terrain, shadows, lighting and post. Compare content changes against controlled runs; keep the single Medium profile and render-resolution option. Sustained 60 FPS and crowded-colony performance remain unproven.
- Residual terrain patterns, bare grass strips on slopes, repeated grass silhouettes, a single near-tree species and close-up surface wear remain quality limits. Manor Lords parity has not been reached.
- Wall clearance follows whole segments. Nearby tree crowns can still overlap the wall in angled views; review readability before widening the cleared strip.

## 5. Rex likeness

- The Hunyuan3D-derived revision 7 is retired from the current asset/release tree because its licence did not meet the project's commercial-use requirement. The original, locally authored procedural revision-5 fallback preserves gameplay and animation. See [provenance](../Art/CompanionDog/TEXTURE_PROVENANCE.md); earlier Git history is unchanged.
- The fallback is visibly a prototype. Improve Rex's face, ear attachment, leg contours, coat and movement against the supplied photographs using compatible original assets. Do not mark the requested likeness complete.
- Grass interaction and grounded visual contact remain later polish work.

## 6. AI, economy and sustained play

- Authored Developed-AI scenarios are the accepted loading model. Player and Starting AI retain the finite 9.57-tonne kit and six workers; ordinary paid simulation continues after all starts. Do not restore hours of simulated growth as a loading requirement.
- The retained first-playable test passed after the robotic-parts works moved earlier in the AI plan and support recovery favored short grid connections. Job 51's diagnostic won at **19,420 simulation seconds**. That win precedes towers 2–4; it does not prove a complete long-term defensive layout.
- Extend the timeline past victory and across seeds to check exposure-driven tower bearings, service recovery, power shortages, economic reserves and late upgrades. The past-win diagnostic is separate from release acceptance.
- Material gates, construction times, production rates and prices remain provisional. Keep tuning in external rule files and verify finite-stock bootstrap and dependency consistency.

## 7. Later systems and tooling

- Persistent multiplayer, player extraction across regions, general inter-colony trade and full relocation/scavenging remain future work. Existing privateering does not complete those systems.
- Keep build/test artifacts local and versioned. Verify the fresh-checkout asset manifest and actual staged files, not only the current workstation's Content folder. Git history size and any future LFS migration are separate maintenance tasks.
- Preserve the hardened verification gate: it rejects empty/partial native reports, failed or unrecognized validator output, incomplete gameplay/network evidence and mismatched source/package inputs. Asset licence/provenance review remains a separate requirement; hashes establish identity, not rights.

## Retained completed design and earlier fixes

The earlier v0.9.2 passes established canonical-JSON save fingerprints, cumulative upgrade-bill checks, the production-chain panel, paid AI upgrades, stored-worker service berths, family-specific industry meshes and distinct tower/port/solar/factory levels. These remain in v0.9.3 and passed again where covered by the release suite. This list records completed scope; the verification record identifies the actual tested source and package.

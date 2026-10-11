# seige2222 — remaining work

Updated **2026-10-11** for the v0.9.3 integration candidate. This list separates the current release gate from later development. Historical measurements identify their build; no previous pass certifies the candidate.

## 1. Complete the v0.9.3 release gate

- Compile and validate the integrated source through Claude commit `33ac182` plus the Codex integration fixes. The expected native suite is **156 tests**, including adjacent foundations and sector-readiness lifecycle; a count is not a pass until the complete report is read.
- Build Shipping, run the **117-stage** route with no assertions, verify display/offline behavior and inspect rendered terrain, campus, damage and Rex views. Measure static and moving-camera performance independently of build/import activity.
- Record the actual executable/package identity, test reports and source/assets in `docs/verification/v0.9.3.json`; promote `Play-seige2222.cmd` only after acceptance, then commit and push. Job-51 meshes, matching portraits and material dependencies previously existed only on the PC; the integration release must deliver the complete set in Git.
- The last retained full gate is v0.9.2, source `90bff58` (job 51): **154 / 154** native and **117 / 0** packaged stages/failures. It is historical evidence only.

## 2. Verify the integrated presentation changes

- **Sector transitions:** terrain uploads and both forests are spread across frames. Readiness now includes the entire geometry pipeline before screenshot settling; benchmark coverage cannot accept the old sector's finished ground cover. Review rapid crossings, reversals and map-to-world returns for gaps, pop-in or stalls.
- **Foundations:** adjacent plots on slopes retain their own level interiors instead of averaging overlapping pads. Check the new native regression and rendered edges; narrow gaps and terrain interpolation still need visual review.
- **Battle damage:** health drives scorch, smoke, fire and collapsed ruins. Damage also extinguishes lamps/windows; review daylight and night views after regenerating materials.
- **Kit delivery:** level-2/3 command campuses retain the original upright shuttle. All mapped meshes and rendered portraits must resolve in a fresh checkout. Native readiness fixes and the restored original Rex mesh must be included in the same final build.

## 3. Performance and scene quality

- The retained job-51 Shipping boundary crossing peaked at **43.3 ms**, down from **416.8 ms** in the early v0.9.2 baseline; boundary p99 was **28.5 ms**. New forest batching and spike attribution require fresh measurements before claiming another gain.
- Historical native **3840×1600, Medium, 100%** meadow/ground views ran near 51 FPS. The GPU cost is spread across trees, terrain, shadows, lighting and post. Compare content changes against controlled runs; keep the single Medium profile and render-resolution option. Sustained 60 FPS and crowded-colony performance remain unproven.
- Residual terrain patterns, bare grass strips on slopes, repeated grass silhouettes, a single near-tree species and close-up surface wear remain quality limits. Manor Lords parity has not been reached.
- Wall clearance follows whole segments. Nearby tree crowns can still overlap the wall in angled views; review readability before widening the cleared strip.

## 4. Rex likeness

- The Hunyuan3D-derived revision 7 is retired from the current asset/release tree because its licence did not meet the project's commercial-use requirement. The original, locally authored procedural revision-5 fallback preserves gameplay and animation. See [provenance](../Art/CompanionDog/TEXTURE_PROVENANCE.md); earlier Git history is unchanged.
- The fallback is visibly a prototype. Improve Rex's face, ear attachment, leg contours, coat and movement against the supplied photographs using compatible original assets. Do not mark the requested likeness complete.
- Grass interaction and grounded visual contact remain later polish work.

## 5. AI, economy and sustained play

- Authored Developed-AI scenarios are the accepted loading model. Player and Starting AI retain the finite 9.57-tonne kit and six workers; ordinary paid simulation continues after all starts. Do not restore hours of simulated growth as a loading requirement.
- The retained first-playable test passed after the robotic-parts works moved earlier in the AI plan and support recovery favored short grid connections. Job 51's diagnostic won at **19,420 simulation seconds**. That win precedes towers 2–4; it does not prove a complete long-term defensive layout.
- Extend the timeline past victory and across seeds to check exposure-driven tower bearings, service recovery, power shortages, economic reserves and late upgrades. The past-win diagnostic is separate from release acceptance.
- Material gates, construction times, production rates and prices remain provisional. Keep tuning in external rule files and verify finite-stock bootstrap and dependency consistency.

## 6. Later systems and tooling

- Persistent multiplayer, player extraction across regions, general inter-colony trade and full relocation/scavenging remain future work. Existing privateering does not complete those systems.
- Keep build/test artifacts local and versioned. Verify the fresh-checkout asset manifest and actual staged files, not only the current workstation's Content folder. Git history size and any future LFS migration are separate maintenance tasks.
- The verification recorder must reject empty/partial native reports, failed or unrecognized validator output, missing package files and incomplete asset provenance. Its record should identify the tested source and final package without truncating changed dependencies.

## Retained completed design and earlier fixes

The earlier v0.9.2 passes established canonical-JSON save fingerprints, cumulative upgrade-bill checks, the production-chain panel, paid AI upgrades, stored-worker service berths, family-specific industry meshes and distinct tower/port/solar/factory levels. These remain in the integration candidate and are rerun where covered by the release suite. This list records completed scope, not a substitute for the new release evidence.

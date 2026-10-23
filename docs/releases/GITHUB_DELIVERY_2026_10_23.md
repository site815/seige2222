# GitHub delivery review — October 23, 2026

The owner reports that GitHub reinstated the account and authorizes publication of the current source and latest Windows build. Repository access was verified; remote `main` was `c2c5f317fc55f6996cdb20f194b7682b5a6ac9e8`, with no remote-only commits. Existing local history is preserved.

## Historical validation failure

A bounded query covering October 8–10 UTC returned 20 runs: ten successful October 8 runs and ten failed October 9 runs, each on its first attempt. The representative [October 9 run 37956396271](https://github.com/site815/seige2222/actions/runs/37956396271) completed checkout and rule validation, then failed configuration validation because `building_visuals.biofuel_refinery` referenced the missing `/Game/Art/SM_BiofuelRefinery` mesh. This confirms that job's failure; it does not establish the reason for the account restriction or the cause of every historical failure. No old workflow was rerun.

Commit `c2c5f31` already added that 445,328-byte mesh as an ordinary Git asset. Current tracked-only validation passes all 54 building visuals and ten nature roles. Removing just that mesh from the test inventory reproduces the original error, confirming that validation remains effective.

## Workflow safeguards

- Push validation runs only for `main`, so the release tag does not create another run. Pull requests and explicit manual runs remain supported.
- Same-workflow/ref concurrency cancels superseded work, and each job has a ten-minute timeout.
- The job receives read-only content access. LFS downloads are disabled because configuration validation uses asset filenames, not binary asset contents.
- Wind regression checks join the existing rules, AI, combat, environment, UI and verification-record checks.

All seven Node validation/regression commands and all 15 Python verification tests passed locally before publication. Configuration validation also passed using only Git-tracked Content package names, excluding ignored or untracked local assets. Windows filesystem sandbox restrictions required the Python fixture tests to run outside that sandbox. Linux-hosted results belong to the new GitHub run and are not inferred from these local passes.

Delivery uses one consolidated normal branch/tag push, then sequential release asset uploads. The archive stays outside Git history. API work is bounded and serial, with no automatic retries after permission or rate-limit errors. These measures reduce unnecessary requests and duplicate work; they cannot guarantee GitHub's account-policy decisions.

## Build identity

The latest build is the already-tested **v0.10.6** Windows Shipping package from October 17, not a new runtime build. October 23 changes are delivery documentation and CI only. The release is a prototype prerelease.

- Archive: `seige2222-v0.10.6-win64.zip`, **759,184,438 bytes**.
- SHA-256: `e0f0c33a9e49342ad9e0d3b772f57574b15434fd488e40f6b2690a15a745c42c`.
- Rechecked all 557 source/runtime inputs, six package binaries/paks, 25 archived runtime JSON files, ZIP CRC, and recorded native/capture evidence against [v0.10.6 verification](../verification/v0.10.6.json).
- Publication inventory was checked for credential/private-key patterns and accidental secret files; none were found. LFS objects passed local integrity checks.

The [release page](https://github.com/site815/seige2222/releases/tag/v0.10.6) supplies the ZIP and checksum. Extract and run `Seige.exe`, and start a new scenario. Asset credits and documented prototype limitations remain in the build and source.

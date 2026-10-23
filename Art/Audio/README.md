# Original seige2222 sound effects

All WAV files in this directory were created locally by `Tools/create_original_audio.py` using resonators, deterministic filtered noise and amplitude envelopes. No recordings, external samples, hosted generators, trained models or third-party sound packs were used. The Rex cue is now a gentle companion-return instrument signal, not an imitation of the owner's dog.

The project dedicates these original WAV files to the public domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). They can be used commercially without attribution. The source generator is retained so they can be reproduced or edited. `manifest.json` records purpose, generator revision, duration, PCM format, peak, RMS and SHA-256 for every clip.

`Tools/import_original_audio.py` creates `/Game/Art/Audio/S_*` SoundWave assets, explicitly replacing source and import settings. `import_report.json` maps asset references and `import_source_hashes.json` records the source checksum imported into each asset. `Graphics/audio.json` owns the master/category/cue volumes, cooldowns and mixing lanes. Regenerating WAVs does not overwrite that mix. Missing mix entries remain silent; all authored functional cues are enabled in the current mix.

The v0.10.5 palette restores functional feedback rather than muting interaction. Navigation uses a 70 ms tactile console contact; confirmation uses two rounded latch contacts; cancellation is a lower return latch. Acceptance, rejection and placement are separate cues emitted from actual action outcomes, not from text parsing or hover events. Build sound is no longer inferred from polling buildings, preventing unsolicited AI-build or duplicate placement cues.

The mixer uses monotonic real time at pause, 1×, 10× and 20×. Independent bounded lanes reserve interface and notification capacity, so combat cannot consume a click, error or damage alert's budget. Defaults are world: 3 voices/frame and 5/second; interface: 2/frame and 6/second; notifications: 2/frame and 3/second. Combat additionally shares a 0.25-second cooldown; completion a 2.5-second cooldown. Damage alerts play once per incident, rearmed only after 30 real seconds without fresh damage. Damage is considered before completion feedback. Rejected events are discarded, never queued into a later burst. These are voice-start limits; short cue tails may overlap.

| Cues | Event |
| --- | --- |
| select / confirm / cancel | Tactile navigation / accepted action / return or cancellation |
| error | Two restrained descending knocks after a rejected action |
| build / complete | Mechanical lock after committed construction / two consonant completion notes |
| alert | Two separated attention notes when a building first takes damage in an incident |
| energy / kinetic / missile / plasma | Corresponding weapon fire; energy uses the laser cue |
| rex | Gentle descending instrument signal when Rex begins retreating; no synthetic bark |

Weapon effects use Unreal attenuation and panning. Events beyond the listener's attenuation radius are rejected before consuming the world budget. Notifications are nonspatial so the player can hear a home-colony warning while surveying another area. No speculative impact sound is inferred from a disappearing projectile: projectiles can also expire or be cleared during a reset.

Run `Tools/verify_audio.py --output Saved/v0105-audio-validation.json` after importing. It verifies source and imported checksums, positive functional gains, silent PCM boundaries, DC, clipping, transient steps and a locally computed Welch spectrum. `--source-only` permits an import that is still pending and reports it explicitly. Current source clips have zero clipped samples and 95% of spectral energy below 2.34 kHz. Nominal unattenuated UI RMS is about -31 to -30 dBFS; the damage alert is about -23 dBFS. These measurements establish signal levels and restrained high-frequency content, not subjective listening quality. No subjective listening claim is made.

Native `Seige.Audio` tests check actual real-time rate-gate behavior under 1×/5×/10×/20× event bursts, saturated-combat protection for interface and warnings, result-to-cue semantics, damage incident rearming and neighboring-sector coordinates. Generation and validation use only the Python standard library. Imported sounds remain original synthesized game effects rather than recorded machinery or animal sounds.

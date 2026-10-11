# Original seige2222 sound effects

All WAV files in this directory were created locally by `Tools/create_original_audio.py` using oscillators, deterministic filtered noise and amplitude envelopes. No recordings, external samples, hosted generators or third-party sound packs were used. The synthetic Rex woof is a stylized placeholder, not a recording of the owner's dog; it is muted in the shipping mix.

The project dedicates these original WAV files to the public domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). They can be used commercially without attribution. The source generator is retained so they can be reproduced or edited. `manifest.json` records durations, PCM format, peak amplitudes and hashes.

`Tools/import_original_audio.py` creates `/Game/Art/Audio/S_*` SoundWave assets. `Graphics/audio.json` owns the master/category/cue volumes and per-category/per-cue cooldowns. Regenerating WAVs does not overwrite that mix. Missing mix entries are silent.

The v0.10.1 mix silences routine selection, confirmation and cancellation. Error feedback remains available as a soft low tone. The old rising click chirps, alternating alarm and sharp weapon sweeps have been replaced by shorter, softer low-frequency cues with smooth attacks/releases and two-stage low-pass filtered noise. Construction sounds share a 1.5-second cooldown, damage notifications share a 12-second cooldown, and weapon sounds share a 0.2-second cooldown. At most three voices begin per rendered frame and eight per rolling real second. Timing uses the monotonic wall clock, so 10× simulation does not accelerate the audio rate. Muted cues consume no audible budget, and suppressed events are discarded instead of queued for later playback.

| Cues | Event |
| --- | --- |
| select / confirm / cancel | Ordinary interface actions; silent by default |
| error | Soft interface error feedback, if invoked |
| build / complete | New construction / completed construction |
| alert | A building actually receives damage (throttled) |
| energy / kinetic / missile / plasma | Corresponding weapon fire; energy uses the laser cue |
| rex | Reserved for Rex retreat; muted until a convincing canine source is available |

Spatial gameplay effects use Unreal's attenuation and panning. The WAV manifest records peak, RMS and SHA-256 for the quieter original clips. These effects remain synthesized game cues rather than recorded machinery or animal sounds.

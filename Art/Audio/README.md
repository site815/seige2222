# Original seige2222 sound effects

All WAV files in this directory were created locally by `Tools/create_original_audio.py` using oscillators, deterministic filtered noise and amplitude envelopes. No recordings, external samples, hosted generators or third-party sound packs were used. The synthetic Rex woof is a stylized game cue, not a recording of the owner's dog.

The project dedicates these original WAV files to the public domain under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). They can be used commercially without attribution. The source generator is retained so they can be reproduced or edited. `manifest.json` records durations, PCM format, peak amplitudes and hashes.

`Tools/import_original_audio.py` creates `/Game/Art/Audio/S_*` SoundWave assets. `Graphics/audio.json` maps editable cue names and master volume. Runtime voices are capped per frame; UI clicks are debounced; gameplay cues are emitted once per observed event rather than once per render frame.

| Cues | Event |
| --- | --- |
| select / confirm / cancel / error | Interface actions |
| build / complete | New construction / completed construction |
| alert | A building actually receives damage (throttled) |
| energy / kinetic / missile / plasma | Corresponding weapon fire; energy uses the laser cue |
| rex | Rex starts retreating after nearby building damage |

Spatial gameplay effects use Unreal's attenuation and panning. These are a first original effects pass, not a finished audio mix.

"""Original instrument-console and machinery cues, authored entirely offline.

Only deterministic noise, resonators and envelopes are used. No recordings,
sample packs, trained models or network resources. Graphics/audio.json owns the
runtime mix; rebuilding PCM sources never overwrites the designer's mix.
"""
from pathlib import Path
import array
import hashlib
import json
import math
import random
import wave

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Art/Audio"
RATE = 48000
TAU = 2 * math.pi


def envelope(t, length, attack=.006, release=.030, decay=0):
    if t <= 0 or t >= length:
        return 0.
    return (math.sin(min(1., t / attack) * math.pi / 2) ** 2
            * math.sin(min(1., (length - t) / release) * math.pi / 2) ** 2
            * math.exp(-decay * t / length))


def note(t, start, length, frequency, gain=1., decay=2.):
    """Soft struck, slightly inharmonic console resonator; no square-wave buzz."""
    q = t - start
    env = envelope(q, length, .010, .045, decay)
    return gain * env * (math.sin(TAU * frequency * q)
                        + .19 * math.sin(TAU * frequency * 2.007 * q)
                        + .055 * math.sin(TAU * frequency * 3.011 * q))


def click(t, start, length, low, band, frequency, gain=1.):
    q = t - start
    env = envelope(q, length, .0035, .015, 4.)
    return gain * env * (.65 * band + .27 * low
                        + .28 * math.sin(TAU * frequency * q)
                        + .11 * math.sin(TAU * frequency * 1.79 * q))


SPECS = {
    "select": (.070, .085, "A single soft tactile console key; navigation and selection"),
    "confirm": (.180, .085, "Two rounded latch contacts resolving upward; accepted command"),
    "cancel": (.120, .080, "A soft return latch, lower than selection; back or cancel"),
    "error": (.330, .115, "Two muted descending knocks; command rejected"),
    "build": (.240, .105, "Low mechanical clamp and locking contact; construction order committed"),
    "complete": (.640, .115, "Two gentle consonant notes; construction completed"),
    "alert": (.740, .135, "Two separated attention notes with a soft body; one damage incident"),
    "laser": (.230, .140, "Brief electrical discharge with filtered hiss and resonant body"),
    "kinetic": (.360, .170, "Low cannon impulse, mechanical contact and decaying pressure"),
    "missile": (.600, .155, "Ignition contact into a broad filtered rocket whoosh; launch only"),
    "plasma": (.480, .155, "Slower resonant energy discharge, rough low core and soft tail"),
    "rex": (.520, .105, "Gentle companion-return instrument signal; not a dog voice"),
}


def synthesize(name, duration, target_rms):
    rng = random.Random("seige2222:functional-console-v2:" + name)
    count = round(RATE * duration)
    samples = []
    slow = fast = smooth = output = 0.
    phase = 0.
    # Band-limited noise supplies texture without sharp clicks or high hiss.
    low_a = 1 - math.exp(-TAU * 320 / RATE)
    fast_a = 1 - math.exp(-TAU * 1900 / RATE)
    smooth_a = 1 - math.exp(-TAU * 2600 / RATE)
    for i in range(count):
        t = i / RATE
        p = t / duration
        noise = rng.uniform(-1., 1.)
        slow += low_a * (noise - slow)
        fast += fast_a * (noise - fast)
        smooth += smooth_a * (fast - smooth)
        band = smooth - slow
        if name == "select":
            value = click(t, 0, .068, slow, band, 740)
        elif name == "confirm":
            value = click(t, 0, .085, slow, band, 640, .78) + click(t, .054, .120, slow, band, 940, .9)
        elif name == "cancel":
            value = click(t, 0, .115, slow, band, 440, .85)
        elif name == "error":
            value = note(t, 0, .160, 260, .52, 3.) + note(t, .137, .185, 195, .46, 3.)
        elif name == "build":
            value = note(t, 0, .228, 135, .48, 3.) + click(t, .025, .180, slow, band, 620, .60)
        elif name == "complete":
            value = note(t, 0, .410, 392, .60, 2.2) + note(t, .155, .475, 523.25, .53, 2.2)
        elif name == "alert":
            value = note(t, 0, .300, 440, .65, 1.4) + note(t, .340, .380, 554.37, .62, 1.4)
        elif name == "rex":
            value = note(t, 0, .310, 523.25, .43) + note(t, .145, .365, 392, .46)
        elif name == "laser":
            phase += TAU * (640 - 170 * p) / RATE
            value = envelope(t, duration, .007, .060, 3.) * (.28 * math.sin(phase) + .48 * band + .17 * math.sin(phase * 1.49))
        elif name == "kinetic":
            phase += TAU * (110 - 48 * p) / RATE
            value = envelope(t, duration, .006, .095, 4.5) * (.85 * slow + .27 * math.sin(phase))
            value += click(t, .008, .075, slow, band, 680, .45)
        elif name == "missile":
            value = envelope(t, duration, .040, .130, 1.8) * (1.8 * slow + .65 * band)
            value += click(t, 0, .090, slow, band, 340, .30)
        elif name == "plasma":
            phase += TAU * (230 - 105 * p) / RATE
            value = envelope(t, duration, .020, .140, 2.5) * (.30 * math.sin(phase + .30 * math.sin(phase * .49)) + .9 * slow + .14 * band)
        else:
            raise ValueError(name)
        output += smooth_a * (value - output)
        samples.append(output * envelope(t, duration, .002, .008))
    # Remove residual DC and restore exact silent boundaries with a final fade.
    dc = sum(samples) / len(samples)
    samples = [(value - dc) * envelope(i / RATE, duration, .002, .008) for i, value in enumerate(samples)]
    rms = math.sqrt(sum(value * value for value in samples) / count)
    gain = min(target_rms / max(rms, 1.e-9), .70 / max(abs(value) for value in samples))
    return array.array("h", (round(value * gain * 32767) for value in samples))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    records = {}
    for name, (duration, target, purpose) in SPECS.items():
        pcm = synthesize(name, duration, target)
        path = OUT / (name + ".wav")
        with wave.open(str(path), "wb") as sound:
            sound.setnchannels(1)
            sound.setsampwidth(2)
            sound.setframerate(RATE)
            sound.writeframes(pcm.tobytes())
        records[name] = {
            "file": path.name, "seconds": duration, "sample_rate": RATE, "channels": 1,
            "peak": max(abs(x) for x in pcm) / 32767,
            "rms": math.sqrt(sum(x * x for x in pcm) / len(pcm)) / 32767,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "purpose": purpose, "generator_revision": "functional-console-v2",
            "source": "Original deterministic mathematical synthesis", "license": "CC0-1.0",
        }
    (OUT / "manifest.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print("ORIGINAL_AUDIO_READY", len(records), "clips; maximum peak", max(v["peak"] for v in records.values()))


if __name__ == "__main__":
    main()

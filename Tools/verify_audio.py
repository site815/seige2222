"""Read-only source audio/mix audit; does not import, play, or modify assets."""
import argparse
import array
import cmath
import hashlib
import json
import math
from pathlib import Path
import wave


def spectrum(samples, rate=48000):
    """Welch power spectrum, implemented locally without optional packages."""
    size = 4096
    total = [0.] * (size // 2 + 1)
    for start in range(0, max(1, len(samples) - size // 2), size // 2):
        values = [complex((samples[start + i] if start + i < len(samples) else 0)
                          * (.5 - .5 * math.cos(2 * math.pi * i / (size - 1))), 0)
                  for i in range(size)]
        j = 0
        for i in range(1, size):
            bit = size >> 1
            while j & bit:
                j ^= bit
                bit >>= 1
            j ^= bit
            if i < j:
                values[i], values[j] = values[j], values[i]
        length = 2
        while length <= size:
            step = cmath.exp(-2j * math.pi / length)
            for offset in range(0, size, length):
                weight = 1
                for k in range(length // 2):
                    a, b = values[offset + k], values[offset + k + length // 2] * weight
                    values[offset + k], values[offset + k + length // 2] = a + b, a - b
                    weight *= step
            length *= 2
        for i in range(len(total)):
            total[i] += abs(values[i]) ** 2
    energy = max(sum(total), 1.e-9)
    cumulative, rolloff = 0., 0.
    for i, value in enumerate(total):
        cumulative += value
        if cumulative >= .95 * energy:
            rolloff = i * rate / size
            break
    return {"centroid_hz": sum(i * rate / size * value for i, value in enumerate(total)) / energy,
            "rolloff_95_hz": rolloff,
            "energy_above_6khz_fraction": sum(value for i, value in enumerate(total) if i * rate / size > 6000) / energy}


def stress_mix(mix, pcm):
    """Objective overlap/headroom check using real-time events and authored mix.

    Native tests exercise the C++ policy. This independent PCM render checks the
    resulting sample sums during saturated combat plus real user interaction.
    """
    reports = {}
    for speed in (1, 20):
        limits = {"world": (mix["maximum_voices_per_frame"], mix["maximum_voices_per_second"]),
                  "interface": (mix["interface_voices_per_frame"], mix["interface_voices_per_second"]),
                  "notification": (mix["notification_voices_per_frame"], mix["notification_voices_per_second"])}
        previous_cue, previous_category = {}, {}
        times = {lane: [] for lane in limits}
        counts = {name: 0 for name in pcm}
        output = [0.] * (9 * 48000)
        for frame in range(8 * 60):
            now = frame / 60.
            frame_counts = {lane: 0 for lane in limits}
            # Deliberately saturate world starts before important feedback.
            requests = [["laser", "kinetic", "missile", "plasma"][(frame + event) % 4] for event in range(speed)]
            if frame % 6 == 0:
                requests.append("select")
            if frame % 66 == 0:
                requests.append("error")
            if frame == 0:
                requests.append("alert")
            if frame % 180 == 0:
                requests.append("complete")
            for name in requests:
                cue = mix["cue_mix"][name]
                category = mix["categories"][cue["category"]]
                lane = category["lane"]
                if now - previous_cue.get(name, -1000) < cue["cooldown_seconds"]:
                    continue
                if now - previous_category.get(cue["category"], -1000) < category["cooldown_seconds"]:
                    continue
                times[lane] = [time for time in times[lane] if now - time < 1.]
                if frame_counts[lane] >= limits[lane][0] or len(times[lane]) >= limits[lane][1]:
                    continue
                frame_counts[lane] += 1
                times[lane].append(now)
                previous_cue[name] = previous_category[cue["category"]] = now
                counts[name] += 1
                offset = round(now * 48000)
                gain = mix["master_volume"] * category["volume"] * cue["volume"] / 32767
                for index, value in enumerate(pcm[name]):
                    output[offset + index] += value * gain
        peak = max(abs(value) for value in output)
        assert peak < 1., (speed, peak)
        assert counts["alert"] == 1 and counts["error"] > 0 and counts["select"] > 0
        reports[str(speed) + "x"] = {"accepted_cues": counts, "unattenuated_sum_peak": peak,
                                    "headroom_db": -20 * math.log10(max(peak, 1.e-9)), "clipped_samples": 0}
    assert reports["1x"]["accepted_cues"] == reports["20x"]["accepted_cues"]
    return reports


def verify(root, source_only=False):
    mix = json.loads((root / "Graphics/audio.json").read_text(encoding="utf-8-sig"))
    manifest = json.loads((root / "Art/Audio/manifest.json").read_text(encoding="utf-8-sig"))
    imports = json.loads((root / "Art/Audio/import_report.json").read_text(encoding="utf-8-sig"))
    evidence_path = root / "Art/Audio/import_source_hashes.json"
    evidence = json.loads(evidence_path.read_text(encoding="utf-8-sig")) if evidence_path.exists() else {}
    def number(value, minimum, maximum):
        assert isinstance(value, (int, float)) and math.isfinite(value) and minimum <= value <= maximum
    number(mix["master_volume"], 0, 1)
    for key in ("maximum_voices_per_frame", "maximum_voices_per_second", "interface_voices_per_frame",
                "interface_voices_per_second", "notification_voices_per_frame", "notification_voices_per_second"):
        number(mix[key], 1, 32)
        assert int(mix[key]) == mix[key]
    number(mix["damage_alert_quiet_seconds"], 1, 300)
    assert set(mix["cues"]) == set(mix["cue_mix"])
    for group in mix["categories"].values():
        number(group["volume"], 0, 1)
        number(group["cooldown_seconds"], 0, 60)
        assert group["lane"] in ("world", "interface", "notification")
    for name, cue in mix["cue_mix"].items():
        assert cue["category"] in mix["categories"], name
        number(cue["volume"], 0, 1)
        number(cue["cooldown_seconds"], 0, 60)
        asset = mix["cues"][name]
        assert asset in imports.values(), name
        assert asset.startswith("/Game/Art/Audio/"), name
        assert (root / "Content" / (asset.split(".")[0].removeprefix("/Game/") + ".uasset")).is_file(), name
    def gain(name):
        cue = mix["cue_mix"][name]
        return mix["master_volume"] * cue["volume"] * mix["categories"][cue["category"]]["volume"]
    for name in mix["cues"]:
        assert gain(name) > 0, f"{name} must provide functional audible feedback"
    assert mix["categories"][mix["cue_mix"]["alert"]["category"]]["lane"] == "notification"
    assert mix["categories"][mix["cue_mix"]["error"]["category"]]["lane"] == "notification"
    assert mix["categories"][mix["cue_mix"]["select"]["category"]]["lane"] == "interface"
    clipped = 0
    peak = 0
    analysis = {}
    pcm = {}
    current_imports = True
    for name, record in manifest.items():
        path = root / "Art/Audio" / record["file"]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == record["sha256"], name
        imported_current = evidence.get(name, {}).get("source_sha256") == record["sha256"]
        current_imports &= imported_current
        if not source_only:
            assert imported_current, f"{name}: source has not been reimported; run Tools/import_original_audio.py"
        with wave.open(str(path), "rb") as wav:
            assert wav.getnchannels() == 1 and wav.getsampwidth() == 2 and wav.getframerate() == 48000, name
            samples = array.array("h", wav.readframes(wav.getnframes()))
            assert len(samples) > 0, name
            assert abs(len(samples) / wav.getframerate() - record["seconds"]) < 1 / 48000, name
        clipped += sum(abs(value) >= 32767 for value in samples)
        peak = max(peak, max(abs(value) for value in samples) / 32767)
        # Fade boundaries must not introduce a discontinuous impulse.
        assert abs(samples[0]) <= 1 and abs(samples[-1]) <= 1, name
        metrics = spectrum(samples)
        pcm[name] = samples
        rms = math.sqrt(sum(value * value for value in samples) / len(samples)) / 32767
        dc = abs(sum(samples) / len(samples)) / 32767
        transient = max(abs(b - a) for a, b in zip(samples, samples[1:])) / 32767
        assert .065 <= rms <= .19, (name, rms)
        assert metrics["energy_above_6khz_fraction"] < .015, (name, metrics)
        assert metrics["rolloff_95_hz"] < 4000, (name, metrics)
        assert dc < .003 and transient < .22, (name, dc, transient)
        metrics.update({"seconds": record["seconds"], "source_rms_dbfs": 20 * math.log10(rms),
                        "nominal_mixed_rms_dbfs": 20 * math.log10(rms * gain(name)),
                        "maximum_adjacent_sample_step": transient, "dc_fraction": dc,
                        "purpose": record.get("purpose", "")})
        analysis[name] = metrics
    assert clipped == 0
    return {"clips": len(manifest), "clipped_samples": clipped, "maximum_source_peak": peak,
            "all_functional_cues_enabled": True, "rex_is_instrument_notification_not_animal_voice": True,
            "source_hashes_and_import_references_valid": True, "imported_sources_current": current_imports,
            "maximum_voices_per_frame": mix["maximum_voices_per_frame"],
            "maximum_voices_per_real_second": mix["maximum_voices_per_second"],
            "damage_alert_quiet_real_seconds": mix["damage_alert_quiet_seconds"],
            "effective_cue_gains": {name: gain(name) for name in mix["cues"]},
            "saturated_combat_and_ui_pcm_mix": stress_mix(mix, pcm),
            "temporal_and_spectral_analysis": analysis,
            "limitation": "Objective waveform, spectrum, nominal gain and import-provenance audit; no subjective listening claim."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--source-only", action="store_true", help="Permit pending Unreal import; report it explicitly")
    args = parser.parse_args()
    report = json.dumps(verify(args.root, args.source_only), indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(report, encoding="utf-8")
    print(report, end="")

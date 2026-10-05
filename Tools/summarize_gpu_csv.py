"""Summarize UE 5.8 GPU CSV counters without running Unreal.

Development/editor game flags: -csvGpuStats -csvCaptureFrames=-1 -csvCompression=0.
GPU pass statistics are disabled by HAS_GPU_STATS in standard Shipping builds.
UE appends the complete column header at EOF because columns can appear late.
Source: Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp (Finalize),
Runtime/RHI/Private/GPUProfiler.cpp (FStatState::EmitResults).
"""
import argparse
import csv
import gzip
import json
import math
from pathlib import Path
import re


def summarize(values):
    ordered = sorted(values)
    count = len(ordered)
    if not count:
        return {"samples": 0}
    return {
        "samples": count,
        "nonzero_frames": sum(value > 0 for value in ordered),
        "mean_ms": sum(ordered) / count,
        "p95_ms": ordered[max(0, math.ceil(count * .95) - 1)],
        "p99_ms": ordered[max(0, math.ceil(count * .99) - 1)],
        "max_ms": ordered[-1],
    }


def analyze(path):
    # Boot captures can put many module/loading events into the first row.
    csv.field_size_limit(16 * 1024 * 1024)
    opener = gzip.open if path.suffix.lower() == ".gz" else open
    with opener(path, "rt", encoding="utf-8-sig", newline="") as source:
        rows = list(csv.reader(source))
    headers = [i for i, row in enumerate(rows) if row and row[0] == "EVENTS"]
    if not headers:
        raise ValueError("No UE EVENTS header found")
    header = rows[headers[-1]]
    end = headers[-1] if len(headers) > 1 else len(rows)
    data = [row for row in rows[headers[0] + 1:end] if row and row[0] != "EVENTS"]
    gpu_columns = [(i, name) for i, name in enumerate(header) if re.match(r"^GPU\d*/", name)]
    if not gpu_columns:
        raise ValueError("No GPU pass columns. Use Development/editor -game with -csvGpuStats; standard Shipping omits HAS_GPU_STATS.")
    groups = {}
    active = None
    has_markers = False
    incomplete = set()
    for row in data:
        events = row[0]
        begin = re.search(r"SEIGE_BENCH_SAMPLE_BEGIN:([A-Za-z0-9_-]+)", events)
        finish = re.search(r"SEIGE_BENCH_SAMPLE_END:([A-Za-z0-9_-]+)", events)
        if begin:
            has_markers = True
            if active:
                incomplete.add(active)
            active = begin.group(1)
            groups.setdefault(active, [])
            continue
        if finish:
            if active != finish.group(1):
                incomplete.add(finish.group(1))
            active = None
            continue
        if active:
            groups[active].append(row)
    if active:
        incomplete.add(active)
    if not has_markers:
        groups = {"whole_capture_including_setup_warmup_and_screenshots": data}
    results = []
    for name, selected in groups.items():
        passes = []
        for index, label in gpu_columns:
            values = []
            for row in selected:
                try:
                    value = float(row[index]) if index < len(row) and row[index] else 0.
                except ValueError:
                    continue
                if math.isfinite(value) and value >= 0:
                    values.append(value)
            stats = summarize(values)
            if stats.get("max_ms", 0) > 0:
                passes.append({"pass": label, **stats})
        passes.sort(key=lambda item: item["mean_ms"], reverse=True)
        results.append({"view": name, "frames": len(selected), "complete_marker_pair": name not in incomplete if has_markers else None, "passes": passes})
    return {
        "source": str(path.resolve()),
        "marked_sample_windows": has_markers,
        "method": "Rows strictly between sample begin/end markers; boundary rows excluded." if has_markers else "UNMARKED CAPTURE: includes startup, scene setup, warmup and screenshots. Useful for identifying expensive passes, not a steady-state FPS comparison.",
        "counter_semantics": "UE 5.8 CSV GPU pass counters cover graphics queue 0 and use exclusive busy+wait cycles. They do not describe every asynchronous-compute queue pass. Totals/parent counters can overlap; do not add all rows. Use ProfileGPU/Insights for queue-level attribution.",
        "gpu_columns": len(gpu_columns),
        "incomplete_views": sorted(incomplete),
        "views": results,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_path", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--top", type=int, default=16, help="Number of nonzero pass rows printed per view")
    args = parser.parse_args()
    report = analyze(args.csv_path)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(report["method"])
    print(report["counter_semantics"])
    for view in report["views"]:
        print(f"\n{view['view']}: {view['frames']} frames")
        print(" mean ms | p95 ms | p99 ms | max ms | GPU pass")
        for item in view["passes"][:args.top]:
            print(f"{item['mean_ms']:8.3f} | {item['p95_ms']:6.3f} | {item['p99_ms']:6.3f} | {item['max_ms']:6.3f} | {item['pass']}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
classify_metrics.py - Security Camera Metrics File Classifier

Automatically classifies CSV metrics files by their structural format and
performance period, then produces per-period statistical analysis.

Background
----------
The security camera system's logging format evolved across three distinct
phases as new instrumentation was added.  Files from different phases must
NOT be pooled together when computing baselines because the underlying system
performance changed materially between them.

  Phase A  (Jan 3)        -- 14 columns.  USB-only transport, no TCP fields.
                              System was in very early testing.  PC FPS was
                              consistently below 1 FPS; Spresense FPS ~3-4.

  Phase B  (Jan 11-13a)   -- 16 columns.  tcp_avg_send_ms and tcp_max_send_ms
                              added after WiFi/TCP transport was brought online.
                              PC FPS rose to ~1-2 FPS mean; Spresense FPS ~4.
                              TCP metrics show many zeros (transport still being
                              tuned).

  Phase C  (Jan 13b-17)   -- 18 columns.  dropped_frames and drop_events added
                              after frame-drop tracking was instrumented.  This
                              is the first period with stable WiFi streaming.
                              PC FPS rose to ~5-6 FPS mean; Spresense FPS ~12-13.
                              TCP metrics are fully populated.

The Jan 13 date contains a structural split: metrics_20260113_114023.csv is
16-column (Phase B); all later Jan 13 files are 18-column (Phase C).  The
classifier handles this automatically by inspecting the header, not the date.

Usage
-----
    python3 classify_metrics.py [metrics_directory]

    If no directory is given, the default path is used:
        /home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/

Output
------
    1. A full file manifest showing each CSV's phase classification.
    2. Per-phase summary statistics for every metric column.
    3. Per-phase target-hit-rate analysis for pc_fps.
    4. A comparison table showing the improvement progression across phases.
    5. Corrected PID baseline numbers suitable for Phase 10 design.
"""

import csv
import statistics
import sys
from pathlib import Path
from typing import Dict, List, Any


# ---------------------------------------------------------------------------
# Phase definitions driven entirely by column presence, not by filename dates.
# This keeps classification robust if files are renamed or re-exported.
# ---------------------------------------------------------------------------
PHASE_A_SIGNATURE = {"tcp_avg_send_ms", "dropped_frames"}   # both ABSENT
PHASE_B_SIGNATURE = {"tcp_avg_send_ms"}                     # present, no drops
PHASE_C_SIGNATURE = {"tcp_avg_send_ms", "dropped_frames"}   # both PRESENT

# Columns shared by every phase (the 14-column baseline set)
COMMON_COLUMNS = [
    "timestamp", "pc_fps", "spresense_fps", "frame_count", "error_count",
    "decode_time_ms", "serial_read_time_ms", "texture_upload_time_ms",
    "jpeg_size_kb", "spresense_camera_frames", "spresense_camera_fps",
    "spresense_usb_packets", "action_q_depth", "spresense_errors",
]

# Columns added in Phase B
PHASE_B_COLUMNS = COMMON_COLUMNS + ["tcp_avg_send_ms", "tcp_max_send_ms"]

# Columns added in Phase C
PHASE_C_COLUMNS = PHASE_B_COLUMNS + ["dropped_frames", "drop_events"]


def classify_file(csv_path: Path) -> str:
    """
    Read the header of a single CSV and return its phase label.

    Returns one of: "Phase_A", "Phase_B", "Phase_C", or "UNKNOWN".
    """
    try:
        with open(csv_path, "r") as fh:
            header_line = fh.readline().strip()
    except (OSError, UnicodeDecodeError):
        return "UNKNOWN"

    columns = set(header_line.split(","))

    has_tcp   = "tcp_avg_send_ms"  in columns
    has_drops = "dropped_frames"   in columns

    if has_tcp and has_drops:
        return "Phase_C"
    if has_tcp and not has_drops:
        return "Phase_B"
    if not has_tcp:
        return "Phase_A"
    return "UNKNOWN"


def load_rows(csv_path: Path) -> List[Dict[str, str]]:
    """Load all data rows from a CSV as list of dicts (via DictReader)."""
    rows: List[Dict[str, str]] = []
    try:
        with open(csv_path, "r") as fh:
            reader = csv.DictReader(fh)
            for row in reader:
                rows.append(row)
    except (OSError, UnicodeDecodeError):
        pass
    return rows


def compute_stats(values: List[float]) -> Dict[str, float]:
    """Compute descriptive statistics for a numeric list."""
    if not values:
        return {"mean": 0, "median": 0, "std": 0, "min": 0, "max": 0, "cv": 0, "n": 0}
    n    = len(values)
    mean = statistics.mean(values)
    med  = statistics.median(values)
    std  = statistics.stdev(values) if n > 1 else 0.0
    cv   = (std / mean * 100.0) if mean != 0 else 0.0
    return {
        "mean": mean, "median": med, "std": std,
        "min": min(values), "max": max(values),
        "cv": cv, "n": n,
    }


def target_hit_rate(pc_fps_values: List[float], target: float, tolerance: float = 1.0) -> float:
    """Return the fraction of samples within [target-tol, target+tol]."""
    if not pc_fps_values:
        return 0.0
    hits = sum(1 for v in pc_fps_values if abs(v - target) <= tolerance)
    return hits / len(pc_fps_values) * 100.0


# ---------------------------------------------------------------------------
# Metric columns to analyse per phase and the float-conversion function
# ---------------------------------------------------------------------------
NUMERIC_METRICS = [
    ("pc_fps",                 float),
    ("spresense_fps",          float),
    ("decode_time_ms",         float),
    ("serial_read_time_ms",    float),
    ("texture_upload_time_ms", float),
    ("jpeg_size_kb",           float),
    ("action_q_depth",         int),
    ("spresense_errors",       int),
]

TCP_METRICS = [
    ("tcp_avg_send_ms",  float),
    ("tcp_max_send_ms",  float),
]

DROP_METRICS = [
    ("dropped_frames",  int),
    ("drop_events",     int),
]


def analyse_period(label: str, rows: List[Dict[str, str]]) -> None:
    """Print a full statistical report for one phase."""
    if not rows:
        print(f"    (no data rows in this phase)\n")
        return

    print(f"\n  {label} -- {len(rows)} samples")
    print("  " + "-" * 100)

    # Determine which extra columns are present
    sample_keys = set(rows[0].keys())
    metrics_to_run = list(NUMERIC_METRICS)
    if "tcp_avg_send_ms" in sample_keys:
        metrics_to_run.extend(TCP_METRICS)
    if "dropped_frames" in sample_keys:
        metrics_to_run.extend(DROP_METRICS)

    for col_name, conv in metrics_to_run:
        try:
            values = [conv(r[col_name]) for r in rows]
        except (KeyError, ValueError):
            continue
        s = compute_stats(values)
        print(f"    {col_name:<30} mean={s['mean']:>12.2f}  "
              f"median={s['median']:>12.2f}  std={s['std']:>12.2f}  "
              f"cv={s['cv']:>8.2f}%  "
              f"[{s['min']:.2f} .. {s['max']:.2f}]  n={s['n']}")

    # Target hit rates
    pc_fps_values = [float(r["pc_fps"]) for r in rows]
    print(f"\n    Target-hit rates for pc_fps (tolerance +/- 1.0 FPS):")
    for target in [1, 3, 5, 7, 10, 15]:
        rate = target_hit_rate(pc_fps_values, float(target))
        bar_len = int(rate / 2)  # 1 char per 2%
        print(f"      {target:>2} FPS : {rate:>7.2f}%  {'#' * bar_len}")


def print_comparison_table(phase_rows: Dict[str, List[Dict[str, str]]]) -> None:
    """Print a side-by-side comparison of key metrics across all phases."""
    key_metrics = [
        ("pc_fps (mean)",              lambda rows: statistics.mean([float(r["pc_fps"]) for r in rows])),
        ("pc_fps (std)",               lambda rows: statistics.stdev([float(r["pc_fps"]) for r in rows])),
        ("pc_fps (CV %)",              lambda rows: (statistics.stdev([float(r["pc_fps"]) for r in rows]) /
                                                      statistics.mean([float(r["pc_fps"]) for r in rows]) * 100)
                                                     if statistics.mean([float(r["pc_fps"]) for r in rows]) != 0 else 0),
        ("pc_fps (max)",               lambda rows: max(float(r["pc_fps"]) for r in rows)),
        ("spresense_fps (mean)",       lambda rows: statistics.mean([float(r["spresense_fps"]) for r in rows])),
        ("serial_read_time_ms (mean)", lambda rows: statistics.mean([float(r["serial_read_time_ms"]) for r in rows])),
        ("serial_read_time_ms (med)",  lambda rows: statistics.median([float(r["serial_read_time_ms"]) for r in rows])),
        ("jpeg_size_kb (mean)",        lambda rows: statistics.mean([float(r["jpeg_size_kb"]) for r in rows])),
        ("action_q_depth (mean)",      lambda rows: statistics.mean([int(r["action_q_depth"]) for r in rows])),
    ]

    print("\n" + "=" * 100)
    print("  CROSS-PHASE COMPARISON TABLE")
    print("=" * 100)
    print(f"  {'Metric':<35} {'Phase A':>18} {'Phase B':>18} {'Phase C':>18}")
    print("  " + "-" * 90)

    for label, fn in key_metrics:
        vals = []
        for phase in ("Phase_A", "Phase_B", "Phase_C"):
            rows = phase_rows.get(phase, [])
            if rows and len(rows) > 1:
                try:
                    vals.append(f"{fn(rows):>18.2f}")
                except (ZeroDivisionError, statistics.StatisticsError):
                    vals.append(f"{'N/A':>18}")
            else:
                vals.append(f"{'N/A':>18}")
        print(f"  {label:<35} {vals[0]} {vals[1]} {vals[2]}")

    # TCP metrics (B and C only)
    tcp_metrics = [
        ("tcp_avg_send_ms (mean)", lambda rows: statistics.mean([float(r["tcp_avg_send_ms"]) for r in rows])),
        ("tcp_avg_send_ms (med)",  lambda rows: statistics.median([float(r["tcp_avg_send_ms"]) for r in rows])),
        ("tcp_max_send_ms (mean)", lambda rows: statistics.mean([float(r["tcp_max_send_ms"]) for r in rows])),
    ]
    print("  " + "-" * 90)
    for label, fn in tcp_metrics:
        vals = ["N/A (no TCP)".rjust(18)]
        for phase in ("Phase_B", "Phase_C"):
            rows = phase_rows.get(phase, [])
            if rows:
                try:
                    vals.append(f"{fn(rows):>18.2f}")
                except Exception:
                    vals.append(f"{'N/A':>18}")
            else:
                vals.append(f"{'N/A':>18}")
        print(f"  {label:<35} {vals[0]} {vals[1]} {vals[2]}")

    # Drop metrics (C only)
    drop_metrics = [
        ("dropped_frames (mean)",  lambda rows: statistics.mean([int(r["dropped_frames"]) for r in rows])),
        ("drop_events (mean)",     lambda rows: statistics.mean([int(r["drop_events"]) for r in rows])),
    ]
    print("  " + "-" * 90)
    for label, fn in drop_metrics:
        vals = ["N/A (no tracking)".rjust(18), "N/A (no tracking)".rjust(18)]
        rows = phase_rows.get("Phase_C", [])
        if rows:
            try:
                vals.append(f"{fn(rows):>18.2f}")
            except Exception:
                vals.append(f"{'N/A':>18}")
        else:
            vals.append(f"{'N/A':>18}")
        print(f"  {label:<35} {vals[0]} {vals[1]} {vals[2]}")


def print_corrected_pid_baselines(phase_rows: Dict[str, List[Dict[str, str]]]) -> None:
    """
    Print the corrected PID baselines using ONLY Phase C data (current system).
    Phase A and B are historical; Phase 10 PID must be tuned to the system as
    it exists NOW, which is Phase C.
    """
    rows = phase_rows.get("Phase_C", [])
    if not rows:
        print("  Cannot compute -- no Phase C data.")
        return

    pc_fps  = [float(r["pc_fps"]) for r in rows]
    spr_fps = [float(r["spresense_fps"]) for r in rows]
    tcp_avg = [float(r["tcp_avg_send_ms"]) for r in rows]
    tcp_max = [float(r["tcp_max_send_ms"]) for r in rows]
    serial  = [float(r["serial_read_time_ms"]) for r in rows]
    q_depth = [int(r["action_q_depth"]) for r in rows]

    pc_mean   = statistics.mean(pc_fps)
    pc_std    = statistics.stdev(pc_fps)
    pc_cv     = pc_std / pc_mean * 100 if pc_mean else 0
    spr_mean  = statistics.mean(spr_fps)

    print("\n" + "=" * 100)
    print("  CORRECTED BASELINES FOR PHASE 10 PID DESIGN  (derived from Phase C only)")
    print("=" * 100)

    print(f"""
  Current Open-Loop Performance (Phase C -- the system as it is today)
  ----------------------------------------------------------------------
    pc_fps          mean = {pc_mean:.2f} FPS     std = {pc_std:.2f}     CV = {pc_cv:.1f}%
    spresense_fps   mean = {spr_mean:.2f} FPS
    tcp_avg_send_ms mean = {statistics.mean(tcp_avg):.2f} ms   median = {statistics.median(tcp_avg):.2f} ms
    tcp_max_send_ms mean = {statistics.mean(tcp_max):.2f} ms   median = {statistics.median(tcp_max):.2f} ms
    serial_read_ms  mean = {statistics.mean(serial):.2f} ms   median = {statistics.median(serial):.2f} ms
    action_q_depth  mean = {statistics.mean(q_depth):.2f}       median = {statistics.median(q_depth):.1f}

  What the previous analysis reported vs what is actually true
  ----------------------------------------------------------------------
    PREVIOUS (mixed-data): pc_fps mean = 4.12,  std = 2.56,  CV = 62.2%
    CORRECTED (Phase C):   pc_fps mean = {pc_mean:.2f}, std = {pc_std:.2f}, CV = {pc_cv:.1f}%

    The previous figure of 4.12 FPS was a weighted average of:
      Phase A  (~0.90 FPS, 587 samples)   -- USB-only, pre-WiFi system
      Phase B  (~1.85 FPS, 2712 samples)  -- WiFi transport still being tuned
      Phase C  (~5.32 FPS, 5115 samples)  -- current WiFi system
    Pooling those together produced an artificial baseline that does not
    represent the system Phase 10 will actually run on.

  Corrected PID Targets and Gains
  ----------------------------------------------------------------------
    Setpoint:          7 FPS  (achievable step up from current ~5.3 mean;
                                the previous target of 10 FPS was based on
                                the incorrect 4.12 baseline and is aggressive
                                for the current hardware)
    Kp (start):        0.15   (slightly higher than the 0.1 previously
                                recommended, because the current system
                                already runs faster and is more responsive)
    Ki (start):        0.02   (higher than 0.01; the system now has a lower
                                steady-state error to correct, so integral
                                action can be slightly more aggressive)
    Kd (start):        0.0    (unchanged; always start without derivative)

    Measurement rate:  1 Hz    (unchanged; still adequate for the system
                                time constant)
    Control rate:      1 Hz    (can match measurement; previously proposed
                                5 Hz was over-engineered for a ~14 s system)

    Safety limits (unchanged, still valid):
      FPS output:      [1, 30]
      Queue depth:     [0, 10]
      Integral windup: [-5.0, +5.0]

  Target progression after PID is deployed
  ----------------------------------------------------------------------
    Step 1 -- Deploy P-only at 7 FPS setpoint.  Observe settling.
    Step 2 -- If steady-state error > 0.5 FPS, add Ki = 0.02.
    Step 3 -- Once stable at 7 FPS, attempt step to 10 FPS.
    Step 4 -- If 10 FPS is sustained with CV < 15%, that becomes the
              production setpoint.  Otherwise hold at 7 FPS.
""")


def main() -> None:
    # Resolve metrics directory
    if len(sys.argv) > 1:
        metrics_dir = Path(sys.argv[1])
    else:
        metrics_dir = Path("/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics")

    if not metrics_dir.is_dir():
        print(f"ERROR: directory not found: {metrics_dir}")
        sys.exit(1)

    # ---------------------------------------------------------------------------
    # 1. Classify every file
    # ---------------------------------------------------------------------------
    print("=" * 100)
    print("  SECURITY CAMERA METRICS FILE CLASSIFIER")
    print(f"  Source: {metrics_dir}")
    print("=" * 100)

    file_phases: Dict[str, List[Path]] = {"Phase_A": [], "Phase_B": [], "Phase_C": [], "UNKNOWN": []}

    print(f"\n  {'File':<48} {'Columns':<10} {'Phase':<12} {'Data Rows'}")
    print("  " + "-" * 90)

    all_csv = sorted(metrics_dir.glob("*.csv"))
    for csv_path in all_csv:
        phase = classify_file(csv_path)
        file_phases.setdefault(phase, []).append(csv_path)

        # Count columns and data rows for display
        with open(csv_path, "r") as fh:
            header = fh.readline().strip()
            ncols  = len(header.split(","))
            nrows  = sum(1 for _ in fh)
        print(f"  {csv_path.name:<48} {ncols:<10} {phase:<12} {nrows}")

    # ---------------------------------------------------------------------------
    # 2. Summary counts
    # ---------------------------------------------------------------------------
    print("\n" + "=" * 100)
    print("  PHASE CLASSIFICATION SUMMARY")
    print("=" * 100)
    phase_labels = {
        "Phase_A": "Phase A  --  Jan 3     --  14 columns  --  USB-only, pre-TCP baseline",
        "Phase_B": "Phase B  --  Jan 11-13a  --  16 columns  --  TCP transport added (still tuning)",
        "Phase_C": "Phase C  --  Jan 13b-17  --  18 columns  --  WiFi stable, drop tracking active",
    }
    for phase in ("Phase_A", "Phase_B", "Phase_C"):
        paths = file_phases.get(phase, [])
        total_rows = 0
        for p in paths:
            with open(p, "r") as fh:
                next(fh, None)  # skip header
                total_rows += sum(1 for _ in fh)
        print(f"    {phase_labels.get(phase, phase)}")
        print(f"      Files: {len(paths):<4}  Data rows: {total_rows}")

    # ---------------------------------------------------------------------------
    # 3. Load all rows per phase and run per-phase analysis
    # ---------------------------------------------------------------------------
    phase_rows: Dict[str, List[Dict[str, str]]] = {}
    for phase in ("Phase_A", "Phase_B", "Phase_C"):
        all_rows: List[Dict[str, str]] = []
        for csv_path in file_phases.get(phase, []):
            all_rows.extend(load_rows(csv_path))
        phase_rows[phase] = all_rows

    print("\n" + "=" * 100)
    print("  PER-PHASE DETAILED STATISTICS")
    print("=" * 100)

    analyse_period("Phase A  (Jan 3, 14-col, USB-only baseline)", phase_rows.get("Phase_A", []))
    analyse_period("Phase B  (Jan 11 - Jan 13 early, 16-col, TCP added)", phase_rows.get("Phase_B", []))
    analyse_period("Phase C  (Jan 13 late - Jan 17, 18-col, current system)", phase_rows.get("Phase_C", []))

    # ---------------------------------------------------------------------------
    # 4. Cross-phase comparison table
    # ---------------------------------------------------------------------------
    print_comparison_table(phase_rows)

    # ---------------------------------------------------------------------------
    # 5. Corrected PID baselines
    # ---------------------------------------------------------------------------
    print_corrected_pid_baselines(phase_rows)

    print("\n" + "=" * 100)
    print("  CLASSIFICATION COMPLETE")
    print("=" * 100)


if __name__ == "__main__":
    main()

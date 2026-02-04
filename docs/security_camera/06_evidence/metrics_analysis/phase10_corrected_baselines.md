# Phase 10 -- Corrected Performance Baselines

**Date:** 2026-02-04
**Supersedes (partially):** The baseline numbers in `SUMMARY.md`, `performance_trends.md`,
and `control_system_validation.md` wherever they quote the pooled-data figures
(pc_fps mean = 4.12, CV = 62.2 %).  All other content in those documents
(system dynamics identification, actuator validation, safety limits, control
architecture) remains correct and is not affected.

**Classification tool:** `analysis_tools/classify_metrics.py`
**Raw data:** `/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/`

---

## 1. Why the previous baselines were wrong

The original analysis pooled all 57 CSV files into a single dataset and
computed one set of summary statistics.  Those files span three structurally
different logging formats that were produced by three different versions of
the viewer software, each representing a materially different system state.

Pooling them together is equivalent to averaging the fuel economy of a
bicycle, a sedan, and a pickup truck and calling the result "the fuel economy
of your vehicle."  The number is arithmetically correct but operationally
meaningless.

### The structural evidence

Each time new instrumentation was added to the viewer, two columns were
appended to the CSV header.  The column count is therefore an unambiguous
proxy for the software version -- and, by extension, for the system
configuration that was running when the data was recorded.

| Column count | Added columns | Dates observed | Label used here |
|:---:|---|---|---|
| 14 | (none -- original set) | Jan 3 only | Phase A |
| 16 | `tcp_avg_send_ms`, `tcp_max_send_ms` | Jan 11 -- Jan 13 (early) | Phase B |
| 18 | `dropped_frames`, `drop_events` | Jan 13 (late) -- Jan 17 | Phase C |

Jan 13 contains a split: `metrics_20260113_114023.csv` is 16-column (Phase B);
all six later Jan 13 files are 18-column (Phase C).  The classifier handles
this by inspecting each file's header independently.

### What changed between phases

| Transition | What happened | Effect on pc_fps |
|---|---|---|
| A to B | WiFi/TCP transport brought online (replacing USB-only serial) | Mean roughly doubled: 0.90 to 1.85 FPS |
| B to C | WiFi transport stabilised; frame-drop tracking instrumented | Mean roughly tripled again: 1.85 to 5.32 FPS |

Phase 10 PID control will run on the system as it exists today -- that is,
Phase C.  Tuning targets and gain starting points must be derived from Phase C
data alone.

---

## 2. File classification manifest

57 files total.  Classification is driven by header column set, not by date.

| Phase | Files | Data rows | Date range |
|---|:---:|:---:|---|
| A | 13 | 587 | Jan 3 |
| B | 20 | 2 712 | Jan 11 -- Jan 13 (early) |
| C | 24 | 5 115 | Jan 13 (late) -- Jan 17 |

Run `python3 analysis_tools/classify_metrics.py` to regenerate the full
per-file manifest and all statistics below from the live data.

---

## 3. Per-phase statistics

All values are computed from the raw CSV rows in each phase.  No rows from
any other phase are included in any column.

### 3.1 Phase A -- Jan 3 -- USB-only baseline (587 samples)

| Metric | Mean | Median | Std | CV | Min | Max |
|---|---:|---:|---:|---:|---:|---:|
| pc_fps | 0.90 | 0.83 | 0.46 | 51.2 % | 0.10 | 2.98 |
| spresense_fps | 3.67 | 3.52 | 0.72 | 19.5 % | 0.00 | 6.86 |
| serial_read_time_ms | 1 113.9 | 1 053.7 | 521.4 | 46.8 % | 221.2 | 4 071.6 |
| decode_time_ms | 3.32 | 2.95 | 1.23 | 37.0 % | 2.06 | 13.08 |
| jpeg_size_kb | 50.28 | 53.21 | 6.11 | 12.2 % | 27.0 | 58.9 |
| action_q_depth | 0.00 | 0.00 | 0.00 | -- | 0 | 0 |

**pc_fps target hits (tolerance +/- 1.0 FPS):**
1 FPS: 98.6 % -- 3 FPS: 1.5 % -- 5 FPS: 0.0 % -- 7 FPS: 0.0 % -- 10 FPS: 0.0 %

**Interpretation.**  The system was delivering fewer than 1 frame per second on
the PC side.  The queue was always empty and Spresense errors were zero:
nothing was actually arriving fast enough to queue up.  This phase is
historically interesting but has no bearing on Phase 10 tuning.

### 3.2 Phase B -- Jan 11 to Jan 13 (early) -- TCP transport added (2 712 samples)

| Metric | Mean | Median | Std | CV | Min | Max |
|---|---:|---:|---:|---:|---:|---:|
| pc_fps | 1.85 | 1.22 | 1.45 | 78.5 % | 0.01 | 9.80 |
| spresense_fps | 4.39 | 3.80 | 2.27 | 51.8 % | 0.00 | 17.33 |
| serial_read_time_ms | 775.3 | 785.4 | 961.0 | 123.9 % | 94.3 | 30 964.1 |
| decode_time_ms | 3.44 | 2.99 | 1.35 | 39.2 % | 1.87 | 16.50 |
| jpeg_size_kb | 48.00 | 52.42 | 10.86 | 22.6 % | 12.3 | 60.0 |
| action_q_depth | 0.21 | 0.00 | 0.87 | 411.9 % | 0 | 4 |
| tcp_avg_send_ms | 9.97 | 0.00 | 39.06 | 391.9 % | 0.00 | 172.7 |
| tcp_max_send_ms | 28.95 | 0.00 | 113.4 | 391.7 % | 0.00 | 472.9 |

**pc_fps target hits (tolerance +/- 1.0 FPS):**
1 FPS: 69.2 % -- 3 FPS: 27.4 % -- 5 FPS: 1.1 % -- 7 FPS: 0.4 % -- 10 FPS: 1.0 %

**Interpretation.**  TCP was present but not yet working reliably -- the median
for both tcp_avg_send_ms and tcp_max_send_ms is 0.00, meaning the majority of
samples recorded no TCP activity at all.  The serial read path was still the
dominant transport.  This phase documents the transition period and is not
representative of the current system.

### 3.3 Phase C -- Jan 13 (late) to Jan 17 -- current system (5 115 samples)

**This is the only phase whose numbers should be used for Phase 10 design.**

| Metric | Mean | Median | Std | CV | Min | Max |
|---|---:|---:|---:|---:|---:|---:|
| pc_fps | 5.32 | 5.72 | 2.18 | 41.0 % | 0.03 | 16.81 |
| spresense_fps | 12.61 | 12.86 | 3.89 | 30.9 % | 0.00 | 30.00 |
| serial_read_time_ms | 275.5 | 183.2 | 580.8 | 210.8 % | 11.5 | 34 228.2 |
| decode_time_ms | 2.22 | 2.16 | 1.13 | 51.1 % | 0.00 | 14.81 |
| jpeg_size_kb | 33.16 | 31.21 | 7.83 | 23.6 % | 10.2 | 60.0 |
| action_q_depth | 3.91 | 4.00 | 1.10 | 28.3 % | 0 | 5 |
| tcp_avg_send_ms | 155.3 | 158.9 | 57.0 | 36.7 % | 0.00 | 1 090.7 |
| tcp_max_send_ms | 1 519.6 | 1 741.3 | 656.6 | 43.2 % | 0.00 | 2 712.9 |
| dropped_frames | 11 983.7 | 8 787 | 11 289.3 | 94.2 % | 0 | 41 389 |
| drop_events | 3 995.3 | 2 929 | 3 763.0 | 94.2 % | 0 | 13 797 |

**pc_fps target hits (tolerance +/- 1.0 FPS):**
1 FPS: 9.1 % -- 3 FPS: 18.7 % -- 5 FPS: 30.2 % -- 7 FPS: 33.9 % -- 10 FPS: 2.0 %

**Interpretation.**  The system is now centered around 5-7 FPS in open loop.
The 7 FPS band has the highest hit rate (33.9 %), meaning that is roughly where
the system naturally settles when it is working well.  10 FPS is achievable but
rare (2.0 %) without active control.  TCP is fully populated and the drop
counters are cumulative (they grow monotonically within each session; they are
not per-sample rates).

---

## 4. What the previous analysis said vs what is actually true

| Metric | Previous (pooled) | Corrected (Phase C only) | Source of discrepancy |
|---|---|---|---|
| pc_fps mean | 4.12 FPS | 5.32 FPS | Phase A (0.90) and B (1.85) dragged average down |
| pc_fps std | 2.56 FPS | 2.18 FPS | Cross-phase variance inflated pooled std |
| pc_fps CV | 62.2 % | 41.0 % | Same cause as above |
| pc_fps max | 16.81 FPS | 16.81 FPS | Unchanged -- max came from Phase C |
| tcp_avg_send_ms mean | 104.95 ms | 155.32 ms | Phase B zeros (median = 0) dragged pooled mean down |
| serial_read_time_ms mean | 448.71 ms | 275.54 ms | Phase A (1 114 ms) and B (775 ms) dragged pooled mean up |
| action_q_depth mean | 2.63 | 3.91 | Phase A (0.0) and B (0.21) dragged pooled mean down |

Every single pooled figure was pulled away from the current system's actual
behaviour.  The direction of the error was not consistent -- some metrics were
over-reported, some under-reported -- which is exactly why pooling across
structurally different periods is dangerous.

---

## 5. Corrected PID starting parameters for Phase 10

The following replaces the "Start Conservative" recommendation in
`control_system_validation.md` (Section "Recommendations for Phase 10") and
the initial gains in `SUMMARY.md` (Section "For Implementation").

### 5.1 Initial setpoint

**7 FPS.**

Rationale:
- The 7 FPS band already has the highest open-loop hit rate (33.9 %).  The
  system naturally drifts through this region.  A PID controller only needs to
  hold it there, which is the easiest possible first task.
- The previous recommendation of 10 FPS as the initial tuning target was
  derived from the pooled mean of 4.12 FPS (a 143 % step up).  From the
  corrected mean of 5.32 FPS, 10 FPS is a 88 % step up -- still large for a
  first deployment.  7 FPS is a 32 % step up, which is conservative and safe.
- 10 FPS remains the production target.  The procedure in Section 5.3 below
  describes how to reach it after 7 FPS is locked in.

### 5.2 Initial gains

| Parameter | Previous recommendation | Corrected recommendation | Rationale |
|---|---|---|---|
| Kp | 0.1 | 0.15 | System is already running faster than the pooled baseline assumed.  The plant is more responsive.  0.15 is the value already present in the gain-scheduling table in `quick_reference/PID_Quick_Reference.md` for the 5-10 FPS band. |
| Ki | 0.01 | 0.02 | The steady-state error from 7 FPS is smaller than the error from 10 FPS that the previous analysis was sizing Ki for.  Integral action can be slightly more aggressive.  Again, 0.02 is already in the existing gain-scheduling table for this band. |
| Kd | 0.0 | 0.0 | Unchanged.  Never start with derivative. |

These gains are consistent with the gain-scheduling table already present in
`quick_reference/PID_Quick_Reference.md`:

```rust
fn get_gains(setpoint: f32) -> (f32, f32, f32) {
    match setpoint {
        sp if sp <= 5.0  => (0.10, 0.01, 0.0),  // Conservative
        sp if sp <= 10.0 => (0.15, 0.02, 0.0),  // <-- use this row
        sp if sp <= 15.0 => (0.20, 0.03, 0.0),  // Aggressive
        _                => (0.25, 0.05, 0.0),  // Maximum
    }
}
```

### 5.3 Target progression after deployment

```
Step 1:  Deploy P-only (Kp=0.15, Ki=0, Kd=0) with setpoint = 7 FPS.
         Observe open-loop response for at least 5 minutes.
         Confirm settling time and that FPS does not oscillate.

Step 2:  Enable integral (Ki=0.02).  Monitor steady-state error.
         Target: error < 0.5 FPS within 30 seconds.

Step 3:  Once 7 FPS is stable (CV < 15 % over a 2-minute window),
         step to 10 FPS.  Use the setpoint ramper at 5 FPS/sec
         (already specified in PID_Control_Architecture.md).

Step 4:  If 10 FPS is sustained with CV < 15 % for 5 minutes,
         10 FPS becomes the production setpoint.
         If not, hold at 7 FPS and diagnose before retrying.
```

### 5.4 What stays the same

The following items from the previous analysis are correct and do not need
revision:

- **System order:** 2nd order, confirmed by step-response analysis.
- **Time constant:** 14.05 s.  This is a property of the plant dynamics, not
  of the data pooling.
- **Measurement rate:** 1 Hz is adequate (Nyquist criterion exceeded by 45x).
- **Control loop rate:** 1 Hz is sufficient for a 14 s time constant.  The
  previously proposed 10 Hz loop is over-engineered; 1 Hz matches the
  measurement rate and simplifies implementation.  10 Hz can be added later if
  needed for smoothness.
- **Safety limits:** FPS output [1, 30], queue depth [0, 10], integral windup
  [-5.0, +5.0] -- all validated against the Phase C operating range.
- **Setpoint ramper:** max rate 5 FPS/sec, as specified in the architecture doc.
- **Fault detection thresholds:** unchanged; they are sized against the
  hardware limits, not the baseline statistics.
- **Disturbance ranking:** serial_read_time_ms remains the primary disturbance
  (CV = 210.8 % in Phase C).  Network latency is secondary.  This ranking was
  already identified as a refinement in `control_system_validation.md`.

---

## 6. Corrected improvement projections

The previous document projected improvements relative to the pooled baseline.
The table below re-states those projections relative to the Phase C baseline.

| Metric | Phase C open-loop | PID target | Improvement | Notes |
|---|---|---|---|---|
| pc_fps mean | 5.32 FPS | 7.0 FPS (step 1), 10.0 FPS (production) | +32 % / +88 % | Smaller gap than the pooled analysis assumed |
| pc_fps CV | 41.0 % | < 15 % | 63 % reduction | Previous said 76 % reduction from 62.2 % -- same absolute target, smaller gap to close |
| Steady-state error | ~1.7 FPS (from 7 FPS) | < 0.5 FPS | 71 % reduction | Measured as `setpoint - mean` for current open-loop |
| Target hit rate (7 FPS) | 33.9 % | > 90 % | 165 % increase | Achievable because system already visits this region frequently |
| Target hit rate (10 FPS) | 2.0 % | > 90 % | 4 400 % increase | Requires sustained control; validate at Step 4 |
| serial_read_time_ms | 275.5 ms mean, 183.2 ms median | (not directly controllable) | -- | PID will absorb its effect through feedback; feed-forward optional |

---

## 7. Relationship to other documents

| Document | Status after this correction |
|---|---|
| `SUMMARY.md` -- "Key Metrics at a Glance" | Baseline numbers are superseded by Section 3.3 of this document.  Everything else in SUMMARY.md remains valid. |
| `performance_trends.md` -- "Performance Baseline for Phase 10" | The comparison table in that section used pooled numbers.  Use the Phase C column from Section 3.3 here instead. |
| `control_system_validation.md` -- "Recommendations for Phase 10" | The gain recommendations (Kp=0.1, Ki=0.01) are superseded by Section 5.2 here.  All validation conclusions remain valid. |
| `quick_reference/PID_Quick_Reference.md` -- "Performance Targets" table | The "Current Baseline" column used pooled numbers.  Replace with Phase C values from Section 3.3. |
| `quick_reference/PID_Quick_Reference.md` -- gain-scheduling table | Already correct.  The (0.15, 0.02, 0.0) row for 5-10 FPS matches the corrected starting gains. |
| `architecture/PID_Control_Architecture.md` -- Disturbance Model | The disturbance numbers quoted there are pooled.  Replace tcp_avg_send_ms with 155.3 ms (mean) and serial_read_time_ms with 275.5 ms (mean) / 183.2 ms (median) from Phase C. |
| `control_system_validation.md` -- system dynamics, actuator, safety limits | All correct.  No changes needed. |

---

**Generated:** 2026-02-04
**Tool:** `analysis_tools/classify_metrics.py` (run against 57 files, 8 414 total data rows)
**Data version:** files dated Jan 3 -- Jan 17 in the metrics directory; no files have been added or removed.

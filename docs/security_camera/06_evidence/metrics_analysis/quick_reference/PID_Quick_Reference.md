# PID Control Quick Reference Guide

**For:** Phase 10 Security Camera Implementation
**Target:** FPS Regulation via PI Control

---

## Quick Start Configuration

```rust
// Initial conservative tuning parameters
const KP: f32 = 0.1;          // Proportional gain
const KI: f32 = 0.01;         // Integral gain
const KD: f32 = 0.0;          // Derivative gain (disabled)

const SAMPLE_RATE_HZ: f32 = 10.0;  // 100ms sample period
const INTEGRAL_LIMIT: f32 = 5.0;    // Anti-windup limit

const FPS_MIN: f32 = 1.0;
const FPS_MAX: f32 = 30.0;
```

---

## Controller Equation

```
dt = 1.0 / sample_rate_hz

error = setpoint - measured_fps

p_term = Kp * error

integral += error * dt
integral = clamp(integral, -integral_limit, +integral_limit)
i_term = Ki * integral

d_term = Kd * (error - last_error) / dt

output = clamp(p_term + i_term + d_term, FPS_MIN, FPS_MAX)
```

---

## Recommended Setpoints

| FPS  | Use Case                    | Expected Performance       |
|------|----------------------------|----------------------------|
| 5    | Low bandwidth / power save | Best stability (CV < 10%)  |
| 10   | Balanced performance       | Good stability (CV < 15%)  |
| 15   | Smoother playback          | Moderate stability         |
| 30   | Maximum quality            | Requires tuning            |

**Start with 10 FPS for initial tuning**

---

## Tuning Procedure

### Step 1: Find Proportional Gain

```
1. Set Ki = 0, Kd = 0
2. Set Kp = 0.05
3. Apply step from 5 → 10 FPS
4. Increase Kp by 0.05 until oscillation
5. Record critical gain (Kc)
6. Set Kp = 0.5 * Kc
```

### Step 2: Add Integral Gain

```
1. Set Ki = Kp / 10
2. Apply step from 5 → 10 FPS
3. Check steady-state error:
   - If error > 0.5 FPS: Increase Ki by 0.005
   - If overshoot > 10%: Decrease Ki by 0.005
4. Repeat until settled
```

### Step 3: Validate

```
- Steady-state error < 0.5 FPS ✓
- Settling time < 2 seconds ✓
- Overshoot < 10% ✓
- No sustained oscillation ✓
```

---

## Performance Targets

| Metric                 | Target      | Current Baseline |
|------------------------|-------------|------------------|
| Steady-state error     | < 0.5 FPS   | 2.35 FPS         |
| Settling time          | < 2 sec     | N/A (no control) |
| Overshoot              | < 10%       | N/A              |
| Setpoint tracking (5)  | > 95%       | 20.1%            |
| Setpoint tracking (10) | > 90%       | 1.6%             |
| FPS stability (CV)     | < 15%       | 62.2%            |

---

## Common Issues and Solutions

### Issue: Large Steady-State Error

**Symptoms:** FPS settles below/above setpoint
**Cause:** Ki too small or integral term saturated
**Solution:**
```
- Increase Ki by 0.005
- Check integral term isn't hitting limits
- Verify actuator not saturated
```

### Issue: Oscillation

**Symptoms:** FPS alternates above/below setpoint
**Cause:** Kp too high or Ki too high
**Solution:**
```
- Reduce Kp by 20%
- Reduce Ki by 50%
- Reset integral term to zero
```

### Issue: Slow Response

**Symptoms:** Takes > 5 seconds to reach setpoint
**Cause:** Kp too small
**Solution:**
```
- Increase Kp by 0.05
- Monitor for overshoot
```

### Issue: Overshoot

**Symptoms:** FPS exceeds setpoint by > 10%
**Cause:** Kp too high or derivative term needed
**Solution:**
```
- Reduce Kp by 10%
- Consider adding Kd = 0.1 * Kp
```

---

## Data to Log (for tuning)

```rust
struct ControlLog {
    timestamp: f64,
    setpoint: f32,
    measured_fps: f32,
    error: f32,
    p_term: f32,
    i_term: f32,
    d_term: f32,
    output: f32,
    queue_depth: usize,
}
```

**Log at 10 Hz during tuning, 1 Hz during normal operation**

---

## Safety Checks

```rust
// Before each control update
assert!(measured_fps >= 0.0 && measured_fps <= 40.0);
assert!(output >= FPS_MIN && output <= FPS_MAX);
assert!(integral.abs() <= INTEGRAL_LIMIT);

// Fault detection
if error.abs() > 10.0 for > 5 seconds {
    reset_controller();
    log_fault("Excessive error");
}

if output saturated for > 5 seconds {
    reset_integral();
    log_warning("Actuator saturated");
}
```

---

## Expected Timeline

| Week | Phase           | Deliverable                        |
|------|-----------------|-------------------------------------|
| 1    | Infrastructure  | FPS measurement, timing framework   |
| 2    | P Control       | Working proportional controller     |
| 3    | PI Control      | Tuned PI controller                 |
| 4    | Validation      | Performance tests passed            |

---

## Key Equations

### FPS Measurement (Rolling Window)

```rust
fn calculate_fps(window: &VecDeque<(Instant, u32)>) -> f32 {
    if window.len() < 2 { return 0.0; }

    let oldest = window.front().unwrap();
    let newest = window.back().unwrap();

    let duration_sec = newest.0.duration_since(oldest.0).as_secs_f32();
    let frame_delta = newest.1 - oldest.1;

    frame_delta as f32 / duration_sec
}
```

### Anti-Windup (Integral Clamping)

```rust
fn update_integral(integral: &mut f32, error: f32, dt: f32, limit: f32) {
    *integral += error * dt;
    *integral = integral.clamp(-limit, limit);
}
```

### Setpoint Ramping (Bumpless Transfer)

```rust
fn ramp_setpoint(current: f32, target: f32, max_rate: f32, dt: f32) -> f32 {
    let delta = target - current;
    let max_change = max_rate * dt;
    current + delta.clamp(-max_change, max_change)
}
```

---

## Test Cases

### TC-01: Step Response (5 → 10 FPS)

```
1. Set setpoint to 5 FPS
2. Wait for settling (< 2 sec)
3. Verify error < 0.5 FPS
4. Step to 10 FPS
5. Measure:
   - Settling time (should be < 2 sec)
   - Overshoot (should be < 10%)
   - Final error (should be < 0.5 FPS)
```

### TC-02: Disturbance Rejection

```
1. Maintain 10 FPS setpoint
2. Introduce network delay (+200ms)
3. Verify:
   - FPS dip < 2 FPS
   - Recovery time < 2 sec
   - Final error < 0.5 FPS
```

### TC-03: Setpoint Tracking

```
1. Ramp from 5 → 30 FPS over 10 sec
2. Verify:
   - Tracking error < 1 FPS throughout
   - No oscillation
   - Smooth response
```

### TC-04: Stability Test

```
1. Set to 10 FPS
2. Run for 30 minutes
3. Calculate:
   - Mean FPS (should be 10.0 ± 0.2)
   - Std dev (should be < 0.5)
   - Frame drops (should be < 1%)
```

---

## Gain Scheduling (Advanced)

If single set of gains doesn't work for all setpoints:

```rust
fn get_gains(setpoint: f32) -> (f32, f32, f32) {
    match setpoint {
        sp if sp <= 5.0  => (0.10, 0.01, 0.0),  // Conservative
        sp if sp <= 10.0 => (0.15, 0.02, 0.0),  // Balanced
        sp if sp <= 15.0 => (0.20, 0.03, 0.0),  // Aggressive
        _                => (0.25, 0.05, 0.0),  // Maximum
    }
}
```

---

## Useful Metrics

### Coefficient of Variation (Stability Metric)

```
CV = std_dev / mean

Interpretation:
  CV < 10%  - Excellent stability
  CV < 15%  - Good stability (target)
  CV < 25%  - Acceptable stability
  CV > 25%  - Poor stability (needs tuning)
```

### Control Effort

```
effort = |output - setpoint| / setpoint

Interpretation:
  effort < 10%  - Minimal correction needed
  effort < 30%  - Normal operation (target)
  effort > 50%  - System struggling (check disturbances)
```

### Settling Time

```
Time for |error| < 2% of setpoint

Example for 10 FPS setpoint:
  Settled when |error| < 0.2 FPS
```

---

## Code Checklist

Implementation checklist:

- [ ] FPS measurement with 1-second rolling window
- [ ] 10 Hz control loop timer
- [ ] PI controller implementation
- [ ] Anti-windup (integral clamping)
- [ ] Output saturation (1-30 FPS)
- [ ] Error checking (sensor range, timeout)
- [ ] Fault detection and recovery
- [ ] Logging (10 Hz during tuning, 1 Hz normal)
- [ ] Setpoint change handling (ramping)
- [ ] Reset/initialization logic
- [ ] Performance metrics collection
- [ ] Unit tests (P term, I term, saturation)
- [ ] Integration tests (step response, tracking)

---

## Resources

**Analysis Report:** `/home/ken/Spr_ws/GH_wk_test/docs/security_camera/01_requirements/PID_Control_Analysis_Report.md`

**Analysis Tool:** `/home/ken/Spr_ws/GH_wk_test/docs/security_camera/01_requirements/metrics_pid_analyzer.py`

**Metrics Data:** `/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/`

**Sample Count:** 7,827 from 57 files

---

**Last Updated:** 2026-02-03
**Status:** Ready for implementation

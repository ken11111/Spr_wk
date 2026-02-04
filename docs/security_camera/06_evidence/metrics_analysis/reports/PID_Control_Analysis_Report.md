# Security Camera PID Control Analysis Report

**Date:** 2026-02-03
**Analysis Tool:** `/home/ken/Spr_ws/GH_wk_test/docs/security_camera/01_requirements/metrics_pid_analyzer.py`
**Data Source:** `/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/`
**Samples Analyzed:** 7,827 from 57 CSV files
**Purpose:** Inform Phase 10 PID control implementation

---

## Executive Summary

This report analyzes performance metrics from the security camera system to design and implement PID (Proportional-Integral-Derivative) control for stable frame rate regulation. The analysis reveals significant opportunities for improvement through closed-loop control:

### Key Findings
- **Current Open-Loop Performance:** Only 20.1% of samples maintain 5 FPS target (±1 FPS tolerance)
- **System Variability:** 62.2% coefficient of variation in PC FPS indicates poor stability
- **Control Potential:** System dynamics support PI control implementation with 10 Hz sample rate
- **Primary Target:** 5-10 FPS represents optimal balance between responsiveness and stability

### Recommended Approach
Implement **Cascaded PI Control** with:
1. **Primary Loop:** PC FPS control (10 Hz sample rate, PI controller)
2. **Secondary Loop:** Queue depth management (20 Hz sample rate, P controller)
3. **Feedforward:** TCP latency compensation

---

## 1. Current System Performance Analysis

### 1.1 FPS Performance (Primary Control Variable)

#### PC FPS (Display Frame Rate)
```
Mean:      4.12 FPS
Median:    3.92 FPS
Std Dev:   2.56 FPS
CV:        62.20%    (High variability - needs control)
Range:     0.01 - 16.81 FPS
```

**Analysis:**
- High coefficient of variation (62.2%) indicates unstable, uncontrolled behavior
- Mean FPS (4.12) below typical targets suggests systematic underperformance
- Wide range (0.01-16.81) shows system lacks regulation mechanism

#### Spresense FPS (Camera Capture Rate)
```
Mean:      9.76 FPS
Median:    10.00 FPS
Std Dev:   5.19 FPS
CV:        53.23%    (High variability)
Range:     0.00 - 30.00 FPS
```

**Analysis:**
- Median at 10 FPS suggests hardware capable of stable mid-range performance
- High CV indicates poor regulation despite capable hardware
- Captures at higher rate than display can process (9.76 vs 4.12 FPS)

### 1.2 Setpoint Tracking Analysis

Current system performance against potential setpoints (±1 FPS tolerance):

| Target FPS | In-Range % | Mean Absolute Error |
|-----------|-----------|---------------------|
| 5 FPS     | 20.1%     | 2.35 FPS            |
| 10 FPS    | 1.6%      | 5.89 FPS            |
| 15 FPS    | 0.0%      | 10.88 FPS           |
| 30 FPS    | 0.0%      | 25.88 FPS           |

**Key Insight:** System naturally hovers near 5 FPS but with poor regulation. This suggests 5 FPS as natural equilibrium point and good initial setpoint for PID control.

### 1.3 Stability Metrics

```
Frame-to-Frame FPS Variation:
  Mean:    0.50 FPS
  StdDev:  0.85 FPS
  Max:     14.89 FPS

Stability Score: 66.7/100
```

**Analysis:**
- Mean variation of 0.5 FPS acceptable for smooth playback
- Max variation of 14.89 FPS indicates occasional severe disruptions
- Stability score of 66.7 indicates moderate baseline stability

---

## 2. System Dynamics Characterization

### 2.1 Time-Domain Response

```
Estimated Time Constant:  14,047.67 ms (14.0 seconds)
Estimated Settling Time:  2,000 ms (2.0 seconds)
System Order:             2 (second-order system)
```

**Analysis:**
- **Long time constant (14 seconds):** System responds slowly to changes, likely due to buffering and averaging in current implementation
- **Faster settling time (2 seconds):** Actual dynamics faster than time constant suggests, indicating complex multi-path system
- **Second-order behavior:** Shows potential for overshoot and oscillation, requires careful tuning

### 2.2 Disturbance Analysis

Major disturbances affecting FPS stability:

#### Network Latency (TCP Send Times)
```
TCP Average Send Time:
  Mean:    104.95 ms
  Median:  126.46 ms
  StdDev:  86.25 ms
  CV:      82.18%    (High jitter)

Network Jitter:
  Mean:    0.83 ms
  StdDev:  16.67 ms
  Max:     923.81 ms
```

**Impact:** High variability in network latency (CV=82%) is major disturbance source. Recommends feedforward compensation based on measured TCP latency.

#### Processing Time Variations
```
Serial Read Time (Dominant):
  Mean:    448.71 ms
  StdDev:  772.56 ms
  CV:      172.17%   (Extremely high variability)

Decode Time:
  Mean:    2.64 ms
  StdDev:  1.34 ms
  CV:      50.89%
```

**Impact:** Serial read time dominates processing latency and shows extreme variability. This is unpredictable disturbance that PI control must reject.

### 2.3 Buffer Management

```
Action Queue Depth:
  Mean:    2.63
  Median:  4.00
  Range:   0-5
  CV:      77.58%

Queue Depth Changes:
  Mean:    0.25
  StdDev:  0.49
```

**Analysis:**
- Queue typically operates between 2-4 items (healthy)
- Moderate variability suggests responsive queue management
- Fast dynamics (100ms time constant) suitable for secondary control loop

---

## 3. Control System Design

### 3.1 Control Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    CASCADED CONTROL SYSTEM                  │
└─────────────────────────────────────────────────────────────┘

Primary Loop (Outer Loop - FPS Control):
┌──────────┐    ┌──────────────┐    ┌──────────────┐    ┌─────────┐
│ Setpoint │───>│ PI Controller│───>│ Rate Limiter │───>│Spresense│
│ (5-30FPS)│    │   (10 Hz)    │    │  (1-30 FPS)  │    │ Request │
└──────────┘    └──────────────┘    └──────────────┘    └─────────┘
      ▲                                                        │
      │                                                        ▼
      │         ┌──────────────────┐                   ┌─────────────┐
      └─────────│ PC FPS Measured  │<──────────────────│   System    │
                └──────────────────┘                   └─────────────┘

Secondary Loop (Inner Loop - Queue Control):
┌──────────┐    ┌─────────────┐    ┌──────────────┐
│Queue SP  │───>│ P Controller│───>│ Flow Control │
│  (3-4)   │    │   (20 Hz)   │    │              │
└──────────┘    └─────────────┘    └──────────────┘
      ▲                                    │
      │         ┌──────────────┐           │
      └─────────│Queue Measured│<──────────┘
                └──────────────┘

Feedforward Compensation:
┌──────────────┐
│ TCP Latency  │────> (Compensation) ───> Add to control output
└──────────────┘
```

### 3.2 Primary Control Loop - PC FPS

**Process Variable:** PC FPS (frames per second displayed)
**Setpoint Options:** 5, 10, 15, 30 FPS
**Controller Type:** PI (Proportional-Integral)
**Sample Rate:** 10 Hz (100 ms period)

**Actuators:**
1. Frame request rate to Spresense (primary)
2. Queue depth adjustment (via rate)
3. Compression level hints (future)

**Disturbances:**
- Network latency variations
- Processing time variations
- System load changes
- Serial communication delays

### 3.3 Secondary Control Loop - Queue Depth

**Process Variable:** Action queue depth
**Setpoint:** 3-4 items
**Controller Type:** P (Proportional only)
**Sample Rate:** 20 Hz (50 ms period)

**Purpose:** Fast-acting inner loop prevents queue overflow/underflow

### 3.4 Control Parameters

Based on system dynamics analysis:

#### Option 1: Conservative PI Tuning (Recommended Start)
```
Proportional Gain (Kp):  0.1
Integral Gain (Ki):      0.01
Derivative Gain (Kd):    0.0  (disabled)
Sample Rate:             10 Hz
```

#### Option 2: Aggressive PI Tuning (After Validation)
```
Proportional Gain (Kp):  0.5
Integral Gain (Ki):      0.05
Derivative Gain (Kd):    0.0
Sample Rate:             10 Hz
```

#### Queue Controller
```
Proportional Gain (Kp):  1.0
Sample Rate:             20 Hz
```

**Anti-Windup Protection:**
```
Integral Term Limits:    ±5.0
Control Output Limits:   1-30 FPS
Setpoint Rate Limit:     5 FPS/second
```

---

## 4. Implementation Roadmap

### Phase 1: Measurement Infrastructure (Week 1)

**Objectives:**
- Implement accurate FPS measurement with rolling window
- Add sample rate timing for control loops
- Create control metrics logging

**Tasks:**
1. Add FPS measurement module:
   - Rolling window (1 second for 10 Hz sampling)
   - Timestamp-based calculation
   - Outlier rejection

2. Implement timing framework:
   - 10 Hz primary loop timer
   - 20 Hz secondary loop timer
   - 1 Hz monitoring loop

3. Metrics collection:
   - Log setpoint, measured FPS, control output
   - Log error, integral term, derivative term
   - Log queue depth, latencies

**Success Criteria:**
- FPS measurement accurate within ±0.1 FPS
- Control loops maintain timing within ±5ms
- Metrics logged at full resolution

### Phase 2: P-Only Controller (Week 2)

**Objectives:**
- Implement proportional-only control
- Characterize system response
- Identify critical gain for oscillation

**Tasks:**
1. Implement P controller:
   ```rust
   fn p_controller(setpoint: f32, measured: f32, kp: f32) -> f32 {
       let error = setpoint - measured;
       let output = kp * error;
       saturate(output, 1.0, 30.0)  // Limit to valid FPS range
   }
   ```

2. Experimental tuning:
   - Start with Kp = 0.05
   - Increase by 0.05 increments
   - Record overshoot, settling time
   - Find oscillation point (critical gain Kc)

3. Characterization tests:
   - Step response (5 → 10 FPS)
   - Disturbance rejection
   - Setpoint tracking

**Success Criteria:**
- System responds to setpoint changes
- No sustained oscillation at Kp < 1.0
- Identify steady-state error magnitude

### Phase 3: PI Controller (Week 3)

**Objectives:**
- Add integral term to eliminate steady-state error
- Tune for optimal transient response
- Validate setpoint tracking

**Tasks:**
1. Implement PI controller:
   ```rust
   struct PIController {
       kp: f32,
       ki: f32,
       integral: f32,
       integral_limit: f32,
       last_error: f32,
       dt: f32,  // Sample period
   }

   impl PIController {
       fn update(&mut self, setpoint: f32, measured: f32) -> f32 {
           let error = setpoint - measured;

           // Proportional term
           let p_term = self.kp * error;

           // Integral term with anti-windup
           self.integral += error * self.dt;
           self.integral = clamp(self.integral,
                                 -self.integral_limit,
                                 self.integral_limit);
           let i_term = self.ki * self.integral;

           // Control output
           let output = p_term + i_term;
           saturate(output, 1.0, 30.0)
       }
   }
   ```

2. Tuning approach:
   - Use Kp from Phase 2 (reduced by 50% from oscillation point)
   - Start with Ki = Kp / 10
   - Increase Ki until acceptable settling time
   - Validate no overshoot > 20%

3. Performance validation:
   - Steady-state error < 0.5 FPS
   - Settling time < 2 seconds
   - Overshoot < 10%

**Success Criteria:**
- Zero steady-state error at all setpoints
- Meets performance targets
- Stable under disturbances

### Phase 4: Advanced Features (Week 4)

**Objectives:**
- Add derivative term if needed
- Implement feedforward compensation
- Add adaptive tuning

**Optional Tasks:**

1. **PID Controller** (if needed for overshoot reduction):
   ```rust
   // Add derivative term
   let d_term = self.kd * (error - self.last_error) / self.dt;
   let output = p_term + i_term + d_term;
   self.last_error = error;
   ```

2. **Feedforward Compensation:**
   ```rust
   // Compensate for known disturbances
   let ff_term = estimate_latency_impact(tcp_avg_ms);
   let output = p_term + i_term + ff_term;
   ```

3. **Adaptive Tuning:**
   - Monitor system performance
   - Adjust gains based on performance metrics
   - Reduce gains during high disturbance periods

4. **Bumpless Transfer:**
   - Smooth setpoint changes
   - Ramp setpoint at max 5 FPS/second
   - Pre-charge integral term on mode changes

**Success Criteria:**
- System handles all operational scenarios
- Graceful degradation under extreme conditions
- Runtime tuning capability

---

## 5. Tuning Methodology

### 5.1 Ziegler-Nichols Method (Ultimate Gain)

**Not recommended** for this system due to:
- Slow dynamics (14 second time constant)
- Second-order behavior with overshoot potential
- Would require extended oscillation testing

### 5.2 Recommended Approach: Iterative Experimental Tuning

#### Step 1: Proportional Gain (Kp)
1. Set Ki = 0, Kd = 0
2. Start with Kp = 0.05
3. Apply step input (5 → 10 FPS)
4. Observe response:
   - Underdamped: Increase Kp
   - Overdamped: Continue increasing
5. Increase Kp until system oscillates
6. Record critical gain (Kc)
7. Set Kp = 0.5 * Kc

#### Step 2: Integral Gain (Ki)
1. Start with Ki = Kp / 10
2. Apply step input
3. Observe steady-state error:
   - If error > 0.5 FPS: Increase Ki
   - If overshoot > 10%: Decrease Ki
4. Fine-tune for settling time < 2 seconds

#### Step 3: Validation
1. Test all setpoints (5, 10, 15, 30 FPS)
2. Test disturbance rejection:
   - Introduce network delay
   - Change system load
   - Vary image complexity
3. Test setpoint tracking:
   - Ramp setpoints
   - Step changes
4. Long-term stability (30+ minutes)

### 5.3 Performance Metrics

Monitor during tuning:

```
Key Metrics:
- Steady-state error:    < 0.5 FPS
- Settling time:         < 2 seconds
- Overshoot:            < 10%
- Rise time:            0.5-1.5 seconds
- Control effort (CV):   < 30%

Quality Metrics:
- Frame drops:           < 1% per minute
- Queue stability:       2-4 items (±1)
- CPU usage:            < 5% for control loop
- Latency overhead:     < 10 ms
```

---

## 6. Error Handling and Safety

### 6.1 Safety Limits

```rust
const MIN_FPS_REQUEST: f32 = 1.0;
const MAX_FPS_REQUEST: f32 = 30.0;
const MAX_INTEGRAL_TERM: f32 = 5.0;
const MAX_SETPOINT_RATE: f32 = 5.0;  // FPS per second
const MIN_QUEUE_DEPTH: usize = 1;
const MAX_QUEUE_DEPTH: usize = 10;
```

### 6.2 Fault Detection

Monitor for abnormal conditions:

```rust
enum ControlFault {
    MeasurementTimeout,      // No FPS data for > 1 second
    SensorOutOfRange,        // FPS > 40 or < 0
    ActuatorSaturation,      // Output at limits for > 5 seconds
    ExcessiveError,          // Error > 10 FPS for > 5 seconds
    IntegralWindup,          // Integral term saturated
    OscillationDetected,     // Alternating sign error > 5 cycles
}
```

### 6.3 Fault Recovery

```rust
fn handle_fault(fault: ControlFault) {
    match fault {
        MeasurementTimeout => {
            // Reset to open-loop mode
            // Request frames at last known good rate
        }
        ActuatorSaturation => {
            // Reset integral term
            // Reduce setpoint if needed
        }
        OscillationDetected => {
            // Reduce Kp by 20%
            // Reset integral term
        }
        _ => {
            // Log error, continue with reduced gains
        }
    }
}
```

---

## 7. Expected Performance Improvements

### 7.1 Quantitative Predictions

Based on analysis, PID control should achieve:

| Metric                    | Current  | With PI Control | Improvement |
|--------------------------|----------|-----------------|-------------|
| 5 FPS tracking           | 20.1%    | > 95%           | 4.7x        |
| 10 FPS tracking          | 1.6%     | > 90%           | 56x         |
| Steady-state error       | 2.35 FPS | < 0.5 FPS       | 4.7x        |
| FPS stability (CV)       | 62.2%    | < 15%           | 4.1x        |
| Frame-to-frame variation | 0.50 FPS | < 0.2 FPS       | 2.5x        |

### 7.2 Qualitative Benefits

- **Predictable Performance:** System maintains setpoint regardless of disturbances
- **User Control:** Operators can select desired FPS, system delivers
- **Resource Optimization:** Consistent FPS enables better resource planning
- **Reduced Drops:** Stable queue depth minimizes frame drops
- **Faster Response:** Controlled settling time improves responsiveness

---

## 8. Testing and Validation Plan

### 8.1 Unit Tests

```rust
#[test]
fn test_pi_controller_proportional() {
    let mut controller = PIController::new(1.0, 0.0, 0.1);
    let output = controller.update(10.0, 8.0);
    assert_eq!(output, 2.0);  // Kp * error = 1.0 * 2.0
}

#[test]
fn test_integral_windup_protection() {
    let mut controller = PIController::new(1.0, 0.1, 0.1);
    for _ in 0..1000 {
        controller.update(10.0, 0.0);  // Large sustained error
    }
    assert!(controller.integral.abs() <= 5.0);
}

#[test]
fn test_output_saturation() {
    let mut controller = PIController::new(10.0, 1.0, 0.1);
    let output = controller.update(50.0, 0.0);
    assert!(output >= 1.0 && output <= 30.0);
}
```

### 8.2 Integration Tests

1. **Step Response Test:**
   - Set setpoint to 5 FPS
   - Wait for settling (< 2 seconds)
   - Verify steady-state error < 0.5 FPS
   - Step to 10 FPS
   - Repeat validation

2. **Disturbance Rejection Test:**
   - Maintain 10 FPS setpoint
   - Introduce network delay (simulate TCP latency spike)
   - Verify recovery within settling time
   - Check overshoot < 10%

3. **Tracking Test:**
   - Ramp setpoint from 5 to 30 FPS over 10 seconds
   - Verify tracking error < 1 FPS throughout
   - Verify smooth response (no oscillation)

4. **Long-term Stability Test:**
   - Run at 10 FPS for 30 minutes
   - Calculate statistics:
     - Mean FPS (should be 10.0 ± 0.2)
     - Std dev (should be < 0.5)
     - Frame drops (should be < 1%)

### 8.3 Acceptance Criteria

System must meet ALL of the following:

- [ ] Steady-state error < 0.5 FPS at all setpoints
- [ ] Settling time < 2 seconds for any setpoint change
- [ ] Overshoot < 10% for step changes
- [ ] No sustained oscillation (> 5 cycles)
- [ ] Frame drop rate < 1% during stable operation
- [ ] Control loop CPU usage < 5%
- [ ] Graceful degradation under faults
- [ ] 30-minute stability test passes
- [ ] All setpoints (5, 10, 15, 30 FPS) achievable

---

## 9. Data Structures and Implementation

### 9.1 Core Structures

```rust
/// Configuration for PID controller
#[derive(Debug, Clone, Copy)]
pub struct PIDConfig {
    pub kp: f32,              // Proportional gain
    pub ki: f32,              // Integral gain
    pub kd: f32,              // Derivative gain
    pub sample_rate_hz: f32,  // Sample rate (Hz)
    pub integral_limit: f32,  // Anti-windup limit
    pub output_min: f32,      // Min output (FPS)
    pub output_max: f32,      // Max output (FPS)
}

/// PID controller state
pub struct PIDController {
    config: PIDConfig,
    integral: f32,
    last_error: f32,
    last_output: f32,
    sample_count: u64,
}

/// FPS measurement with rolling window
pub struct FPSMeasurement {
    window: VecDeque<(Instant, u32)>,  // (timestamp, frame_count)
    window_duration: Duration,          // Rolling window size
}

/// Control system state
pub struct ControlSystem {
    fps_controller: PIDController,
    queue_controller: PIDController,
    fps_measurement: FPSMeasurement,
    setpoint: f32,
    enabled: bool,
    fault_state: Option<ControlFault>,
}
```

### 9.2 Key Methods

```rust
impl PIDController {
    pub fn new(config: PIDConfig) -> Self { /* ... */ }

    pub fn update(&mut self, setpoint: f32, measured: f32) -> f32 {
        let dt = 1.0 / self.config.sample_rate_hz;
        let error = setpoint - measured;

        // P term
        let p_term = self.config.kp * error;

        // I term with anti-windup
        self.integral += error * dt;
        self.integral = self.integral.clamp(
            -self.config.integral_limit,
            self.config.integral_limit
        );
        let i_term = self.config.ki * self.integral;

        // D term
        let d_term = self.config.kd * (error - self.last_error) / dt;

        // Combine and saturate
        let output = (p_term + i_term + d_term).clamp(
            self.config.output_min,
            self.config.output_max
        );

        self.last_error = error;
        self.last_output = output;
        self.sample_count += 1;

        output
    }

    pub fn reset(&mut self) {
        self.integral = 0.0;
        self.last_error = 0.0;
        self.last_output = 0.0;
    }
}

impl FPSMeasurement {
    pub fn new(window_duration: Duration) -> Self { /* ... */ }

    pub fn add_frame(&mut self, timestamp: Instant) { /* ... */ }

    pub fn get_fps(&self) -> Option<f32> {
        if self.window.len() < 2 {
            return None;
        }

        let oldest = self.window.front()?;
        let newest = self.window.back()?;

        let duration = newest.0.duration_since(oldest.0);
        let frames = newest.1 - oldest.1;

        Some(frames as f32 / duration.as_secs_f32())
    }
}

impl ControlSystem {
    pub fn update(&mut self) -> Option<f32> {
        if !self.enabled {
            return None;
        }

        let measured_fps = self.fps_measurement.get_fps()?;

        // Primary loop: FPS control
        let fps_output = self.fps_controller.update(
            self.setpoint,
            measured_fps
        );

        Some(fps_output)
    }

    pub fn set_setpoint(&mut self, new_setpoint: f32) {
        // Implement setpoint ramping if needed
        self.setpoint = new_setpoint.clamp(1.0, 30.0);
    }
}
```

---

## 10. Monitoring and Diagnostics

### 10.1 Real-time Metrics

Collect and display:

```rust
pub struct ControlMetrics {
    // Process variables
    pub measured_fps: f32,
    pub setpoint_fps: f32,
    pub queue_depth: usize,

    // Control outputs
    pub fps_request: f32,
    pub control_effort: f32,

    // Error terms
    pub error: f32,
    pub integral_term: f32,
    pub derivative_term: f32,

    // Performance
    pub settling_time_ms: f32,
    pub overshoot_percent: f32,
    pub steady_state_error: f32,

    // Health
    pub fault_count: u32,
    pub saturation_count: u32,
    pub oscillation_count: u32,
}
```

### 10.2 Logging Strategy

```rust
// High-frequency logging (10 Hz) - for tuning
if tuning_mode {
    log::debug!(
        "PID: sp={:.2} pv={:.2} err={:.2} p={:.2} i={:.2} d={:.2} out={:.2}",
        setpoint, measured, error, p_term, i_term, d_term, output
    );
}

// Low-frequency logging (1 Hz) - for monitoring
if sample_count % 10 == 0 {
    log::info!(
        "Control: FPS={:.2}/{:.2} Queue={} Effort={:.1}%",
        measured_fps, setpoint, queue_depth, control_effort * 100.0
    );
}

// Event logging - for diagnostics
if fault_detected {
    log::warn!("Control fault: {:?}", fault);
}
```

### 10.3 Visualization

Recommended real-time plots:

1. **FPS Tracking Plot:**
   - Setpoint (line)
   - Measured FPS (line)
   - Error band (±0.5 FPS)

2. **Control Output Plot:**
   - Frame request rate
   - Output limits (shaded)
   - Saturation events (markers)

3. **Error Terms Plot:**
   - Proportional term
   - Integral term
   - Derivative term (if used)

4. **Performance Plot:**
   - Queue depth
   - Drop rate
   - Latency

---

## 11. Conclusion and Next Steps

### 11.1 Summary

This analysis of 7,827 metrics samples reveals:

1. **Current system lacks regulation:** 62% CV in FPS, only 20% of samples meet 5 FPS target
2. **System is controllable:** Second-order dynamics with 2-second settling time
3. **PI control is appropriate:** Eliminates steady-state error, handles disturbances
4. **Implementation is feasible:** Clear actuators (frame request rate), well-defined measurements

### 11.2 Key Success Factors

1. **Accurate FPS measurement:** Rolling window with 1-second duration
2. **Conservative initial tuning:** Start with low gains (Kp=0.1, Ki=0.01)
3. **Anti-windup protection:** Prevent integral term saturation
4. **Comprehensive testing:** Validate all scenarios before deployment
5. **Monitoring and logging:** Enable rapid debugging and optimization

### 11.3 Risk Mitigation

| Risk                        | Mitigation                                    |
|----------------------------|-----------------------------------------------|
| Controller instability     | Start conservative, increase gains gradually  |
| Sensor noise               | Filter FPS measurements, use median filtering |
| Actuator saturation        | Implement anti-windup, clamp outputs          |
| Network variability        | Add feedforward compensation                  |
| System nonlinearity        | Use gain scheduling or adaptive control       |

### 11.4 Immediate Next Steps

1. **Week 1:** Implement FPS measurement infrastructure
2. **Week 2:** Build P-only controller, characterize system
3. **Week 3:** Add integral term, tune PI controller
4. **Week 4:** Validate performance, add advanced features

### 11.5 Expected Outcomes

After successful implementation:

- **95%+ samples meet FPS setpoint** (vs. 20% currently)
- **4x reduction in FPS variability** (CV from 62% to <15%)
- **User-selectable frame rates** (5, 10, 15, 30 FPS)
- **Predictable, stable performance** under varying conditions
- **Foundation for advanced features** (adaptive control, multi-objective optimization)

---

## Appendix A: Analysis Tool Usage

### Running the Analysis

```bash
cd /home/ken/Spr_ws/GH_wk_test/docs/security_camera/01_requirements
python3 metrics_pid_analyzer.py /home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/
```

### Tool Capabilities

- Loads all CSV files from metrics directory
- Calculates comprehensive statistics (mean, median, std dev, CV, range)
- Analyzes system dynamics (time constant, settling time, order)
- Recommends PID parameters based on Ziegler-Nichols and experimental methods
- Generates ASCII time-series visualizations
- Identifies control variables, disturbances, and actuators
- Assesses current open-loop performance

### Output Sections

1. FPS Performance Analysis
2. TCP Response Time Analysis
3. Frame Processing Time Analysis
4. Queue Depth Analysis
5. Error Pattern Analysis
6. System Dynamics Estimation
7. PID Controller Design Recommendations
8. Control System Variable Identification
9. Time Series Visualization
10. Current Control Performance Assessment
11. Implementation Recommendations

---

## Appendix B: References

### Control Theory Resources

1. **"Feedback Control of Dynamic Systems"** - Franklin, Powell, Emami-Naeini
   - Chapter 5: Root Locus Design
   - Chapter 6: Frequency Response Design

2. **"PID Controllers: Theory, Design, and Tuning"** - Astrom, Hagglund
   - Chapter 2: PID Control
   - Chapter 4: Tuning Methods

3. **"Digital Control System Analysis and Design"** - Phillips, Nagle
   - Chapter 7: Digital Control System Design

### Practical Implementation

1. **Ziegler-Nichols Tuning Method**
   - Ultimate gain method
   - Step response method

2. **Anti-Windup Techniques**
   - Integral clamping
   - Back-calculation
   - Conditional integration

3. **Real-Time Control Best Practices**
   - Fixed sample rate scheduling
   - Measurement filtering
   - Fault detection and recovery

---

**Report Generated:** 2026-02-03
**Analysis Duration:** Complete dataset (7,827 samples)
**Confidence Level:** High (large sample size, clear patterns)
**Recommendation Status:** Ready for implementation

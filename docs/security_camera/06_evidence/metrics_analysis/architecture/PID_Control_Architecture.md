# PID Control System Architecture

**Security Camera FPS Control - Phase 10 Implementation**

---

## System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                  SECURITY CAMERA CONTROL SYSTEM                     │
│                                                                     │
│  Objective: Maintain stable, configurable frame rate               │
│  Method: PI (Proportional-Integral) feedback control               │
│  Sample Rate: 10 Hz (100 ms period)                                │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Primary Control Loop (FPS Regulation)

```
                    SETPOINT INPUT
                         │
                         │ FPS Target
                         │ (5, 10, 15, or 30 FPS)
                         ▼
                    ┌─────────┐
                    │ Setpoint│
                    │ Ramper  │◄───── Max rate: 5 FPS/sec
                    └────┬────┘
                         │
                         │ Ramped Setpoint
                         ▼
    ┌───────────────────────────────────────┐
    │                                       │
    │        FEEDBACK CONTROL LOOP          │
    │                                       │
    │    ┌──────────────────────────┐       │
    │    │                          │       │
    │    │   ┌──────────────────┐   │       │
    │    │   │  ERROR           │   │       │
    │    ▼   │  CALCULATION     │   │       │
    │  ┌───────────────┐        │   │       │
    │  │  Σ  (-)       │◄───────┘   │       │
    │  └───────────────┘            │       │
    │    │                          │       │
    │    │ Error                    │       │
    │    ▼                          │       │
    │  ┌───────────────────────┐    │       │
    │  │  PI CONTROLLER        │    │       │
    │  │                       │    │       │
    │  │  P = Kp × error       │    │       │
    │  │                       │    │       │
    │  │  I += error × dt      │    │       │
    │  │  I = clamp(I, ±5.0)   │    │       │
    │  │                       │    │       │
    │  │  Out = P + Ki×I       │    │       │
    │  └───────────────────────┘    │       │
    │    │                          │       │
    │    │ Control Output           │       │
    │    ▼                          │       │
    │  ┌───────────────────────┐    │       │
    │  │  OUTPUT LIMITER       │    │       │
    │  │  clamp(1.0, 30.0 FPS) │    │       │
    │  └───────────────────────┘    │       │
    │    │                          │       │
    └────┼──────────────────────────┘       │
         │                                  │
         │ Frame Request Rate               │
         ▼                                  │
    ┌────────────────────────┐              │
    │                        │              │
    │  SPRESENSE CAMERA      │              │
    │  SYSTEM (PLANT)        │              │
    │                        │              │
    │  - TCP Command Handler │              │
    │  - Frame Capture       │              │
    │  - JPEG Compression    │              │
    │  - Serial Transmission │              │
    │                        │              │
    └────────────────────────┘              │
         │                                  │
         │ JPEG Frames                      │
         ▼                                  │
    ┌────────────────────────┐              │
    │                        │              │
    │  PC DISPLAY SYSTEM     │              │
    │                        │              │
    │  - Serial Reception    │              │
    │  - JPEG Decoding       │              │
    │  - Display Rendering   │              │
    │                        │              │
    └────────────────────────┘              │
         │                                  │
         │ Displayed Frames                 │
         ▼                                  │
    ┌────────────────────────┐              │
    │  FPS MEASUREMENT       │              │
    │                        │              │
    │  - Rolling Window      │              │
    │  - 1 second duration   │              │
    │  - Timestamp-based     │              │
    │                        │              │
    └────────────────────────┘              │
         │                                  │
         │ Measured FPS                     │
         └──────────────────────────────────┘
              (Feedback Signal)
```

---

## Control System Components

### 1. Setpoint Manager

```rust
struct SetpointManager {
    target: f32,           // User-requested FPS
    current: f32,          // Current ramped setpoint
    max_rate: f32,         // Max change rate (FPS/sec)
}

impl SetpointManager {
    fn update(&mut self, dt: f32) -> f32 {
        let delta = self.target - self.current;
        let max_change = self.max_rate * dt;
        let change = delta.clamp(-max_change, max_change);

        self.current += change;
        self.current
    }
}
```

**Purpose:** Prevents step changes that could destabilize control

### 2. PI Controller

```rust
struct PIController {
    // Configuration
    kp: f32,                  // Proportional gain
    ki: f32,                  // Integral gain
    integral_limit: f32,      // Anti-windup limit

    // State
    integral: f32,            // Accumulated error
    last_error: f32,          // For derivative (if added)
    sample_period: f32,       // dt (seconds)
}

impl PIController {
    fn update(&mut self, error: f32) -> f32 {
        // Proportional term
        let p_term = self.kp * error;

        // Integral term with anti-windup
        self.integral += error * self.sample_period;
        self.integral = self.integral.clamp(
            -self.integral_limit,
            self.integral_limit
        );
        let i_term = self.ki * self.integral;

        // Output
        p_term + i_term
    }

    fn reset(&mut self) {
        self.integral = 0.0;
        self.last_error = 0.0;
    }
}
```

**Purpose:** Calculates control action to minimize error

### 3. FPS Measurement

```rust
struct FPSMeasurement {
    window: VecDeque<FrameTimestamp>,
    window_duration: Duration,
}

struct FrameTimestamp {
    instant: Instant,
    count: u32,
}

impl FPSMeasurement {
    fn add_frame(&mut self, timestamp: Instant) {
        // Add new frame
        self.window.push_back(FrameTimestamp {
            instant: timestamp,
            count: self.window.back()
                .map(|f| f.count + 1)
                .unwrap_or(0),
        });

        // Remove old frames
        let cutoff = timestamp - self.window_duration;
        while let Some(front) = self.window.front() {
            if front.instant < cutoff {
                self.window.pop_front();
            } else {
                break;
            }
        }
    }

    fn get_fps(&self) -> Option<f32> {
        if self.window.len() < 2 {
            return None;
        }

        let oldest = self.window.front()?;
        let newest = self.window.back()?;

        let duration = newest.instant
            .duration_since(oldest.instant)
            .as_secs_f32();
        let frames = newest.count - oldest.count;

        Some(frames as f32 / duration)
    }
}
```

**Purpose:** Provides accurate, filtered FPS measurement

### 4. Output Limiter

```rust
fn limit_output(output: f32, min: f32, max: f32) -> f32 {
    output.clamp(min, max)
}

const FPS_MIN: f32 = 1.0;
const FPS_MAX: f32 = 30.0;

// Usage
let limited_output = limit_output(controller_output, FPS_MIN, FPS_MAX);
```

**Purpose:** Prevents invalid frame request rates

---

## Disturbance Model

```
DISTURBANCES (affecting FPS):
│
├── Network Latency
│   ├── TCP send time variations
│   │   Mean:   104.95 ms
│   │   StdDev: 86.25 ms
│   │   CV:     82.18%
│   └── Network jitter
│       Max spike: 923.81 ms
│
├── Processing Time Variations
│   ├── Serial read time
│   │   Mean:   448.71 ms
│   │   StdDev: 772.56 ms (HIGHLY VARIABLE)
│   │   CV:     172.17%
│   └── JPEG decode time
│       Mean:   2.64 ms
│       StdDev: 1.34 ms
│
├── System Load
│   ├── CPU usage variations
│   ├── Memory pressure
│   └── I/O contention
│
└── Camera Hardware
    ├── Capture timing jitter
    ├── USB bandwidth limits
    └── Processing queue depth
        Mean:   2.63 items
        Range:  0-5 items
```

**Control Strategy:**
- PI controller rejects disturbances through feedback
- Optional: Feedforward compensation for measured disturbances (TCP latency)

---

## Control Loop Timing

```
TIME SCALE:

0ms          100ms        200ms        300ms        400ms
│────────────│────────────│────────────│────────────│
│            │            │            │            │
▼            ▼            ▼            ▼            ▼
Measure      Measure      Measure      Measure      Measure
FPS          FPS          FPS          FPS          FPS
│            │            │            │            │
Calculate    Calculate    Calculate    Calculate    Calculate
Error        Error        Error        Error        Error
│            │            │            │            │
Update       Update       Update       Update       Update
PI           PI           PI           PI           PI
Controller   Controller   Controller   Controller   Controller
│            │            │            │            │
Send         Send         Send         Send         Send
Frame        Frame        Frame        Frame        Frame
Request      Request      Request      Request      Request

PARALLEL LOOPS:

Primary Loop (FPS):        ────────────────────────────  10 Hz (100ms)
Secondary Loop (Queue):    ──────────────────────────── 20 Hz (50ms)
Monitoring Loop:           ───────────────────────────── 1 Hz (1000ms)
```

---

## State Machine

```
┌─────────────────────────────────────────────────────────┐
│                  CONTROL STATES                         │
└─────────────────────────────────────────────────────────┘

   ┌──────────┐
   │  INIT    │
   │          │
   └────┬─────┘
        │
        │ Initialize controller,
        │ reset integral term
        ▼
   ┌──────────┐
   │  IDLE    │◄──────────────────┐
   │          │                   │
   └────┬─────┘                   │
        │                         │
        │ User sets setpoint      │ Disable command
        ▼                         │
   ┌──────────┐                   │
   │ ACTIVE   │                   │
   │          │                   │
   │ Running  │                   │
   │ PI       │                   │
   │ Control  │───────────────────┤
   └────┬─────┘                   │
        │                         │
        │ Fault detected          │
        ▼                         │
   ┌──────────┐                   │
   │  FAULT   │                   │
   │          │                   │
   │ Logging  │                   │
   │ Error    │                   │
   └────┬─────┘                   │
        │                         │
        │ Recovery successful     │
        └─────────────────────────┘


STATE DESCRIPTIONS:

INIT:
  - Initialize data structures
  - Reset controller state
  - Load configuration
  - Transition: → IDLE

IDLE:
  - No control action
  - Monitor FPS passively
  - Wait for setpoint command
  - Transition: setpoint set → ACTIVE

ACTIVE:
  - Run PI control loop at 10 Hz
  - Update FPS measurement
  - Send frame requests
  - Monitor for faults
  - Transitions:
    * disable → IDLE
    * fault → FAULT

FAULT:
  - Log error condition
  - Attempt recovery
  - Reset integral term
  - Reduce gains if needed
  - Transitions:
    * recovery → ACTIVE
    * timeout → IDLE
```

---

## Data Flow

```
┌────────────────────────────────────────────────────────────────┐
│                      DATA FLOW DIAGRAM                         │
└────────────────────────────────────────────────────────────────┘

USER INPUT                    MEASUREMENTS               CONTROL OUTPUT
    │                              │                          │
    │ Setpoint                     │ Frame                    │ Frame
    │ Selection                    │ Rendered                 │ Request
    │                              │                          │
    ▼                              ▼                          │
┌────────┐                    ┌─────────┐                     │
│Setpoint│                    │   FPS   │                     │
│Manager │                    │Measure- │                     │
└───┬────┘                    │ment     │                     │
    │                         └────┬────┘                     │
    │ Ramped                       │                          │
    │ Setpoint                     │ Measured                 │
    │                              │ FPS                      │
    │                              │                          │
    ▼                              ▼                          │
┌─────────────────────────────────────────┐                  │
│              ERROR CALCULATION          │                  │
│         error = setpoint - measured     │                  │
└─────────────────────────────────────────┘                  │
                    │                                        │
                    │ Error                                  │
                    ▼                                        │
┌─────────────────────────────────────────┐                  │
│            PI CONTROLLER                │                  │
│                                         │                  │
│  State:                                 │                  │
│    - integral term                      │                  │
│    - last error                         │                  │
│                                         │                  │
│  Calculation:                           │                  │
│    p_term = Kp × error                  │                  │
│    integral += error × dt               │                  │
│    i_term = Ki × integral               │                  │
│    output = p_term + i_term             │                  │
└─────────────────────────────────────────┘                  │
                    │                                        │
                    │ Control                                │
                    │ Output                                 │
                    ▼                                        │
┌─────────────────────────────────────────┐                  │
│           OUTPUT LIMITER                │                  │
│        clamp(output, 1.0, 30.0)         │                  │
└─────────────────────────────────────────┘                  │
                    │                                        │
                    │ Limited                                │
                    │ Output                                 │
                    └────────────────────────────────────────┘
                                                             │
                                                             ▼
                                                    ┌────────────────┐
                                                    │   TCP COMMAND  │
                                                    │   TO SPRESENSE │
                                                    └────────────────┘
```

---

## Error Handling

```
┌────────────────────────────────────────────────────────────────┐
│                   FAULT DETECTION & RECOVERY                   │
└────────────────────────────────────────────────────────────────┘

FAULT TYPE                    DETECTION                 RECOVERY
───────────────────────────────────────────────────────────────────

Measurement Timeout           No FPS data for           Switch to
                             > 1 second                open-loop,
                                                       use last
                                                       known rate
                                  │
                                  ▼
                             ┌─────────┐
                             │  FAULT  │
                             │  STATE  │
                             └─────────┘


Sensor Out of Range          FPS < 0 or                Reject
                             FPS > 40                  measurement,
                                                       use previous
                                  │
                                  ▼
                             ┌─────────┐
                             │  FAULT  │
                             │  STATE  │
                             └─────────┘


Actuator Saturation          Output at                 Reset
                             limits for                integral term,
                             > 5 seconds               check setpoint
                                  │
                                  ▼
                             ┌─────────┐
                             │WARNING  │
                             │  STATE  │
                             └─────────┘


Excessive Error              |error| > 10 FPS          Reduce gains
                             for > 5 seconds           by 20%,
                                                       reset integral
                                  │
                                  ▼
                             ┌─────────┐
                             │  FAULT  │
                             │  STATE  │
                             └─────────┘


Integral Windup              Integral term             Clamp integral,
                             saturated                 verify actuator
                                                       not saturated
                                  │
                                  ▼
                             ┌─────────┐
                             │PREVENTED│
                             │(Anti-   │
                             │windup)  │
                             └─────────┘


Oscillation Detected         Error changes             Reduce Kp by
                             sign > 5 times            20%, reset
                             in 10 samples             integral
                                  │
                                  ▼
                             ┌─────────┐
                             │  FAULT  │
                             │  STATE  │
                             └─────────┘
```

---

## Performance Monitoring

```
┌────────────────────────────────────────────────────────────────┐
│                    METRICS COLLECTION                          │
└────────────────────────────────────────────────────────────────┘

HIGH FREQUENCY (10 Hz - During Tuning):
┌──────────────────────────────────────────────────────────────┐
│ Timestamp │ Setpoint │ Measured │ Error │ P │ I │ D │ Output │
├───────────┼──────────┼──────────┼───────┼───┼───┼───┼────────┤
│ 1234.0    │ 10.00    │  9.50    │ 0.50  │...│...│...│ 10.5   │
│ 1234.1    │ 10.00    │  9.75    │ 0.25  │...│...│...│ 10.3   │
│ 1234.2    │ 10.00    │  9.90    │ 0.10  │...│...│...│ 10.1   │
└──────────────────────────────────────────────────────────────┘

LOW FREQUENCY (1 Hz - Normal Operation):
┌──────────────────────────────────────────────────────────────┐
│ Timestamp │ Mean FPS │ Std Dev │ Queue │ Drops │ Effort │
├───────────┼──────────┼─────────┼───────┼───────┼────────┤
│ 1234      │ 10.05    │ 0.25    │  3    │  0    │ 15%    │
│ 1235      │  9.98    │ 0.30    │  4    │  0    │ 18%    │
│ 1236      │ 10.02    │ 0.22    │  3    │  1    │ 12%    │
└──────────────────────────────────────────────────────────────┘

PERFORMANCE SUMMARY (Calculated Periodically):
┌──────────────────────────────────────────────────────────────┐
│ Metric                  │ Value      │ Target     │ Status  │
├─────────────────────────┼────────────┼────────────┼─────────┤
│ Mean Error              │  0.25 FPS  │ < 0.5 FPS  │ ✓ PASS  │
│ Settling Time           │  1.8 sec   │ < 2.0 sec  │ ✓ PASS  │
│ Overshoot               │  8.0 %     │ < 10 %     │ ✓ PASS  │
│ Stability (CV)          │ 12.0 %     │ < 15 %     │ ✓ PASS  │
│ Frame Drop Rate         │  0.5 %     │ < 1.0 %    │ ✓ PASS  │
│ Control Effort          │ 25.0 %     │ < 30 %     │ ✓ PASS  │
└──────────────────────────────────────────────────────────────┘
```

---

## System Integration

```
┌────────────────────────────────────────────────────────────────┐
│                 INTEGRATION WITH EXISTING SYSTEM               │
└────────────────────────────────────────────────────────────────┘

EXISTING CODE                      NEW PID CONTROL
────────────────                   ───────────────

┌─────────────────┐               ┌──────────────────┐
│  Main Loop      │               │ Control System   │
│                 │               │                  │
│  - Read frames  │◄──────────────┤ - Measure FPS    │
│  - Display      │               │ - Update PI      │
│  - Handle input │               │ - Send requests  │
└─────────────────┘               └──────────────────┘
        │                                  ▲
        │                                  │
        ▼                                  │
┌─────────────────┐                       │
│  Frame Handler  │                       │
│                 │                       │
│  - Decode JPEG  │───────────────────────┘
│  - Update       │       Frame rendered event
│    texture      │
└─────────────────┘

MINIMAL CHANGES REQUIRED:
1. Add FPS measurement call on each frame render
2. Periodically call control.update()
3. Use control output for frame request rate
4. Add configuration UI for setpoint selection
```

---

## Configuration Structure

```rust
#[derive(Debug, Clone, Copy)]
pub struct ControlConfig {
    // Controller gains
    pub kp: f32,
    pub ki: f32,
    pub kd: f32,

    // Timing
    pub sample_rate_hz: f32,

    // Limits
    pub integral_limit: f32,
    pub output_min_fps: f32,
    pub output_max_fps: f32,

    // Setpoint management
    pub setpoint_ramp_rate: f32,  // FPS/sec

    // Measurement
    pub fps_window_duration_ms: u32,

    // Fault detection
    pub max_error_duration_ms: u32,
    pub max_saturation_duration_ms: u32,
    pub oscillation_detection_cycles: u32,
}

impl Default for ControlConfig {
    fn default() -> Self {
        Self {
            // Conservative initial tuning
            kp: 0.1,
            ki: 0.01,
            kd: 0.0,

            // 10 Hz control loop
            sample_rate_hz: 10.0,

            // Anti-windup
            integral_limit: 5.0,

            // FPS limits
            output_min_fps: 1.0,
            output_max_fps: 30.0,

            // Smooth setpoint changes
            setpoint_ramp_rate: 5.0,

            // 1 second measurement window
            fps_window_duration_ms: 1000,

            // Fault detection thresholds
            max_error_duration_ms: 5000,
            max_saturation_duration_ms: 5000,
            oscillation_detection_cycles: 5,
        }
    }
}
```

---

## Visual Summary

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  USER                                                            │
│   │                                                              │
│   │ Sets Setpoint (5, 10, 15, or 30 FPS)                        │
│   ▼                                                              │
│ ┌────────────────────────────────────────────────┐              │
│ │         CONTROL SYSTEM (10 Hz)                 │              │
│ │                                                │              │
│ │  Measure FPS → Calculate Error → PI Update →  │              │
│ │  → Limit Output → Send Frame Request          │              │
│ │                                                │              │
│ │  Safety: Anti-windup, Output limits,          │              │
│ │          Fault detection                      │              │
│ └────────────────────────────────────────────────┘              │
│   │                                                              │
│   │ Frame Request (1-30 FPS)                                    │
│   ▼                                                              │
│ ┌────────────────────────────────────────────────┐              │
│ │      SPRESENSE CAMERA SYSTEM                   │              │
│ │      (Plant / Process)                         │              │
│ │                                                │              │
│ │  Captures → Compresses → Transmits            │              │
│ └────────────────────────────────────────────────┘              │
│   │                                                              │
│   │ JPEG Frames                                                 │
│   ▼                                                              │
│ ┌────────────────────────────────────────────────┐              │
│ │      PC DISPLAY SYSTEM                         │              │
│ │                                                │              │
│ │  Receives → Decodes → Displays                 │              │
│ └────────────────────────────────────────────────┘              │
│   │                                                              │
│   │ Frame Rendered (Feedback)                                   │
│   └──────────────────────────────────────────────┐              │
│                                                  │              │
│                                      ────────────┘              │
│                                      │                          │
│                                      FPS Measurement            │
│                                                                  │
│  RESULT: Stable FPS at user-selected setpoint                   │
│          (±0.5 FPS, settling time < 2 sec)                      │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

**Document Version:** 1.0
**Last Updated:** 2026-02-03
**Status:** Ready for Phase 10 implementation

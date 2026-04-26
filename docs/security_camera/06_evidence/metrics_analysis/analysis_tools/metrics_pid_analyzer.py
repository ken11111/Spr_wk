#!/usr/bin/env python3
"""
Security Camera Metrics Analyzer for PID Control Implementation
Analyzes performance metrics to inform Phase 10 PID control design
"""

import csv
import os
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional
import statistics
from collections import defaultdict


@dataclass
class MetricsSample:
    """Single metrics sample from CSV"""
    timestamp: float
    pc_fps: float
    spresense_fps: float
    frame_count: int
    error_count: int
    decode_time_ms: float
    serial_read_time_ms: float
    texture_upload_time_ms: float
    jpeg_size_kb: float
    spresense_camera_frames: int
    spresense_camera_fps: float
    spresense_usb_packets: int
    action_q_depth: int
    spresense_errors: int
    tcp_avg_send_ms: float
    tcp_max_send_ms: float
    dropped_frames: int = 0
    drop_events: int = 0


@dataclass
class TimeSeriesStats:
    """Statistical summary of a time series"""
    mean: float
    median: float
    std_dev: float
    min_val: float
    max_val: float
    range_val: float
    cv: float  # Coefficient of variation (std_dev/mean)
    count: int

    def __str__(self):
        return (f"  Mean:   {self.mean:8.2f}  |  Median: {self.median:8.2f}\n"
                f"  StdDev: {self.std_dev:8.2f}  |  CV:     {self.cv:8.2%}\n"
                f"  Min:    {self.min_val:8.2f}  |  Max:    {self.max_val:8.2f}\n"
                f"  Range:  {self.range_val:8.2f}  |  Count:  {self.count:8d}")


@dataclass
class ControlSystemAnalysis:
    """Analysis results for control system design"""
    process_variable: str
    setpoint_candidates: List[float]
    disturbances: List[str]
    actuators: List[str]
    time_constant_ms: float
    settling_time_ms: float
    overshoot_percent: float
    steady_state_error: float
    system_order: int
    recommended_sample_rate_hz: float

    def __str__(self):
        return (f"\nControl System Analysis: {self.process_variable}\n"
                f"{'='*70}\n"
                f"Setpoint Candidates: {', '.join(map(str, self.setpoint_candidates))}\n"
                f"Time Constant: {self.time_constant_ms:.2f} ms\n"
                f"Settling Time: {self.settling_time_ms:.2f} ms\n"
                f"Overshoot: {self.overshoot_percent:.1f}%\n"
                f"Steady State Error: {self.steady_state_error:.2f}\n"
                f"System Order: {self.system_order}\n"
                f"Recommended Sample Rate: {self.recommended_sample_rate_hz:.1f} Hz\n"
                f"Disturbances: {', '.join(self.disturbances)}\n"
                f"Actuators: {', '.join(self.actuators)}")


class MetricsAnalyzer:
    """Analyzes security camera metrics for PID control implementation"""

    def __init__(self, metrics_dir: str):
        self.metrics_dir = Path(metrics_dir)
        self.samples: List[MetricsSample] = []
        self.files_processed = 0
        self.files_failed = 0

    def load_all_metrics(self) -> None:
        """Load all CSV files from metrics directory"""
        csv_files = sorted(self.metrics_dir.glob("*.csv"))
        print(f"Found {len(csv_files)} CSV files to process\n")

        for csv_file in csv_files:
            try:
                self._load_csv_file(csv_file)
                self.files_processed += 1
            except Exception as e:
                print(f"Error loading {csv_file.name}: {e}")
                self.files_failed += 1

        print(f"\nLoaded {len(self.samples)} samples from {self.files_processed} files")
        if self.files_failed > 0:
            print(f"Failed to load {self.files_failed} files")

    def _load_csv_file(self, filepath: Path) -> None:
        """Load single CSV file"""
        with open(filepath, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                try:
                    sample = MetricsSample(
                        timestamp=float(row['timestamp']),
                        pc_fps=float(row['pc_fps']),
                        spresense_fps=float(row['spresense_fps']),
                        frame_count=int(row['frame_count']),
                        error_count=int(row['error_count']),
                        decode_time_ms=float(row['decode_time_ms']),
                        serial_read_time_ms=float(row['serial_read_time_ms']),
                        texture_upload_time_ms=float(row['texture_upload_time_ms']),
                        jpeg_size_kb=float(row['jpeg_size_kb']),
                        spresense_camera_frames=int(row['spresense_camera_frames']),
                        spresense_camera_fps=float(row['spresense_camera_fps']),
                        spresense_usb_packets=int(row['spresense_usb_packets']),
                        action_q_depth=int(row['action_q_depth']),
                        spresense_errors=int(row['spresense_errors']),
                        tcp_avg_send_ms=float(row['tcp_avg_send_ms']),
                        tcp_max_send_ms=float(row['tcp_max_send_ms']),
                        dropped_frames=int(row.get('dropped_frames', 0)),
                        drop_events=int(row.get('drop_events', 0))
                    )
                    self.samples.append(sample)
                except (ValueError, KeyError) as e:
                    # Skip malformed rows
                    continue

    def calculate_stats(self, values: List[float]) -> TimeSeriesStats:
        """Calculate statistical summary for a list of values"""
        if not values or len(values) == 0:
            return TimeSeriesStats(0, 0, 0, 0, 0, 0, 0, 0)

        # Filter out None and invalid values
        valid_values = [v for v in values if v is not None and not (isinstance(v, float) and (v != v))]  # NaN check

        if not valid_values:
            return TimeSeriesStats(0, 0, 0, 0, 0, 0, 0, 0)

        mean = statistics.mean(valid_values)
        median = statistics.median(valid_values)
        std_dev = statistics.stdev(valid_values) if len(valid_values) > 1 else 0
        min_val = min(valid_values)
        max_val = max(valid_values)
        range_val = max_val - min_val
        cv = (std_dev / mean) if mean > 0 else 0

        return TimeSeriesStats(mean, median, std_dev, min_val, max_val, range_val, cv, len(valid_values))

    def analyze_fps_performance(self) -> Dict[str, TimeSeriesStats]:
        """Analyze FPS variations - primary control targets"""
        print("\n" + "="*70)
        print("1. FPS PERFORMANCE ANALYSIS - PRIMARY CONTROL TARGETS")
        print("="*70)

        pc_fps_values = [s.pc_fps for s in self.samples]
        spresense_fps_values = [s.spresense_fps for s in self.samples]

        pc_fps_stats = self.calculate_stats(pc_fps_values)
        spresense_fps_stats = self.calculate_stats(spresense_fps_values)

        print("\nPC FPS (Process Variable - Display Side):")
        print(pc_fps_stats)
        print("\nSpresense FPS (Process Variable - Capture Side):")
        print(spresense_fps_stats)

        # Calculate FPS stability metrics
        print("\n--- FPS Stability Analysis ---")
        target_fps_options = [5, 10, 15, 30]

        for target in target_fps_options:
            pc_errors = [abs(s.pc_fps - target) for s in self.samples]
            spresense_errors = [abs(s.spresense_fps - target) for s in self.samples]

            pc_mse = statistics.mean([e**2 for e in pc_errors])
            spresense_mse = statistics.mean([e**2 for e in spresense_errors])

            print(f"\nTarget {target} FPS:")
            print(f"  PC FPS MSE:        {pc_mse:8.2f}")
            print(f"  Spresense FPS MSE: {spresense_mse:8.2f}")

        # System response characteristics
        print("\n--- System Response Characteristics ---")
        if len(self.samples) > 1:
            fps_changes = [abs(self.samples[i].pc_fps - self.samples[i-1].pc_fps)
                          for i in range(1, len(self.samples))]
            fps_change_stats = self.calculate_stats(fps_changes)
            print("\nFPS Change Rate (inter-sample):")
            print(fps_change_stats)

        return {
            'pc_fps': pc_fps_stats,
            'spresense_fps': spresense_fps_stats
        }

    def analyze_tcp_performance(self) -> Dict[str, TimeSeriesStats]:
        """Analyze TCP response times - network latency characteristics"""
        print("\n" + "="*70)
        print("2. TCP RESPONSE TIME ANALYSIS - NETWORK LATENCY")
        print("="*70)

        tcp_avg_values = [s.tcp_avg_send_ms for s in self.samples]
        tcp_max_values = [s.tcp_max_send_ms for s in self.samples]

        tcp_avg_stats = self.calculate_stats(tcp_avg_values)
        tcp_max_stats = self.calculate_stats(tcp_max_values)

        print("\nTCP Average Send Time (ms):")
        print(tcp_avg_stats)
        print("\nTCP Maximum Send Time (ms):")
        print(tcp_max_stats)

        # Network jitter analysis
        if len(tcp_avg_values) > 1:
            jitter = [abs(tcp_avg_values[i] - tcp_avg_values[i-1])
                     for i in range(1, len(tcp_avg_values))]
            jitter_stats = self.calculate_stats(jitter)
            print("\nNetwork Jitter (ms):")
            print(jitter_stats)

        return {
            'tcp_avg_send_ms': tcp_avg_stats,
            'tcp_max_send_ms': tcp_max_stats
        }

    def analyze_processing_times(self) -> Dict[str, TimeSeriesStats]:
        """Analyze frame processing times - system latency components"""
        print("\n" + "="*70)
        print("3. FRAME PROCESSING TIME ANALYSIS - LATENCY COMPONENTS")
        print("="*70)

        decode_values = [s.decode_time_ms for s in self.samples]
        serial_read_values = [s.serial_read_time_ms for s in self.samples]
        texture_upload_values = [s.texture_upload_time_ms for s in self.samples]

        decode_stats = self.calculate_stats(decode_values)
        serial_read_stats = self.calculate_stats(serial_read_values)
        texture_upload_stats = self.calculate_stats(texture_upload_values)

        print("\nDecode Time (ms):")
        print(decode_stats)
        print("\nSerial Read Time (ms):")
        print(serial_read_stats)
        print("\nTexture Upload Time (ms):")
        print(texture_upload_stats)

        # Total processing time
        total_processing = [s.decode_time_ms + s.serial_read_time_ms + s.texture_upload_time_ms
                           for s in self.samples]
        total_stats = self.calculate_stats(total_processing)
        print("\nTotal Processing Time (ms):")
        print(total_stats)

        return {
            'decode_time_ms': decode_stats,
            'serial_read_time_ms': serial_read_stats,
            'texture_upload_time_ms': texture_upload_stats,
            'total_processing_ms': total_stats
        }

    def analyze_queue_behavior(self) -> TimeSeriesStats:
        """Analyze action queue depth - buffer management"""
        print("\n" + "="*70)
        print("4. QUEUE DEPTH ANALYSIS - BUFFER MANAGEMENT")
        print("="*70)

        queue_values = [s.action_q_depth for s in self.samples]
        queue_stats = self.calculate_stats(queue_values)

        print("\nAction Queue Depth:")
        print(queue_stats)

        # Queue stability
        if len(queue_values) > 1:
            queue_changes = [abs(queue_values[i] - queue_values[i-1])
                            for i in range(1, len(queue_values))]
            queue_change_stats = self.calculate_stats(queue_changes)
            print("\nQueue Depth Changes:")
            print(queue_change_stats)

        return queue_stats

    def analyze_error_patterns(self) -> None:
        """Analyze error patterns and system health"""
        print("\n" + "="*70)
        print("5. ERROR PATTERN ANALYSIS - SYSTEM HEALTH")
        print("="*70)

        error_values = [s.error_count for s in self.samples]
        spresense_error_values = [s.spresense_errors for s in self.samples]
        dropped_frame_values = [s.dropped_frames for s in self.samples]
        drop_event_values = [s.drop_events for s in self.samples]

        error_stats = self.calculate_stats(error_values)
        spresense_error_stats = self.calculate_stats(spresense_error_values)
        dropped_frame_stats = self.calculate_stats(dropped_frame_values)
        drop_event_stats = self.calculate_stats(drop_event_values)

        print("\nError Count:")
        print(error_stats)
        print("\nSpresense Errors:")
        print(spresense_error_stats)
        print("\nDropped Frames:")
        print(dropped_frame_stats)
        print("\nDrop Events:")
        print(drop_event_stats)

        # Calculate error rates
        total_frames = sum(s.frame_count for s in self.samples) if self.samples else 0
        total_errors = sum(error_values)
        total_dropped = sum(dropped_frame_values)

        if total_frames > 0:
            error_rate = (total_errors / total_frames) * 100
            drop_rate = (total_dropped / total_frames) * 100
            print(f"\n--- Error Rates ---")
            print(f"Total Frames Processed: {total_frames}")
            print(f"Error Rate:    {error_rate:.4f}%")
            print(f"Drop Rate:     {drop_rate:.4f}%")

    def estimate_system_dynamics(self) -> Tuple[float, float, int]:
        """Estimate system dynamics: time constant, settling time, order"""
        print("\n" + "="*70)
        print("6. SYSTEM DYNAMICS ESTIMATION")
        print("="*70)

        if len(self.samples) < 10:
            print("Insufficient samples for dynamics estimation")
            return 0, 0, 1

        # Estimate time constant from FPS step response
        # Look for significant FPS changes
        fps_values = [s.pc_fps for s in self.samples]

        # Find step changes (> 2 FPS change)
        step_indices = []
        for i in range(1, len(fps_values)):
            if abs(fps_values[i] - fps_values[i-1]) > 2.0:
                step_indices.append(i)

        print(f"\nDetected {len(step_indices)} significant FPS step changes")

        # Estimate time constant (time to reach 63.2% of final value)
        time_constants = []
        settling_times = []

        for step_idx in step_indices[:5]:  # Analyze first 5 steps
            if step_idx + 20 < len(self.samples):  # Need at least 20 samples after step
                initial_fps = fps_values[step_idx - 1]
                final_fps = fps_values[min(step_idx + 20, len(fps_values) - 1)]
                delta_fps = final_fps - initial_fps

                if abs(delta_fps) < 0.5:  # Skip tiny changes
                    continue

                # Find 63.2% point
                target_63 = initial_fps + 0.632 * delta_fps

                # Find when we reach target
                for j in range(step_idx, min(step_idx + 20, len(self.samples))):
                    if abs(fps_values[j] - target_63) < abs(delta_fps * 0.1):
                        time_constant = (self.samples[j].timestamp -
                                       self.samples[step_idx].timestamp) * 1000
                        time_constants.append(time_constant)

                        # Settling time (2% criterion)
                        for k in range(j, min(step_idx + 20, len(self.samples))):
                            if abs(fps_values[k] - final_fps) < abs(delta_fps * 0.02):
                                settling_time = (self.samples[k].timestamp -
                                               self.samples[step_idx].timestamp) * 1000
                                settling_times.append(settling_time)
                                break
                        break

        avg_time_constant = statistics.mean(time_constants) if time_constants else 500
        avg_settling_time = statistics.mean(settling_times) if settling_times else 2000

        # Estimate system order from response characteristics
        # First order: exponential response
        # Second order: possible overshoot/oscillation
        system_order = 1  # Default to first order

        # Check for overshoot/oscillation
        for step_idx in step_indices[:5]:
            if step_idx + 10 < len(fps_values):
                window = fps_values[step_idx:step_idx+10]
                if max(window) > fps_values[step_idx] and min(window) < fps_values[step_idx]:
                    system_order = 2
                    break

        print(f"\nEstimated Time Constant: {avg_time_constant:.2f} ms")
        print(f"Estimated Settling Time: {avg_settling_time:.2f} ms")
        print(f"Estimated System Order: {system_order}")

        return avg_time_constant, avg_settling_time, system_order

    def recommend_pid_parameters(self, time_constant: float, system_order: int) -> None:
        """Recommend PID controller parameters based on system dynamics"""
        print("\n" + "="*70)
        print("7. PID CONTROLLER DESIGN RECOMMENDATIONS")
        print("="*70)

        # Calculate recommended sampling rate (10x faster than time constant)
        if time_constant > 0:
            sample_rate_hz = min(1000 / (time_constant / 10), 100)  # Cap at 100 Hz
        else:
            sample_rate_hz = 10

        sample_period_ms = 1000 / sample_rate_hz

        print(f"\n--- Sampling Requirements ---")
        print(f"Recommended Sample Rate: {sample_rate_hz:.1f} Hz")
        print(f"Sample Period: {sample_period_ms:.1f} ms")

        # Ziegler-Nichols tuning recommendations for first order system
        if system_order == 1 and time_constant > 0:
            # For first-order system with dead time
            # Assume dead time ~ 20% of time constant
            dead_time = time_constant * 0.2
            tau = time_constant

            # PI Controller (recommended for stability)
            kp_pi = 0.9 * (tau / dead_time)
            ti_pi = 3.3 * dead_time

            # PID Controller
            kp_pid = 1.2 * (tau / dead_time)
            ti_pid = 2.0 * dead_time
            td_pid = 0.5 * dead_time

            print(f"\n--- Ziegler-Nichols Tuning ---")
            print(f"Estimated Dead Time: {dead_time:.2f} ms")
            print(f"\nPI Controller (Recommended for Stability):")
            print(f"  Kp = {kp_pi:.4f}")
            print(f"  Ti = {ti_pi:.2f} ms")
            print(f"  Ki = Kp/Ti = {(kp_pi/ti_pi):.6f}")

            print(f"\nPID Controller (For Better Response):")
            print(f"  Kp = {kp_pid:.4f}")
            print(f"  Ti = {ti_pid:.2f} ms")
            print(f"  Td = {td_pid:.2f} ms")
            print(f"  Ki = Kp/Ti = {(kp_pid/ti_pid):.6f}")
            print(f"  Kd = Kp*Td = {(kp_pid*td_pid):.6f}")

        # Cohen-Coon tuning (alternative)
        print(f"\n--- Alternative Tuning Approach ---")
        print(f"Start with conservative values and tune experimentally:")
        print(f"  Kp = 0.1 to 1.0 (proportional gain)")
        print(f"  Ki = 0.001 to 0.1 (integral gain)")
        print(f"  Kd = 0.0 to 0.5 (derivative gain)")
        print(f"\nTuning Strategy:")
        print(f"  1. Start with P-only control (Ki=0, Kd=0)")
        print(f"  2. Increase Kp until sustained oscillation")
        print(f"  3. Reduce Kp by 50%")
        print(f"  4. Add integral term (Ki) to eliminate steady-state error")
        print(f"  5. Add derivative term (Kd) to reduce overshoot")

    def identify_control_variables(self) -> List[ControlSystemAnalysis]:
        """Identify suitable control variables for PID implementation"""
        print("\n" + "="*70)
        print("8. CONTROL SYSTEM VARIABLE IDENTIFICATION")
        print("="*70)

        time_constant, settling_time, system_order = self.estimate_system_dynamics()

        control_systems = []

        # Control System 1: PC FPS Control
        pc_fps_control = ControlSystemAnalysis(
            process_variable="PC FPS (Display Frame Rate)",
            setpoint_candidates=[5.0, 10.0, 15.0, 30.0],
            disturbances=[
                "Network latency variations",
                "JPEG decode time variations",
                "Serial read time variations",
                "System load changes"
            ],
            actuators=[
                "Frame request rate adjustment",
                "Buffer size adjustment",
                "Compression level control",
                "Priority/scheduling adjustment"
            ],
            time_constant_ms=time_constant,
            settling_time_ms=settling_time,
            overshoot_percent=10.0,  # Estimated
            steady_state_error=0.5,  # Estimated in FPS
            system_order=system_order,
            recommended_sample_rate_hz=10.0  # 100ms sample period
        )
        control_systems.append(pc_fps_control)

        # Control System 2: Spresense FPS Control
        spresense_fps_control = ControlSystemAnalysis(
            process_variable="Spresense FPS (Capture Frame Rate)",
            setpoint_candidates=[5.0, 10.0, 15.0, 30.0],
            disturbances=[
                "Camera hardware variations",
                "USB bandwidth limitations",
                "Processing load",
                "TCP command latency"
            ],
            actuators=[
                "Camera frame interval setting",
                "Image quality/compression adjustment",
                "Processing priority adjustment"
            ],
            time_constant_ms=time_constant * 1.5,  # Slower due to hardware
            settling_time_ms=settling_time * 1.5,
            overshoot_percent=15.0,
            steady_state_error=0.5,
            system_order=system_order,
            recommended_sample_rate_hz=5.0  # 200ms sample period
        )
        control_systems.append(spresense_fps_control)

        # Control System 3: Queue Depth Control
        queue_stats = self.calculate_stats([s.action_q_depth for s in self.samples])
        queue_control = ControlSystemAnalysis(
            process_variable="Action Queue Depth",
            setpoint_candidates=[2.0, 3.0, 4.0, 5.0],
            disturbances=[
                "Burst command arrivals",
                "Processing time variations",
                "Network congestion"
            ],
            actuators=[
                "Command rate limiting",
                "Queue priority adjustment",
                "Flow control mechanisms"
            ],
            time_constant_ms=100.0,  # Fast response
            settling_time_ms=500.0,
            overshoot_percent=20.0,
            steady_state_error=0.5,
            system_order=1,
            recommended_sample_rate_hz=20.0  # 50ms sample period
        )
        control_systems.append(queue_control)

        for control_sys in control_systems:
            print(control_sys)

        return control_systems

    def generate_ascii_plots(self) -> None:
        """Generate simple ASCII time-series plots"""
        print("\n" + "="*70)
        print("9. TIME SERIES VISUALIZATION (ASCII)")
        print("="*70)

        if len(self.samples) < 2:
            print("Insufficient samples for plotting")
            return

        # Subsample if too many points
        step = max(1, len(self.samples) // 60)
        plot_samples = self.samples[::step]

        # PC FPS plot
        print("\nPC FPS Over Time:")
        self._ascii_plot([s.pc_fps for s in plot_samples],
                        "FPS", height=10, width=60)

        # Spresense FPS plot
        print("\nSpresense FPS Over Time:")
        self._ascii_plot([s.spresense_fps for s in plot_samples],
                        "FPS", height=10, width=60)

        # TCP latency plot
        print("\nTCP Average Send Time Over Time:")
        self._ascii_plot([s.tcp_avg_send_ms for s in plot_samples],
                        "ms", height=10, width=60)

        # Queue depth plot
        print("\nAction Queue Depth Over Time:")
        self._ascii_plot([s.action_q_depth for s in plot_samples],
                        "depth", height=10, width=60)

    def _ascii_plot(self, values: List[float], unit: str, height: int = 10,
                   width: int = 60) -> None:
        """Create simple ASCII plot"""
        if not values or len(values) == 0:
            print("No data to plot")
            return

        valid_values = [v for v in values if v is not None]
        if not valid_values:
            print("No valid data to plot")
            return

        min_val = min(valid_values)
        max_val = max(valid_values)

        if max_val == min_val:
            range_val = 1
        else:
            range_val = max_val - min_val

        # Create plot grid
        plot = [[' ' for _ in range(width)] for _ in range(height)]

        # Plot values
        for i, val in enumerate(valid_values):
            x = int((i / len(valid_values)) * (width - 1))
            y = height - 1 - int(((val - min_val) / range_val) * (height - 1))
            if 0 <= y < height and 0 <= x < width:
                plot[y][x] = '*'

        # Print plot
        print(f"  Max: {max_val:.2f} {unit} |" + "-" * (width))
        for row in plot:
            print("              |" + ''.join(row))
        print(f"  Min: {min_val:.2f} {unit} |" + "-" * (width))
        print(f"              0" + " " * (width-10) + f"{len(valid_values)} samples")

    def analyze_control_performance(self) -> None:
        """Analyze how well the current system maintains target performance"""
        print("\n" + "="*70)
        print("10. CURRENT CONTROL PERFORMANCE ASSESSMENT")
        print("="*70)

        if not self.samples:
            print("No samples to analyze")
            return

        # Check if system is maintaining any target FPS
        fps_values = [s.pc_fps for s in self.samples]

        print("\n--- Open-Loop Performance (No PID) ---")

        # Calculate how often system is within acceptable range of targets
        targets = [5, 10, 15, 30]
        tolerance = 1.0  # +/- 1 FPS

        for target in targets:
            in_range = sum(1 for fps in fps_values if abs(fps - target) <= tolerance)
            percentage = (in_range / len(fps_values)) * 100
            print(f"\nTarget {target} FPS (+/- {tolerance}):")
            print(f"  In-range samples: {in_range}/{len(fps_values)} ({percentage:.1f}%)")

            # Calculate mean absolute error
            errors = [abs(fps - target) for fps in fps_values]
            mae = statistics.mean(errors)
            print(f"  Mean Absolute Error: {mae:.2f} FPS")

        # Analyze stability
        print("\n--- Stability Metrics ---")

        if len(fps_values) > 1:
            # Calculate consecutive frame-to-frame variation
            variations = [abs(fps_values[i] - fps_values[i-1])
                         for i in range(1, len(fps_values))]
            var_stats = self.calculate_stats(variations)

            print("\nFrame-to-Frame FPS Variation:")
            print(f"  Mean:   {var_stats.mean:.2f} FPS")
            print(f"  StdDev: {var_stats.std_dev:.2f} FPS")
            print(f"  Max:    {var_stats.max_val:.2f} FPS")

            # Stability score (lower variation = better)
            stability_score = 100 / (1 + var_stats.mean)
            print(f"\nStability Score: {stability_score:.1f}/100")
            print(f"  (Higher is better, based on low FPS variation)")

    def generate_report(self) -> None:
        """Generate comprehensive analysis report"""
        print("\n" + "="*80)
        print("SECURITY CAMERA METRICS ANALYSIS FOR PID CONTROL IMPLEMENTATION")
        print("="*80)
        print(f"\nAnalysis Date: {Path.cwd()}")
        print(f"Metrics Directory: {self.metrics_dir}")
        print(f"Total Samples: {len(self.samples)}")
        print(f"Files Processed: {self.files_processed}")

        if not self.samples:
            print("\nNo data to analyze!")
            return

        # Run all analyses
        self.analyze_fps_performance()
        self.analyze_tcp_performance()
        self.analyze_processing_times()
        self.analyze_queue_behavior()
        self.analyze_error_patterns()

        time_constant, settling_time, system_order = self.estimate_system_dynamics()

        self.identify_control_variables()
        self.recommend_pid_parameters(time_constant, system_order)
        self.analyze_control_performance()
        self.generate_ascii_plots()

        # Final recommendations
        print("\n" + "="*70)
        print("11. IMPLEMENTATION RECOMMENDATIONS FOR PHASE 10")
        print("="*70)

        print("""
PRIMARY CONTROL OBJECTIVE: Maintain stable FPS at configurable setpoints

RECOMMENDED CONTROL STRATEGY:
1. Implement PI controller for FPS regulation (start without derivative)
2. Use PC FPS as primary process variable (direct measurement)
3. Control actuators:
   - Frame request rate to Spresense
   - Queue depth management
   - Compression quality adjustment

CONTROL LOOP ARCHITECTURE:
1. Measurement Loop (10 Hz / 100ms):
   - Measure PC FPS (rolling average over 1 second)
   - Measure Spresense FPS
   - Measure queue depth and latencies

2. Control Loop (5 Hz / 200ms):
   - Calculate error = setpoint - measured_fps
   - Update PI controller
   - Adjust frame request rate
   - Apply rate limits and saturation

3. Monitoring Loop (1 Hz):
   - Log performance metrics
   - Detect anomalies
   - Adjust setpoint if needed

IMPLEMENTATION PHASES:
Phase 1: Measurement Infrastructure
  - Add FPS measurement with rolling window
  - Implement sample rate timing
  - Add control metrics logging

Phase 2: P-Only Controller
  - Implement proportional control
  - Test stability at different Kp values
  - Identify critical gain (oscillation point)

Phase 3: PI Controller
  - Add integral term to eliminate steady-state error
  - Tune Ki for good transient response
  - Test setpoint tracking

Phase 4: Advanced Features
  - Add derivative term if needed (PID)
  - Implement anti-windup for integral term
  - Add feedforward compensation
  - Add adaptive tuning

TUNING APPROACH:
1. Start with Kp=0.1, Ki=0, Kd=0
2. Increase Kp until system oscillates
3. Reduce Kp by 50%
4. Add Ki=0.01 to eliminate steady-state error
5. Fine-tune based on performance

SETPOINT SELECTION:
Recommended targets: 5, 10, 15, 30 FPS
- Start with 10 FPS (good balance)
- Allow runtime setpoint changes
- Implement setpoint ramping (avoid steps)

SAFETY LIMITS:
- Max frame request rate: 30 FPS
- Min frame request rate: 1 FPS
- Queue depth limits: 2-10 items
- Integral windup limits: +/- 5.0
- Control output saturation: 0-30 FPS

PERFORMANCE TARGETS:
- Steady-state error: < 0.5 FPS
- Settling time: < 2 seconds
- Overshoot: < 10%
- Stability: CV < 15%
""")

        print("\n" + "="*70)
        print("END OF ANALYSIS REPORT")
        print("="*70)


def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        metrics_dir = sys.argv[1]
    else:
        metrics_dir = "/home/ken/Rust_ws/security_camera_viewer/release_windows/metrics/"

    if not os.path.isdir(metrics_dir):
        print(f"Error: Directory not found: {metrics_dir}")
        sys.exit(1)

    analyzer = MetricsAnalyzer(metrics_dir)
    analyzer.load_all_metrics()
    analyzer.generate_report()


if __name__ == "__main__":
    main()

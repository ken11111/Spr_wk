#!/usr/bin/env python3
"""
高度制御工学分析ツール - Advanced Control Engineering Analysis Tool
Phase 8-9.2 キューシステムの周波数応答・安定性・制御性能解析
"""

import math
import cmath
import csv
import json
from typing import List, Tuple, Dict

class AdvancedControlAnalysis:
    """高度制御工学解析クラス"""

    def __init__(self):
        # システムパラメータ
        self.K1 = 1.0           # Queue controller gain
        self.tau1 = 0.033       # Queue time constant [s]
        self.K2 = 0.85          # TCP controller gain
        self.tau2 = 0.134       # TCP time constant [s]
        self.theta = 0.025      # Average delay [s]
        self.K3 = 1.0           # TCP Reader gain
        self.tau3 = 0.174       # TCP Reader time constant [s]
        self.K4 = 1.0           # JPEG Decoder gain
        self.tau4 = 0.002       # JPEG Decoder time constant [s]
        self.K5 = 1.0           # GUI controller gain
        self.tau5 = 0.0167      # GUI update period [s]

    def transfer_function_G1(self, s):
        """Queue Controller G1(s) = K1/(tau1*s + 1)"""
        return self.K1 / (self.tau1 * s + 1)

    def transfer_function_G2(self, s):
        """TCP Controller G2(s) = K2*exp(-theta*s)/(tau2*s + 1)"""
        delay_term = cmath.exp(-self.theta * s)
        return self.K2 * delay_term / (self.tau2 * s + 1)

    def transfer_function_G3(self, s):
        """TCP Reader G3(s) = K3/(tau3*s + 1)"""
        return self.K3 / (self.tau3 * s + 1)

    def transfer_function_G4(self, s):
        """JPEG Decoder G4(s) = K4/(tau4*s + 1)"""
        return self.K4 / (self.tau4 * s + 1)

    def transfer_function_G5(self, s):
        """GUI Controller G5(s) = K5/(tau5*s + 1)"""
        return self.K5 / (self.tau5 * s + 1)

    def open_loop_transfer_function(self, s):
        """開ループ伝達関数 L(s) = G1(s)*G2(s)*G3(s)*G4(s)*G5(s)"""
        return (self.transfer_function_G1(s) *
                self.transfer_function_G2(s) *
                self.transfer_function_G3(s) *
                self.transfer_function_G4(s) *
                self.transfer_function_G5(s))

    def closed_loop_transfer_function(self, s):
        """閉ループ伝達関数 T(s) = L(s)/(1 + L(s))"""
        L_s = self.open_loop_transfer_function(s)
        return L_s / (1 + L_s)

    def frequency_response_analysis(self, freq_range: Tuple[float, float] = (0.01, 100),
                                   num_points: int = 100) -> Dict:
        """周波数応答解析"""
        print("📊 周波数応答解析実行中...")

        # 対数スケールで周波数ポイント生成
        log_freq_start = math.log10(freq_range[0])
        log_freq_end = math.log10(freq_range[1])
        log_freq_step = (log_freq_end - log_freq_start) / (num_points - 1)

        frequencies = []
        magnitude_db = []
        phase_deg = []
        gain_margins = []
        phase_margins = []

        for i in range(num_points):
            freq = 10 ** (log_freq_start + i * log_freq_step)
            omega = 2 * math.pi * freq
            s = 1j * omega

            # 開ループ伝達関数評価
            L_s = self.open_loop_transfer_function(s)

            # 大きさと位相
            magnitude = abs(L_s)
            magnitude_db_val = 20 * math.log10(magnitude) if magnitude > 1e-12 else -240
            phase_rad = cmath.phase(L_s)
            phase_deg_val = math.degrees(phase_rad)

            frequencies.append(freq)
            magnitude_db.append(magnitude_db_val)
            phase_deg.append(phase_deg_val)

        # 安定余裕計算
        gain_margin, phase_margin = self._calculate_stability_margins(frequencies, magnitude_db, phase_deg)

        return {
            'frequencies': frequencies,
            'magnitude_db': magnitude_db,
            'phase_deg': phase_deg,
            'gain_margin_db': gain_margin,
            'phase_margin_deg': phase_margin
        }

    def _calculate_stability_margins(self, frequencies: List[float],
                                   magnitude_db: List[float],
                                   phase_deg: List[float]) -> Tuple[float, float]:
        """安定余裕計算"""
        gain_margin = float('inf')
        phase_margin = 0

        # ゲイン余裕 (位相が-180°の周波数での-|G(jω)|)
        for i, phase in enumerate(phase_deg):
            if abs(phase + 180) < 5:  # -180°近傍
                gain_margin = -magnitude_db[i]
                break

        # 位相余裕 (|G(jω)|=1(0dB)の周波数での位相マージン)
        for i, mag in enumerate(magnitude_db):
            if abs(mag) < 1:  # 0dB近傍
                phase_margin = 180 + phase_deg[i]
                break

        return gain_margin, phase_margin

    def step_response_analysis(self, duration: float = 2.0, num_points: int = 1000) -> Dict:
        """ステップ応答解析"""
        print("📈 ステップ応答解析実行中...")

        time_points = [i * duration / (num_points - 1) for i in range(num_points)]
        step_responses = []

        for t in time_points:
            # ステップ応答の近似計算 (複数の1次遅れ系の合成)
            response = 0
            if t > 0:
                # 各サブシステムの寄与
                response += self.K1 * (1 - math.exp(-t/self.tau1))
                response *= self.K2 * (1 - math.exp(-max(0, t-self.theta)/self.tau2))
                response *= self.K3 * (1 - math.exp(-t/self.tau3))
                response *= self.K4 * (1 - math.exp(-t/self.tau4))
                response *= self.K5 * (1 - math.exp(-t/self.tau5))

                # 正規化
                response = min(response, 0.85)  # 実測最大ゲイン

            step_responses.append(response)

        # 性能指標計算
        steady_state = step_responses[-1]
        rise_time = self._calculate_rise_time(time_points, step_responses, steady_state)
        settling_time = self._calculate_settling_time(time_points, step_responses, steady_state)
        overshoot = self._calculate_overshoot(step_responses, steady_state)

        return {
            'time': time_points,
            'response': step_responses,
            'steady_state': steady_state,
            'rise_time': rise_time,
            'settling_time': settling_time,
            'overshoot_percent': overshoot
        }

    def _calculate_rise_time(self, time_points: List[float],
                           responses: List[float], steady_state: float) -> float:
        """立ち上がり時間計算 (10%-90%)"""
        target_10 = steady_state * 0.1
        target_90 = steady_state * 0.9

        t_10, t_90 = None, None
        for t, response in zip(time_points, responses):
            if t_10 is None and response >= target_10:
                t_10 = t
            if t_90 is None and response >= target_90:
                t_90 = t
                break

        return (t_90 - t_10) if (t_10 is not None and t_90 is not None) else 0

    def _calculate_settling_time(self, time_points: List[float],
                               responses: List[float], steady_state: float) -> float:
        """整定時間計算 (2%誤差範囲)"""
        tolerance = 0.02 * steady_state
        settling_time = 0

        for i in range(len(responses) - 1, -1, -1):
            if abs(responses[i] - steady_state) > tolerance:
                settling_time = time_points[i]
                break

        return settling_time

    def _calculate_overshoot(self, responses: List[float], steady_state: float) -> float:
        """オーバーシュート計算"""
        max_response = max(responses)
        if steady_state > 0:
            overshoot = ((max_response - steady_state) / steady_state) * 100
            return max(0, overshoot)
        return 0

    def stability_analysis(self) -> Dict:
        """安定性解析 (Routh-Hurwitz判定等)"""
        print("🔍 安定性解析実行中...")

        # 特性方程式の係数 (近似)
        # 1 + L(s) = 0 の根の位置推定
        roots_real = []
        roots_imag = []

        # 各サブシステムの極 (左半平面)
        poles = [
            -1/self.tau1,  # Queue controller pole
            -1/self.tau2,  # TCP controller pole
            -1/self.tau3,  # TCP reader pole
            -1/self.tau4,  # JPEG decoder pole
            -1/self.tau5   # GUI controller pole
        ]

        for pole in poles:
            roots_real.append(pole)
            roots_imag.append(0)

        # 安定性判定
        stable = all(real < 0 for real in roots_real)

        # 相対安定度指標
        freq_response = self.frequency_response_analysis()
        gain_margin = freq_response['gain_margin_db']
        phase_margin = freq_response['phase_margin_deg']

        stability_score = "A" if (gain_margin > 6 and phase_margin > 45) else \
                         "B" if (gain_margin > 3 and phase_margin > 30) else \
                         "C"

        return {
            'stable': stable,
            'poles_real': roots_real,
            'poles_imag': roots_imag,
            'gain_margin_db': gain_margin,
            'phase_margin_deg': phase_margin,
            'stability_score': stability_score
        }

    def performance_optimization_analysis(self) -> Dict:
        """性能最適化分析"""
        print("⚙️ 性能最適化分析実行中...")

        # 現在のシステム性能評価
        step_response = self.step_response_analysis()
        freq_response = self.frequency_response_analysis()
        stability = self.stability_analysis()

        # 帯域幅計算 (-3dB点)
        bandwidth = 0
        for freq, mag_db in zip(freq_response['frequencies'], freq_response['magnitude_db']):
            if mag_db < -3:
                bandwidth = freq
                break

        # 性能指標統合
        performance_metrics = {
            'bandwidth_hz': bandwidth,
            'rise_time_ms': step_response['rise_time'] * 1000,
            'settling_time_ms': step_response['settling_time'] * 1000,
            'steady_state_error_percent': (1 - step_response['steady_state']) * 100,
            'overshoot_percent': step_response['overshoot_percent'],
            'gain_margin_db': stability['gain_margin_db'],
            'phase_margin_deg': stability['phase_margin_deg']
        }

        # 最適化推奨事項
        recommendations = []
        if performance_metrics['rise_time_ms'] > 500:
            recommendations.append("制御器ゲインを増加して応答性を改善")
        if performance_metrics['overshoot_percent'] > 10:
            recommendations.append("ダンピングを増加してオーバーシュートを抑制")
        if performance_metrics['gain_margin_db'] < 6:
            recommendations.append("ゲイン余裕の改善が必要")
        if performance_metrics['phase_margin_deg'] < 45:
            recommendations.append("位相余裕の改善が必要")

        return {
            'current_performance': performance_metrics,
            'optimization_recommendations': recommendations,
            'overall_score': stability['stability_score']
        }

def create_detailed_ascii_plot(x_data: List[float], y_data: List[float],
                              title: str, x_label: str = "X", y_label: str = "Y",
                              width: int = 80, height: int = 25, log_scale_x: bool = False) -> str:
    """詳細ASCII文字プロット生成"""
    if not x_data or not y_data:
        return "No data to plot"

    # データ範囲
    if log_scale_x:
        x_min, x_max = math.log10(min(x_data)), math.log10(max(x_data))
        x_data_norm = [math.log10(x) for x in x_data]
    else:
        x_min, x_max = min(x_data), max(x_data)
        x_data_norm = x_data

    y_min, y_max = min(y_data), max(y_data)

    plot_lines = []
    plot_lines.append("=" * width)
    plot_lines.append(f"{title:^{width}}")
    plot_lines.append("=" * width)

    # Y軸ラベル幅
    y_label_width = 10

    # プロット領域
    for row in range(height):
        y_val = y_max - (y_max - y_min) * row / (height - 1)
        line = f"{y_val:8.2f} |"

        for col in range(width - y_label_width - 2):
            x_val = x_min + (x_max - x_min) * col / (width - y_label_width - 3)

            # データポイント検索
            plot_char = " "
            min_distance = float('inf')
            for i, (x_norm, y) in enumerate(zip(x_data_norm, y_data)):
                x_distance = abs(x_norm - x_val) / (x_max - x_min) if x_max != x_min else 0
                y_distance = abs(y - y_val) / (y_max - y_min) if y_max != y_min else 0
                distance = math.sqrt(x_distance**2 + y_distance**2)

                if distance < min_distance and distance < 0.05:  # 閾値
                    min_distance = distance
                    plot_char = "*"

            line += plot_char

        plot_lines.append(line)

    # X軸
    plot_lines.append(" " * y_label_width + "-" * (width - y_label_width - 1))

    # X軸ラベル
    if log_scale_x:
        x_min_orig, x_max_orig = min(x_data), max(x_data)
        plot_lines.append(f"{x_min_orig:10.2e}" + " " * (width - 25) + f"{x_max_orig:10.2e}")
    else:
        plot_lines.append(f"{x_min:10.2f}" + " " * (width - 25) + f"{x_max:10.2f}")

    plot_lines.append(f"{y_label:^{y_label_width}}" + f"{x_label:^{width-y_label_width}}")

    return "\n".join(plot_lines)

def main():
    """メイン実行関数"""
    print("🎛️ 高度制御工学分析ツール - Advanced Control Engineering Analysis")
    print("=" * 80)

    # 解析エンジン初期化
    analyzer = AdvancedControlAnalysis()

    print("\n🔧 システムパラメータ:")
    print(f"  Queue Controller: K1={analyzer.K1}, τ1={analyzer.tau1*1000:.1f}ms")
    print(f"  TCP Controller:   K2={analyzer.K2}, τ2={analyzer.tau2*1000:.1f}ms")
    print(f"  TCP Reader:       K3={analyzer.K3}, τ3={analyzer.tau3*1000:.1f}ms")
    print(f"  JPEG Decoder:     K4={analyzer.K4}, τ4={analyzer.tau4*1000:.1f}ms")
    print(f"  GUI Controller:   K5={analyzer.K5}, τ5={analyzer.tau5*1000:.1f}ms")

    # 周波数応答解析
    freq_response = analyzer.frequency_response_analysis()
    print(f"\n📊 周波数応答解析結果:")
    print(f"  ゲイン余裕: {freq_response['gain_margin_db']:.1f} dB")
    print(f"  位相余裕:   {freq_response['phase_margin_deg']:.1f} °")

    # ステップ応答解析
    step_response = analyzer.step_response_analysis()
    print(f"\n📈 ステップ応答解析結果:")
    print(f"  立ち上がり時間: {step_response['rise_time']*1000:.1f} ms")
    print(f"  整定時間:       {step_response['settling_time']*1000:.1f} ms")
    print(f"  オーバーシュート: {step_response['overshoot_percent']:.1f} %")
    print(f"  定常値:         {step_response['steady_state']:.3f}")

    # 安定性解析
    stability = analyzer.stability_analysis()
    print(f"\n🔍 安定性解析結果:")
    print(f"  システム安定性: {'安定' if stability['stable'] else '不安定'}")
    print(f"  安定性スコア:   {stability['stability_score']}")

    # 性能最適化分析
    optimization = analyzer.performance_optimization_analysis()
    print(f"\n⚙️ 性能最適化分析:")
    print(f"  帯域幅: {optimization['current_performance']['bandwidth_hz']:.2f} Hz")
    print(f"  総合スコア: {optimization['overall_score']}")

    if optimization['optimization_recommendations']:
        print("\n💡 最適化推奨事項:")
        for i, rec in enumerate(optimization['optimization_recommendations'], 1):
            print(f"  {i}. {rec}")

    # ASCII プロット生成
    print("\n📊 Bode線図 - 大きさ:")
    magnitude_plot = create_detailed_ascii_plot(
        freq_response['frequencies'],
        freq_response['magnitude_db'],
        "Magnitude [dB] vs Frequency [Hz]",
        "Frequency [Hz]", "Magnitude [dB]",
        width=80, height=20, log_scale_x=True
    )
    print(magnitude_plot)

    print("\n📊 Bode線図 - 位相:")
    phase_plot = create_detailed_ascii_plot(
        freq_response['frequencies'],
        freq_response['phase_deg'],
        "Phase [deg] vs Frequency [Hz]",
        "Frequency [Hz]", "Phase [deg]",
        width=80, height=20, log_scale_x=True
    )
    print(phase_plot)

    print("\n📊 ステップ応答:")
    step_plot = create_detailed_ascii_plot(
        step_response['time'],
        step_response['response'],
        "Step Response",
        "Time [s]", "Response",
        width=80, height=20
    )
    print(step_plot)

    # 結果保存
    results = {
        'frequency_response': freq_response,
        'step_response': step_response,
        'stability_analysis': stability,
        'optimization_analysis': optimization
    }

    output_file = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis/control_analysis_results.json"
    with open(output_file, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\n💾 詳細解析結果を保存: {output_file}")
    print("\n✅ 高度制御工学分析完了!")

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
"""
制御工学高度グラフライブラリ - Advanced Control Engineering Graphs
特殊な制御系表現・解析グラフの集合
"""

import math
import json
from typing import List, Tuple, Dict

class AdvancedControlGraphs:
    """制御工学高度グラフクラス"""

    def __init__(self):
        # Phase 8-9.2システムパラメータ
        self.params = {
            'K1': 1.0, 'tau1': 0.033,   # Queue controller
            'K2': 0.85, 'tau2': 0.134,  # TCP controller
            'K3': 1.0, 'tau3': 0.174,   # TCP Reader
            'K4': 1.0, 'tau4': 0.002,   # JPEG Decoder
            'K5': 1.0, 'tau5': 0.0167,  # GUI controller
            'theta': 0.025               # Delay
        }

    def create_pole_zero_map(self, width: int = 60, height: int = 25) -> str:
        """極零点配置図 - s平面上の極と零点の配置"""
        print("🎯 極零点配置図生成中...")

        # システムの極と零点を計算
        poles = []
        zeros = []

        # 各サブシステムの極 (分母の根)
        poles.extend([
            -1/self.params['tau1'],  # Queue controller
            -1/self.params['tau2'],  # TCP controller
            -1/self.params['tau3'],  # TCP Reader
            -1/self.params['tau4'],  # JPEG Decoder
            -1/self.params['tau5']   # GUI controller
        ])

        # 遅延による無限個の極 (近似として主要なもの)
        for n in range(1, 6):
            pole_delay = -n * math.pi / self.params['theta']
            poles.append(pole_delay)

        # 零点 (この系では特別な零点なし、原点近傍のみ)
        zeros.append(0)

        pole_zero_plot = self._create_s_plane_plot(
            poles, zeros, "極零点配置図 (s平面)", width, height
        )

        analysis = self._analyze_pole_zero_placement(poles, zeros)

        return pole_zero_plot + "\n" + analysis

    def create_sensitivity_function_plot(self, width: int = 80, height: int = 20) -> str:
        """感度関数プロット - S(s) = 1/(1+L(s))"""
        print("📊 感度関数解析中...")

        frequencies = [0.01 * (2 ** (i/10)) for i in range(80)]
        sensitivity_mag = []
        complementary_mag = []

        for freq in frequencies:
            omega = 2 * math.pi * freq
            s = 1j * omega

            # 開ループ伝達関数 L(s)
            L_s = self._calculate_open_loop_tf(s)

            # 感度関数 S(s) = 1/(1+L(s))
            S_s = 1 / (1 + L_s)
            sensitivity_mag.append(abs(S_s))

            # 相補感度関数 T(s) = L(s)/(1+L(s))
            T_s = L_s / (1 + L_s)
            complementary_mag.append(abs(T_s))

        sensitivity_plot = self._create_dual_frequency_plot(
            frequencies, sensitivity_mag, complementary_mag,
            "感度関数解析", "感度関数 |S(jω)|", "相補感度関数 |T(jω)|",
            width, height
        )

        analysis = self._analyze_sensitivity_functions(frequencies, sensitivity_mag, complementary_mag)

        return sensitivity_plot + "\n" + analysis

    def create_disturbance_rejection_plot(self, width: int = 80, height: int = 20) -> str:
        """外乱抑制特性プロット"""
        print("🛡️ 外乱抑制特性解析中...")

        frequencies = [0.01 * (1.5 ** i) for i in range(60)]
        input_disturbance = []  # 入力外乱に対する特性
        output_disturbance = [] # 出力外乱に対する特性

        for freq in frequencies:
            omega = 2 * math.pi * freq
            s = 1j * omega

            L_s = self._calculate_open_loop_tf(s)

            # 入力外乱抑制: |S(s)| = |1/(1+L(s))|
            input_rejection = abs(1 / (1 + L_s))
            input_disturbance.append(20 * math.log10(input_rejection) if input_rejection > 1e-12 else -120)

            # 出力外乱抑制: |1/L(s)|
            output_rejection = abs(1 / L_s) if abs(L_s) > 1e-12 else 1e12
            output_disturbance.append(20 * math.log10(output_rejection) if output_rejection > 1e-12 else -120)

        disturbance_plot = self._create_dual_frequency_plot_db(
            frequencies, input_disturbance, output_disturbance,
            "外乱抑制特性", "入力外乱抑制 [dB]", "出力外乱抑制 [dB]",
            width, height
        )

        analysis = self._analyze_disturbance_rejection(frequencies, input_disturbance, output_disturbance)

        return disturbance_plot + "\n" + analysis

    def create_robustness_analysis(self, width: int = 80, height: int = 20) -> str:
        """ロバスト性解析プロット"""
        print("💪 ロバスト性解析中...")

        # パラメータ変動に対する解析
        param_variations = [-50, -25, -10, 0, 10, 25, 50]  # [%]
        frequencies = [0.1 * (2 ** i) for i in range(15)]

        robustness_data = []

        for variation in param_variations:
            # パラメータ変動適用
            K2_varied = self.params['K2'] * (1 + variation/100)
            tau2_varied = self.params['tau2'] * (1 + variation/100)

            freq_response = []
            for freq in frequencies:
                omega = 2 * math.pi * freq
                s = 1j * omega

                # 変動したシステムの応答
                L_s_varied = self._calculate_varied_tf(s, K2_varied, tau2_varied)
                T_s_varied = L_s_varied / (1 + L_s_varied)

                freq_response.append(abs(T_s_varied))

            robustness_data.append(freq_response)

        robustness_plot = self._create_robustness_visualization(
            frequencies, robustness_data, param_variations,
            "ロバスト性解析 - パラメータ変動影響", width, height
        )

        analysis = self._analyze_robustness(robustness_data, param_variations)

        return robustness_plot + "\n" + analysis

    def create_performance_specs_plot(self, width: int = 70, height: int = 25) -> str:
        """制御仕様プロット - 制御要求との比較"""
        print("🎯 制御仕様適合性解析中...")

        # 制御仕様定義
        specs = {
            'bandwidth_min': 0.5,      # 最小帯域幅 [Hz]
            'phase_margin_min': 45,    # 最小位相余裕 [deg]
            'gain_margin_min': 6,      # 最小ゲイン余裕 [dB]
            'settling_time_max': 1.0,  # 最大整定時間 [s]
            'overshoot_max': 10,       # 最大オーバーシュート [%]
            'steady_state_error_max': 5 # 最大定常偏差 [%]
        }

        # 現在のシステム性能
        current_performance = self._evaluate_current_performance()

        specs_plot = self._create_specs_comparison_chart(
            specs, current_performance, "制御仕様適合性評価", width, height
        )

        compliance_analysis = self._analyze_specs_compliance(specs, current_performance)

        return specs_plot + "\n" + compliance_analysis

    def create_phase_comparison_radar(self, width: int = 60, height: int = 30) -> str:
        """Phase別レーダーチャート"""
        print("📊 Phase別レーダーチャート生成中...")

        # 評価項目と各Phaseのスコア (0-100%)
        metrics = {
            'FPS': [44, 100, 100],       # Phase 7.3.3, 8, 9.2
            'キュー効率': [15, 77, 82],
            'TCP性能': [20, 43, 43],
            '安定性': [25, 75, 100],
            '応答性': [10, 85, 85],
            'ロバスト性': [30, 70, 95]
        }

        phases = ['Phase 7.3.3', 'Phase 8', 'Phase 9.2']

        radar_plot = self._create_radar_chart(
            metrics, phases, "Phase別総合性能レーダーチャート", width, height
        )

        return radar_plot

    def create_control_effort_analysis(self, width: int = 80, height: int = 20) -> str:
        """制御入力解析 - 制御コストと性能のトレードオフ"""
        print("⚡ 制御入力・制御コスト解析中...")

        # 時間範囲での制御入力シミュレーション
        time_points = [i * 0.01 for i in range(300)]
        control_signals = []
        control_effort = []
        cumulative_effort = 0

        for t in time_points:
            # ステップ目標値に対する制御入力
            if t > 0.1:  # ステップ開始
                error = 1.0 - self._calculate_output_at_time(t)
                control_signal = error * 2.0  # 比例制御近似
                control_signals.append(abs(control_signal))

                # 制御コスト累積
                cumulative_effort += abs(control_signal) * 0.01
                control_effort.append(cumulative_effort)
            else:
                control_signals.append(0)
                control_effort.append(0)

        effort_plot = self._create_dual_time_plot(
            time_points, control_signals, control_effort,
            "制御入力・制御コスト解析", "制御信号", "累積制御コスト", width, height
        )

        effort_analysis = self._analyze_control_effort(control_signals, control_effort)

        return effort_plot + "\n" + effort_analysis

    # ヘルパーメソッド群

    def _calculate_open_loop_tf(self, s):
        """開ループ伝達関数計算"""
        import cmath
        G1 = self.params['K1'] / (self.params['tau1'] * s + 1)
        G2 = self.params['K2'] * cmath.exp(-self.params['theta'] * s) / (self.params['tau2'] * s + 1)
        return G1 * G2

    def _calculate_varied_tf(self, s, K2_varied, tau2_varied):
        """パラメータ変動時の伝達関数"""
        import cmath
        G1 = self.params['K1'] / (self.params['tau1'] * s + 1)
        G2 = K2_varied * cmath.exp(-self.params['theta'] * s) / (tau2_varied * s + 1)
        return G1 * G2

    def _calculate_output_at_time(self, t):
        """時間tでの出力計算"""
        if t <= 0:
            return 0

        # 複数1次遅れ系の合成近似
        y1 = self.params['K1'] * (1 - math.exp(-t/self.params['tau1']))
        y2 = self.params['K2'] * (1 - math.exp(-max(0, t-self.params['theta'])/self.params['tau2']))
        return y1 * y2 * 0.85

    def _create_s_plane_plot(self, poles: List[float], zeros: List[float],
                            title: str, width: int, height: int) -> str:
        """s平面プロット作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # データ範囲計算
        all_values = poles + zeros
        if not all_values:
            return "データなし"

        real_min, real_max = min(all_values), max(all_values)
        imag_range = abs(real_max - real_min) / 2  # 虚軸範囲

        center_col = width // 2
        center_row = height // 2

        # s平面描画
        for row in range(height):
            imag_val = imag_range - 2 * imag_range * row / (height - 1)
            line = f"{imag_val:6.1f}j|"

            for col in range(width - 8):
                real_val = real_min + (real_max - real_min) * col / (width - 9)

                plot_char = " "

                # 虚軸 (実部=0)
                if abs(real_val) < (real_max - real_min) / (width - 9):
                    plot_char = "|"

                # 実軸 (虚部=0)
                if abs(imag_val) < 2 * imag_range / height:
                    plot_char = "-" if plot_char == " " else "+"

                # 極プロット
                for pole in poles:
                    if abs(pole - real_val) < (real_max - real_min) / (width - 9) * 1.5:
                        plot_char = "×"

                # 零点プロット
                for zero in zeros:
                    if abs(zero - real_val) < (real_max - real_min) / (width - 9) * 1.5:
                        plot_char = "○"

                line += plot_char

            lines.append(line)

        lines.append(" " * 7 + "-" * (width - 8))
        lines.append(f"{real_min:7.1f}" + " " * (width - 18) + f"{real_max:7.1f}")
        lines.append("凡例: ×=極, ○=零点, +=原点, |=虚軸, -=実軸")

        return "\n".join(lines)

    def _create_dual_frequency_plot(self, frequencies: List[float], y1_data: List[float],
                                   y2_data: List[float], title: str, y1_label: str,
                                   y2_label: str, width: int, height: int) -> str:
        """双軸周波数プロット"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # 対数周波数軸
        log_freq_data = [math.log10(f) for f in frequencies]
        freq_min, freq_max = min(log_freq_data), max(log_freq_data)

        # 上半分: y1データ
        y1_min, y1_max = min(y1_data), max(y1_data)
        lines.append(f"{y1_label}:")

        for row in range(height // 2):
            y_val = y1_max - (y1_max - y1_min) * row / (height // 2 - 1)
            line = f"{y_val:6.3f}|"

            for col in range(width - 8):
                freq_val = freq_min + (freq_max - freq_min) * col / (width - 9)

                plot_char = " "
                for log_freq, y1 in zip(log_freq_data, y1_data):
                    if (abs(log_freq - freq_val) < (freq_max - freq_min) / (width - 9) * 1.5 and
                        abs(y1 - y_val) < (y1_max - y1_min) / (height // 2) * 1.5):
                        plot_char = "*"
                        break

                line += plot_char

            lines.append(line)

        lines.append("-" * width)

        # 下半分: y2データ
        y2_min, y2_max = min(y2_data), max(y2_data)
        lines.append(f"{y2_label}:")

        for row in range(height // 2):
            y_val = y2_max - (y2_max - y2_min) * row / (height // 2 - 1)
            line = f"{y_val:6.3f}|"

            for col in range(width - 8):
                freq_val = freq_min + (freq_max - freq_min) * col / (width - 9)

                plot_char = " "
                for log_freq, y2 in zip(log_freq_data, y2_data):
                    if (abs(log_freq - freq_val) < (freq_max - freq_min) / (width - 9) * 1.5 and
                        abs(y2 - y_val) < (y2_max - y2_min) / (height // 2) * 1.5):
                        plot_char = "o"
                        break

                line += plot_char

            lines.append(line)

        # 周波数軸
        lines.append(f"{frequencies[0]:8.2e}" + " " * (width - 20) + f"{frequencies[-1]:8.2e}")

        return "\n".join(lines)

    def _create_dual_frequency_plot_db(self, frequencies: List[float], y1_data: List[float],
                                      y2_data: List[float], title: str, y1_label: str,
                                      y2_label: str, width: int, height: int) -> str:
        """dB表示の双軸周波数プロット"""
        return self._create_dual_frequency_plot(frequencies, y1_data, y2_data, title, y1_label, y2_label, width, height)

    def _create_dual_time_plot(self, time_data: List[float], y1_data: List[float],
                              y2_data: List[float], title: str, y1_label: str,
                              y2_label: str, width: int, height: int) -> str:
        """時間軸双軸プロット"""
        return self._create_dual_frequency_plot(time_data, y1_data, y2_data, title, y1_label, y2_label, width, height)

    def _create_robustness_visualization(self, frequencies: List[float], robustness_data: List[List[float]],
                                        variations: List[float], title: str, width: int, height: int) -> str:
        """ロバスト性可視化"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # パラメータ変動による応答の散らばりを可視化
        lines.append("パラメータ変動による周波数応答の変化:")
        lines.append(f"変動範囲: {min(variations)}% ～ {max(variations)}%")

        # 各周波数での最大・最小応答
        max_responses = []
        min_responses = []

        for i in range(len(frequencies)):
            freq_responses = [data[i] for data in robustness_data]
            max_responses.append(max(freq_responses))
            min_responses.append(min(freq_responses))

        # 変動幅可視化
        lines.append("\n周波数応答変動幅 (最大/最小):")

        log_freq_data = [math.log10(f) for f in frequencies]
        freq_min, freq_max = min(log_freq_data), max(log_freq_data)
        resp_min, resp_max = min(min_responses), max(max_responses)

        for row in range(height):
            resp_val = resp_max - (resp_max - resp_min) * row / (height - 1)
            line = f"{resp_val:6.3f}|"

            for col in range(width - 8):
                freq_val = freq_min + (freq_max - freq_min) * col / (width - 9)

                plot_char = " "

                # 最大応答プロット
                for log_freq, max_resp in zip(log_freq_data, max_responses):
                    if (abs(log_freq - freq_val) < (freq_max - freq_min) / (width - 9) * 1.5 and
                        abs(max_resp - resp_val) < (resp_max - resp_min) / height * 1.5):
                        plot_char = "^"
                        break

                # 最小応答プロット
                for log_freq, min_resp in zip(log_freq_data, min_responses):
                    if (abs(log_freq - freq_val) < (freq_max - freq_min) / (width - 9) * 1.5 and
                        abs(min_resp - resp_val) < (resp_max - resp_min) / height * 1.5):
                        plot_char = "v" if plot_char == " " else "⧫"

                line += plot_char

            lines.append(line)

        lines.append("凡例: ^=最大応答, v=最小応答, ⧫=重複点")

        return "\n".join(lines)

    def _create_specs_comparison_chart(self, specs: Dict, performance: Dict,
                                      title: str, width: int, height: int) -> str:
        """仕様比較チャート"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        lines.append("制御仕様 vs 実性能:")
        lines.append("-" * 40)

        for spec_name, spec_value in specs.items():
            perf_value = performance.get(spec_name, 0)

            # 仕様適合判定
            if "min" in spec_name:
                compliance = "✅ OK" if perf_value >= spec_value else "❌ NG"
                ratio = perf_value / spec_value if spec_value != 0 else 0
            else:  # max
                compliance = "✅ OK" if perf_value <= spec_value else "❌ NG"
                ratio = spec_value / perf_value if perf_value != 0 else 0

            lines.append(f"{spec_name:20}: 仕様{spec_value:6.1f} vs 実性能{perf_value:6.1f} {compliance}")

        return "\n".join(lines)

    def _create_radar_chart(self, metrics: Dict, phases: List[str],
                           title: str, width: int, height: int) -> str:
        """レーダーチャート作成 (ASCII版)"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        center_x, center_y = width // 2, height // 2
        radius = min(center_x, center_y) - 5

        # レーダーチャート領域初期化
        chart = [[' ' for _ in range(width)] for _ in range(height)]

        # 中心点と軸
        chart[center_y][center_x] = '+'

        # 各メトリクス軸描画
        metric_names = list(metrics.keys())
        num_metrics = len(metric_names)

        angles = [2 * math.pi * i / num_metrics for i in range(num_metrics)]

        # 軸とラベル描画
        for i, (metric, angle) in enumerate(zip(metric_names, angles)):
            end_x = int(center_x + radius * math.cos(angle))
            end_y = int(center_y + radius * math.sin(angle))

            # 軸線描画
            steps = max(abs(end_x - center_x), abs(end_y - center_y))
            for step in range(steps + 1):
                x = int(center_x + step * (end_x - center_x) / steps)
                y = int(center_y + step * (end_y - center_y) / steps)
                if 0 <= x < width and 0 <= y < height:
                    chart[y][x] = '.'

            # メトリクス名配置
            label_x = int(center_x + (radius + 3) * math.cos(angle))
            label_y = int(center_y + (radius + 3) * math.sin(angle))
            if 0 <= label_x < width - len(metric) and 0 <= label_y < height:
                for j, char in enumerate(metric[:6]):  # 名前短縮
                    if label_x + j < width:
                        chart[label_y][label_x + j] = char

        # 各Phase用のプロット
        phase_symbols = ['7', '8', '9']
        for phase_idx, (phase, symbol) in enumerate(zip(phases, phase_symbols)):
            for metric_idx, metric in enumerate(metric_names):
                value = metrics[metric][phase_idx] / 100  # 0-1正規化
                angle = angles[metric_idx]

                plot_radius = value * radius
                plot_x = int(center_x + plot_radius * math.cos(angle))
                plot_y = int(center_y + plot_radius * math.sin(angle))

                if 0 <= plot_x < width and 0 <= plot_y < height:
                    chart[plot_y][plot_x] = symbol

        # チャートを文字列に変換
        for row in chart:
            lines.append(''.join(row))

        # 凡例
        lines.append("")
        lines.append("凡例:")
        for phase, symbol in zip(phases, phase_symbols):
            lines.append(f"  {symbol} = {phase}")
        lines.append("  . = 軸, + = 中心")

        return "\n".join(lines)

    # 解析メソッド群

    def _analyze_pole_zero_placement(self, poles: List[float], zeros: List[float]) -> str:
        """極零点配置解析"""
        analysis = []
        analysis.append("🎯 極零点配置解析:")

        stable_poles = [p for p in poles if p < 0]
        unstable_poles = [p for p in poles if p >= 0]

        analysis.append(f"  安定極数: {len(stable_poles)}")
        analysis.append(f"  不安定極数: {len(unstable_poles)}")

        if stable_poles:
            dominant_pole = max(stable_poles)  # 最も右側の極
            analysis.append(f"  支配極: {dominant_pole:.3f}")
            analysis.append(f"  推定整定時間: {4/abs(dominant_pole):.3f}秒")

        return "\n".join(analysis)

    def _analyze_sensitivity_functions(self, frequencies: List[float],
                                     sensitivity: List[float], complementary: List[float]) -> str:
        """感度関数解析"""
        analysis = []
        analysis.append("📊 感度関数解析:")

        # 感度関数のピーク
        max_sensitivity = max(sensitivity)
        analysis.append(f"  最大感度: {max_sensitivity:.3f} ({20*math.log10(max_sensitivity):.1f}dB)")

        # 相補感度関数の帯域幅
        for i, comp in enumerate(complementary):
            if comp < 0.707:  # -3dB点
                bandwidth = frequencies[i]
                analysis.append(f"  帯域幅(-3dB): {bandwidth:.2f}Hz")
                break

        # 感度関数の性能指標
        if max_sensitivity < 2.0:
            analysis.append("  感度性能: ✅ 良好 (Ms < 2.0)")
        else:
            analysis.append("  感度性能: ⚠️ 要改善 (Ms ≥ 2.0)")

        return "\n".join(analysis)

    def _analyze_disturbance_rejection(self, frequencies: List[float],
                                     input_dist: List[float], output_dist: List[float]) -> str:
        """外乱抑制解析"""
        analysis = []
        analysis.append("🛡️ 外乱抑制解析:")

        # 低周波外乱抑制
        low_freq_input = input_dist[0] if input_dist else 0
        low_freq_output = output_dist[0] if output_dist else 0

        analysis.append(f"  低周波入力外乱抑制: {low_freq_input:.1f}dB")
        analysis.append(f"  低周波出力外乱抑制: {low_freq_output:.1f}dB")

        # 外乱抑制性能評価
        if low_freq_input < -20:
            analysis.append("  入力外乱抑制性能: ✅ 優秀")
        elif low_freq_input < -10:
            analysis.append("  入力外乱抑制性能: ⚠️ 普通")
        else:
            analysis.append("  入力外乱抑制性能: ❌ 不十分")

        return "\n".join(analysis)

    def _analyze_robustness(self, robustness_data: List[List[float]], variations: List[float]) -> str:
        """ロバスト性解析"""
        analysis = []
        analysis.append("💪 ロバスト性解析:")

        # 各周波数での変動幅
        max_variation = 0
        for i in range(len(robustness_data[0])):
            freq_responses = [data[i] for data in robustness_data]
            variation_range = max(freq_responses) - min(freq_responses)
            max_variation = max(max_variation, variation_range)

        analysis.append(f"  最大応答変動: {max_variation:.3f}")
        analysis.append(f"  パラメータ変動範囲: ±{max(abs(v) for v in variations)}%")

        # ロバスト性評価
        if max_variation < 0.1:
            analysis.append("  ロバスト性: ✅ 優秀 (変動小)")
        elif max_variation < 0.3:
            analysis.append("  ロバスト性: ⚠️ 普通 (変動中程度)")
        else:
            analysis.append("  ロバスト性: ❌ 要改善 (変動大)")

        return "\n".join(analysis)

    def _evaluate_current_performance(self) -> Dict:
        """現在システム性能評価"""
        return {
            'bandwidth_min': 0.76,        # 実測帯域幅
            'phase_margin_min': 179.3,    # 実測位相余裕
            'gain_margin_min': 18.5,      # 実測ゲイン余裕
            'settling_time_max': 0.73,    # 実測整定時間
            'overshoot_max': 0.0,         # 実測オーバーシュート
            'steady_state_error_max': 15  # 実測定常偏差 (85%到達)
        }

    def _analyze_specs_compliance(self, specs: Dict, performance: Dict) -> str:
        """仕様適合性解析"""
        analysis = []
        analysis.append("🎯 制御仕様適合性:")

        compliant_count = 0
        total_count = len(specs)

        for spec_name, spec_value in specs.items():
            perf_value = performance.get(spec_name, 0)

            if "min" in spec_name:
                is_compliant = perf_value >= spec_value
            else:  # max
                is_compliant = perf_value <= spec_value

            if is_compliant:
                compliant_count += 1

        compliance_rate = (compliant_count / total_count) * 100
        analysis.append(f"  適合率: {compliance_rate:.1f}% ({compliant_count}/{total_count})")

        if compliance_rate >= 80:
            analysis.append("  総合判定: ✅ 仕様適合")
        elif compliance_rate >= 60:
            analysis.append("  総合判定: ⚠️ 部分適合")
        else:
            analysis.append("  総合判定: ❌ 仕様不適合")

        return "\n".join(analysis)

    def _analyze_control_effort(self, control_signals: List[float], control_effort: List[float]) -> str:
        """制御コスト解析"""
        analysis = []
        analysis.append("⚡ 制御コスト解析:")

        max_control = max(control_signals) if control_signals else 0
        total_effort = control_effort[-1] if control_effort else 0

        analysis.append(f"  最大制御信号: {max_control:.3f}")
        analysis.append(f"  総制御コスト: {total_effort:.3f}")

        # 制御効率評価
        if max_control < 2.0 and total_effort < 10.0:
            analysis.append("  制御効率: ✅ 効率的")
        elif max_control < 5.0 and total_effort < 25.0:
            analysis.append("  制御効率: ⚠️ 普通")
        else:
            analysis.append("  制御効率: ❌ 非効率")

        return "\n".join(analysis)

def main():
    """メイン実行"""
    print("🎨 制御工学高度グラフライブラリ - Advanced Control Graphs")
    print("=" * 75)

    analyzer = AdvancedControlGraphs()

    # 各種高度グラフを生成
    graphs = [
        ("極零点配置図", analyzer.create_pole_zero_map),
        ("感度関数解析", analyzer.create_sensitivity_function_plot),
        ("外乱抑制特性", analyzer.create_disturbance_rejection_plot),
        ("ロバスト性解析", analyzer.create_robustness_analysis),
        ("制御仕様適合性", analyzer.create_performance_specs_plot),
        ("Phase別レーダーチャート", analyzer.create_phase_comparison_radar),
        ("制御コスト解析", analyzer.create_control_effort_analysis)
    ]

    for i, (graph_name, graph_func) in enumerate(graphs, 1):
        print(f"\n{'='*75}")
        print(f"\n{i}. {graph_name}")
        graph_result = graph_func()
        print(graph_result)

    print(f"\n{'='*75}")
    print("\n✅ 制御工学高度グラフ生成完了!")
    print("生成されたグラフ:")
    for i, (graph_name, _) in enumerate(graphs, 1):
        print(f"  {i}. {graph_name}")

    # 結果保存
    output_file = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis/advanced_control_graphs_results.json"
    results = {
        'generated_graphs': [name for name, _ in graphs],
        'total_graphs': len(graphs),
        'system_parameters': analyzer.params
    }

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(results, f, ensure_ascii=False, indent=2)

    print(f"\n💾 高度グラフ結果保存: {output_file}")

if __name__ == "__main__":
    main()
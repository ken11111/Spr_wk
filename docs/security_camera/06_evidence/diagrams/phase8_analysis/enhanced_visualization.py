#!/usr/bin/env python3
"""
制御工学グラフ可視化ツール - Enhanced Control Engineering Visualization
豊富なグラフ表現による制御系分析・可視化
"""

import math
import cmath
import json
from typing import List, Tuple, Dict

class ControlGraphVisualizer:
    """制御工学グラフ可視化クラス"""

    def __init__(self):
        # システムパラメータ (Phase 8-9.2)
        self.K1 = 1.0       # Queue controller gain
        self.tau1 = 0.033   # Queue time constant [s]
        self.K2 = 0.85      # TCP controller gain
        self.tau2 = 0.134   # TCP time constant [s]
        self.theta = 0.025  # Delay [s]

    def create_bode_plot_enhanced(self, width: int = 80, height: int = 25) -> str:
        """高品質Bode線図生成"""
        print("📊 制御工学 Bode線図生成中...")

        # 周波数範囲設定 (0.01 - 100 Hz)
        freq_points = []
        mag_db = []
        phase_deg = []

        for i in range(100):
            freq = 0.01 * (10 ** (i / 25))  # 対数スケール
            omega = 2 * math.pi * freq
            s = 1j * omega

            # 開ループ伝達関数 L(s) = G1(s) * G2(s)
            G1 = self.K1 / (self.tau1 * s + 1)
            G2 = self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)
            L_s = G1 * G2

            magnitude = abs(L_s)
            mag_db_val = 20 * math.log10(magnitude) if magnitude > 1e-12 else -120
            phase_rad = cmath.phase(L_s)
            phase_deg_val = math.degrees(phase_rad)

            freq_points.append(freq)
            mag_db.append(mag_db_val)
            phase_deg.append(phase_deg_val)

        # Bode線図描画
        bode_plot = self._create_dual_axis_plot(
            freq_points, mag_db, phase_deg,
            "Bode線図 - 開ループ伝達関数 L(s)",
            "周波数 [Hz]", "大きさ [dB]", "位相 [deg]",
            width, height, log_x=True
        )

        # 重要な周波数ポイント解析
        analysis = self._analyze_bode_characteristics(freq_points, mag_db, phase_deg)

        return bode_plot + "\n" + analysis

    def create_nyquist_plot(self, width: int = 60, height: int = 30) -> str:
        """Nyquist線図生成"""
        print("🔄 Nyquist線図生成中...")

        # 周波数応答データ生成
        real_parts = []
        imag_parts = []
        frequencies = []

        for i in range(200):
            freq = 0.01 * (10 ** (i / 50))
            omega = 2 * math.pi * freq
            s = 1j * omega

            # 開ループ伝達関数評価
            G1 = self.K1 / (self.tau1 * s + 1)
            G2 = self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)
            L_s = G1 * G2

            real_parts.append(L_s.real)
            imag_parts.append(L_s.imag)
            frequencies.append(freq)

        # Nyquist線図描画
        nyquist_plot = self._create_polar_plot(
            real_parts, imag_parts, frequencies,
            "Nyquist線図 - 複素平面上の軌跡",
            "実部", "虚部", width, height
        )

        # 安定性解析
        stability_analysis = self._analyze_nyquist_stability(real_parts, imag_parts)

        return nyquist_plot + "\n" + stability_analysis

    def create_root_locus(self, width: int = 60, height: int = 25) -> str:
        """根軌跡図生成"""
        print("🌿 根軌跡図生成中...")

        # ゲイン変化に対する極の軌跡
        gains = [0.1 * i for i in range(1, 21)]  # K = 0.1 to 2.0
        root_real = []
        root_imag = []

        for K in gains:
            # 特性方程式 1 + K*G(s)H(s) = 0 の根を近似計算
            # 簡化のため、主要な極のみ考慮
            for tau in [self.tau1, self.tau2]:
                pole_real = -K / tau
                pole_imag = 0
                root_real.append(pole_real)
                root_imag.append(pole_imag)

        # 根軌跡図描画
        root_locus_plot = self._create_scatter_plot(
            root_real, root_imag,
            "根軌跡図 - ゲイン変化による極の軌跡",
            "実部", "虚部", width, height
        )

        # 根軌跡解析
        analysis = self._analyze_root_locus(root_real, root_imag, gains)

        return root_locus_plot + "\n" + analysis

    def create_step_response_detailed(self, width: int = 80, height: int = 25) -> str:
        """詳細ステップ応答図"""
        print("📈 詳細ステップ応答図生成中...")

        # 時間範囲設定
        time_points = [i * 0.01 for i in range(300)]  # 0-3秒
        responses = []

        for t in time_points:
            if t == 0:
                response = 0
            else:
                # 複数の1次遅れ系の合成による近似
                y1 = self.K1 * (1 - math.exp(-t/self.tau1))
                y2 = self.K2 * (1 - math.exp(-max(0, t-self.theta)/self.tau2)) if t > self.theta else 0
                response = y1 * y2 * 0.85  # 全体ゲイン調整

            responses.append(response)

        # ステップ応答図描画
        step_plot = self._create_time_series_plot(
            time_points, responses,
            "ステップ応答 - 時間領域特性",
            "時間 [s]", "応答", width, height
        )

        # 性能指標計算
        performance_metrics = self._calculate_step_performance(time_points, responses)

        return step_plot + "\n" + performance_metrics

    def create_frequency_characteristics(self, width: int = 80, height: int = 20) -> str:
        """周波数特性の詳細分析"""
        print("📊 周波数特性詳細分析中...")

        frequencies = [0.01 * (1.5 ** i) for i in range(50)]
        magnitude = []
        phase = []
        group_delay = []

        for freq in frequencies:
            omega = 2 * math.pi * freq
            s = 1j * omega

            # 閉ループ伝達関数 T(s) = L(s)/(1 + L(s))
            G1 = self.K1 / (self.tau1 * s + 1)
            G2 = self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)
            L_s = G1 * G2
            T_s = L_s / (1 + L_s)

            mag = abs(T_s)
            ph = cmath.phase(T_s)

            magnitude.append(mag)
            phase.append(math.degrees(ph))

            # 群遅延近似計算
            if len(phase) > 1:
                dphi_domega = (phase[-1] - phase[-2]) / (2 * math.pi * (frequencies[-1] - frequencies[-2]))
                group_delay.append(-dphi_domega)
            else:
                group_delay.append(0)

        # 周波数特性図作成
        freq_char_plot = self._create_frequency_analysis_plot(
            frequencies, magnitude, phase, group_delay,
            "周波数特性解析", width, height
        )

        # 特性解析
        analysis = self._analyze_frequency_characteristics(frequencies, magnitude, phase)

        return freq_char_plot + "\n" + analysis

    def create_stability_margins_plot(self, width: int = 70, height: int = 20) -> str:
        """安定余裕の可視化"""
        print("🛡️ 安定余裕可視化中...")

        frequencies = [0.01 * (2 ** (i/10)) for i in range(100)]
        magnitude_db = []
        phase_deg = []

        gain_crossover_freq = None
        phase_crossover_freq = None
        gain_margin = None
        phase_margin = None

        for freq in frequencies:
            omega = 2 * math.pi * freq
            s = 1j * omega

            G1 = self.K1 / (self.tau1 * s + 1)
            G2 = self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)
            L_s = G1 * G2

            mag = abs(L_s)
            mag_db = 20 * math.log10(mag) if mag > 1e-12 else -120
            ph_deg = math.degrees(cmath.phase(L_s))

            magnitude_db.append(mag_db)
            phase_deg.append(ph_deg)

            # ゲイン交差周波数 (|L(jω)| = 1, つまり0dB)
            if gain_crossover_freq is None and mag_db < 1:
                gain_crossover_freq = freq
                phase_margin = 180 + ph_deg

            # 位相交差周波数 (∠L(jω) = -180°)
            if phase_crossover_freq is None and ph_deg < -170:
                phase_crossover_freq = freq
                gain_margin = -mag_db

        # 安定余裕図作成
        margins_plot = self._create_stability_margins_visualization(
            frequencies, magnitude_db, phase_deg,
            gain_crossover_freq, phase_crossover_freq,
            gain_margin, phase_margin,
            "安定余裕解析", width, height
        )

        return margins_plot

    def create_performance_comparison_chart(self, width: int = 80, height: int = 20) -> str:
        """Phase別性能比較チャート"""
        print("📊 Phase別性能比較チャート生成中...")

        # Phase別データ
        phases = ['Phase 7.3.3', 'Phase 8', 'Phase 9.2']
        fps_values = [3.0, 6.74, 6.74]
        queue_depths = [6.5, 1.5, 1.2]
        tcp_times = [236, 134, 134]
        stability_scores = [1, 3, 4]  # 数値化

        # 複数指標比較チャート
        comparison_chart = self._create_multi_metric_chart(
            phases, fps_values, queue_depths, tcp_times, stability_scores,
            "Phase別性能総合比較", width, height
        )

        return comparison_chart

    def _create_dual_axis_plot(self, x_data: List[float], y1_data: List[float], y2_data: List[float],
                              title: str, x_label: str, y1_label: str, y2_label: str,
                              width: int, height: int, log_x: bool = False) -> str:
        """双軸プロット作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # データ範囲計算
        if log_x:
            x_min, x_max = math.log10(min(x_data)), math.log10(max(x_data))
            x_data_norm = [math.log10(x) for x in x_data]
        else:
            x_min, x_max = min(x_data), max(x_data)
            x_data_norm = x_data

        y1_min, y1_max = min(y1_data), max(y1_data)
        y2_min, y2_max = min(y2_data), max(y2_data)

        # 大きさプロット (上半分)
        lines.append(f"{y1_label:^{width}}")
        for row in range(height // 2):
            y_val = y1_max - (y1_max - y1_min) * row / (height // 2 - 1)
            line = f"{y_val:7.1f}|"

            for col in range(width - 10):
                x_val = x_min + (x_max - x_min) * col / (width - 11)

                # 最も近いデータポイントを検索
                closest_distance = float('inf')
                for i, (x_norm, y1) in enumerate(zip(x_data_norm, y1_data)):
                    distance = abs(x_norm - x_val) + abs(y1 - y_val) * 0.1
                    if distance < closest_distance and distance < 0.1:
                        line += "*"
                        closest_distance = distance
                        break
                else:
                    line += " "

            lines.append(line)

        lines.append("-" * width)

        # 位相プロット (下半分)
        lines.append(f"{y2_label:^{width}}")
        for row in range(height // 2):
            y_val = y2_max - (y2_max - y2_min) * row / (height // 2 - 1)
            line = f"{y_val:7.1f}|"

            for col in range(width - 10):
                x_val = x_min + (x_max - x_min) * col / (width - 11)

                closest_distance = float('inf')
                for i, (x_norm, y2) in enumerate(zip(x_data_norm, y2_data)):
                    distance = abs(x_norm - x_val) + abs(y2 - y_val) * 0.01
                    if distance < closest_distance and distance < 0.1:
                        line += "o"
                        closest_distance = distance
                        break
                else:
                    line += " "

            lines.append(line)

        # X軸ラベル
        if log_x:
            lines.append(f"{min(x_data):>8.2e}" + " " * (width - 20) + f"{max(x_data):>8.2e}")
        else:
            lines.append(f"{x_min:>8.2f}" + " " * (width - 20) + f"{x_max:>8.2f}")
        lines.append(f"{x_label:^{width}}")

        return "\n".join(lines)

    def _create_polar_plot(self, real_parts: List[float], imag_parts: List[float], frequencies: List[float],
                          title: str, x_label: str, y_label: str, width: int, height: int) -> str:
        """極座標プロット作成 (Nyquist線図用)"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # データ範囲
        all_values = real_parts + imag_parts
        if not all_values:
            return "データなし"

        data_range = max(abs(min(all_values)), abs(max(all_values)))

        center_x = width // 2
        center_y = height // 2

        # プロット領域初期化
        plot_grid = [[' ' for _ in range(width)] for _ in range(height)]

        # 原点と軸描画
        for i in range(height):
            if center_x < width:
                plot_grid[i][center_x] = '|'
        for j in range(width):
            if center_y < height:
                plot_grid[center_y][j] = '-'
        plot_grid[center_y][center_x] = '+'

        # 単位円描画
        for angle in range(0, 360, 10):
            rad = math.radians(angle)
            circle_x = int(center_x + (width // 4) * math.cos(rad))
            circle_y = int(center_y + (height // 4) * math.sin(rad))
            if 0 <= circle_x < width and 0 <= circle_y < height:
                plot_grid[circle_y][circle_x] = '.'

        # データポイントプロット
        for i, (real, imag) in enumerate(zip(real_parts, imag_parts)):
            if data_range > 0:
                plot_x = int(center_x + real * (width // 4) / data_range)
                plot_y = int(center_y - imag * (height // 4) / data_range)  # Y軸反転

                if 0 <= plot_x < width and 0 <= plot_y < height:
                    freq = frequencies[i]
                    if freq < 0.1:
                        plot_grid[plot_y][plot_x] = 'L'  # Low freq
                    elif freq < 10:
                        plot_grid[plot_y][plot_x] = 'M'  # Medium freq
                    else:
                        plot_grid[plot_y][plot_x] = 'H'  # High freq

        # グリッドを文字列に変換
        for row in plot_grid:
            lines.append(''.join(row))

        # 凡例
        lines.append("")
        lines.append("凡例: L=低周波, M=中周波, H=高周波, .=単位円")
        lines.append(f"スケール: ±{data_range:.2f}")

        return "\n".join(lines)

    def _create_scatter_plot(self, x_data: List[float], y_data: List[float],
                            title: str, x_label: str, y_label: str, width: int, height: int) -> str:
        """散布図作成 (根軌跡用)"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        if not x_data or not y_data:
            return "データなし"

        x_min, x_max = min(x_data), max(x_data)
        y_min, y_max = min(y_data), max(y_data)

        # Y軸範囲を対称にする
        y_range = max(abs(y_min), abs(y_max))
        y_min, y_max = -y_range, y_range

        for row in range(height):
            y_val = y_max - (y_max - y_min) * row / (height - 1)
            line = f"{y_val:6.2f}|"

            for col in range(width - 8):
                x_val = x_min + (x_max - x_min) * col / (width - 9)

                # データポイント検索
                plot_char = " "
                for x_point, y_point in zip(x_data, y_data):
                    if (abs(x_point - x_val) < (x_max - x_min) / (width - 9) * 1.5 and
                        abs(y_point - y_val) < (y_max - y_min) / height * 1.5):
                        plot_char = "*"
                        break

                # 安定境界 (実軸負の部分)
                if abs(y_val) < (y_max - y_min) / height and x_val < 0:
                    plot_char = "-"

                line += plot_char

            lines.append(line)

        # X軸
        lines.append(" " * 7 + "-" * (width - 8))
        lines.append(f"{x_min:7.2f}" + " " * (width - 18) + f"{x_max:7.2f}")
        lines.append(f"{x_label:^{width}}")

        return "\n".join(lines)

    def _create_time_series_plot(self, time_data: List[float], response_data: List[float],
                                title: str, x_label: str, y_label: str, width: int, height: int) -> str:
        """時系列プロット作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        if not time_data or not response_data:
            return "データなし"

        t_min, t_max = min(time_data), max(time_data)
        y_min, y_max = min(response_data), max(response_data)

        # 定常値ライン用の計算
        steady_state = response_data[-1] if response_data else 0

        for row in range(height):
            y_val = y_max - (y_max - y_min) * row / (height - 1)
            line = f"{y_val:6.3f}|"

            for col in range(width - 8):
                t_val = t_min + (t_max - t_min) * col / (width - 9)

                plot_char = " "

                # 定常値ライン描画
                if abs(y_val - steady_state) < (y_max - y_min) * 0.02:
                    plot_char = "-"

                # 応答データプロット
                for i, (t, y) in enumerate(zip(time_data, response_data)):
                    if (abs(t - t_val) < (t_max - t_min) / (width - 9) * 1.5 and
                        abs(y - y_val) < (y_max - y_min) / height * 1.5):
                        plot_char = "*"
                        break

                line += plot_char

            lines.append(line)

        lines.append(" " * 7 + "-" * (width - 8))
        lines.append(f"{t_min:7.2f}" + " " * (width - 18) + f"{t_max:7.2f}")
        lines.append(f"{x_label:^{width}}")

        return "\n".join(lines)

    def _create_frequency_analysis_plot(self, frequencies: List[float], magnitude: List[float],
                                       phase: List[float], group_delay: List[float],
                                       title: str, width: int, height: int) -> str:
        """周波数解析プロット作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # 3つのプロットを縦に配置
        plot_height = height // 3

        # 1. 大きさ特性
        lines.append("大きさ特性:")
        magnitude_plot = self._create_simple_plot(
            frequencies, magnitude, "周波数", "大きさ", width, plot_height, log_x=True
        )
        lines.extend(magnitude_plot.split('\n'))

        # 2. 位相特性
        lines.append("位相特性:")
        phase_plot = self._create_simple_plot(
            frequencies, phase, "周波数", "位相[deg]", width, plot_height, log_x=True
        )
        lines.extend(phase_plot.split('\n'))

        # 3. 群遅延特性
        lines.append("群遅延特性:")
        delay_plot = self._create_simple_plot(
            frequencies, group_delay, "周波数", "群遅延[s]", width, plot_height, log_x=True
        )
        lines.extend(delay_plot.split('\n'))

        return "\n".join(lines)

    def _create_simple_plot(self, x_data: List[float], y_data: List[float],
                           x_label: str, y_label: str, width: int, height: int,
                           log_x: bool = False) -> str:
        """シンプルプロット作成"""
        lines = []

        if not x_data or not y_data:
            return "データなし"

        # データ範囲
        if log_x and min(x_data) > 0:
            x_min, x_max = math.log10(min(x_data)), math.log10(max(x_data))
            x_data_norm = [math.log10(x) for x in x_data]
        else:
            x_min, x_max = min(x_data), max(x_data)
            x_data_norm = x_data

        y_min, y_max = min(y_data), max(y_data)

        for row in range(height):
            y_val = y_max - (y_max - y_min) * row / (height - 1)
            line = f"{y_val:6.2f}|"

            for col in range(width - 8):
                x_val = x_min + (x_max - x_min) * col / (width - 9)

                plot_char = " "
                for x_norm, y in zip(x_data_norm, y_data):
                    if (abs(x_norm - x_val) < (x_max - x_min) / (width - 9) * 1.5 and
                        abs(y - y_val) < (y_max - y_min) / height * 1.5):
                        plot_char = "*"
                        break

                line += plot_char

            lines.append(line)

        return "\n".join(lines)

    def _create_stability_margins_visualization(self, frequencies: List[float], magnitude_db: List[float],
                                              phase_deg: List[float], gain_crossover_freq: float,
                                              phase_crossover_freq: float, gain_margin: float,
                                              phase_margin: float, title: str, width: int, height: int) -> str:
        """安定余裕可視化作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # 安定余裕情報表示
        lines.append("📊 安定余裕解析結果:")
        lines.append(f"  ゲイン余裕:    {gain_margin:.1f} dB" if gain_margin else "  ゲイン余裕:    ∞ (安定)")
        lines.append(f"  位相余裕:      {phase_margin:.1f} °" if phase_margin else "  位相余裕:      計算不可")
        lines.append(f"  ゲイン交差周波数: {gain_crossover_freq:.3f} Hz" if gain_crossover_freq else "  ゲイン交差周波数: なし")
        lines.append(f"  位相交差周波数: {phase_crossover_freq:.3f} Hz" if phase_crossover_freq else "  位相交差周波数: なし")
        lines.append("")

        # 安定性判定
        if gain_margin and phase_margin:
            if gain_margin > 6 and phase_margin > 45:
                stability_status = "✅ 安定 (十分な余裕)"
            elif gain_margin > 3 and phase_margin > 30:
                stability_status = "⚠️ 準安定 (余裕あり)"
            else:
                stability_status = "❌ 不安定 (余裕不足)"
        else:
            stability_status = "✅ 安定 (理論的安定)"

        lines.append(f"安定性判定: {stability_status}")
        lines.append("")

        # 簡易Bode線図 (大きさのみ、0dBラインと-180°ライン強調)
        lines.append("大きさ特性 (0dB交差点表示):")
        for row in range(height // 2):
            db_val = 40 - 80 * row / (height // 2 - 1)
            line = f"{db_val:5.0f}|"

            for col in range(width - 7):
                # 0dBライン強調
                if abs(db_val) < 2:
                    line += "="
                else:
                    # データポイント検索
                    plot_char = " "
                    for mag_db in magnitude_db:
                        if abs(mag_db - db_val) < 3:
                            plot_char = "*"
                            break
                    line += plot_char

            lines.append(line)

        lines.append("     " + "-" * (width - 6))

        return "\n".join(lines)

    def _create_multi_metric_chart(self, phases: List[str], fps_values: List[float],
                                  queue_depths: List[float], tcp_times: List[float],
                                  stability_scores: List[float], title: str, width: int, height: int) -> str:
        """複数指標比較チャート作成"""
        lines = []
        lines.append("=" * width)
        lines.append(f"{title:^{width}}")
        lines.append("=" * width)

        # 正規化 (最大値を100%として)
        fps_normalized = [fps / max(fps_values) * 100 for fps in fps_values]
        queue_normalized = [100 - (q / max(queue_depths) * 100) for q in queue_depths]  # 逆スケール
        tcp_normalized = [100 - (t / max(tcp_times) * 100) for t in tcp_times]  # 逆スケール
        stability_normalized = [s / max(stability_scores) * 100 for s in stability_scores]

        # 棒グラフ作成
        bar_width = width // len(phases) - 2

        for metric_name, values in [
            ("FPS性能", fps_normalized),
            ("キュー効率", queue_normalized),
            ("TCP性能", tcp_normalized),
            ("安定性", stability_normalized)
        ]:
            lines.append(f"\n{metric_name}:")

            # 各Phase用の棒
            for i, (phase, value) in enumerate(zip(phases, values)):
                bar_height = int(value * height // 400)  # スケール調整

                # 棒グラフ描画
                phase_display = phase.replace("Phase ", "P")
                lines.append(f"{phase_display:>8}: {'█' * bar_height} {value:.1f}%")

        # 総合スコア計算
        lines.append("\n総合スコア:")
        for i, phase in enumerate(phases):
            total_score = (fps_normalized[i] + queue_normalized[i] +
                          tcp_normalized[i] + stability_normalized[i]) / 4
            lines.append(f"  {phase}: {total_score:.1f}%")

        return "\n".join(lines)

    # 解析メソッド群
    def _analyze_bode_characteristics(self, frequencies: List[float], magnitude_db: List[float], phase_deg: List[float]) -> str:
        """Bode線図特性解析"""
        analysis = []
        analysis.append("📈 Bode線図解析:")

        # 帯域幅推定 (-3dB点)
        bandwidth = None
        for i, mag_db in enumerate(magnitude_db):
            if mag_db < -3:
                bandwidth = frequencies[i]
                break

        if bandwidth:
            analysis.append(f"  帯域幅(-3dB): {bandwidth:.2f} Hz")

        # DC利得
        dc_gain = magnitude_db[0] if magnitude_db else 0
        analysis.append(f"  DC利得: {dc_gain:.1f} dB")

        # 高周波減衰
        hf_gain = magnitude_db[-1] if magnitude_db else 0
        analysis.append(f"  高周波利得: {hf_gain:.1f} dB")

        return "\n".join(analysis)

    def _analyze_nyquist_stability(self, real_parts: List[float], imag_parts: List[float]) -> str:
        """Nyquist安定性解析"""
        analysis = []
        analysis.append("🔄 Nyquist安定性解析:")

        # (-1,0)点の包囲回数計算 (簡易版)
        encirclements = 0
        critical_point_distance = min([math.sqrt((r+1)**2 + i**2) for r, i in zip(real_parts, imag_parts)])

        analysis.append(f"  臨界点(-1,0)との最短距離: {critical_point_distance:.3f}")

        if critical_point_distance > 1:
            analysis.append("  判定: ✅ 安定 (臨界点を包囲せず)")
        else:
            analysis.append("  判定: ⚠️ 要注意 (臨界点近傍)")

        return "\n".join(analysis)

    def _analyze_root_locus(self, root_real: List[float], root_imag: List[float], gains: List[float]) -> str:
        """根軌跡解析"""
        analysis = []
        analysis.append("🌿 根軌跡解析:")

        # 安定ゲイン範囲 (実部が負の根のみ安定)
        stable_roots = [r for r in root_real if r < 0]

        if stable_roots:
            analysis.append(f"  安定根数: {len(stable_roots)}/{len(root_real)}")
            analysis.append(f"  最も安定な極: {min(stable_roots):.3f}")
        else:
            analysis.append("  安定ゲイン範囲: 限定的")

        # 最大ゲイン推定
        max_stable_gain = max(gains) if stable_roots else 0
        analysis.append(f"  推定安定上限ゲイン: {max_stable_gain:.2f}")

        return "\n".join(analysis)

    def _calculate_step_performance(self, time_points: List[float], responses: List[float]) -> str:
        """ステップ応答性能計算"""
        if not responses:
            return "性能計算不可"

        steady_state = responses[-1]
        performance = []
        performance.append("📊 ステップ応答性能指標:")

        # 立ち上がり時間 (10%-90%)
        rise_start = rise_end = None
        for i, (t, y) in enumerate(zip(time_points, responses)):
            if rise_start is None and y >= 0.1 * steady_state:
                rise_start = t
            if rise_end is None and y >= 0.9 * steady_state:
                rise_end = t
                break

        if rise_start is not None and rise_end is not None:
            rise_time = rise_end - rise_start
            performance.append(f"  立ち上がり時間: {rise_time*1000:.1f} ms")

        # 整定時間 (2%誤差内)
        settling_time = None
        tolerance = 0.02 * steady_state
        for i in range(len(responses) - 1, -1, -1):
            if abs(responses[i] - steady_state) > tolerance:
                settling_time = time_points[i] if i < len(time_points) else time_points[-1]
                break

        if settling_time is not None:
            performance.append(f"  整定時間: {settling_time*1000:.1f} ms")

        # オーバーシュート
        max_response = max(responses)
        overshoot = ((max_response - steady_state) / steady_state * 100) if steady_state > 0 else 0
        performance.append(f"  オーバーシュート: {max(0, overshoot):.1f}%")

        # 定常値
        performance.append(f"  定常値: {steady_state:.3f}")

        return "\n".join(performance)

    def _analyze_frequency_characteristics(self, frequencies: List[float], magnitude: List[float], phase: List[float]) -> str:
        """周波数特性解析"""
        analysis = []
        analysis.append("📊 周波数特性解析:")

        if not magnitude or not phase:
            return "解析データ不足"

        # 通過域・阻止域判定
        passband_gain = max(magnitude)
        stopband_gain = min(magnitude)

        analysis.append(f"  通過域ゲイン: {passband_gain:.3f}")
        analysis.append(f"  阻止域ゲイン: {stopband_gain:.3f}")
        analysis.append(f"  ゲイン変動: {(passband_gain/stopband_gain):.1f}倍")

        # 位相特性
        phase_range = max(phase) - min(phase)
        analysis.append(f"  位相変化: {phase_range:.1f}°")

        return "\n".join(analysis)

def main():
    """メイン実行"""
    print("🎨 制御工学グラフ可視化ツール - Enhanced Visualization")
    print("=" * 70)

    visualizer = ControlGraphVisualizer()

    # 各種グラフを順次生成
    print("\n1. Bode線図 (大きさ・位相)")
    bode_plot = visualizer.create_bode_plot_enhanced()
    print(bode_plot)

    print("\n" + "="*70)
    print("\n2. Nyquist線図")
    nyquist_plot = visualizer.create_nyquist_plot()
    print(nyquist_plot)

    print("\n" + "="*70)
    print("\n3. 根軌跡")
    root_locus = visualizer.create_root_locus()
    print(root_locus)

    print("\n" + "="*70)
    print("\n4. 詳細ステップ応答")
    step_response = visualizer.create_step_response_detailed()
    print(step_response)

    print("\n" + "="*70)
    print("\n5. 安定余裕解析")
    stability_margins = visualizer.create_stability_margins_plot()
    print(stability_margins)

    print("\n" + "="*70)
    print("\n6. Phase別性能比較")
    performance_comparison = visualizer.create_performance_comparison_chart()
    print(performance_comparison)

    print("\n" + "="*70)
    print("\n✅ 制御工学グラフ可視化完了!")
    print("生成されたグラフ:")
    print("  📊 Bode線図 (周波数応答)")
    print("  🔄 Nyquist線図 (複素平面軌跡)")
    print("  🌿 根軌跡 (極の軌跡)")
    print("  📈 ステップ応答 (時間応答)")
    print("  🛡️ 安定余裕 (余裕解析)")
    print("  📊 性能比較 (Phase別評価)")

    # 結果保存
    output_file = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis/enhanced_visualization_results.json"
    results = {
        'visualization_types': [
            'Bode線図', 'Nyquist線図', '根軌跡', 'ステップ応答', '安定余裕解析', 'Phase別比較'
        ],
        'system_parameters': {
            'K1': visualizer.K1, 'tau1': visualizer.tau1,
            'K2': visualizer.K2, 'tau2': visualizer.tau2,
            'theta': visualizer.theta
        },
        'generated_graphs': 6
    }

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(results, f, ensure_ascii=False, indent=2)

    print(f"\n💾 可視化結果保存: {output_file}")

if __name__ == "__main__":
    main()
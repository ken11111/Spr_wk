#!/usr/bin/env python3
"""
matplotlib 3D制御工学可視化ツール
Advanced 3D Control Engineering Visualization with Matplotlib
"""

import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import matplotlib.patches as patches
from matplotlib.animation import FuncAnimation
import json
import os
import cmath

# 日本語フォント設定
plt.rcParams['font.family'] = ['DejaVu Sans', 'Hiragino Sans', 'Yu Gothic', 'Meiryo', 'Takao', 'IPAexGothic', 'IPAPGothic', 'VL PGothic', 'Noto Sans CJK JP']

class Advanced3DControlVisualization:
    """高度3D制御工学可視化クラス"""

    def __init__(self):
        """初期化"""
        # システムパラメータ
        self.K1 = 1.0      # キュー制御ゲイン
        self.tau1 = 0.033  # キュー制御時定数 [s]
        self.K2 = 0.85     # TCP制御ゲイン
        self.tau2 = 0.134  # TCP制御時定数 [s]
        self.theta = 0.025 # TCP通信遅延 [s]

    def transfer_function_G1(self, s):
        """キュー制御系伝達関数 G1(s)"""
        return self.K1 / (self.tau1 * s + 1)

    def transfer_function_G2(self, s):
        """TCP制御系伝達関数 G2(s)"""
        return self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)

    def open_loop_transfer_function(self, s):
        """開ループ伝達関数 L(s)"""
        return self.transfer_function_G1(s) * self.transfer_function_G2(s)

    def create_3d_frequency_surface(self):
        """3D周波数応答サーフェス"""
        # 周波数とゲインの範囲
        frequencies = np.logspace(-2, 2, 50)
        gains = np.logspace(-1, 1, 50)

        F, G = np.meshgrid(frequencies, gains)

        # 大きさと位相の3Dサーフェス計算
        magnitude_surface = np.zeros_like(F)
        phase_surface = np.zeros_like(F)

        for i in range(F.shape[0]):
            for j in range(F.shape[1]):
                omega = 2 * np.pi * F[i, j]
                s = 1j * omega

                try:
                    H = G[i, j] * self.open_loop_transfer_function(s)
                    magnitude_surface[i, j] = 20 * np.log10(abs(H) + 1e-12)
                    phase_surface[i, j] = np.angle(H) * 180 / np.pi
                except:
                    magnitude_surface[i, j] = -100
                    phase_surface[i, j] = -180

        # プロット作成
        fig = plt.figure(figsize=(16, 6))

        # 大きさサーフェス
        ax1 = fig.add_subplot(121, projection='3d')
        surf1 = ax1.plot_surface(np.log10(F), np.log10(G), magnitude_surface,
                                cmap='viridis', alpha=0.8)
        ax1.set_xlabel('log10(周波数) [Hz]', fontsize=10)
        ax1.set_ylabel('log10(ゲイン)', fontsize=10)
        ax1.set_zlabel('大きさ [dB]', fontsize=10)
        ax1.set_title('3D周波数応答 - 大きさ', fontsize=12, fontweight='bold')

        # 位相サーフェス
        ax2 = fig.add_subplot(122, projection='3d')
        surf2 = ax2.plot_surface(np.log10(F), np.log10(G), phase_surface,
                                cmap='plasma', alpha=0.8)
        ax2.set_xlabel('log10(周波数) [Hz]', fontsize=10)
        ax2.set_ylabel('log10(ゲイン)', fontsize=10)
        ax2.set_zlabel('位相 [deg]', fontsize=10)
        ax2.set_title('3D周波数応答 - 位相', fontsize=12, fontweight='bold')

        plt.tight_layout()
        return fig

    def create_3d_nyquist_evolution(self):
        """3DNyquist軌跡の進化"""
        # ゲイン範囲
        gains = np.linspace(0.1, 2.0, 10)
        frequencies = np.logspace(-2, 2, 200)

        fig = plt.figure(figsize=(12, 10))
        ax = fig.add_subplot(111, projection='3d')

        colors = plt.cm.viridis(np.linspace(0, 1, len(gains)))

        for i, gain in enumerate(gains):
            real_parts = []
            imag_parts = []

            for freq in frequencies:
                omega = 2 * np.pi * freq
                s = 1j * omega

                try:
                    H = gain * self.open_loop_transfer_function(s)
                    real_parts.append(H.real)
                    imag_parts.append(H.imag)
                except:
                    real_parts.append(0)
                    imag_parts.append(0)

            # ゲインレベルでの軌跡プロット
            z_level = np.full_like(real_parts, gain)
            ax.plot(real_parts, imag_parts, z_level,
                   color=colors[i], linewidth=2, alpha=0.7,
                   label=f'K={gain:.1f}')

        # 臨界点
        ax.scatter([-1], [0], [1], color='red', s=100,
                  label='臨界点(-1,0)')

        ax.set_xlabel('実部', fontsize=12)
        ax.set_ylabel('虚部', fontsize=12)
        ax.set_zlabel('ゲイン', fontsize=12)
        ax.set_title('3D Nyquist軌跡進化 - ゲイン変化', fontsize=14, fontweight='bold')
        ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')

        return fig

    def create_stability_margin_3d(self):
        """3D安定余裕解析"""
        # パラメータ範囲
        tau2_range = np.linspace(0.05, 0.3, 30)
        K2_range = np.linspace(0.3, 1.2, 30)

        T2, K2 = np.meshgrid(tau2_range, K2_range)

        # 各パラメータ組み合わせでの安定余裕計算
        gain_margin_surface = np.zeros_like(T2)
        phase_margin_surface = np.zeros_like(T2)

        for i in range(T2.shape[0]):
            for j in range(T2.shape[1]):
                # 一時的にパラメータ変更
                original_tau2 = self.tau2
                original_K2 = self.K2

                self.tau2 = T2[i, j]
                self.K2 = K2[i, j]

                try:
                    # 簡易的な余裕計算
                    frequencies = np.logspace(-2, 2, 1000)
                    omega = 2 * np.pi * frequencies
                    s_values = 1j * omega

                    H_values = []
                    for s in s_values:
                        try:
                            H = self.open_loop_transfer_function(s)
                            H_values.append(H)
                        except:
                            H_values.append(complex(0, 0))

                    H_values = np.array(H_values)
                    magnitudes = np.abs(H_values)
                    phases = np.angle(H_values) * 180 / np.pi

                    # ゲイン余裕計算（位相が-180度の点での逆数）
                    phase_180_idx = np.where(np.abs(phases + 180) < 5)[0]
                    if len(phase_180_idx) > 0:
                        gain_margin_surface[i, j] = -20 * np.log10(magnitudes[phase_180_idx[0]] + 1e-12)
                    else:
                        gain_margin_surface[i, j] = 40  # 十分大きな値

                    # 位相余裕計算（利得が0dBの点での位相）
                    gain_0db_idx = np.where(np.abs(20*np.log10(magnitudes + 1e-12)) < 1)[0]
                    if len(gain_0db_idx) > 0:
                        phase_margin_surface[i, j] = phases[gain_0db_idx[0]] + 180
                    else:
                        phase_margin_surface[i, j] = 180

                except:
                    gain_margin_surface[i, j] = 0
                    phase_margin_surface[i, j] = 0

                # パラメータ復元
                self.tau2 = original_tau2
                self.K2 = original_K2

        # プロット作成
        fig = plt.figure(figsize=(16, 6))

        # ゲイン余裕
        ax1 = fig.add_subplot(121, projection='3d')
        surf1 = ax1.plot_surface(T2, K2, gain_margin_surface,
                                cmap='RdYlGn', alpha=0.8)
        ax1.set_xlabel('τ₂ [s]', fontsize=12)
        ax1.set_ylabel('K₂', fontsize=12)
        ax1.set_zlabel('ゲイン余裕 [dB]', fontsize=12)
        ax1.set_title('パラメータ vs ゲイン余裕', fontsize=12, fontweight='bold')

        # 位相余裕
        ax2 = fig.add_subplot(122, projection='3d')
        surf2 = ax2.plot_surface(T2, K2, phase_margin_surface,
                                cmap='RdYlGn', alpha=0.8)
        ax2.set_xlabel('τ₂ [s]', fontsize=12)
        ax2.set_ylabel('K₂', fontsize=12)
        ax2.set_zlabel('位相余裕 [deg]', fontsize=12)
        ax2.set_title('パラメータ vs 位相余裕', fontsize=12, fontweight='bold')

        plt.tight_layout()
        return fig

    def create_time_series_analysis(self):
        """時系列動的解析"""
        # 時間軸
        t = np.linspace(0, 10, 1000)

        # 異なる入力信号での応答
        inputs = {
            'ステップ': np.ones_like(t),
            'ランプ': t,
            'サイン波': np.sin(2*np.pi*0.5*t),
            'ノイズ': np.random.normal(0, 0.1, len(t)) + 1
        }

        # 応答計算
        responses = {}
        for name, input_signal in inputs.items():
            response = np.zeros_like(t)

            # 簡易的な応答計算（畳み込み近似）
            dt = t[1] - t[0]
            tau_total = self.tau1 + self.tau2
            K_total = self.K1 * self.K2

            for i in range(1, len(t)):
                if t[i] > self.theta:
                    delay_idx = max(0, i - int(self.theta / dt))
                    delayed_input = input_signal[delay_idx]

                    # 1次遅れ応答の近似
                    response[i] = response[i-1] + dt * (K_total * delayed_input - response[i-1]) / tau_total
                else:
                    response[i] = 0

            responses[name] = response

        # プロット作成
        fig, axes = plt.subplots(2, 2, figsize=(16, 10))
        axes = axes.flatten()

        colors = ['blue', 'green', 'red', 'orange']

        for i, (name, response) in enumerate(responses.items()):
            ax = axes[i]

            # 入力と出力
            ax.plot(t, inputs[name], '--', color='gray', alpha=0.7,
                   label=f'入力: {name}')
            ax.plot(t, response, color=colors[i], linewidth=2,
                   label='システム応答')

            ax.set_xlabel('時間 [s]', fontsize=12)
            ax.set_ylabel('振幅', fontsize=12)
            ax.set_title(f'{name}入力に対する応答', fontsize=12, fontweight='bold')
            ax.grid(True, alpha=0.3)
            ax.legend()

            # 統計情報表示
            if name != 'ノイズ':
                steady_state = np.mean(response[-100:])
                ax.text(0.7, 0.9, f'定常値: {steady_state:.3f}',
                       transform=ax.transAxes, bbox=dict(boxstyle="round,pad=0.3",
                       facecolor="yellow", alpha=0.7))

        plt.tight_layout()
        return fig

    def create_phase_evolution_3d(self):
        """Phase進化の3D可視化"""
        # Phase データ
        phases = ['Phase 7.3.3', 'Phase 8', 'Phase 9.2']
        metrics = ['FPS', 'TCP時間', 'キュー深度', '安定性', 'CPU使用率', 'メモリ使用率']

        # 正規化されたデータ（0-1スケール）
        data = np.array([
            [0.3, 0.2, 0.1, 0.3, 0.8, 0.9],  # Phase 7.3.3（低性能）
            [1.0, 0.8, 0.7, 0.7, 0.6, 0.5],  # Phase 8（改善）
            [1.0, 0.8, 0.9, 1.0, 0.5, 0.4]   # Phase 9.2（最適）
        ])

        fig = plt.figure(figsize=(14, 10))
        ax = fig.add_subplot(111, projection='3d')

        # 各Phaseの3Dプロット
        colors = ['red', 'blue', 'green']
        alphas = [0.6, 0.7, 0.8]

        for i, (phase, color, alpha) in enumerate(zip(phases, colors, alphas)):
            # レーダーチャート風の3D表示
            angles = np.linspace(0, 2*np.pi, len(metrics), endpoint=False)

            # 円形配置での座標計算
            x = data[i] * np.cos(angles)
            y = data[i] * np.sin(angles)
            z = np.full_like(x, i)  # Phase別の高さ

            # 各点をプロット
            ax.scatter(x, y, z, s=100, c=color, alpha=alpha, label=phase)

            # メトリクス名をラベル表示
            for j, metric in enumerate(metrics):
                ax.text(x[j]*1.1, y[j]*1.1, z[j], metric, fontsize=8)

            # 線で結ぶ
            x_closed = np.append(x, x[0])
            y_closed = np.append(y, y[0])
            z_closed = np.append(z, z[0])
            ax.plot(x_closed, y_closed, z_closed, color=color, alpha=alpha)

        ax.set_xlabel('X軸 (性能指標1)', fontsize=12)
        ax.set_ylabel('Y軸 (性能指標2)', fontsize=12)
        ax.set_zlabel('Phase レベル', fontsize=12)
        ax.set_title('Phase進化の3D可視化 - 多次元性能指標', fontsize=14, fontweight='bold')
        ax.legend()

        # Z軸のラベル設定
        ax.set_zticks([0, 1, 2])
        ax.set_zticklabels(phases)

        return fig

    def create_control_surface_optimization(self):
        """制御最適化サーフェス"""
        # 最適化パラメータ
        K_range = np.linspace(0.5, 1.5, 30)
        tau_range = np.linspace(0.05, 0.25, 30)

        K, TAU = np.meshgrid(K_range, tau_range)

        # 目的関数計算（性能指標の組み合わせ）
        performance_surface = np.zeros_like(K)
        stability_surface = np.zeros_like(K)

        for i in range(K.shape[0]):
            for j in range(K.shape[1]):
                # 一時的パラメータ設定
                original_K2 = self.K2
                original_tau2 = self.tau2

                self.K2 = K[i, j]
                self.tau2 = TAU[i, j]

                try:
                    # 性能評価（応答速度と安定性のバランス）
                    # 帯域幅の計算
                    frequencies = np.logspace(-2, 2, 1000)
                    omega = 2 * np.pi * frequencies
                    s_values = 1j * omega

                    H_values = []
                    for s in s_values:
                        try:
                            H = self.open_loop_transfer_function(s)
                            H_values.append(H)
                        except:
                            H_values.append(complex(0, 0))

                    magnitudes = np.abs(H_values)

                    # 帯域幅（-3dB点）
                    magnitude_db = 20 * np.log10(magnitudes + 1e-12)
                    dc_gain = magnitude_db[0]
                    bandwidth_idx = np.where(magnitude_db <= dc_gain - 3)[0]

                    if len(bandwidth_idx) > 0:
                        bandwidth = frequencies[bandwidth_idx[0]]
                    else:
                        bandwidth = 0.1

                    # 性能指標（帯域幅×安定余裕）
                    performance_surface[i, j] = bandwidth * min(10, abs(dc_gain))

                    # 安定性指標（位相余裕ベース）
                    phases = np.angle(H_values) * 180 / np.pi
                    gain_0db_idx = np.where(np.abs(magnitude_db) < 1)[0]

                    if len(gain_0db_idx) > 0:
                        phase_margin = phases[gain_0db_idx[0]] + 180
                        stability_surface[i, j] = max(0, phase_margin) / 180
                    else:
                        stability_surface[i, j] = 1.0

                except:
                    performance_surface[i, j] = 0
                    stability_surface[i, j] = 0

                # パラメータ復元
                self.K2 = original_K2
                self.tau2 = original_tau2

        # 総合最適化サーフェス
        optimization_surface = performance_surface * stability_surface

        # プロット作成
        fig = plt.figure(figsize=(18, 6))

        # 性能サーフェス
        ax1 = fig.add_subplot(131, projection='3d')
        surf1 = ax1.plot_surface(K, TAU, performance_surface,
                                cmap='Blues', alpha=0.8)
        ax1.set_xlabel('ゲイン K₂', fontsize=10)
        ax1.set_ylabel('時定数 τ₂ [s]', fontsize=10)
        ax1.set_zlabel('性能指標', fontsize=10)
        ax1.set_title('性能サーフェス', fontsize=12, fontweight='bold')

        # 安定性サーフェス
        ax2 = fig.add_subplot(132, projection='3d')
        surf2 = ax2.plot_surface(K, TAU, stability_surface,
                                cmap='Reds', alpha=0.8)
        ax2.set_xlabel('ゲイン K₂', fontsize=10)
        ax2.set_ylabel('時定数 τ₂ [s]', fontsize=10)
        ax2.set_zlabel('安定性指標', fontsize=10)
        ax2.set_title('安定性サーフェス', fontsize=12, fontweight='bold')

        # 総合最適化サーフェス
        ax3 = fig.add_subplot(133, projection='3d')
        surf3 = ax3.plot_surface(K, TAU, optimization_surface,
                                cmap='RdYlGn', alpha=0.8)

        # 最適点の表示
        max_idx = np.unravel_index(np.argmax(optimization_surface), optimization_surface.shape)
        optimal_K = K[max_idx]
        optimal_tau = TAU[max_idx]
        optimal_value = optimization_surface[max_idx]

        ax3.scatter([optimal_K], [optimal_tau], [optimal_value],
                   color='black', s=100, label=f'最適点\nK={optimal_K:.3f}\nτ={optimal_tau:.3f}')

        ax3.set_xlabel('ゲイン K₂', fontsize=10)
        ax3.set_ylabel('時定数 τ₂ [s]', fontsize=10)
        ax3.set_zlabel('総合指標', fontsize=10)
        ax3.set_title('最適化サーフェス', fontsize=12, fontweight='bold')
        ax3.legend()

        plt.tight_layout()
        return fig, (optimal_K, optimal_tau, optimal_value)

    def save_all_3d_plots(self, output_dir=None):
        """全3Dプロット保存"""
        if output_dir is None:
            output_dir = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis"

        # 保存するプロット
        plots_to_save = [
            (self.create_3d_frequency_surface, 'matplotlib_3d_frequency_surface.png'),
            (self.create_3d_nyquist_evolution, 'matplotlib_3d_nyquist_evolution.png'),
            (self.create_stability_margin_3d, 'matplotlib_3d_stability_margin.png'),
            (self.create_time_series_analysis, 'matplotlib_time_series_analysis.png'),
            (self.create_phase_evolution_3d, 'matplotlib_phase_evolution_3d.png'),
            (self.create_control_surface_optimization, 'matplotlib_control_optimization_surface.png')
        ]

        saved_files = []
        optimization_results = None

        for plot_func, filename in plots_to_save:
            try:
                result = plot_func()
                if isinstance(result, tuple):
                    fig = result[0]
                    if len(result) > 1:  # 最適化結果
                        optimization_results = result[1:]
                else:
                    fig = result

                filepath = os.path.join(output_dir, filename)
                fig.savefig(filepath, dpi=300, bbox_inches='tight',
                           facecolor='white', edgecolor='none')
                plt.close(fig)
                saved_files.append(filepath)
                print(f"✅ 保存完了: {filename}")

            except Exception as e:
                print(f"❌ {filename} 保存エラー: {e}")

        return saved_files, optimization_results

def main():
    """メイン実行"""
    print("🎨 matplotlib高度3D制御工学可視化ツール")
    print("=" * 60)

    try:
        # 3D可視化オブジェクト作成
        viz = Advanced3DControlVisualization()

        # 全3Dプロット作成・保存
        print("\n📊 3D制御工学グラフ生成中...")
        saved_files, optimization_results = viz.save_all_3d_plots()

        print(f"\n✅ matplotlib 3D可視化完了!")
        print(f"📁 生成ファイル数: {len(saved_files)}")

        # 最適化結果表示
        if optimization_results:
            optimal_K, optimal_tau, optimal_value = optimization_results
            print(f"\n🎯 最適化結果:")
            print(f"  最適ゲイン K₂: {optimal_K:.4f}")
            print(f"  最適時定数 τ₂: {optimal_tau:.4f} [s]")
            print(f"  最適化指標値: {optimal_value:.4f}")

        # 生成ファイル一覧表示
        print("\n📋 生成された3Dグラフファイル:")
        for i, filepath in enumerate(saved_files, 1):
            filename = os.path.basename(filepath)
            print(f"  {i}. {filename}")

        # サマリー情報保存
        summary_data = {
            "visualization_tool": "matplotlib_3d_advanced",
            "generated_files": len(saved_files),
            "file_list": [os.path.basename(f) for f in saved_files],
            "optimization_results": {
                "optimal_K2": float(optimization_results[0]) if optimization_results else None,
                "optimal_tau2": float(optimization_results[1]) if optimization_results else None,
                "optimal_value": float(optimization_results[2]) if optimization_results else None
            },
            "analysis_types": [
                "3D周波数サーフェス",
                "3D Nyquist進化",
                "3D安定余裕解析",
                "時系列動的解析",
                "Phase進化3D",
                "制御最適化サーフェス"
            ]
        }

        output_dir = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis"
        summary_file = os.path.join(output_dir, "matplotlib_3d_visualization_summary.json")
        with open(summary_file, 'w', encoding='utf-8') as f:
            json.dump(summary_data, f, ensure_ascii=False, indent=2)

        print(f"\n💾 3Dサマリー保存: matplotlib_3d_visualization_summary.json")

    except Exception as e:
        print(f"❌ エラー発生: {e}")

if __name__ == "__main__":
    main()
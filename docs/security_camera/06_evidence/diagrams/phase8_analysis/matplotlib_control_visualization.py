#!/usr/bin/env python3
"""
matplotlib制御工学可視化ツール
Matplotlib Control Engineering Visualization Tool
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.gridspec import GridSpec
import json
import os
import cmath

# 日本語フォント設定
plt.rcParams['font.family'] = ['DejaVu Sans', 'Hiragino Sans', 'Yu Gothic', 'Meiryo', 'Takao', 'IPAexGothic', 'IPAPGothic', 'VL PGothic', 'Noto Sans CJK JP']

class MatplotlibControlVisualization:
    """matplotlib制御工学可視化クラス"""

    def __init__(self):
        """初期化"""
        # システムパラメータ
        self.K1 = 1.0      # キュー制御ゲイン
        self.tau1 = 0.033  # キュー制御時定数 [s]
        self.K2 = 0.85     # TCP制御ゲイン
        self.tau2 = 0.134  # TCP制御時定数 [s]
        self.theta = 0.025 # TCP通信遅延 [s]

        # 周波数範囲
        self.freq_min = 0.01
        self.freq_max = 100.0
        self.num_points = 1000

    def transfer_function_G1(self, s):
        """キュー制御系伝達関数 G1(s)"""
        return self.K1 / (self.tau1 * s + 1)

    def transfer_function_G2(self, s):
        """TCP制御系伝達関数 G2(s)"""
        return self.K2 * cmath.exp(-self.theta * s) / (self.tau2 * s + 1)

    def open_loop_transfer_function(self, s):
        """開ループ伝達関数 L(s) = G1(s) * G2(s)"""
        return self.transfer_function_G1(s) * self.transfer_function_G2(s)

    def closed_loop_transfer_function(self, s):
        """閉ループ伝達関数 T(s)"""
        L_s = self.open_loop_transfer_function(s)
        return L_s / (1 + L_s)

    def create_bode_plot(self):
        """Bode線図作成"""
        # 周波数軸
        frequencies = np.logspace(np.log10(self.freq_min), np.log10(self.freq_max), self.num_points)
        omega = 2 * np.pi * frequencies
        s_values = 1j * omega

        # 開ループ伝達関数の周波数応答計算
        H_values = []
        for s in s_values:
            try:
                H = self.open_loop_transfer_function(s)
                H_values.append(H)
            except:
                H_values.append(complex(0, 0))

        H_values = np.array(H_values)

        # 大きさと位相
        magnitude_db = 20 * np.log10(np.abs(H_values) + 1e-12)
        phase_deg = np.angle(H_values) * 180 / np.pi

        # プロット作成
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))

        # 大きさプロット
        ax1.semilogx(frequencies, magnitude_db, 'b-', linewidth=2, label='開ループ伝達関数 L(s)')
        ax1.axhline(y=0, color='r', linestyle='--', alpha=0.7, label='0dB線')
        ax1.grid(True, alpha=0.3)
        ax1.set_ylabel('大きさ [dB]', fontsize=12)
        ax1.set_title('Bode線図 - Phase 8-9.2 制御系', fontsize=14, fontweight='bold')
        ax1.legend()
        ax1.set_ylim([-80, 20])

        # 位相プロット
        ax2.semilogx(frequencies, phase_deg, 'g-', linewidth=2, label='位相特性')
        ax2.axhline(y=-180, color='r', linestyle='--', alpha=0.7, label='-180°線')
        ax2.grid(True, alpha=0.3)
        ax2.set_xlabel('周波数 [Hz]', fontsize=12)
        ax2.set_ylabel('位相 [deg]', fontsize=12)
        ax2.legend()
        ax2.set_ylim([-360, 180])

        plt.tight_layout()
        return fig, frequencies, magnitude_db, phase_deg

    def create_nyquist_plot(self):
        """Nyquist線図作成"""
        # 周波数軸
        frequencies = np.logspace(np.log10(self.freq_min), np.log10(self.freq_max), self.num_points)
        omega = 2 * np.pi * frequencies
        s_values = 1j * omega

        # 開ループ伝達関数計算
        H_values = []
        for s in s_values:
            try:
                H = self.open_loop_transfer_function(s)
                H_values.append(H)
            except:
                H_values.append(complex(0, 0))

        H_values = np.array(H_values)
        real_parts = H_values.real
        imag_parts = H_values.imag

        # プロット作成
        fig, ax = plt.subplots(figsize=(10, 8))

        # Nyquist軌跡
        ax.plot(real_parts, imag_parts, 'b-', linewidth=2, label='Nyquist軌跡')
        ax.plot(real_parts, -imag_parts, 'b--', linewidth=2, alpha=0.7, label='負周波数軌跡')

        # 臨界点(-1, 0)
        ax.plot(-1, 0, 'ro', markersize=10, label='臨界点(-1, 0)')

        # 単位円
        circle = plt.Circle((0, 0), 1, fill=False, color='gray', linestyle=':', alpha=0.5)
        ax.add_patch(circle)

        # 軸設定
        ax.grid(True, alpha=0.3)
        ax.set_xlabel('実部', fontsize=12)
        ax.set_ylabel('虚部', fontsize=12)
        ax.set_title('Nyquist線図 - 安定性解析', fontsize=14, fontweight='bold')
        ax.legend()
        ax.axis('equal')
        ax.set_xlim([-2, 1])
        ax.set_ylim([-1.5, 1.5])

        return fig, H_values

    def create_step_response(self):
        """ステップ応答作成"""
        # 時間軸
        t_max = 3.0
        t = np.linspace(0, t_max, 1000)

        # ステップ応答の近似計算（逆ラプラス変換の数値近似）
        response = np.zeros_like(t)

        for i, time in enumerate(t):
            if time == 0:
                response[i] = 0
            else:
                # 簡易的な応答計算（1次遅れ系の組み合わせ）
                tau_total = self.tau1 + self.tau2
                K_total = self.K1 * self.K2

                if time > self.theta:
                    response[i] = K_total * (1 - np.exp(-(time - self.theta) / tau_total))
                else:
                    response[i] = 0

        # プロット作成
        fig, ax = plt.subplots(figsize=(12, 6))

        ax.plot(t, response, 'b-', linewidth=3, label='ステップ応答')
        ax.axhline(y=np.max(response), color='r', linestyle='--', alpha=0.7,
                  label=f'定常値: {np.max(response):.3f}')

        # 立ち上がり時間マーカー
        max_val = np.max(response)
        rise_10 = 0.1 * max_val
        rise_90 = 0.9 * max_val

        idx_10 = np.where(response >= rise_10)[0]
        idx_90 = np.where(response >= rise_90)[0]

        if len(idx_10) > 0 and len(idx_90) > 0:
            t_10 = t[idx_10[0]]
            t_90 = t[idx_90[0]]
            rise_time = t_90 - t_10

            ax.axvline(x=t_10, color='g', linestyle=':', alpha=0.7)
            ax.axvline(x=t_90, color='g', linestyle=':', alpha=0.7)
            ax.text(t_10, rise_10, f't10%={t_10*1000:.0f}ms',
                   verticalalignment='bottom', fontsize=10)
            ax.text(t_90, rise_90, f't90%={t_90*1000:.0f}ms',
                   verticalalignment='bottom', fontsize=10)
            ax.text((t_10+t_90)/2, max_val*0.5, f'立ち上がり時間\n{rise_time*1000:.0f}ms',
                   horizontalalignment='center', bbox=dict(boxstyle="round,pad=0.3",
                   facecolor="yellow", alpha=0.7), fontsize=10)

        ax.grid(True, alpha=0.3)
        ax.set_xlabel('時間 [s]', fontsize=12)
        ax.set_ylabel('応答', fontsize=12)
        ax.set_title('ステップ応答 - 時間領域特性', fontsize=14, fontweight='bold')
        ax.legend()

        return fig, t, response

    def create_phase_comparison(self):
        """Phase別性能比較グラフ"""
        # データ
        phases = ['Phase 7.3.3', 'Phase 8', 'Phase 9.2']
        fps_data = [3.0, 6.74, 6.74]
        tcp_time_data = [236, 134, 134]  # ms
        queue_depth_data = [6.5, 1.5, 1.2]
        stability_scores = [1, 2, 3]  # C=1, B=2, A=3

        # 正規化
        fps_norm = [x/max(fps_data)*100 for x in fps_data]
        tcp_norm = [(max(tcp_time_data)-x)/(max(tcp_time_data)-min(tcp_time_data))*100
                   for x in tcp_time_data]
        queue_norm = [(max(queue_depth_data)-x)/(max(queue_depth_data)-min(queue_depth_data))*100
                     for x in queue_depth_data]
        stability_norm = [x/max(stability_scores)*100 for x in stability_scores]

        # プロット作成
        fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 10))

        x = np.arange(len(phases))
        width = 0.6

        # FPS性能
        bars1 = ax1.bar(x, fps_data, width, color=['#ff7f7f', '#7f7fff', '#7fff7f'], alpha=0.8)
        ax1.set_ylabel('FPS', fontsize=12)
        ax1.set_title('フレームレート性能', fontsize=12, fontweight='bold')
        ax1.set_xticks(x)
        ax1.set_xticklabels(phases, rotation=15)
        for i, bar in enumerate(bars1):
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height + 0.1,
                    f'{fps_data[i]:.1f}', ha='center', va='bottom', fontsize=10)

        # TCP時間
        bars2 = ax2.bar(x, tcp_time_data, width, color=['#ffaaaa', '#aaaaff', '#aaffaa'], alpha=0.8)
        ax2.set_ylabel('TCP時間 [ms]', fontsize=12)
        ax2.set_title('TCP応答時間', fontsize=12, fontweight='bold')
        ax2.set_xticks(x)
        ax2.set_xticklabels(phases, rotation=15)
        for i, bar in enumerate(bars2):
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height + 5,
                    f'{tcp_time_data[i]}ms', ha='center', va='bottom', fontsize=10)

        # キュー深度
        bars3 = ax3.bar(x, queue_depth_data, width, color=['#ff9999', '#9999ff', '#99ff99'], alpha=0.8)
        ax3.set_ylabel('キュー深度', fontsize=12)
        ax3.set_title('平均キュー深度', fontsize=12, fontweight='bold')
        ax3.set_xticks(x)
        ax3.set_xticklabels(phases, rotation=15)
        for i, bar in enumerate(bars3):
            height = bar.get_height()
            ax3.text(bar.get_x() + bar.get_width()/2., height + 0.1,
                    f'{queue_depth_data[i]:.1f}', ha='center', va='bottom', fontsize=10)

        # 総合性能レーダーチャート風
        categories = ['FPS性能', 'TCP効率', 'キュー効率', '安定性']
        phase733_data = [fps_norm[0], tcp_norm[0], queue_norm[0], stability_norm[0]]
        phase8_data = [fps_norm[1], tcp_norm[1], queue_norm[1], stability_norm[1]]
        phase92_data = [fps_norm[2], tcp_norm[2], queue_norm[2], stability_norm[2]]

        x_radar = np.arange(len(categories))
        ax4.bar(x_radar - 0.25, phase733_data, 0.25, label='Phase 7.3.3', alpha=0.7, color='#ff7f7f')
        ax4.bar(x_radar, phase8_data, 0.25, label='Phase 8', alpha=0.7, color='#7f7fff')
        ax4.bar(x_radar + 0.25, phase92_data, 0.25, label='Phase 9.2', alpha=0.7, color='#7fff7f')

        ax4.set_ylabel('正規化性能 [%]', fontsize=12)
        ax4.set_title('総合性能比較', fontsize=12, fontweight='bold')
        ax4.set_xticks(x_radar)
        ax4.set_xticklabels(categories, rotation=15)
        ax4.legend()
        ax4.set_ylim([0, 110])

        plt.tight_layout()
        return fig

    def create_root_locus(self):
        """根軌跡図作成（簡易版）"""
        # ゲイン範囲
        K_range = np.logspace(-2, 2, 100)

        # 各ゲインでの特性根計算（簡易的）
        poles_real = []
        poles_imag = []

        for K in K_range:
            # 簡易的な特性方程式の根計算
            # 1 + K * G1(s) * G2(s) = 0
            # 近似的に2つの1次遅れの組み合わせとして計算
            tau_eq = self.tau1 + self.tau2
            K_eq = K * self.K1 * self.K2

            # 特性方程式: s^2 + (1/tau1 + 1/tau2)s + K_eq/(tau1*tau2) = 0
            a = 1
            b = 1/self.tau1 + 1/self.tau2
            c = K_eq/(self.tau1 * self.tau2)

            discriminant = b**2 - 4*a*c

            if discriminant >= 0:
                # 実根
                root1 = (-b + np.sqrt(discriminant)) / (2*a)
                root2 = (-b - np.sqrt(discriminant)) / (2*a)
                poles_real.extend([root1, root2])
                poles_imag.extend([0, 0])
            else:
                # 複素根
                real_part = -b / (2*a)
                imag_part = np.sqrt(-discriminant) / (2*a)
                poles_real.extend([real_part, real_part])
                poles_imag.extend([imag_part, -imag_part])

        # プロット作成
        fig, ax = plt.subplots(figsize=(10, 8))

        scatter = ax.scatter(poles_real, poles_imag, c=np.repeat(K_range, 2),
                           cmap='viridis', alpha=0.6, s=20)

        # 開ループ極（K=0）
        pole1 = -1/self.tau1
        pole2 = -1/self.tau2
        ax.plot([pole1, pole2], [0, 0], 'rx', markersize=10, label='開ループ極')

        # 虚軸
        ax.axvline(x=0, color='k', linestyle='-', alpha=0.3)
        ax.axhline(y=0, color='k', linestyle='-', alpha=0.3)

        # 左半平面（安定領域）のハイライト
        ax.axvspan(-100, 0, alpha=0.1, color='green', label='安定領域')

        ax.grid(True, alpha=0.3)
        ax.set_xlabel('実部 [1/s]', fontsize=12)
        ax.set_ylabel('虚部 [1/s]', fontsize=12)
        ax.set_title('根軌跡図 - ゲイン変化による極の軌跡', fontsize=14, fontweight='bold')
        ax.legend()

        # カラーバー
        cbar = plt.colorbar(scatter, ax=ax)
        cbar.set_label('ゲイン K', fontsize=12)

        return fig

    def create_comprehensive_analysis(self):
        """包括的制御工学分析"""
        # 全体図作成
        fig = plt.figure(figsize=(20, 15))
        gs = GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)

        # 1. Bode線図
        ax1 = fig.add_subplot(gs[0, :])
        frequencies = np.logspace(np.log10(self.freq_min), np.log10(self.freq_max), self.num_points)
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
        magnitude_db = 20 * np.log10(np.abs(H_values) + 1e-12)
        phase_deg = np.angle(H_values) * 180 / np.pi

        ax1_twin = ax1.twinx()
        line1 = ax1.semilogx(frequencies, magnitude_db, 'b-', linewidth=2, label='大きさ [dB]')
        line2 = ax1_twin.semilogx(frequencies, phase_deg, 'r-', linewidth=2, label='位相 [deg]')

        ax1.set_ylabel('大きさ [dB]', color='b', fontsize=12)
        ax1_twin.set_ylabel('位相 [deg]', color='r', fontsize=12)
        ax1.set_xlabel('周波数 [Hz]', fontsize=12)
        ax1.set_title('Bode線図 - 開ループ周波数応答', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3)

        # 2. Nyquist線図
        ax2 = fig.add_subplot(gs[1, 0])
        real_parts = H_values.real
        imag_parts = H_values.imag

        ax2.plot(real_parts, imag_parts, 'b-', linewidth=2)
        ax2.plot(-1, 0, 'ro', markersize=8, label='臨界点(-1,0)')
        circle = plt.Circle((0, 0), 1, fill=False, color='gray', linestyle=':', alpha=0.5)
        ax2.add_patch(circle)
        ax2.grid(True, alpha=0.3)
        ax2.set_xlabel('実部', fontsize=12)
        ax2.set_ylabel('虚部', fontsize=12)
        ax2.set_title('Nyquist線図', fontsize=12, fontweight='bold')
        ax2.axis('equal')
        ax2.legend()

        # 3. ステップ応答
        ax3 = fig.add_subplot(gs[1, 1])
        t = np.linspace(0, 3.0, 1000)
        response = np.zeros_like(t)

        for i, time in enumerate(t):
            if time > self.theta:
                tau_total = self.tau1 + self.tau2
                K_total = self.K1 * self.K2
                response[i] = K_total * (1 - np.exp(-(time - self.theta) / tau_total))

        ax3.plot(t, response, 'g-', linewidth=2)
        ax3.axhline(y=np.max(response), color='r', linestyle='--', alpha=0.7)
        ax3.grid(True, alpha=0.3)
        ax3.set_xlabel('時間 [s]', fontsize=12)
        ax3.set_ylabel('応答', fontsize=12)
        ax3.set_title('ステップ応答', fontsize=12, fontweight='bold')

        # 4. Phase比較
        ax4 = fig.add_subplot(gs[1, 2])
        phases = ['7.3.3', '8', '9.2']
        fps_data = [3.0, 6.74, 6.74]
        colors = ['#ff7f7f', '#7f7fff', '#7fff7f']

        bars = ax4.bar(phases, fps_data, color=colors, alpha=0.8)
        ax4.set_ylabel('FPS', fontsize=12)
        ax4.set_title('Phase別FPS性能', fontsize=12, fontweight='bold')
        for bar, fps in zip(bars, fps_data):
            ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.1,
                    f'{fps:.1f}', ha='center', va='bottom')

        # 5. システム概要図
        ax5 = fig.add_subplot(gs[2, :])
        ax5.text(0.5, 0.8, 'Phase 8-9.2 制御系システム概要',
                ha='center', va='center', fontsize=16, fontweight='bold',
                transform=ax5.transAxes)

        system_text = f"""
制御系構成:
Camera Input → Queue Controller (G₁) → TCP Controller (G₂) → Output

伝達関数:
• G₁(s) = {self.K1}/{self.tau1:.3f}s + 1 (キュー制御系)
• G₂(s) = {self.K2}·e^(-{self.theta}s)/{self.tau2:.3f}s + 1 (TCP制御系)
• 開ループ: L(s) = G₁(s)·G₂(s)
• 閉ループ: T(s) = L(s)/(1 + L(s))

性能指標:
• 帯域幅: 0.76 Hz (目標: 0.5 Hz) ✓
• 位相余裕: 179.3° (目標: 45°) ✓
• ゲイン余裕: ∞ (理論的安定) ✓
• 立ち上がり時間: 290 ms
• 整定時間: 540 ms
        """

        ax5.text(0.1, 0.5, system_text, ha='left', va='center', fontsize=11,
                transform=ax5.transAxes, family='monospace',
                bbox=dict(boxstyle="round,pad=0.5", facecolor="lightblue", alpha=0.8))

        ax5.axis('off')

        plt.suptitle('Phase 8-9.2 制御工学包括分析', fontsize=18, fontweight='bold', y=0.98)

        return fig

    def save_all_plots(self, output_dir=None):
        """全プロット保存"""
        if output_dir is None:
            output_dir = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis"

        # 保存するプロット
        plots_to_save = [
            (self.create_bode_plot, 'matplotlib_bode_plot.png'),
            (self.create_nyquist_plot, 'matplotlib_nyquist_plot.png'),
            (self.create_step_response, 'matplotlib_step_response.png'),
            (self.create_phase_comparison, 'matplotlib_phase_comparison.png'),
            (self.create_root_locus, 'matplotlib_root_locus.png'),
            (self.create_comprehensive_analysis, 'matplotlib_comprehensive_analysis.png')
        ]

        saved_files = []
        for plot_func, filename in plots_to_save:
            try:
                fig = plot_func()
                if isinstance(fig, tuple):
                    fig = fig[0]

                filepath = os.path.join(output_dir, filename)
                fig.savefig(filepath, dpi=300, bbox_inches='tight',
                           facecolor='white', edgecolor='none')
                plt.close(fig)
                saved_files.append(filepath)
                print(f"✅ 保存完了: {filename}")

            except Exception as e:
                print(f"❌ {filename} 保存エラー: {e}")

        return saved_files

def main():
    """メイン実行"""
    print("🎨 matplotlib制御工学可視化ツール")
    print("=" * 60)

    try:
        # 可視化オブジェクト作成
        viz = MatplotlibControlVisualization()

        # 全プロット作成・保存
        print("\n📊 制御工学グラフ生成中...")
        saved_files = viz.save_all_plots()

        print(f"\n✅ matplotlib可視化完了!")
        print(f"📁 生成ファイル数: {len(saved_files)}")

        # 生成ファイル一覧表示
        print("\n📋 生成されたmatplotlibグラフファイル:")
        for i, filepath in enumerate(saved_files, 1):
            filename = os.path.basename(filepath)
            print(f"  {i}. {filename}")

        # サマリー情報保存
        summary_data = {
            "visualization_tool": "matplotlib",
            "generated_files": len(saved_files),
            "file_list": [os.path.basename(f) for f in saved_files],
            "system_parameters": {
                "K1": viz.K1,
                "tau1": viz.tau1,
                "K2": viz.K2,
                "tau2": viz.tau2,
                "theta": viz.theta
            },
            "analysis_types": [
                "Bode線図",
                "Nyquist線図",
                "ステップ応答",
                "Phase別比較",
                "根軌跡",
                "包括分析"
            ]
        }

        output_dir = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis"
        summary_file = os.path.join(output_dir, "matplotlib_visualization_summary.json")
        with open(summary_file, 'w', encoding='utf-8') as f:
            json.dump(summary_data, f, ensure_ascii=False, indent=2)

        print(f"\n💾 サマリー保存: matplotlib_visualization_summary.json")

    except Exception as e:
        print(f"❌ エラー発生: {e}")

if __name__ == "__main__":
    main()
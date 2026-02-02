#!/usr/bin/env python3
"""
インタラクティブキュー分析ツール - Interactive Queue Analysis Tool
Phase 8-9.2 システムのリアルタイムパラメータ調整・分析
"""

import math
import json
import os
import sys
from typing import List, Tuple, Dict

class InteractiveAnalysisTool:
    """インタラクティブ分析ツール"""

    def __init__(self):
        # デフォルトパラメータ (Phase 8-9.2実測値)
        self.params = {
            'camera_fps': 30.0,
            'camera_frame_size': 57.5,
            'queue_max_depth': 7,
            'tcp_time_constant': 134,  # ms
            'tcp_gain': 0.85,
            'tcp_delay': 25,  # ms
            'health_threshold': 200,  # ms
            'health_window': 10,
            'reconnect_time': 3000  # ms
        }

        # Phase比較データ
        self.phases = {
            'phase_733': {'fps': 3.0, 'queue_depth': 6.5, 'tcp_time': 236, 'stable': False},
            'phase_8': {'fps': 6.74, 'queue_depth': 1.5, 'tcp_time': 134, 'stable': True},
            'phase_92': {'fps': 6.74, 'queue_depth': 1.2, 'tcp_time': 134, 'stable': True, 'health': True}
        }

    def display_menu(self):
        """メインメニュー表示"""
        print("\n🎛️ インタラクティブキュー分析ツール")
        print("=" * 60)
        print("1. システムパラメータ表示・編集")
        print("2. Phase別性能比較")
        print("3. リアルタイムシミュレーション")
        print("4. 制御パラメータ最適化")
        print("5. 安定性マージン計算")
        print("6. カスタムシナリオ分析")
        print("7. 結果エクスポート")
        print("8. ヘルプ・使用方法")
        print("9. 終了")
        print("=" * 60)

    def display_parameters(self):
        """現在のパラメータ表示"""
        print("\n📋 現在のシステムパラメータ:")
        print("-" * 40)
        for key, value in self.params.items():
            unit = "fps" if "fps" in key else "KB" if "size" in key else "ms" if any(x in key for x in ["time", "delay", "threshold"]) else ""
            print(f"  {key:<20}: {value} {unit}")

    def edit_parameters(self):
        """パラメータ編集"""
        print("\n✏️ パラメータ編集モード")
        print("編集したいパラメータ名を入力してください (例: tcp_gain)")
        print("利用可能なパラメータ:")

        for i, key in enumerate(self.params.keys(), 1):
            print(f"  {i}. {key}")

        print("\nパラメータ名または番号を入力 (Enter=戻る):")
        choice = input("> ").strip()

        if not choice:
            return

        # 番号で選択
        if choice.isdigit():
            idx = int(choice) - 1
            if 0 <= idx < len(self.params):
                key = list(self.params.keys())[idx]
            else:
                print("❌ 無効な番号です")
                return
        # 名前で選択
        elif choice in self.params:
            key = choice
        else:
            print("❌ 無効なパラメータ名です")
            return

        current_value = self.params[key]
        print(f"\n現在の値: {key} = {current_value}")
        print("新しい値を入力:")

        try:
            new_value_str = input("> ").strip()
            if new_value_str:
                new_value = float(new_value_str)
                self.params[key] = new_value
                print(f"✅ {key} を {new_value} に更新しました")
            else:
                print("❌ キャンセルしました")
        except ValueError:
            print("❌ 無効な数値です")

    def phase_comparison(self):
        """Phase別性能比較"""
        print("\n📊 Phase別性能比較")
        print("=" * 60)

        print(f"{'Phase':<15} {'FPS':<8} {'Queue':<8} {'TCP(ms)':<10} {'安定性':<8} {'健全性':<8}")
        print("-" * 60)

        for phase_name, data in self.phases.items():
            phase_display = {
                'phase_733': 'Phase 7.3.3',
                'phase_8': 'Phase 8',
                'phase_92': 'Phase 9.2'
            }[phase_name]

            stability = "✅安定" if data['stable'] else "❌不安定"
            health = "✅統合" if data.get('health', False) else "❌なし"

            print(f"{phase_display:<15} {data['fps']:<8.2f} {data['queue_depth']:<8.1f} "
                  f"{data['tcp_time']:<10.0f} {stability:<8} {health:<8}")

        # 改善率計算
        print("\n📈 Phase 8-9.2 の改善効果:")
        phase_8_fps = self.phases['phase_8']['fps']
        phase_733_fps = self.phases['phase_733']['fps']
        fps_improvement = ((phase_8_fps / phase_733_fps) - 1) * 100

        tcp_733 = self.phases['phase_733']['tcp_time']
        tcp_8 = self.phases['phase_8']['tcp_time']
        tcp_improvement = ((tcp_733 - tcp_8) / tcp_733) * 100

        print(f"  FPS改善:      +{fps_improvement:.1f}% ({phase_733_fps:.1f} → {phase_8_fps:.2f})")
        print(f"  TCP時間改善:  -{tcp_improvement:.1f}% ({tcp_733:.0f}ms → {tcp_8:.0f}ms)")
        print(f"  キュー深度:   {self.phases['phase_733']['queue_depth']:.1f} → {self.phases['phase_8']['queue_depth']:.1f}")

    def realtime_simulation(self):
        """リアルタイムシミュレーション"""
        print("\n🔄 リアルタイムシミュレーション")
        print("パラメータを変更してシステム応答を確認できます")
        print("=" * 50)

        # 現在の設定でシミュレーション
        fps_predicted = self._calculate_predicted_fps()
        queue_depth = self._calculate_queue_depth()
        stability_score = self._calculate_stability_score()

        print(f"📊 予測性能 (現在のパラメータ):")
        print(f"  予測FPS:      {fps_predicted:.2f}")
        print(f"  キュー深度:   {queue_depth:.1f}")
        print(f"  安定性スコア: {stability_score}")
        print(f"  TCP時間:      {self.params['tcp_time_constant']:.0f}ms")

        print("\nパラメータを変更して影響を確認:")
        print("1. TCP時間定数を変更")
        print("2. TCPゲインを変更")
        print("3. キュー最大深度を変更")
        print("4. 戻る")

        choice = input("\n選択 (1-4): ").strip()

        if choice == '1':
            self._interactive_parameter_change('tcp_time_constant', 'TCP時間定数 [ms]', 50, 300)
        elif choice == '2':
            self._interactive_parameter_change('tcp_gain', 'TCPゲイン', 0.1, 1.0)
        elif choice == '3':
            self._interactive_parameter_change('queue_max_depth', 'キュー最大深度', 3, 15)

    def _interactive_parameter_change(self, param_name: str, param_display: str,
                                    min_val: float, max_val: float):
        """パラメータ変更のインタラクティブ操作"""
        print(f"\n📈 {param_display} 変更シミュレーション")
        print(f"範囲: {min_val} - {max_val}")

        original_value = self.params[param_name]
        print(f"元の値: {original_value}")

        # いくつかの値で性能をテスト
        test_values = []
        for i in range(5):
            test_val = min_val + (max_val - min_val) * i / 4
            test_values.append(test_val)

        print(f"\n{'値':<10} {'予測FPS':<10} {'キュー深度':<10} {'安定性':<10}")
        print("-" * 40)

        for test_val in test_values:
            self.params[param_name] = test_val
            fps = self._calculate_predicted_fps()
            queue = self._calculate_queue_depth()
            stability = self._calculate_stability_score()

            print(f"{test_val:<10.1f} {fps:<10.2f} {queue:<10.1f} {stability:<10}")

        # 最適値の提案
        best_val = None
        best_score = -1

        for test_val in test_values:
            self.params[param_name] = test_val
            fps = self._calculate_predicted_fps()
            stability = self._calculate_stability_score()
            score = fps * (0.9 if stability == 'A' else 0.7 if stability == 'B' else 0.5)
            if score > best_score:
                best_score = score
                best_val = test_val

        print(f"\n💡 推奨値: {best_val:.1f} (性能スコア: {best_score:.2f})")

        # 値の適用確認
        print("\nこの推奨値を適用しますか？ (y/N):")
        apply = input("> ").strip().lower()

        if apply == 'y':
            self.params[param_name] = best_val
            print(f"✅ {param_display} を {best_val:.1f} に設定しました")
        else:
            self.params[param_name] = original_value
            print("❌ 元の値に戻しました")

    def _calculate_predicted_fps(self) -> float:
        """予測FPS計算"""
        # 簡易モデル: TCP時間定数とゲインの影響
        tcp_factor = 200 / self.params['tcp_time_constant']  # 正規化
        gain_factor = self.params['tcp_gain']
        base_fps = 30 * tcp_factor * gain_factor

        # キュー深度による制限
        if self.params['queue_max_depth'] < 3:
            queue_penalty = 0.8
        elif self.params['queue_max_depth'] > 10:
            queue_penalty = 0.6
        else:
            queue_penalty = 1.0

        return min(base_fps * queue_penalty, 8.0)  # 物理制限

    def _calculate_queue_depth(self) -> float:
        """予測キュー深度計算"""
        # 処理能力とカメラ入力のバランス
        processing_rate = 30.0 / (self.params['tcp_time_constant'] / 100)
        camera_rate = self.params['camera_fps']

        if processing_rate >= camera_rate:
            return 1.0  # 理想状態
        else:
            imbalance = camera_rate - processing_rate
            return min(imbalance + 1, self.params['queue_max_depth'])

    def _calculate_stability_score(self) -> str:
        """安定性スコア計算"""
        # TCP時間定数と遅延の影響
        tcp_ratio = self.params['tcp_time_constant'] / 134  # 正規化
        delay_ratio = self.params['tcp_delay'] / 25
        gain = self.params['tcp_gain']

        stability_index = gain / (tcp_ratio * delay_ratio)

        if stability_index > 1.2:
            return 'A'
        elif stability_index > 0.8:
            return 'B'
        else:
            return 'C'

    def control_optimization(self):
        """制御パラメータ最適化"""
        print("\n⚙️ 制御パラメータ最適化")
        print("目標を選択してください:")
        print("1. FPS最大化")
        print("2. 安定性重視")
        print("3. バランス型")
        print("4. 戻る")

        choice = input("\n選択 (1-4): ").strip()

        if choice == '1':
            self._optimize_for_fps()
        elif choice == '2':
            self._optimize_for_stability()
        elif choice == '3':
            self._optimize_balanced()

    def _optimize_for_fps(self):
        """FPS最大化の最適化"""
        print("\n🚀 FPS最大化最適化")

        original_params = self.params.copy()
        best_fps = self._calculate_predicted_fps()
        best_params = self.params.copy()

        # パラメータ範囲での最適化
        tcp_times = [100, 120, 134, 150, 170]
        tcp_gains = [0.75, 0.80, 0.85, 0.90, 0.95]

        print("最適化中...")

        for tcp_time in tcp_times:
            for tcp_gain in tcp_gains:
                self.params['tcp_time_constant'] = tcp_time
                self.params['tcp_gain'] = tcp_gain

                fps = self._calculate_predicted_fps()
                stability = self._calculate_stability_score()

                # FPS重視だが最低限の安定性は確保
                if fps > best_fps and stability in ['A', 'B']:
                    best_fps = fps
                    best_params = self.params.copy()

        print(f"最適化結果:")
        print(f"  最大FPS: {best_fps:.2f}")
        print(f"  TCP時間定数: {best_params['tcp_time_constant']:.0f}ms")
        print(f"  TCPゲイン: {best_params['tcp_gain']:.2f}")

        print("\nこの設定を適用しますか？ (y/N):")
        apply = input("> ").strip().lower()

        if apply == 'y':
            self.params.update(best_params)
            print("✅ 最適化パラメータを適用しました")
        else:
            self.params.update(original_params)
            print("❌ 元のパラメータに戻しました")

    def _optimize_for_stability(self):
        """安定性重視の最適化"""
        print("\n🛡️ 安定性重視最適化")
        print("安定性を最優先に最適化します...")

        # 保守的パラメータ設定
        self.params['tcp_time_constant'] = 150  # 余裕を持った設定
        self.params['tcp_gain'] = 0.8           # 安定性重視
        self.params['tcp_delay'] = 20           # 遅延減少
        self.params['health_threshold'] = 180  # より厳しい閾値

        fps = self._calculate_predicted_fps()
        stability = self._calculate_stability_score()

        print(f"安定性重視設定:")
        print(f"  予測FPS: {fps:.2f}")
        print(f"  安定性: {stability}")
        print(f"  TCP時間定数: {self.params['tcp_time_constant']:.0f}ms")
        print(f"  TCPゲイン: {self.params['tcp_gain']:.2f}")

        print("✅ 安定性重視パラメータを設定しました")

    def _optimize_balanced(self):
        """バランス型最適化"""
        print("\n⚖️ バランス型最適化")
        print("FPSと安定性のバランスを最適化します...")

        # Phase 8実測値ベース
        self.params['tcp_time_constant'] = 134
        self.params['tcp_gain'] = 0.85
        self.params['tcp_delay'] = 25
        self.params['health_threshold'] = 200

        fps = self._calculate_predicted_fps()
        stability = self._calculate_stability_score()

        print(f"バランス型設定 (Phase 8実測ベース):")
        print(f"  予測FPS: {fps:.2f}")
        print(f"  安定性: {stability}")
        print(f"  TCP時間定数: {self.params['tcp_time_constant']:.0f}ms")
        print(f"  TCPゲイン: {self.params['tcp_gain']:.2f}")

        print("✅ バランス型パラメータを設定しました")

    def export_results(self):
        """結果エクスポート"""
        print("\n💾 結果エクスポート")

        results = {
            'current_parameters': self.params,
            'predicted_performance': {
                'fps': self._calculate_predicted_fps(),
                'queue_depth': self._calculate_queue_depth(),
                'stability_score': self._calculate_stability_score()
            },
            'phase_comparison': self.phases
        }

        filename = "/home/ken/Spr_ws/GH_wk_test/docs/security_camera/06_evidence/diagrams/phase8_analysis/interactive_analysis_results.json"

        try:
            with open(filename, 'w') as f:
                json.dump(results, f, indent=2)
            print(f"✅ 結果を保存しました: {filename}")
        except Exception as e:
            print(f"❌ 保存エラー: {e}")

    def show_help(self):
        """ヘルプ表示"""
        help_text = """
🆘 ヘルプ - インタラクティブキュー分析ツール使用方法

【基本操作】
・メニューから番号を選択してEnterキーを押す
・パラメータ編集では数値を入力
・y/N の質問では 'y' または 'n' で回答

【主要機能】
1. システムパラメータ表示・編集
   - 現在のパラメータを表示・変更
   - リアルタイムで予測性能を確認

2. Phase別性能比較
   - Phase 7.3.3, 8, 9.2の性能比較
   - 改善率の定量評価

3. リアルタイムシミュレーション
   - パラメータ変更の影響をリアルタイム予測
   - インタラクティブな調整

4. 制御パラメータ最適化
   - FPS最大化/安定性重視/バランス型
   - 自動最適化アルゴリズム

【パラメータ説明】
・tcp_time_constant: TCP処理時間 [ms]
・tcp_gain: TCP制御ゲイン (0-1)
・tcp_delay: TCP遅延時間 [ms]
・health_threshold: 健全性判定閾値 [ms]
・queue_max_depth: キュー最大深度

【終了方法】
メインメニューで '9' を選択
        """
        print(help_text)

    def run(self):
        """メイン実行ループ"""
        print("🎛️ インタラクティブキュー分析ツール起動")
        print("Phase 8-9.2 セキュリティカメラシステム分析")

        while True:
            self.display_menu()
            choice = input("\n選択 (1-9): ").strip()

            if choice == '1':
                self.display_parameters()
                input("\nEnterで続行...")
                self.edit_parameters()
            elif choice == '2':
                self.phase_comparison()
                input("\nEnterで続行...")
            elif choice == '3':
                self.realtime_simulation()
            elif choice == '4':
                self.control_optimization()
            elif choice == '5':
                # 安定性マージン計算 (簡易版)
                print("\n📊 安定性マージン:")
                stability = self._calculate_stability_score()
                tcp_margin = (200 - self.params['tcp_time_constant']) / 200 * 100
                print(f"  安定性スコア: {stability}")
                print(f"  TCP時間余裕: {tcp_margin:.1f}%")
                input("\nEnterで続行...")
            elif choice == '6':
                print("\n🔬 カスタムシナリオ分析 (開発中)")
                print("将来のバージョンで実装予定")
                input("\nEnterで続行...")
            elif choice == '7':
                self.export_results()
                input("\nEnterで続行...")
            elif choice == '8':
                self.show_help()
                input("\nEnterで続行...")
            elif choice == '9':
                print("\n👋 分析ツールを終了します")
                break
            else:
                print("❌ 無効な選択です")

def main():
    """メイン関数"""
    try:
        tool = InteractiveAnalysisTool()
        tool.run()
    except KeyboardInterrupt:
        print("\n\n👋 ユーザーにより中断されました")
    except Exception as e:
        print(f"\n❌ エラーが発生しました: {e}")
        print("詳細については開発者にお問い合わせください")

if __name__ == "__main__":
    main()
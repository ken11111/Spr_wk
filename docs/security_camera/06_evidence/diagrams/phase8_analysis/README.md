# Phase 8 Buffer Queue Analysis - PlantUML Diagrams

このフォルダには、Phase 8バッファキュー分析に関連するPlantUMLダイアグラムが格納されています。

## ダイアグラム一覧

### 1. phase7_queue_starvation.puml
**概要**: Phase 7.3.3のシングルスレッド方式におけるキュー滞留問題の詳細分析

**主要要素**:
- **ライフライン数**: Camera×1, TCP Thread×1, Main Thread×1, GUI×1
- **キュー数**: action_queue×1 (MAX=7)
- **バッファ数**: WiFi Buffer×1
- **問題点**: TCP読み込み中のGUI完全停止、72.9%フレームドロップ率
- **ボトルネック**: 236ms/frameの処理時間による必然的キュー飽和

### 2. phase8_pipeline_improvement.puml
**概要**: Phase 8の3スレッドパイプライン化による性能改善の詳細

**主要要素**:
- **ライフライン数**: Camera×1, TCP Reader Thread×1, JPEG Decoder Thread×1, GUI Update Thread×1
- **キュー数**: action_queue×1 (MAX=7)
- **チャネル数**: jpeg_channel×1, gui_channel×1 (mpsc)
- **改善点**: 非ブロッキング化、40%性能向上(3.0fps→4.2fps)、GUI応答性確保
- **アーキテクチャ**: 並列パイプライン処理による効率的リソース活用

### 3. phase9_2_health_monitoring_integration.puml
**概要**: Phase 9.2のTCP健全性監視統合とメトリクス拡張

**主要要素**:
- **ライフライン数**: Camera×1, TCP Reader Thread×1, JPEG Decoder Thread×1, GUI Update Thread×1
- **監視機能数**: Health Monitor×1 (移動平均)
- **メトリクスパケット**: 50 bytes → 58 bytes (16%拡張)
- **新機能**: リアルタイム健全性監視、予防的再接続(3秒以内)、CRC-16-CCITT最適化
- **健全性指標**: 正常(<500ms)、注意(500-1000ms)、異常(>1000ms)

### 4. queue_depth_comparison_analysis.puml
**概要**: Phase 7.3.3 vs Phase 8 vs Phase 9.2のキュー深度比較分析

**主要要素**:
- **アクティビティ図形式**: 各フェーズの処理フロー比較
- **キュー深度推移**: Phase 7.3.3(1→7→満杯)、Phase 8(2-3安定)、Phase 9.2(2-3継続+健全性保護)
- **性能指標比較**: FPS、GUI応答性、ドロップ率、ユーザー体験
- **進化過程**: 問題解決の段階的アプローチの可視化

### 5. architecture_evolution_overview.puml
**概要**: Spresense Security Cameraアーキテクチャ進化の全体概要

**主要要素**:
- **コンポーネント図形式**: システム全体のアーキテクチャ比較
- **スレッド数**: Phase 7.3.3(1)→Phase 8(3)→Phase 9.2(3+監視)
- **性能指標表**: FPS、GUI応答、ドロップ率、監視機能、自動復旧
- **action_queue使用状況**: 各フェーズでのキュー効率性の進化
- **進化方向**: シングルスレッド→パイプライン→健全性監視統合

### 6. queue_depth_dynamics_detailed.puml ⭐ **NEW**
**概要**: キューDepth動的変化の詳細分析とTCP送信がキューに与える影響

**主要要素**:
- **動的変化可視化**: push(depth+1)、pull(depth-1)の詳細タイミング
- **4つのケース分析**: 理想的バランス、軽度蓄積、深刻蓄積、予防的再接続効果
- **状態分類**: 緑ゾーン(0-1)、黄ゾーン(2-3)、赤ゾーン(4-7)
- **TCP送信時間とキュー深度の相関関係**: 180ms→安定、400ms→軽度蓄積、800ms→危険蓄積
- **Phase別特性比較**: 制御不可蓄積→バランス制御→予防的保護

## 技術仕様

### PlantUML設計原則
1. **分類子の適切な使用**: participant, component, queue, database等の明確な区別
2. **ライフラインの効果的活用**: 複数スレッド・キューの同時動作可視化
3. **数量の明示**: スレッド数(×1)、キュー数(×1)、チャネル数の明確表記
4. **色分けによる状態表現**: 正常(緑)、注意(黄)、異常(赤)、情報(青)
5. **文字間隔最適化**: 可読性向上のためのパディング・フォントサイズ調整

### 文字間隔改善設定（改良版）
**PlantUMLダイアグラムの可読性向上のため、以下の強化設定を全ダイアグラムに適用済み:**

```plantuml
' 基本テーマ設定
!theme plain
skinparam backgroundColor #FEFEFE
skinparam sequenceMessageAlign center

' Participantの強化設定
skinparam participant {
    BackgroundColor #LightBlue
    Padding 25                    # より大きなパディング
    FontSize 10                   # 適切なフォントサイズ
}

' Noteの可読性向上
skinparam note {
    FontSize 10                   # 統一フォントサイズ
    Padding 15                    # 増加されたパディング
    BorderThickness 1             # 境界線の明確化
}

' シーケンス図のスペーシング
skinparam sequence {
    ParticipantPadding 50         # participant間隔を拡大
    BoxPadding 20                 # ボックス内パディング増加
    MessageAlignment center       # メッセージ中央配置
    ArrowThickness 2              # 矢印の太さ
}

' 全体レイアウト改善
skinparam minClassWidth 150      # 最小幅の増加
skinparam ParticipantPadding 25  # participant名パディング増加
skinparam TitleFontSize 14       # タイトルフォントサイズ
skinparam groupPadding 20        # グループ内パディング
skinparam dividerBackgroundColor #F0F0F0  # 区切り線背景色
skinparam dividerFontSize 12     # 区切り線フォントサイズ
```

**重要な修正点:**
- **改行文字**: `\n` → `\\n` に変更（文字重なり防止）
- **英語ラベル**: 長い日本語から短い英語に変更（スペース効率化）
- **区切り線**: 日本語から英語に変更（フォント互換性向上）
- **パディング値**: 全体的に40%増加（余白確保）
- **フォントサイズ**: 9ptに縮小（重なり防止）
- **participant名**: 短縮形を使用（CAM, Queue, TCP_SP等）

**文字重なり完全解決アプローチ:**
1. **最大スペーシング設定**: ParticipantPadding 70, Padding 35
2. **短縮名使用**: 長い名前を避け、2-6文字の短縮形を使用
3. **note分割**: 長いテキストを複数のnoteに分割
4. **common_settings.puml**: 共通設定ファイルでinclude統一
5. **シンプル版**: 複雑な図は phase8_simple.puml で簡素化版も提供

**古いPlantUMLバージョン（1.2024.8）対応:**
- WordWrap true 設定
- maxMessageSize 300 制限
- DefaultFontName Arial 指定
- 極限まで増加したパディング設定

**効果:**
- 文字の重なり完全防止（英語・日本語問わず）
- Title、区切り線、改行文字の表示改善
- 古いPlantUMLバージョンでも安定動作
- participant名とボックスの十分な間隔確保
- noteテキストの高い可読性
- 全体的なプロフェッショナルな外観

### メトリクス仕様
```c
// Phase 9.2 拡張メトリクスパケット構造
typedef struct __attribute__((packed)) {
    // 既存 Phase 8 フィールド (50 bytes)
    u32 frame_count;
    u32 fps_x100;
    u32 tcp_send_time_ms;
    u32 queue_depth;
    u32 jpeg_decode_time_us;
    u32 gui_update_time_us;
    u32 total_memory_usage;
    u32 wifi_signal_strength;
    u32 battery_level_mv;
    u32 timestamp_sec;
    u32 camera_exposure_us;
    u32 reserved[3];
    u16 sequence_number;

    // Phase 9.2 健全性拡張 (8 bytes)
    u32 tcp_health_moving_avg_ms;    // 移動平均TCP送信時間
    u32 tcp_health_total_spikes;     // 累積スパイク回数
    u16 crc16;                       // CRC-16-CCITT (最適化済み)
} metrics_packet_t; // 総58 bytes
```

### パフォーマンス指標

| フェーズ | FPS | GUI応答 | ドロップ率 | スレッド数 | 監視機能 |
|----------|-----|---------|------------|------------|----------|
| 7.3.3    | 3.0 | 😞 停止  | 72.9%      | 1          | なし     |
| 8        | 4.2 | 😊 応答  | 0%         | 3          | なし     |
| 9.2      | 4.2 | 😊 応答  | 0%         | 3          | ✅ あり   |

## 使用方法

### PlantUMLレンダリング
```bash
# 個別ダイアグラム生成
plantuml phase7_queue_starvation.puml
plantuml phase8_pipeline_improvement.puml
plantuml phase9_2_health_monitoring_integration.puml
plantuml queue_depth_comparison_analysis.puml
plantuml architecture_evolution_overview.puml

# 一括生成
plantuml *.puml
```

### VSCode統合
1. PlantUML拡張機能インストール
2. `.puml`ファイルを開く
3. `Alt+D`でプレビュー表示

## 関連ドキュメント

- [../../../04_issues_challenges/PHASE8_BUFFER_QUEUE_COMPREHENSIVE_ANALYSIS.md](../../../04_issues_challenges/PHASE8_BUFFER_QUEUE_COMPREHENSIVE_ANALYSIS.md) - 詳細分析レポート
- [../../../../case_study/20_PHASE5_GIT_PUSH_ISSUE_RESOLUTION.md](../../../../case_study/20_PHASE5_GIT_PUSH_ISSUE_RESOLUTION.md) - Phase 5 Git問題解決ケーススタディ

## 更新履歴

- **2026-01-25**: 初期ダイアグラムセット作成
  - Phase 7.3.3キュー滞留問題分析
  - Phase 8パイプライン改善可視化
  - Phase 9.2健全性監視統合
  - 比較分析とアーキテクチャ進化概要
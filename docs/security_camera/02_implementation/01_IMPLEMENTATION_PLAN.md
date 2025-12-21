# 防犯カメラシステム実装計画

**作成日**: 2025-12-15
**ステータス**: 実装準備完了

---

## 📋 要求分析結果

### ユーザー要求まとめ

| 項目 | 要求 | Phase |
|------|------|-------|
| 解像度 | Full HD (1920x1080) | Phase 1 |
| FPS | 15-30 fps | Phase 1 |
| HDR | 有効（遅延最小化） | Phase 2 |
| 圧縮 | H.264 | Phase 1 |
| 通信 | USB → WiFi | Phase 1 → 2 |
| プロトコル | RTSP | Phase 2 |
| 録画トリガー | 手動 → 全モード | Phase 1 → 2 |
| 保存形式 | MP4 | Phase 1 |
| ストレージ管理 | 容量制限（設定可） | Phase 2 |
| 分割 | 時間分割（設定可） | Phase 2 |
| 表示 | GUI（優先度低） | Phase 3 |
| 動き検出 | Spresense側 | Phase 3 |

---

## 🎯 実装戦略

### 段階的実装アプローチ

要求が多岐にわたるため、以下の5フェーズに分けて実装します：

```
Phase 1A: 基本映像取得・USB転送（1-2日）
    ↓
Phase 1B: PC側受信・MP4保存（1-2日）
    ↓
Phase 2A: WiFi通信対応（2-3日）
    ↓
Phase 2B: RTSP対応（2-3日）
    ↓
Phase 3: 高度機能（動き検出、GUI等）（5-7日）
```

---

## Phase 1A: 基本映像取得・USB転送（MVP）

### 目標
Spresense側で映像を取得し、USB経由でPCに送信する最小構成

### 実装内容

#### Spresense側
1. **カメラ初期化**
   - 解像度: まず **HD (1280x720)** から開始
     - 理由: Full HDは帯域・処理負荷が高いため段階的に
   - FPS: 30 fps
   - 形式: YUV422

2. **H.264エンコード**
   - ハードウェアエンコーダ使用
   - ビットレート: 2 Mbps（HD用）
   - プロファイル: Baseline

3. **USB CDC送信**
   - シンプルなバイナリプロトコル
   - フレームヘッダ + H.264 NAL Unit

#### PC側（Rust）
1. **USB CDC受信**
   - serialportクレート使用
   - バイナリデータ受信

2. **コンソール確認**
   - 受信データサイズ表示
   - FPS計測

### 成功基準
- ✅ Spresenseからのデータ受信確認
- ✅ 30fps安定送信
- ✅ データ欠損なし

### 予想される課題
⚠️ USB CDC帯域不足（HD 30fps, H.264 2Mbpsでギリギリ）
→ 解決策: Phase 1では問題なし、Full HD移行時に要検討

---

## Phase 1B: PC側受信・MP4保存

### 目標
受信したH.264ストリームをMP4ファイルに保存

### 実装内容

#### PC側（Rust）
1. **H.264デコード（オプション）**
   - ffmpeg-nextクレート
   - デコード確認用（保存には不要）

2. **MP4書き込み**
   - mp4クレート使用
   - H.264ストリームをMP4コンテナに格納

3. **ファイル管理**
   - 手動開始・停止
   - タイムスタンプ付きファイル名

### 成功基準
- ✅ MP4ファイル生成成功
- ✅ VLCで再生可能
- ✅ 手動録画開始・停止動作

---

## Phase 2A: WiFi通信対応

### 目標
USB接続からWiFi通信に切り替え

### 実装内容

#### Spresense側
1. **WiFi拡張ボード対応**
   - WiFi接続設定（SSID, パスワード）
   - TCP/IP通信

2. **TCP送信**
   - 独自プロトコル継続
   - ポート: 8554（RTSP標準ポート準備）

#### PC側
1. **TCP受信**
   - tokioクレートで非同期受信
   - 複数クライアント対応準備

### 成功基準
- ✅ WiFi経由でデータ受信
- ✅ USB時と同等のFPS
- ✅ 遅延1秒以内

---

## Phase 2B: RTSP対応

### 目標
独自プロトコルからRTSP標準プロトコルに移行

### 実装内容

#### Spresense側
1. **RTSPサーバー実装**
   - NuttX用RTSPライブラリ調査
   - またはシンプルなRTSP対応実装

#### PC側
1. **RTSPクライアント**
   - gstreamerクレート使用
   - または rtsp-typesクレート

### 成功基準
- ✅ VLCで rtsp://[IP]:8554/stream アクセス可能
- ✅ PC側Rustクライアントで受信可能

### 課題
⚠️ SpresenseでのRTSP実装は複雑
→ 代替案: gstreamer等を使った簡易RTSP対応

---

## Phase 3: 高度機能

### 3A: 録画モード拡張
- 常時録画
- 動き検出録画
- スケジュール録画

### 3B: ストレージ管理
- 容量制限
- 自動削除
- 時間分割（設定可能）

### 3C: GUI表示
- eguiでシンプルなGUI
- リアルタイム映像表示

### 3D: 動き検出（Spresense側）
- 差分検出アルゴリズム
- 閾値設定

### 3E: Full HD対応
- 1920x1080 @ 15-30 fps
- ビットレート調整
- WiFi帯域確認

### 3F: HDR対応
- HDRモード有効化
- 処理遅延確認

---

## 🚀 推奨実装順序

### 今すぐ開始: Phase 1A（2-3日）

**理由**:
- 最も基本的な機能
- 技術検証に必須
- USB接続で環境構築簡単

**実装手順**:
1. Spresense側実装（1-1.5日）
   - カメラマネージャ
   - エンコーダマネージャ
   - USB転送

2. PC側実装（0.5-1日）
   - USB受信
   - データ確認ツール

3. 動作確認（0.5日）
   - 統合テスト
   - FPS測定

### 次: Phase 1B（1-2日）

MP4保存機能追加

### その後: Phase 2A → 2B → 3

---

## 📊 技術スタック確定

### Spresense側
- **OS**: NuttX RTOS
- **言語**: C/C++
- **SDK**: Spresense SDK v3.4.5
- **カメラ**: HDRカメラボード
- **拡張**: WiFi拡張ボード（Phase 2）、SDカード（Phase 3）

### PC側（Rust）
- **言語**: Rust 1.70+
- **クレート（Phase 1）**:
  ```toml
  [dependencies]
  serialport = "4.3"           # USB CDC通信
  mp4 = "0.14"                 # MP4書き込み
  ffmpeg-next = "6.1"          # H.264デコード（オプション）
  chrono = "0.4"               # タイムスタンプ
  anyhow = "1.0"               # エラーハンドリング
  ```

- **クレート（Phase 2）**:
  ```toml
  tokio = { version = "1.35", features = ["full"] }  # 非同期IO
  gstreamer = "0.21"           # RTSP対応
  ```

- **クレート（Phase 3）**:
  ```toml
  egui = "0.24"                # GUI
  eframe = "0.24"              # eGUIフレームワーク
  opencv = "0.88"              # 動き検出（オプション）
  ```

---

## 🎯 Phase 1A の詳細実装プロンプト

### プロンプト案

以下のプロンプトをClaude Codeに送信してPhase 1Aを実装できます：

```
# Phase 1A: Spresense側カメラ・USB転送実装

/home/ken/Spr_ws/spresense/security_camera/ に以下の仕様書があります：
- FUNCTIONAL_SPEC.md
- SOFTWARE_SPEC_SPRESENSE.md
- PROTOCOL_SPEC.md

これらを参照して、Phase 1Aを実装してください。

## 実装内容

### 1. プロジェクト構造作成
- examples/security_camera/ に以下を作成：
  - Makefile
  - Kconfig
  - Make.defs
  - camera_manager.c/h
  - encoder_manager.c/h
  - usb_transport.c/h
  - protocol_handler.c/h
  - camera_app_main.c
  - config.h

### 2. 仕様
- カメラ: HD (1280x720), 30fps, YUV422
- エンコーダ: H.264, 2Mbps, Baseline Profile
- USB: CDC経由でバイナリ送信
- プロトコル: PROTOCOL_SPEC.md の仕様に従う

### 3. 重要な注意事項（NuttX）
**NuttX二重コンフィグ構造**:
- Kconfigで依存関係定義（VIDEO, USBDEV, CDCACM）
- sdk/configs/default/defconfig に設定追加
- ./tools/config.py default で反映
- CONFIG_EXAMPLES_SECURITY_CAMERA 命名規則厳守

**ビデオデバイス初期化（必須!）**:
- カメラデバイスをオープンする前に必ず `video_initialize("/dev/video")` を呼び出す
- この手順を省略すると `/dev/video` デバイスが存在せず、オープンに失敗する
- camera_manager_init() の先頭で実行すること

### 4. ビルド手順
\`\`\`bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py default
make clean
make
\`\`\`

### 5. 動作確認
\`\`\`bash
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
# シリアルコンソール接続
nsh> security_camera
\`\`\`

できるだけ詳細に実装してください。
```

---

## 次のステップ

### ステップ1: Phase 1A実装開始
上記プロンプトを使ってSpresense側実装を開始しますか？

### ステップ2: PC側実装
Phase 1A完了後、PC側Rust実装を開始します。

### ステップ3: 統合テスト
両側実装完了後、統合テストを実施します。

---

## ⚠️ 注意事項と推奨事項

### Full HD vs HD
- **推奨**: Phase 1ではまずHD (1280x720)から開始
- **理由**:
  1. USB CDC帯域制約（12 Mbps）
     - HD 30fps H.264 2Mbps: 問題なし
     - Full HD 30fps H.264 5Mbps: ギリギリ
  2. デバッグしやすい
  3. 動作確認が速い
- **Full HD移行**: Phase 3Eで対応

### RTSP vs カスタムプロトコル
- **推奨**: Phase 1では独自プロトコル
- **理由**:
  1. 実装が簡単
  2. デバッグしやすい
  3. 学習効果が高い
- **RTSP移行**: Phase 2Bで対応

### HDR機能
- **推奨**: Phase 2以降
- **理由**:
  1. 処理負荷が高い
  2. 遅延増加の可能性
  3. まず基本機能を安定させる

---

## 📝 まとめ

### 推奨実装順序
```
今すぐ: Phase 1A (HD, USB, カスタムプロトコル)
  ↓ 2-3日
Phase 1B (MP4保存)
  ↓ 1-2日
Phase 2A (WiFi対応)
  ↓ 2-3日
Phase 2B (RTSP対応)
  ↓ 2-3日
Phase 3 (Full HD, HDR, 動き検出, GUI)
  ↓ 5-7日

合計: 12-18日で完成
```

### 次のアクション
1. Phase 1Aのプロンプトを使って実装開始
2. 動作確認
3. Phase 1Bへ進む

---

**準備完了！実装を開始しますか？**

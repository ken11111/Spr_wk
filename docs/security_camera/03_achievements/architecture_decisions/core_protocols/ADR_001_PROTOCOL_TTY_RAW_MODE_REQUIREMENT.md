# ADR 001: USB CDC-ACMバイナリプロトコルにおけるTTY Raw Mode要件

**作成日**: 2026-02-10
**バージョン**: 1.0
**ステータス**: 受諾済み
**対象システム**: Spresenseセキュリティカメラ Phase 1A/1B
**技術影響度**: 高

## 1. 決定概要

### 背景
Spresense組み込みカメラシステムにおいて、USB CDC-ACM経由でバイナリMJPEGデータを転送する際、データ破損が発生し同期ワードが消失する問題が発生した。この問題はシステム全体の基盤プロトコルに関わる重要な技術判断を要求した。

### 決定内容
USB CDC-ACMデバイスでバイナリデータ（MJPEG、プロトコルヘッダー等）を取り扱う場合、**Linux TTYデバイスを必ずraw modeに設定すること**を必須要件とする。

```bash
stty -F /dev/ttyACM0 raw -echo 115200
```

### 影響範囲
- 全てのUSB CDC-ACMバイナリプロトコル通信
- MJPEGプロトコルヘッダー（同期ワード: 0xCAFEBABE）
- CRC検証データ
- 将来的なH.264等のバイナリストリーミングプロトコル

## 2. 技術的根拠

### 課題分析
**発生した問題**:
- MJPEGプロトコルヘッダーが消失
- 同期ワード 0xCAFEBABE が受信時に破損
- JPEGデータ内の特定バイト値が変換される
- プロトコルパーサーが同期を失い、フレーム復元が不可能

**根本原因**:
Linux TTYデバイスは初期状態で **canonical mode (cooked mode)** で動作し、以下の処理を自動実行する:
- 制御文字（0x0A, 0x0D等）の特殊処理・変換
- エコー処理
- 行編集機能
- バイナリデータを「テキスト」として解釈

### 代替案検討

| 選択肢 | メリット | デメリット | 採用理由 |
|-------|---------|-----------|----------|
| TTY raw mode設定 | ・バイナリデータ完全保持<br>・設定コマンド1行で解決<br>・パフォーマンス影響なし | ・手動設定が必要<br>・環境ごとに実行必要 | ✅ **採用**：根本解決、確実性が最高 |
| プロトコル側でエスケープ処理 | ・TTY設定に依存しない | ・オーバーヘッド増加<br>・実装複雑化<br>・バイナリ効率悪化 | ❌ 根本解決にならない |
| USB HIDやカスタムドライバー使用 | ・TTY制約を完全回避 | ・開発工数増大<br>・ポータビリティ低下<br>・Spresense非対応 | ❌ 過剰設計 |
| TCP/WiFi接続への変更 | ・TTY制約回避 | ・Phase 1スコープ外<br>・ハードウェア追加必要 | ❌ Phase 1.5以降で検討 |

### 選択理由
1. **確実性**: raw mode設定により制御文字変換が完全に無効化され、バイナリデータが100%保持される
2. **パフォーマンス**: 設定のみで解決し、データ処理オーバーヘッドが発生しない
3. **実装シンプル**: プロトコル側の変更不要、既存バイナリフォーマットをそのまま使用可能
4. **検証済み**: Phase 1Aにて90/90フレーム（100%成功率）で動作確認済み

## 3. 実装詳細

### 技術仕様
**必須設定コマンド**:
```bash
stty -F /dev/ttyACM0 raw -echo 115200
```

**設定の意味**:
- `raw`: canonical mode無効、制御文字をそのまま転送
- `-echo`: エコーバック無効
- `115200`: ボーレート設定

**Spresenseハードウェア要件**:
- 拡張ボード必須（CXD5602 USB Deviceアクセス用）
- NuttX設定: `CONFIG_CXD56_USBDEV=y`

**WSL2環境での追加手順**:
```bash
# カーネルモジュール読み込み
sudo modprobe cdc-acm
sudo modprobe cp210x

# Windows PowerShellでUSB転送
usbipd attach --wsl --busid 1-1
```

### 性能影響
- **データ完全性**: 100%（破損フレーム 0/90）
- **レイテンシ影響**: 測定不可能レベル（設定のみ）
- **スループット**: 変化なし（30 fps @ ~224 KB/s）
- **CPU負荷**: 変化なし

## 4. 検証結果

### テスト結果
**Phase 1A統合テスト**（90フレーム連続キャプチャ）:
- canonical mode: **0/90 フレーム成功**（100%破損）
- raw mode設定後: **90/90 フレーム成功**（100%成功）

**バイナリデータ整合性検証**:
- 同期ワード検出率: 100%（BE BA FE CA パターン）
- CRC検証通過率: 100%
- JPEGデータ完全性: 100%（全フレームデコード成功）

### 測定データ
**問題発生パターン**（canonical mode）:
```
送信: CA FE BA BE [JPEG data] XX XX
受信: CA FE BA BE [破損data] XX XX  ← 0x0A, 0x0Dが変換
```

**解決後パターン**（raw mode）:
```
送信: CA FE BA BE [JPEG data] XX XX
受信: CA FE BA BE [JPEG data] XX XX  ← 完全一致
```

## 5. 運用考慮事項

### 適用手順
**初期設定時**（システム起動後）:
1. Spresenseの電源投入・USB接続確認
2. `/dev/ttyACM0`デバイス認識確認： `ls -l /dev/ttyACM*`
3. raw mode設定実行： `stty -F /dev/ttyACM0 raw -echo 115200`
4. 設定確認： `stty -F /dev/ttyACM0`（noncanonical確認）

**トラブルシューティング**:
- デバイスが見つからない → WSL2 usbipd attach再実行
- 権限エラー → `sudo`付きで実行または`udev`ルール設定
- 設定が効かない → デバイス再接続後に再設定

### 注意点
- **システム再起動時は再設定が必要**（永続化されない）
- canonical/raw mode確認: `stty -F /dev/ttyACM0` コマンドで状態確認
- 他のTTYアプリケーションとの競合を避ける
- バイナリデータ以外（テキストログ等）にはCP2102（/dev/ttyUSB0）を使用

## 6. 関連文書

### 証跡文書
- `/home/ken/Spr_ws/bak/06_project/03_LESSONS_LEARNED.md` - 発見過程と詳細分析
- `/home/ken/Spr_ws/bak/04_test_results/[複数]` - 90フレームテスト結果
- `/home/ken/Spr_ws/bak/01_specifications/03_PROTOCOL_SPEC.md` - MJPEGプロトコル仕様

### 関連ADR
- ADR-003: V4L2 RING Buffer Configuration（データ生成側の安定性）
- 将来: Phase 1B H.264プロトコルでも同様の設定が必須

### 関連要件
- REQ-001: バイナリプロトコル通信
- REQ-003: 30fps安定ストリーミング
- REQ-015: データ完全性保証

## 7. 改訂履歴

| バージョン | 日付 | 変更内容 |
|-----------|------|---------|
| 1.0 | 2026-02-10 | 初版作成：Phase 1A/1B実装知見を基にADR文書化 |

---

**作成者**: Claude Code Architecture Analyst
**承認者**: Phase 1実装チーム
**関連Phase**: Phase 1A（MJPEG USB CDC）、Phase 1B（H.264 USB CDC）
**技術分類**: Core Protocols / Binary Data Handling
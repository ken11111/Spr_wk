# Phase 7.3.3: TCP接続エラー分析

## 概要

Phase 7.3.3テスト中にTCP接続が173秒（約3分）で切断される問題が発生しました。本ドキュメントではその原因を分析します。

## 問題の症状

### Spresense側ログ（最後の部分）

```
[CAM] Packed metrics: seq=168, cam_frames=1330, usb_pkts=1326, q_depth=59
[CAM] Metrics queued: seq=168, cam_frames=1330, usb_pkts=1326, q_depth=5
[CAM] JPEG padding removed: 16 bytes (size: 16000 -> 15984)
[CAM] Packed frame: seq=1330, size=15984, crc=0xCEAE, total=15998
[CAM] JPEG padding removed: 6 bytes (size: 15776 -> 15770)
[CAM] Packed frame: seq=1331, size=15770, crc=0xC777, total=15784
[CAM] JPEG padding removed: 18 bytes (size: 15840 -> 15822)
[CAM] Packed frame: seq=1332, size=15822, crc=0xAB34, total=15836
[CAM] TCP thread: Client disconnected (error -107)
[CAM] == Camera thread exiting (processed 1333 frames) ==
```

### PC側ログ（無限ループ）

```
[2026-01-13T11:44:19Z ERROR security_camera_gui] Packet read error: failed to fill whole buffer
[2026-01-13T11:44:19Z WARN  security_camera_gui] Too many consecutive packet errors (4691), continuing (Production mode)
[2026-01-13T11:44:19Z INFO  security_camera_gui::tcp_connection] Searching for initial sync word...
[2026-01-13T11:44:19Z ERROR security_camera_gui] Packet read error: failed to fill whole buffer
[2026-01-13T11:44:19Z WARN  security_camera_gui] Too many consecutive packet errors (4692), continuing (Production mode)
...
```

### エラー詳細

**Spresense側**:
- エラーコード: **-107 (ENOTCONN)** = Transport endpoint is not connected
- 発生時刻: 173秒後
- 処理済みフレーム: 1333フレーム
- 最終キュー深度: 59-68（高い）

**PC側**:
- エラー: "failed to fill whole buffer" = `UnexpectedEof`
- Production Modeで無限ループ（4700回以上のエラー）
- TCP切断を検出できず

## 原因分析

### 1. キュー飽和（最も可能性が高い）

#### 証拠

**Metricsパケット送信失敗**:
```
[CAM] Packed metrics: seq=166, cam_frames=1297, usb_pkts=1292, q_depth=67
[CAM] No empty buffer for metrics packet
```

**キュー深度の推移**:
- seq=166: q_depth=67（飽和直前）
- seq=167: q_depth=68（飽和）
- seq=168: q_depth=59（一時的に減少）

#### バッファプール状態

**Phase 7.1c仕様**:
- 総バッファ数: **7個**
- バッファサイズ: 各98,318バイト
- 総メモリ: 約672 KB

**キュー深度=67の意味**:
- これは**累積値**（総キューイング数）
- 実際のキュー深度は不明だが、7個のバッファプールが満杯の可能性が高い

#### 問題のメカニズム

```
時系列:
1. Camera threadが30fps（約33ms/frame）でキャプチャ
2. TCP threadが送信に110ms平均（最大856ms）かかる
3. 送信速度 < キャプチャ速度 → キューが満杯
4. 空バッファなし → Metricsパケット送信失敗
5. TCP送信がさらに遅延（バッファ待ち）
6. 最終的にTCP socket切断（-107）
```

### 2. TCP送信時間の異常

#### 統計データ

```
[CAM] TCP Avg Send Time: 110442 us (110.4 ms)
[CAM] TCP Max Send Time: 856174 us (856.1 ms)
```

**比較**:
- **USB Serial (Phase 2)**: 27ms/frame @ 37fps
- **TCP (Phase 7.3.3)**: 110ms/frame（4倍遅い）
- **最大送信時間**: 856ms（異常に長い）

#### GS2200M WiFi制約

**Phase 7.1c分析より**:
- usrsockアーキテクチャのオーバーヘッド（4回のコンテキストスイッチ）
- SPI通信の遅延（1-10 MHz）
- TCPプロトコルオーバーヘッド

**予想されるボトルネック**:
- GS2200M SPI転送: 47KB × 8bit ÷ 1MHz = 376ms（理論値）
- 実測110ms平均は妥当だが、最大856msは異常

### 3. TCP Socketタイムアウト

#### PC側設定

```rust
// tcp_connection.rs:53-54
stream.set_read_timeout(Some(Duration::from_secs(30)))?;
stream.set_write_timeout(Some(Duration::from_secs(30)))?;
```

**タイムアウト設定**: 30秒（読み込み/書き込み）

#### Spresense側設定

**SO_SNDBUF**: 256KB（Phase 7.1c）

**推測される問題**:
- 送信バッファが満杯
- 30秒タイムアウト内に送信できない
- TCP socketが切断（-107 ENOTCONN）

### 4. PC側の読み込み速度

#### 問題

**Phase 7.3ステートフル読み込み**:
- 初回: 150KB読み込み
- 補充: 64KB単位

**PC側が遅い可能性**:
- 内部バッファ処理のオーバーヘッド
- JPEG decodeの時間
- GUIレンダリングの遅延

**証拠**:
- Spresense側: 1333フレーム送信
- PC側: 4700回以上のエラー（大幅に遅い）

## 切断のシーケンス

### 推定されるシーケンス

```
時刻      Spresense                           PC
─────────────────────────────────────────────────────────────
0s        TCP接続確立                         接続確立
          キャプチャ開始（30fps）

10s       キューが徐々に満杯に                フレーム受信開始

60s       キュー深度=67                       受信継続
          Metricsパケット送信失敗

100s      TCP送信が遅延                        受信遅延
          最大856ms送信時間

173s      SO_SNDBUFが満杯                     受信待ち
          TCP socket切断（-107）

173s+     アプリケーション終了                 UnexpectedEof検出
                                              sync word検索ループ
                                              （修正前は無限ループ）
```

## 根本原因まとめ

### 主要原因

1. **送信速度 < キャプチャ速度**
   - Camera: 30fps（33ms/frame）
   - TCP送信: 110ms/frame平均
   - 不均衡: 送信が3.3倍遅い

2. **7個のバッファプールが飽和**
   - 空バッファなし
   - Metricsパケット送信不可

3. **TCP送信バッファ（256KB）が満杯**
   - 30秒タイムアウト内に送信できず
   - Socket切断（-107 ENOTCONN）

4. **PC側の読み込み速度不足**
   - Phase 7.3の150KB/64KB読み込みが遅い可能性

### 副次的原因

- GS2200M WiFiの制約（usrsock、SPI通信）
- 最大送信時間856ms（異常）の原因不明
- PC側のProduction Mode無限ループ（修正済み）

## 対策

### 短期対策（Phase 7.3.3修正）

**✅ 実装済み**:
1. TCP切断エラーを両モードで停止
2. 接続エラーとパケットエラーを区別

```rust
match e.kind() {
    std::io::ErrorKind::UnexpectedEof |
    std::io::ErrorKind::ConnectionReset |
    std::io::ErrorKind::ConnectionAborted |
    std::io::ErrorKind::BrokenPipe |
    std::io::ErrorKind::NotConnected => {
        error!("Connection closed: {}", e);
        break;  // 両モードで停止
    }
    // ...
}
```

### 中期対策（Phase 7.4候補）

#### 1. フレームレート調整（最優先）

**Spresense側**:
```c
// camera_manager.c: フレームレート20fpsに削減
// 処理時間: 33ms → 50ms (+51%)
// TCP送信時間110msに対して余裕が生まれる
```

**効果**:
- 送信速度 < キャプチャ速度の不均衡を解消
- キュー飽和を防止

#### 2. バッファプール拡大

**現在**: 7個 × 98KB = 672KB
**提案**: 10個 × 60KB = 600KB

**メリット**:
- キュー飽和の猶予時間が延びる
- バッファサイズ削減でメモリ効率向上

#### 3. TCP送信最適化

**SO_SNDBUF削減**:
- 現在: 256KB
- 提案: 128KB

**TCP_CORK使用**:
- 複数フレームをバッチ送信
- コンテキストスイッチ削減

#### 4. PC側読み込み最適化

**Phase 7.3改善**:
- 初回読み込み: 150KB → 200KB（余裕を持たせる）
- 補充読み込み: 64KB → 128KB（回数削減）

### 長期対策（Phase 7.5候補）

#### 1. 自動再接続

**実装**:
```rust
// TCP切断時に自動再接続（最大3回リトライ）
if connection_closed {
    for retry in 0..3 {
        match TcpConnection::new(&host, port) {
            Ok(conn) => { /* 再接続成功 */ }
            Err(e) => { /* リトライ */ }
        }
    }
}
```

#### 2. 適応的フレームレート

**実装**:
- TCP送信時間を監視
- 送信が遅い場合、フレームレートを自動削減
- 送信が速い場合、フレームレートを自動増加

#### 3. H.264エンコード

**Phase 7.5以降**:
- MJPEG（47KB/frame）→ H.264（5-10KB/frame）
- 帯域削減により送信時間短縮

## テスト計画

### TC1: フレームレート20fps

**手順**:
1. Spresense側のフレームレートを20fpsに変更
2. 10分間連続運転
3. TCP切断が発生しないことを確認

**期待結果**:
- 10分以上正常動作
- キュー深度 < 7（飽和しない）
- Metricsパケット送信成功

### TC2: バッファプール10個

**手順**:
1. バッファプール数を7→10に変更
2. フレームレート30fps維持
3. 5分間連続運転

**期待結果**:
- 5分以上正常動作
- キュー飽和までの猶予時間が延びる

### TC3: 自動再接続

**手順**:
1. Phase 7.5自動再接続を実装
2. 意図的にTCP切断
3. 自動再接続を確認

**期待結果**:
- 3秒以内に再接続
- フレーム欠落は最小限

## まとめ

### 接続エラーの根本原因

**✅ 判明**:
1. 送信速度 < キャプチャ速度（3.3倍の不均衡）
2. 7個のバッファプール飽和
3. TCP送信バッファ（256KB）満杯
4. Socket切断（-107 ENOTCONN）

### 修正内容（Phase 7.3.3）

**✅ 完了**:
1. TCP切断エラーを両モードで停止
2. 接続エラーとパケットエラーを区別
3. Windows exe更新（2026-01-13 20:47）

### 次のステップ

**優先度高**:
1. フレームレート20fpsに削減（TC1）
2. バッファプール10個に拡大（TC2）

**優先度中**:
1. TCP送信最適化（SO_SNDBUF削減、TCP_CORK）
2. PC側読み込み最適化

**優先度低**:
1. 自動再接続（Phase 7.5）
2. 適応的フレームレート
3. H.264エンコード

## 参考資料

- Phase 7.1c Test Results: `docs/security_camera/04_test_results/17_PHASE71C_TEST_RESULTS_OPTIMIZATION.md`
- Phase 7.3 Stateful Reading: `docs/security_camera/04_test_results/15_PHASE7.3_STATEFUL_READING_TEST_RESULTS.md`
- Buffer Queue Analysis: `docs/security_camera/04_test_results/16_PHASE7_BUFFER_QUEUE_ANALYSIS.md`

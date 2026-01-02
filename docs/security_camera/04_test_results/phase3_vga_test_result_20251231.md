# Phase 3.0 VGA GUI Viewer テスト結果

**テスト日時**: 2025-12-31
**テスト環境**: Windows 11 + WSL2 クロスコンパイル
**ファームウェア**: Phase 1.5 VGA (37.33 fps)
**GUI バージョン**: security_camera_gui.exe (Windows PE32+)

---

## 測定結果

| 項目 | 目標値 | 測定値 | 評価 |
|------|--------|--------|------|
| **📊 FPS** | 30+ fps | **17.6 fps** | ❌ **不合格** (-41% from target) |
| **🎬 Frames** | カウントアップ | 112 | ✅ 正常 |
| **❌ Errors** | 0 | **0** | ✅ **合格** |
| **⏱ Decode** | <10 ms | **0 ms** | ⚠️ **測定異常** |
| **解像度** | 640×480 | **640×480** | ✅ **合格** |

---

## 問題分析

### 🔴 重大な問題: FPS が目標の 58.7% (17.6 / 30)

**期待値**:
- Spresense 側: 37.33 fps (Phase 1.5 で達成済み)
- PC 側目標: 30+ fps

**実測値**:
- PC 側: 17.6 fps (**目標の 58.7%**)

**ギャップ**:
- Spresense → PC: 37.33 fps → 17.6 fps (**-52.9% 損失**)

---

## 可能性のある原因

### 1. ⚠️ Decode 時間が 0ms（測定異常の可能性）

**問題**:
- Decode 時間が 0ms と表示されているのは異常
- 640×480 JPEG のデコードには通常 8-10ms かかるはず

**可能性**:
- 統計計算のバグ
- デコード時間が測定されていない
- 整数演算による切り捨て（μs → ms 変換時）

**確認方法**:
```rust
// src/gui_main.rs の該当箇所を確認
// デコード時間の計算ロジックをチェック
```

---

### 2. USB 通信のボトルネック（WSL2 ↔ Windows）

**仮説**:
- WSL2 の `/dev/ttyACM0` → Windows の COM4 経由でデータを取得
- WSL2 ↔ Windows 間の USB パススルーがボトルネック

**検証方法**:
- WSL2 側で直接 CLI ビューアを実行して FPS を測定
- Windows ネイティブと WSL2 経由の差を比較

**検証コマンド**:
```bash
# WSL2 側で CLI ビューア実行（比較用）
cd ~/Rust_ws/security_camera_viewer
cargo run --release --bin security_camera_viewer -- --port /dev/ttyACM0
```

---

### 3. シリアルポート設定（ボーレート・バッファ）

**仮説**:
- ボーレート設定が適切でない
- バッファサイズが小さい
- フロー制御の問題

**確認方法**:
```bash
# WSL2 側で設定確認
stty -F /dev/ttyACM0 -a
```

**期待される設定**:
- `speed 115200 baud`
- `-parenb -parodd -cmspar cs8 -hupcl -cstopb cread clocal -crtscts`
- `-ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff`

---

### 4. GUI レンダリングのボトルネック

**仮説**:
- eframe/egui のレンダリングが遅い
- Windows OpenGL ドライバーの問題
- テクスチャ更新が遅い

**検証方法**:
- GUI の更新頻度を測定
- レンダリング時間のプロファイリング

---

### 5. Spresense 側の実際の送信レート

**仮説**:
- Phase 1.5 テスト時と異なる条件
- 実際には 37.33 fps で送信していない可能性

**検証方法**:
```bash
# Spresense 側でログ確認（別ターミナル）
# /dev/ttyUSB1 でコンソール接続して性能ログを確認

picocom -b 115200 /dev/ttyUSB1

# [PERF STATS] の出力を確認
# FPS が 37.33 fps 付近か確認
```

---

## 次のステップ: デバッグ手順

### Step 1: Spresense 側の送信レート確認（最優先）

**目的**: Spresense が実際に 37.33 fps で送信しているか確認

**手順**:
```bash
# 新しいターミナルを開く
cd ~/Spr_ws/GH_wk_test/spresense/sdk

# Spresense のシリアルコンソールに接続
picocom -b 115200 /dev/ttyUSB1

# [PERF STATS] 出力を確認
# 例:
# [PERF STATS] Window: 30 frames in 1.00 sec (30.00 fps)
# または
# [PERF STATS] Window: 30 frames in 0.80 sec (37.50 fps)
```

**期待される出力**:
```
[PERF STATS] Window: 30 frames in X.XX sec (XX.XX fps)
```

**FPS が 30 fps 付近の場合**:
- ✅ Spresense 側は正常（30 fps で送信）
- ❌ PC 側のボトルネック（17.6 fps で受信）

**FPS が 17-18 fps の場合**:
- ❌ Spresense 側で問題発生
- Phase 1.5 ファームウェアが正しくフラッシュされていない可能性

---

### Step 2: WSL2 CLI ビューアで FPS 測定（USB パススルー検証）

**目的**: WSL2 ↔ Windows の USB パススルーがボトルネックか確認

**手順**:
```bash
# WSL2 側で CLI ビューア実行
cd ~/Rust_ws/security_camera_viewer

# GUI を一旦停止してから実行
cargo run --release --bin security_camera_viewer -- --port /dev/ttyACM0

# 30秒間観察し、FPS を記録
```

**期待される結果**:
- **FPS ≥ 30**: WSL2 側は正常 → Windows GUI のボトルネック
- **FPS ≈ 17-18**: WSL2 ↔ Windows の USB パススルーがボトルネック
- **FPS ≥ 30 && GUI FPS ≈ 17-18**: GUI レンダリングがボトルネック

---

### Step 3: シリアルポート設定確認

**手順**:
```bash
# WSL2 側で設定確認
stty -F /dev/ttyACM0 -a

# ボーレート確認
# 期待: speed 115200 baud

# Raw モード確認
# 期待: -parenb -parodd cs8 -hupcl -cstopb cread clocal -crtscts
```

---

### Step 4: Decode 時間測定の修正

**目的**: 実際のデコード時間を正しく測定

**確認箇所**: `src/gui_main.rs`

**修正候補**:
```rust
// デコード開始時刻
let decode_start = std::time::Instant::now();

// JPEG デコード
let image = image::load_from_memory(&jpeg_data)?;

// デコード終了時刻
let decode_elapsed = decode_start.elapsed();
let decode_time_ms = decode_elapsed.as_micros() as f32 / 1000.0;

// 統計に追加
println!("Decode time: {:.2}ms", decode_time_ms);
```

---

## 推奨される調査順序

1. **Step 1**: Spresense 側の FPS 確認（5分）
   - → **17-18 fps なら**: Spresense 側の問題（ファームウェア再フラッシュ）
   - → **30+ fps なら**: PC 側の問題（Step 2 へ）

2. **Step 2**: WSL2 CLI ビューアで FPS 測定（5分）
   - → **30+ fps なら**: GUI レンダリングの問題（Step 4 へ）
   - → **17-18 fps なら**: USB パススルーの問題（Step 3 へ）

3. **Step 3**: シリアルポート設定確認と調整（5分）

4. **Step 4**: GUI コードのプロファイリングとデコード時間測定修正（15分）

---

## 参考: Phase 1.5 での性能

**Phase 1.5 達成値**:
- Spresense VGA: **37.33 fps** (vs 9.94 fps baseline, **3.76倍改善**)
- USB 帯域: 14.30 Mbps (119.2% of 12 Mbps Full Speed)
- カメラ取得時間: 8234 μs (8.2ms)
- USB 書き込み時間: 2345 μs (2.3ms)

**今回の測定値との比較**:
- Spresense 期待値: 37.33 fps
- PC 測定値: 17.6 fps
- **損失率**: 52.9%

---

## 結論

**現状**:
- ✅ エラーなし（通信は安定）
- ✅ 解像度正確（640×480）
- ❌ FPS が目標の 58.7%（大幅に不足）
- ⚠️ Decode 時間が 0ms（測定異常）

**優先度**:
1. **最優先**: Spresense 側の実際の送信レート確認（Step 1）
2. **次点**: WSL2 CLI ビューアでの FPS 測定（Step 2）
3. **その後**: GUI コードのデバッグ（Step 4）

**次のアクション**:
- ユーザーに Step 1 を実行してもらい、Spresense 側の FPS を確認

---

**記録者**: Claude Code (Sonnet 4.5)
**作成日時**: 2025-12-31

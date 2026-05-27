# PTZ アーム ハードウェア設計 — E-Stop / Soft Limit / 配線

**バージョン**: 0.1 (PoC 前 draft)
**作成日**: 2026-05-27
**ステータス**: 🟡 **PoC 着手前 draft** — Phase 12 PoC X.2/X.4 で実装し確定
**目的**: LeRobot SO-ARM101 Pro 統合に必要なハードウェア設計 (E-Stop ハード回路 / Soft Limit 角度範囲 / 配線レイアウト) を draft として記録。
**関連**: [`../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md) (M-41 / M-43 対策)

---

## 📋 サマリ

| 項目 | 設計値 (案) | STAMP 対応 |
|---|---|---|
| E-Stop 応答時間 | **≤ 100 ms** (押下 → サーボ電源遮断) | SC-19 |
| Soft Limit (pan) | **-90° 〜 +90°** | M-41, SC-18 |
| Soft Limit (tilt) | **-45° 〜 +45°** | M-41, SC-18 |
| Soft Limit (roll/grip 等) | **使用しない** (姿勢固定) | (簡略化) |
| 配線方式 | **WiFi-only data + 電源のみ有線** | M-43, H-22 |
| 末端ペイロード上限 | ≤ 100 g (Spresense + カメラ + 配線) | (機械的) |

---

## §1 E-Stop ハード回路

### 1.1 設計目標

- **押下 → 全サーボ電源遮断 ≤ 100 ms** (SC-19)
- ソフトウェア (Python) のクラッシュ・フリーズに依存しない (ハード割込み)
- **2 重化**: 物理ボタン + ソフト割込み両方
- 復旧: リセット手順を運用ランブック (M-6 統合) に明記

### 1.2 回路構成案

```
  [12V DC 電源]
       │
       ├────[ E-Stop NC ボタン ]────┐ (常時閉、押下で開放)
       │                             │
       │                             ▼
       │                      [ ラッチング Relay ]
       │                             │
       └──────────────────────────┬──┴────► [ SO-ARM101 サーボバス電源 ]
                                  │
                                  ▼
                          [ MCU 監視 GPIO ]
                                  │
                                  ▼
                          USB-Serial → Robotics_ws (notify)
```

### 1.3 主要部品

| 部品 | 用途 | 想定価格 |
|---|---|---|
| 物理 E-Stop ボタン (NC, ラッチング, 22mm 大型) | ユーザー操作 | ¥1,500-2,500 |
| ラッチング Relay (DC 12V, 5A) | サーボ電源切断 | ¥1,000 |
| 小型 MCU or Arduino Nano | GPIO 監視 + ソフト通知 | ¥500-1,500 |
| 配線材・ターミナル | 接続 | ¥500 |

**合計**: ~¥3,500-5,500 (`PTZ_ARM_POC_PLAN.md §3.1` の概算範囲内)

### 1.4 ソフトウェア側通知

物理ボタン押下 → MCU が GPIO 監視 → USB-Serial で Robotics_ws の `tracking_controller.py` に notify → `POST /v1/estop` を内部発火 → viewer (Rust) に状態通知

**Robotics_ws の対応**:
```python
# ptz_integration/e_stop_handler.py (概念コード)
class EStopHandler:
    def on_hardware_estop_detected(self):
        # 1. 既に物理電源は切れているが、ソフト側でも arm_state を 'estop' に
        self.arm_state = "estop"
        # 2. LeRobot SDK の disable_torque() を全サーボに対し発行 (フェイルセーフ)
        for servo in self.arm.servos:
            servo.disable_torque()
        # 3. viewer に notify
        self.notify_viewer({"event": "estop", "source": "hardware"})
```

### 1.5 E-Stop の復旧手順 (運用ランブック)

1. 物理 E-Stop ボタンを **時計回りに回す** (ラッチ解除)
2. Robotics_ws の `tracking_controller.py` を再起動
3. ホーム位置確認 (`POST /v1/home`)
4. 通常運用開始

---

## §2 Soft Limit 角度範囲

### 2.1 サーボ別可動範囲 (SO-ARM101 Pro 仕様 + 安全マージン)

| サーボ ID | 物理可動範囲 | **Soft Limit (運用範囲)** | 用途 | マージン理由 |
|---|---|---|---|---|
| 1 (Base / Pan) | ±180° | **±90°** | カメラ pan | 配線捻れ防止、視野範囲十分 |
| 2 (Shoulder / Tilt) | -90°〜+90° | **±45°** | カメラ tilt | 重力影響軽減、機械的安定 |
| 3 (Elbow) | ±90° | **固定 0°** (ロック) | 不使用 (PTZ では伸ばさない) | カメラ姿勢固定 |
| 4 (Wrist 1) | ±90° | **固定 0°** | 不使用 | 同上 |
| 5 (Wrist 2 / Roll) | ±90° | **固定 0°** | 不使用 | 同上 |
| 6 (Gripper) | 0°〜90° | **固定 30°** (軽くカメラを把持) | カメラ固定治具側 | 末端固定 |

### 2.2 PTZ では 2 軸のみ動作

SO-ARM101 Pro は本来 6 DoF のロボットアームだが、PTZ 用途では **pan + tilt の 2 軸のみ動作** させ、残り 4 軸は固定。これにより:
- 機械的衝突リスク大幅低減
- 制御アルゴリズム単純化 (2 軸 PID のみ)
- サーボ過負荷リスク減
- 配線捻れリスク減

### 2.3 Soft Limit の実装場所

| レイヤ | 役割 |
|---|---|
| **viewer (Rust)** | 偏差算出時に角度オフセットを clamp |
| **Robotics_ws** (Python) | `POST /v1/track` 受信時に再 clamp + 越境ログ |
| **LeRobot SDK** | サーボ指令直前で最終 clamp |

→ **3 重チェック**で UCA-ARM.4 (範囲外指令によるサーボ過負荷) を防止。

### 2.4 Soft Limit 越境時の挙動

1. clamp して動作 (継続)
2. `POST /v1/track` レスポンスに `soft_limit_clamped` を含める ([`../interface/PTZ_ARM_PROTOCOL_SPEC.md §2.1`](../interface/PTZ_ARM_PROTOCOL_SPEC.md#21-post-v1track--追尾指令-高頻度-10-hz))
3. 5 秒間連続越境で warning → M-44 (ロスト時挙動) と同様 中央復帰
4. 10 秒連続越境で **E-Stop 自動発火** (UCA-ARM.4 緩和)

---

## §3 配線レイアウト

### 3.1 配線方針 (M-43 対応)

| データ種別 | 経路 | 理由 |
|---|---|---|
| **映像 + metrics** | **WiFi (無線)** | 6 DoF 動作で USB ケーブル捻れリスク回避 |
| **サーボ指令** | USB-RS485 (PC → アーム base 部分のみ、アーム末端には届かない) | 末端への配線無し |
| **電源 (5V, Spresense 用)** | **末端へ有線** (細い 2 芯シールドケーブル) | バッテリ重量回避 |
| (将来) **電源 (バッテリ式)** | 末端搭載小型 LiPo | 完全無線化、ペイロード余裕次第 |

### 3.2 配線レイアウト図

```
         ┌─────────────────────────┐
         │  末端搭載物              │
         │  ┌─────────────────┐   │
         │  │ Spresense + カメラ │   │
         │  │ WiFi 通信         │   │
         │  └─────────────────┘   │
         │        ▲                 │
         │        │ 5V (細線, ~1m)   │
         └────────┼─────────────────┘
                  │ (関節 1-3 を経由)
                  │ + ケーブルチェーン
                  │ + ZipTie 固定
         ┌────────┴─────────────────┐
         │  アーム base                │
         │  USB-Power Adapter         │
         │  └─→ PC USB                │
         │  USB-RS485 ── Servo Bus    │
         └────────────────────────────┘
                       │
                       ▼
                  [ PC + LeRobot SDK ]
```

### 3.3 配線材選定

| 配線 | 仕様 | 理由 |
|---|---|---|
| 電源ケーブル | **AWG 26 シールド付き 2 芯, 長さ ~1.5 m** | 細さ重視、6 DoF 屈曲に耐える |
| ケーブルチェーン | **15×10mm 開閉式 (関節 1-3 を経由)** | 捻れ・破断防止 |
| ZipTie | **小型 (関節間に 3 cm 間隔で配置)** | 動作中のケーブル暴れ防止 |
| 保護スリーブ | **編組チューブ (Φ5mm)** | 摩擦防止 |

### 3.4 24h 連続動作試験 (Phase X.2 Go ゲート)

| 試験項目 | 合格基準 |
|---|---|
| 配線断線・接触不良の発生数 | 0 |
| WiFi RSSI 変動 (動作中 ↔ 静止時) | ≤ 10 dBm |
| 電源電圧降下 (末端 Spresense 入力電圧) | ≤ 0.3 V |
| 末端温度上昇 (Spresense ボード周辺) | ≤ 50 °C |

---

## §4 STAMP/STPA との対応

| HW 設計要素 | 対応 UCA / Hazard / SC |
|---|---|
| 物理 E-Stop ボタン (ラッチング, NC) | **SC-19** (E-Stop ≤ 100 ms), UCA-MC.4 緩和 |
| Soft Limit (3 重 clamp) | **SC-18** (動作領域制限), UCA-ARM.4 (過負荷) 防止 |
| 4 軸固定 (PTZ のみ 2 軸) | **H-19** (想定外領域進入) 大幅低減 |
| WiFi-only data + 細線電源 | **M-43** (配線設計), H-22 (配線断線) 緩和 |
| ケーブルチェーン + ZipTie | UCA-MC 系 (通信切断) 物理レイヤ防止 |
| 24h 連続動作試験 | Phase X.2 Go ゲート, **PoC 成功基準** |

---

## §5 関連文書

- 親 STAMP draft: [`../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md)
- 通信プロトコル: [`../interface/PTZ_ARM_PROTOCOL_SPEC.md`](../interface/PTZ_ARM_PROTOCOL_SPEC.md)
- PoC 計画: [`../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md`](../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md)

## 改訂履歴

| Version | Date | 変更内容 |
|---|---|---|
| 0.1 | 2026-05-27 | 初版 (PoC 前 draft) — E-Stop ハード / Soft Limit 角度 / 配線レイアウト |

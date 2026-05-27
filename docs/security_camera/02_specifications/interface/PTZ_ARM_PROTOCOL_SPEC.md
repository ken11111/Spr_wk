# PTZ アーム連携プロトコル仕様 — PC viewer (Rust) ↔ Robotics_ws (Python)

**バージョン**: 0.1 (PoC 前 draft)
**作成日**: 2026-05-27
**ステータス**: 🟡 **PoC 着手前 draft** — Phase 12 PoC で実装し詳細を確定
**目的**: PC viewer (Rust, `~/Rust_ws/security_camera_viewer/`) と Robotics_ws の PTZ 制御プロセス (Python, `~/Robotics_ws/ptz_integration/tracking_controller.py`) 間の連携プロトコルを定義する。
**関連**: [`../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md) / [`../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md`](../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md)

---

## 📋 サマリ

### 通信方式の選択

| 方式 | 採用判定 | 理由 |
|---|---|---|
| **HTTP (localhost:8901)** ★推奨 | ✅ | シンプル、デバッグ容易、別プロセス独立性 |
| WebSocket | 🟡 候補 | 双方向リアルタイム性。HTTP で十分なら不要 |
| Unix Domain Socket | 🟡 候補 | HTTP より高速だが Windows 非対応 |
| stdin/stdout JSON-RPC | ❌ 不採用 | エラー処理複雑、プロセス管理難 |
| gRPC | ❌ 不採用 | overkill、Schema 管理コスト > 利得 |

**採用**: **HTTP POST + JSON** (localhost:8901)。E2E 遅延への影響 < 5ms と推定。

### エンドポイント一覧

| Method | Path | 用途 |
|---|---|---|
| POST | `/v1/track` | bbox → 追尾指令 (頻度: 〜10 Hz) |
| GET | `/v1/status` | アーム状態取得 (pan/tilt 角度, 動作状態) |
| POST | `/v1/estop` | E-Stop 発火 (緊急停止) |
| POST | `/v1/home` | ホーム位置 (中央) へ復帰 |
| GET | `/v1/health` | ヘルスチェック |

---

## §1 アーキテクチャ概要

```
[ PC viewer (Rust, Tokio) ]                [ Robotics_ws (Python, FastAPI) ]
        │                                          │
        │ motion_detector → bbox                  │
        │                                          │
        │   HTTP POST /v1/track                    │
        ├─────────── localhost:8901 ──────────────►│
        │   {"cx":340,"cy":180,"w":40,"h":120,...} │
        │                                          │
        │◄────────── HTTP 200 (JSON ack) ──────────┤
        │                                          │
        │                                          │ LeRobot SDK
        │                                          ▼
        │                                  [ SO-ARM101 Pro ]
        │   定期的に GET /v1/status                │
        ├─────────── localhost:8901 ──────────────►│
        │◄────────── pan/tilt 角度 ────────────────┤
        │                                          │
        │ M-39 OSD 重畳に使用                      │
```

### プロセス起動順序

1. `~/Robotics_ws/ptz_integration/tracking_controller.py` を **先に起動** (HTTP サーバ待ち受け)
2. PC viewer (Rust) を後から起動、`POST /v1/health` で疎通確認
3. viewer が motion 検知 → `POST /v1/track`
4. 終了時: viewer が `POST /v1/estop` 後 Robotics_ws プロセス停止

---

## §2 エンドポイント詳細

### 2.1 `POST /v1/track` — 追尾指令 (高頻度, ~10 Hz)

**Request**:
```http
POST /v1/track HTTP/1.1
Host: localhost:8901
Content-Type: application/json

{
  "timestamp_us": 1748345678901234,
  "frame_id": 12345,
  "bbox": {
    "cx": 340.5,
    "cy": 180.2,
    "w": 40.0,
    "h": 120.0,
    "confidence": 0.87
  },
  "image_size": {"width": 640, "height": 480},
  "lost": false
}
```

| Field | Type | 説明 |
|---|---|---|
| `timestamp_us` | u64 | capture 時刻 (Unix microseconds) |
| `frame_id` | u32 | フレーム連番 (E2E trace 用) |
| `bbox.cx, cy` | f32 | 検知 bbox の中心 (pixel) |
| `bbox.w, h` | f32 | 検知 bbox の幅・高さ (pixel) |
| `bbox.confidence` | f32 | 0.0〜1.0 (将来 ML 連携用、現状は固定値) |
| `image_size` | obj | 画像解像度 (VGA = 640×480) |
| `lost` | bool | 追尾対象ロスト時に true (UCA-ARM.3 対応, M-44) |

**Response (成功)**:
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "status": "ok",
  "frame_id": 12345,
  "command": {
    "pan_delta_deg": +2.5,
    "tilt_delta_deg": -1.3,
    "issued_at_us": 1748345678905000
  },
  "arm_state": "tracking"
}
```

**Response (Soft Limit 越境時)**:
```http
HTTP/1.1 200 OK

{
  "status": "soft_limit_clamped",
  "frame_id": 12345,
  "command": {
    "pan_delta_deg": +5.0,
    "pan_clamped_to_deg": 90.0,
    "tilt_delta_deg": -1.3
  },
  "arm_state": "tracking"
}
```

**Response (E-Stop 中)**:
```http
HTTP/1.1 409 Conflict

{
  "status": "estop_active",
  "frame_id": 12345
}
```

### 2.2 `GET /v1/status` — アーム状態取得 (低頻度, ~1 Hz)

**Request**:
```http
GET /v1/status HTTP/1.1
Host: localhost:8901
```

**Response**:
```http
HTTP/1.1 200 OK
Content-Type: application/json

{
  "arm_state": "tracking",
  "pan_deg": 32.5,
  "tilt_deg": -15.2,
  "soft_limit": {
    "pan_min_deg": -90.0,
    "pan_max_deg": +90.0,
    "tilt_min_deg": -45.0,
    "tilt_max_deg": +45.0
  },
  "servo_status": {
    "current_ma": [350, 280, 0, 0, 0, 0],
    "torque_enabled": true,
    "temperature_c": [42, 41, 25, 25, 25, 25]
  },
  "last_command_ms_ago": 105,
  "estop_active": false,
  "warnings": []
}
```

| Field | 用途 |
|---|---|
| `arm_state` | `idle` / `tracking` / `homing` / `estop` / `error` |
| `pan_deg`, `tilt_deg` | M-39 OSD 重畳に使用 |
| `servo_status.current_ma` | UCA-MC.2 (race condition) / UCA-ARM.4 (過負荷) 検知 |
| `last_command_ms_ago` | UCA-MC.1 (通信失敗) 検知 = 1000ms 超で異常 |

### 2.3 `POST /v1/estop` — E-Stop 発火

**Request**:
```http
POST /v1/estop HTTP/1.1
Host: localhost:8901
Content-Type: application/json

{
  "reason": "manual_button" 
}
```

`reason`: `manual_button` / `soft_limit_violation` / `lost_too_long` / `viewer_shutdown` 等

**Response**:
```http
HTTP/1.1 200 OK

{
  "status": "estop_issued",
  "servo_power_cut_at_us": 1748345679001234
}
```

**SLA**: 受信 → 全サーボ電源遮断完了 **≤ 100 ms** (SC-19)

### 2.4 `POST /v1/home` — ホーム位置復帰

**Request**:
```http
POST /v1/home HTTP/1.1
```

**Response**: 完了まで同期 (2-5 秒)

### 2.5 `GET /v1/health` — ヘルスチェック

**Request**:
```http
GET /v1/health HTTP/1.1
```

**Response**:
```http
HTTP/1.1 200 OK

{
  "status": "alive",
  "lerobot_sdk_version": "0.x.y",
  "servo_bus_connected": true,
  "uptime_seconds": 1234
}
```

起動疎通確認、watchdog 用。viewer 側は **5 秒に 1 回 health check** することを推奨 (UCA-MC.4 サーボ クラッシュ検知)。

---

## §3 エラーコード

| HTTP Code | Status | 原因 | viewer 側対応 |
|---|---|---|---|
| 200 | `ok` | 正常受理 | continue |
| 200 | `soft_limit_clamped` | 指令 clamped、動作は実行 | continue + warning log |
| 400 | `bad_request` | JSON 不正 | error log, 次フレームへ |
| 409 | `estop_active` | E-Stop 中で追尾不能 | tracking 停止, GUI に E-Stop 表示 |
| 500 | `internal_error` | Robotics_ws 側エラー | 5 sec 後 retry |
| 503 | `servo_disconnected` | サーボバス断 | error log, 再接続待ち |
| (Connection refused) | — | Robotics_ws プロセス停止 | viewer 起動失敗 or 動作モード切替 |

---

## §4 STAMP/STPA との対応

| プロトコル要素 | 対応 UCA / SC |
|---|---|
| `POST /v1/estop` SLA ≤ 100 ms | **SC-19** (E-Stop 100 ms 応答) |
| `GET /v1/status.warnings[]` | UCA-MC.1 / UCA-MC.4 検知 |
| `soft_limit_clamped` レスポンス | **M-41 (Soft Limit) の通知メカニズム** |
| `lost: true` フィールド | UCA-ARM.3 (ロスト時挙動) → M-44 トリガ |
| `GET /v1/status.servo_status.current_ma` | UCA-ARM.4 (過負荷) 早期検知 |
| `last_command_ms_ago` > 1000ms | UCA-MC.1 (コア間通信失敗 = 本ケースでは IPC 失敗) |

---

## §5 セキュリティ考慮

| 観点 | 対応 |
|---|---|
| **アクセス制御** | `localhost:8901` のみ bind (LAN 公開しない) |
| **認証** | なし (localhost 限定なので不要、PoC 段階) — 本格運用時は token-based 検討 |
| **暗号化** | TLS なし (localhost 通信のため) |
| **STAMP-Sec 対応** | 既存 UCA-AUTH.1 とは別レイヤ (本プロトコルは PC 内 IPC) |

---

## §6 性能要件

| 指標 | 目標 |
|---|---|
| `POST /v1/track` 1 リクエストの RTT (ローカルループバック) | **≤ 5 ms** |
| `POST /v1/track` 受理 → サーボ動作完了 | ≤ 150 ms (含む LeRobot SDK + Feetech バス通信) |
| **E2E 遅延** (capture → 物理移動完了) | **≤ 200 ms** (PoC KPI) |
| 連続 24h 動作で接続断 | 0 |

---

## §7 関連文書

- 親 STAMP draft: [`../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md`](../quality/safety_analysis/STAMP_STPA_PTZ_ARM_REFERENCE.md)
- PoC 計画書: [`../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md`](../../05_future_actions/phase_planned/PTZ_ARM_POC_PLAN.md)
- E-Stop ハード設計: [`../architecture/PTZ_ARM_HW_DESIGN.md`](../architecture/PTZ_ARM_HW_DESIGN.md) (新規, v0.1)
- 既存通信 IF 仕様: 他 13 件 (本ディレクトリ)

## 改訂履歴

| Version | Date | 変更内容 |
|---|---|---|
| 0.1 | 2026-05-27 | 初版 (PoC 前 draft) — HTTP/JSON + 5 endpoint + STAMP 対応マッピング |

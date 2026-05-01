# Cross-cutting Concerns (横断的関心事) — arc42 §8

**バージョン**: 1.0
**作成日**: 2026-05-01
**目的**: コードベース全体に横断する関心事 (ロギング・エラー処理・設定管理・国際化) の現状方針を明文化し、不整合と改善余地を識別する
**位置付け**: P1-B タスク (PENDING_NFR_WORK.md)

> **方針**: 既存実装の **事実 (IS)** を記述してから、**改善案 (TO-BE)** を別 section で扱う。設計と実装の乖離を作らないため、現状を否定的に書かない。

---

## §1 ロギング (Logging)

### §1.1 現状実装 (IS)

#### マクロ定義 (`apps/examples/security_camera/config.h:208-225`)

| マクロ | syslog level | 値 | プレフィクス |
|---|---|---|---|
| `LOG_DEBUG` | LOG_DEBUG | 7 | `[CAM]` |
| `LOG_INFO` | LOG_INFO | 6 | `[CAM]` |
| `LOG_WARN` | LOG_WARNING | 4 | `[CAM]` |
| `LOG_ERROR` | LOG_ERR | 3 | `[CAM]` |

**特性**:
- すべて `syslog(level, fmt, ...)` の薄いラッパ
- `CONFIG_DEBUG_ENABLE=1` でゲート: 0 にすると **LOG_ERROR 以外は無効化**
- フォーマット: `[CAM] <message>\n` (改行は LOG マクロ側で付与)

#### 並存する 2 系統のスタイル

| スタイル | 使用箇所 | 例 |
|---|---|---|
| **`LOG_*` (アプリ層)** | camera_threads.c / camera_manager.c / fps_controller.c / mjpeg_protocol.c 等 | `LOG_INFO("Camera thread started")` |
| **`_err / _warn / _info / _debug` (NuttX std)** | tcp_server.c (ネットワーク層) | `_err("ERROR: Failed to bind: %d\n", errno)` |
| **`syslog()` 直接** | 一部の初期化コード | `syslog(LOG_INFO, ...)` |

**観察**: スタイルが**3 系統混在**。tcp_server.c が NuttX kernel ログ慣例に従っているのに対し、アプリ層は独自 `LOG_*` を使用。

#### CONFIG_LOG_LEVEL の扱い

- `config.h:181` で `CONFIG_LOG_LEVEL = LOG_INFO` 定義
- ただし **マクロ展開上は使われていない** (LOG_DEBUG/INFO/WARN/ERROR 全部が CONFIG_DEBUG_ENABLE で同時 on/off)
- 実質的にレベル別フィルタは syslog 側 (`setlogmask`) でしか行えない
- ⚠ **コメントは "LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR" を選べると示唆するが、実装と乖離**

### §1.2 ログ出力先

- 出力: **NuttX syslog** → 標準シリアル (`/dev/console` 相当)
- 永続化: なし (RAM 上のリングバッファのみ, 再起動で消失)
- ローテーション: なし
- フォーマット: タイムスタンプ無し (NuttX syslog 設定依存)

### §1.3 改善案 (TO-BE)

| 改善 | 優先度 | 規模 |
|---|---|---|
| `_err/_warn/_info` を `LOG_*` に統一 (tcp_server.c) | 中 | 小 (~30 箇所置換) |
| `CONFIG_LOG_LEVEL` でビルド時レベル制御を実装 (`CONFIG_LOG_LEVEL <= LOG_INFO` 等の `#if`) | 中 | 小 |
| タイムスタンプ自動付与 (`LOG_INFO` 内で `clock_gettime` 呼出) | 低 | 小 |
| ファイルログ (SD カード) 出力オプション | 低 | 中 (Phase 13+) |

---

## §2 エラー処理 (Error Handling)

### §2.1 現状実装 (IS)

#### エラーコード方式の混在

**(A) 負の errno 返却 (POSIX/NuttX 慣例)** — 主にネットワーク層:

```c
/* tcp_server.c:75 */
return -errno;
```

**(B) カスタム ERR_* コード (`config.h:185-202`)** — 主にプロトコル/エンコーダ層:

```
#define ERR_OK                  0
#define ERR_CAMERA_INIT        -1
#define ERR_CAMERA_OPEN        -2
#define ERR_CAMERA_CONFIG      -3
#define ERR_CAMERA_CAPTURE     -4
#define ERR_ENCODER_INIT       -5  ← H.264 path 用 (dead code)
...
#define ERR_USB_DISCONNECTED   -12
#define ERR_PROTOCOL_INVALID   -13
#define ERR_NOMEM              -14
#define ERR_TIMEOUT            -15
```

**使用状況** (grep ベース):
- `ERR_OK`: 4 箇所 (encoder_manager / protocol_handler)
- `ERR_PROTOCOL_INVALID`: 2 箇所 (protocol_handler)
- `ERR_NOMEM`: 2 箇所 (encoder_manager)
- `ERR_TIMEOUT`: 1 箇所 (camera_threads から protocol_handler 呼出時)
- 他の `ERR_CAMERA_*` `ERR_USB_*` `ERR_ENCODER_*` は **定義されているが使われていない**

⚠ **乖離**: ERR_* コードはアプリ全体で使う設計だが、実装は **大半が `-errno` 直接返却**または整数 `-1` 直接返却。

#### リトライ戦略

| 場所 | パラメータ | 回数 | 間隔 |
|---|---|---|---|
| TCP 再接続 | `TCP_RECONNECT_MAX = 5` (tcp_server.c) | 最大 5 回 | exp backoff: 1s + N×2s |
| USB 接続初期化 | ハードコード | 最大 10 回 | 100 ms |
| USB write リトライ | `CONFIG_MAX_RECONNECT_RETRY = 3` (config.h:117) | 最大 3 回 | `CONFIG_RECONNECT_DELAY_MS = 1000` |

⚠ **不整合**: 同じ `MAX_RECONNECT` 名で **TCP は 5 回、USB は 3 回**。設定値の意味が経路ごとに違うが命名で区別できていない。

#### リソース解放パターン

- 全初期化は `*_init()` で対称的に `*_cleanup()` がある (camera_manager / wifi_manager / encoder_manager / frame_queue)
- 失敗時の解放は **手動でラダー解放** (`camera_app_main.c:218-281`):

```c
ret = camera_init();
if (ret < 0) goto err_camera;

ret = wifi_init();
if (ret < 0) goto err_wifi;
...

err_wifi:
  wifi_manager_cleanup(&wifi_mgr);
err_camera:
  camera_manager_cleanup();
```

→ **goto-based cleanup ladder** (Linux カーネル流, NuttX で標準)

#### シャットダウン処理

- グローバル `g_shutdown_requested` フラグ (camera_threads.c:107 周辺)
- main task が SIGINT/SIGTERM 受信で立てる (`camera_app_main.c:101`)
- 各スレッドはループ内で確認して exit

### §2.2 改善案 (TO-BE)

| 改善 | 優先度 | 規模 |
|---|---|---|
| ERR_* コードを廃止 or 全面統一 (現状の半端な使用は混乱の元) | 中 | 中 |
| TCP/USB リトライパラメータの命名整理 (`TCP_RECONNECT_MAX` vs `CONFIG_MAX_RECONNECT_RETRY`) | 中 | 小 |
| エラーログのコンテキスト付与 (現状 `errno` 値だけ、stack trace なし) | 低 | 中 |
| sigaction 経由の graceful shutdown を文書化 | 低 | 小 |

---

## §3 設定管理 (Configuration)

### §3.1 現状実装 (IS)

#### 3 つの設定ファイルの分散

| ファイル | 役割 | エントリ数 (#define 概算) |
|---|---|---|
| `config.h` | カメラ / エンコーダ / プロトコル / USB / Phase 10/11 制御 / デバッグ / エラーコード | ~50 |
| `wifi_config.h` | WiFi SSID / Password / TCP Port / DHCP 設定 | ~10 |
| `mjpeg_protocol.h` | sync_word / batch size / packet サイズ定数 | ~15 |

#### config.h セクション構成 (例)

```
/* Camera Configuration */         CONFIG_CAMERA_FORMAT, _WIDTH, _HEIGHT, _FPS, _HDR_ENABLE
/* Encoder Configuration */        CONFIG_ENCODER_CODEC, _BITRATE, _GOP_SIZE (dead code)
/* Protocol Configuration */       CONFIG_PACKET_MAGIC, _VERSION, _MAX_PAYLOAD_SIZE
/* USB Configuration */            CONFIG_USB_DEVICE_PATH, _TX_BUFFER_*, _WRITE_TIMEOUT_MS
/* Application Configuration */    CONFIG_MAX_RECONNECT_RETRY, _DELAY_MS
/* Phase 10: Adaptive Buffer */    CONFIG_QUEUE_DEPTH_MIN/MAX/DEFAULT (5/9/7)
/* Phase 11: Enhanced Control */   CONFIG_PHASE11_ENABLE, _CONTROL_WEIGHT_*, _ADAPTIVE_PID_*, _PREDICTION_*
/* Memory Limits */                CONFIG_PHASE11_MAX_MEMORY_KB
/* Debug Configuration */          CONFIG_DEBUG_ENABLE, _LOG_LEVEL
/* Error Codes */                  ERR_* 系列
```

**観察**: config.h は **機能横断の設定ハブ** として機能している。Phase 10/11 制御パラメータがすべてここに集約されているのは良い設計。

#### 機微情報のハードコード ⚠

**`wifi_config.h:17-18`**:
```c
#define WIFI_SSID        "DESKTOP-GPU979R"
#define WIFI_PASSWORD    "B54p3530"
```

→ **ソース管理に WiFi 認証情報がハードコード**されている。.gitignore 化されておらず、リポジトリに公開されている可能性 (要確認)。

#### 動的 (実行時) 設定

- 動的設定インタフェース: ほぼ無し (再ビルドが必要)
- 唯一: `camera_set_fps_runtime()` (Phase 10) で FPS のみ動的変更可能 (control_thread から呼出)

### §3.2 改善案 (TO-BE)

| 改善 | 優先度 | 規模 |
|---|---|---|
| WiFi 認証情報をリポジトリから分離 (wifi_config.h.example + .gitignore) | **高** | 小 |
| 設定の名前空間整理 (`CONFIG_CAMERA_*`, `CONFIG_USB_*` 等のプレフィクス強化) | 中 | 小 |
| Phase 11 パラメータを `enhanced_control_*.h` 側に移動 (関心の分離) | 中 | 小 |
| ランタイム設定 (UART CLI or metrics packet 経由) | 低 | 大 (Phase 13+) |

---

## §4 国際化 (i18n)

### §4.1 現状実装 (IS)

| 領域 | 現状 |
|---|---|
| Spresense ログメッセージ | **英語** (例: "Camera thread started", "Failed to bind to port") |
| Spresense コメント | **日本語混在** (`/* 撮影完了 */` 等) |
| PC viewer (Rust) ログ | **日本語混在** ("接続失敗" 等を含む) |
| GUI 表示 | **日本語** (egui のラベル等, 確認要) |
| ドキュメント | **日本語** (本セッション含む) |

### §4.2 観察と判断

- 防犯カメラとして主要顧客が日本語話者である前提なら現状で問題なし
- 国際化対応は要求書 v1.0 で明示的に **未定義** ⚪ (Q21 PC環境は OS の話のみ)
- 多言語化が必要になるとしたら GUI 部分のみ (PC 側 Rust)

### §4.3 改善案

国際化は**現状要求外**のため Phase 12 では対応不要。要求が発生したら:
- PC viewer に `fluent-rs` または `gettext` 導入
- Spresense 側ログは英語で統一 (現状ほぼ英語で問題なし)

---

## §5 横断的関心事の影響マトリクス

各品質属性への影響:

| 関心事 | 影響する品質属性 | 現状の課題 |
|---|---|---|
| ロギング | 保守性 (解析性), 信頼性 | スタイル混在、レベル制御未実装 |
| エラー処理 | 信頼性, 保守性 | ERR_* と -errno の混在 |
| 設定管理 | 保守性 (修正性), セキュリティ | WiFi 認証情報のハードコード ⚠ |
| 国際化 | 使用性, 互換性 | 現状要求外 (将来課題) |

---

## §6 結論と次のアクション

### 緊急度高

1. **WiFi 認証情報のリポジトリ分離** (§3 改善案 — セキュリティ問題)
   - `wifi_config.h.example` を新設、実体は .gitignore 化
   - 既にコミット済の場合は git history のサニタイズも検討

### 緊急度中 (Phase 12 序盤)

2. ロギングスタイルの統一 (`_err` → `LOG_ERROR` 置換) — §1 改善案
3. ERR_* コードの統一 or 廃止判断 — §2 改善案
4. リトライパラメータ命名整理 (TCP_RECONNECT_MAX vs CONFIG_MAX_RECONNECT_RETRY) — §2 改善案

### 緊急度低 (Phase 13+)

5. ファイルログ (SD カード)
6. ランタイム設定 (CLI/metrics packet 経由)
7. 国際化 (要求発生時のみ)

---

## 関連文書

- 品質要求集約: [`QUALITY_REQUIREMENTS.md`](QUALITY_REQUIREMENTS.md) §7 (保守性), §6 (セキュリティ)
- セキュリティ乖離: [`SECURITY_GAP_ANALYSIS.md`](SECURITY_GAP_ANALYSIS.md) (§3 設定管理の WiFi 認証情報問題と関連)
- 用語集: [`GLOSSARY.md`](GLOSSARY.md)
- 残タスク: [`PENDING_NFR_WORK.md`](PENDING_NFR_WORK.md)
- 実装根拠:
  - `apps/examples/security_camera/config.h` (LOG_*, ERR_*, CONFIG_*)
  - `apps/examples/security_camera/wifi_config.h` (WIFI_*)
  - `apps/examples/security_camera/tcp_server.c` (`_err` style + 再接続)
  - `apps/examples/security_camera/camera_threads.c` (`LOG_*` style + cleanup ladder)

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-01 | 初版。ロギング (3 スタイル混在発見) / エラー処理 (ERR_* と -errno 混在) / 設定管理 (WiFi 認証情報ハードコード問題発見) / 国際化方針を IS → TO-BE で整理 |

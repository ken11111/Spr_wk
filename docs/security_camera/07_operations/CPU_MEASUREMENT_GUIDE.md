# CPU 利用率 実測ガイド (X-6 P1-A 実測手段)

**バージョン**: 1.0
**作成日**: 2026-05-03
**対象**: 本セッション (2026-05-01〜03) で識別された CPU 利用率の **TBD** 項目を埋める実測手順
**前提**: Spresense 物理ボード + シリアルコンソール + PC viewer

> **本ガイドの位置付け**:
> - [`../02_specifications/quality/CPU_BANDWIDTH_BUDGET.md`](../02_specifications/quality/CPU_BANDWIDTH_BUDGET.md) §2 の推定 CPU% (TBD 項目) を実測値で埋めるため
> - 計測コードは本セッションで `apps/examples/security_camera/perf_logger.{h,c}` に実装済 (X-6 commit)
> - **本ガイドはハードウェアでの実行手順** — code 側は build できる状態

---

## §1 計測アーキテクチャ概要

### 採用方式: clock_gettime(CLOCK_THREAD_CPUTIME_ID) ベース

NuttX には 2 つの CPU 利用率計測手段がある:

| 方式 | 利点 | 欠点 |
|---|---|---|
| **CONFIG_SCHED_CPULOAD** + `top` | システム全体を 1 コマンドで | NuttX shell 介入が必要 / 現状 `=n` |
| **clock_gettime(CLOCK_THREAD_CPUTIME_ID)** | アプリ側で完結、syslog に出力 | アプリコード変更が必要 |

**本実装は後者** (アプリ側完結) を採用。理由:
- CONFIG_SCHED_CPULOAD は現状 OFF (`spresense/nuttx/.config:674`) で、submodule 変更が伴う
- シリアル経由の syslog ログから後処理できる方が運用上便利
- `CONFIG_FS_PROCFS=y` は既に有効なので `top` も併用可能 (cross-validation 用)

### 計測点

`perf_logger.{h,c}` に追加された API:

```c
typedef struct perf_thread_cpu_s {
  const char *name;          /* "camera" / "usb" / "control" */
  uint64_t   prev_cpu_us;    /* CPU time 前回値 */
  uint64_t   prev_wall_us;   /* wall time 前回値 */
  uint32_t   sample_count;
  float      cpu_percent;    /* 直近サンプルの CPU% */
  float      avg_percent;    /* 累計平均 */
  float      max_percent;    /* ピーク値 */
} perf_thread_cpu_t;

void perf_thread_cpu_init(perf_thread_cpu_t *stats, const char *name);
void perf_thread_cpu_sample(perf_thread_cpu_t *stats);
void perf_thread_cpu_log(const perf_thread_cpu_t *stats);
```

各スレッドは自身の CPU 時間を `clock_gettime(CLOCK_THREAD_CPUTIME_ID)` で取得し、wall time との比から CPU% を計算する。

### 計測点の埋込状況 (`camera_threads.c`)

| スレッド | サンプリング周期 | 実装済み箇所 |
|---|---|---|
| `camera_thread_func` | 30 frames (~1 sec @ 30fps) | frame stats 出力時に追加 |
| `usb_thread_func` | 30 frames (~1 sec @ 30fps) | USB stats 出力時に追加 |
| `control_thread_func` (Phase 10 PID) | 10 cycles (~1 sec @ 10Hz) | control loop 先頭に追加 |
| `gs2200m driver task` (kernel) | — | **計測対象外** (kernel 空間, アプリから干渉できない) |

---

## §2 ビルドと書き込み手順

### Step 0: 前提環境の確認

```bash
# Spresense SDK 環境
cd /home/ken/Spr_ws/GH_wk_test
ls spresense/                  # submodule 取得済を確認
which spresense-flash.sh || echo "spresense-flash.sh が PATH にない"
```

### Step 1: 通常通りビルド

`perf_logger.{h,c}` の変更は本セッションで commit 済。Makefile は変更不要。

```bash
cd spresense/sdk
./tools/config.py examples/security_camera   # 設定 (既存)
make
```

ビルドが通れば `nuttx.spk` が生成される。

### Step 2: フラッシュ書込

```bash
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### Step 3: シリアルコンソール起動

```bash
# Linux/WSL
screen /dev/ttyUSB0 115200
# または
minicom -D /dev/ttyUSB0 -b 115200
```

電源投入後、camera_thread / usb_thread / control_thread が起動し、syslog に `[CPU] <name>: cur=X.X% ...` 行が出力されることを確認。

---

## §3 計測実施手順

### Scenario A: ベースライン (通常運用)

1. PC viewer を起動 (TCP 経由)
2. 約 60 秒間ストリーミングを継続 (60 サンプル取得)
3. シリアルログを保存:
   ```bash
   screen -L /dev/ttyUSB0 115200    # screenlog.0 に保存
   # または
   minicom -C measurement_baseline.log -D /dev/ttyUSB0
   ```
4. 終了後、PC viewer を停止 → Spresense を Ctrl+A k (screen) で停止

### Scenario B: 構造的天井 #1 顕在化条件

1. PC viewer を起動した状態で、別端末から大量の WiFi トラフィックを発生 (例: `iperf3 -c <AP_IP>`)
2. または PC viewer 側で連続録画を有効化し負荷を上げる
3. 60 秒間ログ取得
4. `tcp_health_moving_avg_ms` が 1000ms を超える時間帯を観察

### Scenario C: control_thread のみ稼働 (camera_thread / usb_thread を一時無効化)

PID 制御単独の CPU 負荷を測定する場合の参考シナリオ。コードを一時的に変更して camera/usb thread を sleep ループにすれば取得可能。

---

## §4 ログ集計

PC 側で `scripts/cpu_measurement/parse_cpu_log.py` を使う:

```bash
cd /home/ken/Spr_ws/GH_wk_test
python3 scripts/cpu_measurement/parse_cpu_log.py < measurement_baseline.log
```

出力例:

```
=== CPU 利用率サマリ ===
camera : cur 平均=42.3% / 最大=58.1% (n=58)
usb    : cur 平均=27.8% / 最大=49.5% (n=58)
control: cur 平均= 0.4% / 最大= 1.2% (n=58)
合計    : 平均=70.5% / 最大=108.8%

[備考]
- 単一コア (CONFIG_SMP=n) のため合計が ~100% を上限とする
- 100% 超は計測ジッタ (sample 跨ぎでスケジュール時間が CPU 時間にカウント)
- gs2200m driver task (kernel) は本計測対象外、SPRESENSE_TCP_CONSTRAINTS §X で別途
```

---

## §5 計測結果の文書反映

実測完了後、以下を更新:

1. **CPU_BANDWIDTH_BUDGET.md §2**: TBD 推定値 (30-50% 等) を実測値に置換
2. **QUALITY_REQUIREMENTS.md §2.2**: 「CPU 利用率 (per core): TBD」を実測値ベースに更新
3. **PENDING_NFR_WORK.md X-6**: 「実測未済」→「実測済 (YYYY-MM-DD)」に変更
4. 必要なら **タイミング図 T-1, T-2 の推定値** (例: T-2 で `~5ms / 100ms` と書いた箇所) を実測値で置換

---

## §6 トラブルシューティング

### syslog に `[CPU]` 行が出ない

| 原因 | 確認方法 | 対処 |
|---|---|---|
| ビルドに `perf_logger.c` 変更が含まれていない | `nm nuttx | grep perf_thread_cpu_init` | 再ビルド |
| `CONFIG_DEBUG_ENABLE=0` | `config.h:180` 確認 | `=1` に変更 (デフォルト 1) |
| シリアルレベルが低すぎる | `setlogmask` 設定確認 | NuttX デフォルトで LOG_INFO 通る |

### CPU% が 0% のまま (常に)

| 原因 | 対処 |
|---|---|
| `CLOCK_THREAD_CPUTIME_ID` が NuttX 該当版で未対応 | `time.h:86` で定義確認、`clock_gettime` 戻り値を syslog で確認 |
| サンプル間隔が短すぎる (cpu_delta == 0) | サンプリング周期を 30 → 60 frames に変更 |

### CPU% が 100% を超える

ジッタの可能性 (CPU time が wall time の境界を跨いで sampling される)。連続して 100%+ が出る場合は `clock_gettime` の精度や thread スケジューリングの可視化を別手段で検証。

---

## §7 関連文書

- **計測コード**: `apps/examples/security_camera/perf_logger.{h,c}` (X-6 commit)
- **計装箇所**: `apps/examples/security_camera/camera_threads.c` (3 thread)
- **集計スクリプト**: `scripts/cpu_measurement/parse_cpu_log.py`
- **静的分析**: [`../02_specifications/quality/CPU_BANDWIDTH_BUDGET.md`](../02_specifications/quality/CPU_BANDWIDTH_BUDGET.md) §2
- **タイミング図**: 同 §7 (T-1, T-2)
- **運用ランブック**: [`RUNBOOK.md`](RUNBOOK.md) §3.6 (シリアルログ解析)
- **残課題**: [`../02_specifications/quality/PENDING_NFR_WORK.md`](../02_specifications/quality/PENDING_NFR_WORK.md) X-6

---

## 改訂履歴

| バージョン | 日付 | 変更内容 |
|---|---|---|
| 1.0 | 2026-05-03 | 初版。X-6 P1-A 実測手段の確立 — perf_thread_cpu_* API 追加 + 計装 + 計測手順 + PC 側集計スクリプト + 計測結果反映フロー |

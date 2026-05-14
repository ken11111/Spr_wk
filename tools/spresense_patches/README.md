# Spresense ビルド用パッチ + defconfig

**目的**: spresense submodule (公式 fork) を改変せず、本リポジトリで Phase 12 ビルドに必要な変更を保持する。

**位置付け**: X-9 / X-10 タスク (`docs/security_camera/02_specifications/quality/PENDING_NFR_WORK.md`)。

> **方針 (2026-05-14 ユーザー判断)**:
> spresense submodule は公式 fork なので**変更しない** (master = upstream のクリーンミラー)。
> ビルドに必要な custom defconfig + apps repo patches は本ディレクトリで管理し、
> `apply.sh` を実行することで spresense submodule にコピー / 適用する。

---

## ファイル

| ファイル | 役割 |
|---|---|
| `security_camera_defconfig` | `spresense/sdk/configs/examples/security_camera/defconfig` に配置するファイル (X-9) |
| `0001-Patch-upstream-API-drift-for-newer-NuttX-compatibili.patch` | apps repo の `system/readline.c` `system/cle.c` `netutils/netlib/netlib_setifstatus.c` に対する patch (X-10) |
| `apply.sh` | 上記 2 つを spresense submodule にコピー / 適用するスクリプト |

---

## ビルド手順

### 0. 前提

```bash
sudo apt install -y kconfig-frontends gcc-arm-none-eabi
# または kconfig-frontends.deb を $HOME/.local に展開 (sudo 不可な場合)
```

### 1. パッチ適用

```bash
cd /path/to/GH_wk_test
./tools/spresense_patches/apply.sh
```

このスクリプトは以下を実行:

1. `tools/spresense_patches/security_camera_defconfig` を `spresense/sdk/configs/examples/security_camera/defconfig` にコピー
2. `tools/spresense_patches/0001-*.patch` を `spresense/sdk/apps/` 上で `git apply`

注意:
- spresense submodule に commit を作らない (working tree 修正のみ)
- 次回 `git submodule update` で working tree がリセットされる → 再度 apply.sh を実行
- spresense submodule の git status は untracked / modified を出すが、本リポジトリでは無視 (`.gitmodules` の `ignore = dirty` は付けないことで意図的な汚れを可視化)

### 2. SDK 設定

```bash
cd spresense/sdk
./tools/config.py examples/security_camera
```

### 3. ビルド

```bash
make
# 結果: spresense/sdk/nuttx.spk が生成される (~258 KB, X-6 計装含む)
```

### 4. (オプション) Spresense ボードへ書き込み

```bash
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## パッチ内容詳細

### `security_camera_defconfig` (44 エントリ)

X-9 で作成した examples/security_camera のための SDK config 定義。主な内容:

- `EXAMPLES_SECURITY_CAMERA=y` + WiFi モード `_WIFI=y`
- USB CDC-ACM (`CDCACM`)
- カメラ (`CXD56_CISIF`, `DRIVERS_VIDEO`, `VIDEO_ISX012`, `VIDEO_STREAM`)
- WiFi GS2200M 系 (`WIRELESS_GS2200M`, `WL_GS2200M`, `WL_GS2200M_SPI_FREQUENCY=4000000`)
- GS2200M 用 SPI5 + DMAC (`CXD56_SPI5`, `CXD56_DMAC_SPI5_TX/RX`)
- ネットワーク (`NET`, `NET_IPv4`, `NET_USRSOCK`, `NET_TCP_NO_STACK` 等)
- DHCPC / netlib
- 構造的天井 #2 を明示化: `IOB_NBUFFERS=8`, `IOB_BUFSIZE=196`

### `0001-Patch-upstream-API-drift...patch` (3 ファイル変更)

X-10 で識別した apps repo と新しい NuttX upstream の API drift を解消:

1. **`system/readline/readline.c:53`**: `instream->fs_fd` → `fileno(instream)`
   (struct file_struct.fs_fd は newer NuttX で削除済、POSIX fileno() を使用)

2. **`system/cle/cle.c:1126`**: 同上

3. **`netutils/netlib/netlib_setifstatus.c:121`**: `req.ifr_flags |= IFF_DOWN;` → `req.ifr_flags &= ~IFF_UP;`
   (`IFF_DOWN` は `nuttx/include/net/if.h` で未定義、`IFF_UP` を clear するのが NuttX upstream の正規パターン)

---

## 検証

`apply.sh` 実行後 `make` を行い、以下を確認:

```bash
# 1. nuttx.spk が生成されたか
ls -la spresense/sdk/nuttx.spk

# 2. X-6 計装が含まれているか (perf_thread_cpu_* シンボル)
arm-none-eabi-nm spresense/sdk/nuttx | grep perf_thread_cpu
# 期待出力 (3 行):
#   0d025bd4 T perf_thread_cpu_init
#   0d025d54 T perf_thread_cpu_log
#   0d025c54 T perf_thread_cpu_sample
```

両方とも OK なら Phase 12.1 (X-6 CPU 実測 / X-5f ST-1) の実機検証が可能。

---

## 関連文書

- 残タスク台帳: [`../../docs/security_camera/02_specifications/quality/PENDING_NFR_WORK.md`](../../docs/security_camera/02_specifications/quality/PENDING_NFR_WORK.md) X-9 / X-10
- ビルド手順 (アプリ側 README): [`../../apps/examples/security_camera/README.md`](../../apps/examples/security_camera/README.md)
- CPU 計測ガイド: [`../../docs/security_camera/07_operations/CPU_MEASUREMENT_GUIDE.md`](../../docs/security_camera/07_operations/CPU_MEASUREMENT_GUIDE.md)

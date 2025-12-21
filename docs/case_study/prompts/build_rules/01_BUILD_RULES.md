# Spresense ビルドルール - 詳細版

**最終更新**: 2025-12-16
**目的**: NuttX アプリケーションを確実にビルド・登録するための完全ルール

---

## 📖 目次

1. [プロジェクト構造ルール](#1-プロジェクト構造ルール)
2. [Kconfig ルール](#2-kconfig-ルール)
3. [Makefile ルール](#3-makefile-ルール)
4. [Make.defs ルール](#4-makedefs-ルール-最重要)
5. [defconfig ルール](#5-defconfig-ルール)
6. [2重コンフィグ構造](#6-2重コンフィグ構造-最重要)
7. [ビルドコマンドルール](#7-ビルドコマンドルール)
8. [フラッシュルール](#8-フラッシュルール)
9. [検証ルール](#9-検証ルール)

---

## 1. プロジェクト構造ルール

### ✅ 必須のディレクトリ配置

```
/home/ken/Spr_ws/spresense/sdk/apps/examples/{app_name}/
```

**絶対ルール**:
- `sdk/apps/` 配下に配置すること
- `examples/` カテゴリを使用すること (他: `system/`, `graphics/` など)
- アプリ名は小文字・アンダースコア推奨 (例: `bmi160_orientation`, `security_camera`)

### ✅ 必須ファイル一覧

| ファイル | 必須度 | 説明 |
|---------|-------|------|
| `Kconfig` | 必須 | 設定オプション定義 |
| `Makefile` | 必須 | ビルド定義 |
| `Make.defs` | 必須 | アプリ登録 (最重要!) |
| `{app}_main.c` | 必須 | メイン関数 (エントリーポイント) |
| `*.c / *.h` | 任意 | 実装ファイル |
| `README.md` | 推奨 | ドキュメント |

### ❌ 間違った配置

```
# NG: SDK直下
/home/ken/Spr_ws/spresense/sdk/{app_name}/

# NG: apps直下
/home/ken/Spr_ws/spresense/sdk/apps/{app_name}/

# NG: NuttX側
/home/ken/Spr_ws/spresense/nuttx/apps/examples/{app_name}/
```

---

## 2. Kconfig ルール

### ✅ 基本構造

```kconfig
config EXAMPLES_{APP_NAME_UPPER}
	tristate "Application description"
	default n
	---help---
		Help text here.

if EXAMPLES_{APP_NAME_UPPER}

config EXAMPLES_{APP_NAME_UPPER}_PROGNAME
	string "Program name"
	default "{app_name}"
	---help---
		This is the name used in builtin apps.

config EXAMPLES_{APP_NAME_UPPER}_PRIORITY
	int "Task priority"
	default 100

config EXAMPLES_{APP_NAME_UPPER}_STACKSIZE
	int "Stack size"
	default 2048

endif # EXAMPLES_{APP_NAME_UPPER}
```

### ✅ 依存関係の記述

```kconfig
config EXAMPLES_SECURITY_CAMERA
	tristate "Security Camera example"
	default n
	select VIDEO              # カメラを使う
	select VIDEO_STREAM       # ストリーミング必須
	select VIDEO_ISX012       # ISX012センサー
	select VIDEOUTILS_CODEC_H264  # H.264エンコード
	select USBDEV             # USBデバイス
	select CDCACM             # CDC-ACMシリアル
	---help---
		Security camera application with H.264 encoding.
```

**ルール**:
- 必要な機能は `select` で自動有効化
- `default n` で明示的に無効化 (ユーザーが選択)
- `---help---` で説明を記述

### ✅ Kconfig を examples/Kconfig に登録

**ファイル**: `/home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig`

**追加内容**:
```kconfig
source "examples/{app_name}/Kconfig"
```

**例**:
```kconfig
source "examples/bmi160_orientation/Kconfig"
source "examples/security_camera/Kconfig"
```

**注意**: アルファベット順に追加推奨

---

## 3. Makefile ルール

### ✅ 基本構造

```makefile
include $(APPDIR)/Make.defs

# CONFIG variables (Kconfigから取得)
PROGNAME  = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_{APP_NAME_UPPER})

# Source files
CSRCS  = module1.c
CSRCS += module2.c
CSRCS += module3.c

# Main source (エントリーポイント)
MAINSRC = {app}_main.c

include $(APPDIR)/Application.mk
```

### ✅ 実例: security_camera

```makefile
include $(APPDIR)/Make.defs

PROGNAME  = $(CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_SECURITY_CAMERA)

CSRCS  = camera_manager.c
CSRCS += encoder_manager.c
CSRCS += protocol_handler.c
CSRCS += usb_transport.c

MAINSRC = camera_app_main.c

include $(APPDIR)/Application.mk
```

### ❌ よくある間違い

```makefile
# NG: MAINSRC を CSRCS に含める
CSRCS = camera_app_main.c camera_manager.c  # ✗ main は MAINSRC へ

# NG: 存在しないファイルを指定
CSRCS = nonexistent.c  # ✗ リンカエラー

# NG: 拡張子を間違える
CSRCS = camera_manager.cpp  # ✗ C++ は CXXSRCS
```

---

## 4. Make.defs ルール (最重要!)

### ✅ 正しい記述方法

```makefile
############################################################################
# apps/examples/{app_name}/Make.defs
#
#   Copyright notice...
#
############################################################################

ifneq ($(CONFIG_EXAMPLES_{APP_NAME_UPPER}),)
CONFIGURED_APPS += examples/{app_name}
endif
```

### ✅ 実例: security_camera

```makefile
ifneq ($(CONFIG_EXAMPLES_SECURITY_CAMERA),)
CONFIGURED_APPS += examples/security_camera
endif
```

### ❌ 絶対にやってはいけない記述

```makefile
# ❌ 間違い1: $(APPDIR)/ を使う → アプリが登録されない!
ifneq ($(CONFIG_EXAMPLES_SECURITY_CAMERA),)
CONFIGURED_APPS += $(APPDIR)/examples/security_camera
endif

# ❌ 間違い2: 絶対パスを使う
CONFIGURED_APPS += /home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera

# ❌ 間違い3: apps/ から始める
CONFIGURED_APPS += apps/examples/security_camera

# ❌ 間違い4: 末尾スラッシュ
CONFIGURED_APPS += examples/security_camera/
```

### 🔍 なぜ $(APPDIR)/ を使ってはいけないのか?

**理由**: ビルドシステムが内部で既に $(APPDIR)/ を補完するため、二重になってパスが壊れる

**ビルドシステムの動作**:
```makefile
# ビルドシステム内部 (簡略化)
foreach app in $(CONFIGURED_APPS)
    include $(APPDIR)/$(app)/Makefile
end

# 正しい場合:
# app = "examples/security_camera"
# → include $(APPDIR)/examples/security_camera/Makefile ✅

# 間違いの場合:
# app = "$(APPDIR)/examples/security_camera"
# → include $(APPDIR)/$(APPDIR)/examples/security_camera/Makefile ✗
```

### ✅ 検証方法

**ビルド後に確認**:
```bash
# アプリがビルドされたか確認
ls -la /home/ken/Spr_ws/spresense/nuttx/../apps/examples/security_camera/*.o

# NuttShell で確認
nsh> help
# → security_camera が表示されるはず
```

---

## 5. defconfig ルール

### ✅ 設定の追加場所

**ファイル**: `/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig`

**追加内容**:
```bash
# Application configuration
CONFIG_EXAMPLES_{APP_NAME_UPPER}=y
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME="{app_name}"
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY=100
CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE=2048

# その他の必要な設定
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
```

### ✅ 実例: security_camera

```bash
# Security Camera Application
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_WIDTH=1280
CONFIG_EXAMPLES_SECURITY_CAMERA_CAMERA_HEIGHT=720
CONFIG_EXAMPLES_SECURITY_CAMERA_FPS=30
CONFIG_EXAMPLES_SECURITY_CAMERA_BITRATE=2000000
# CONFIG_EXAMPLES_SECURITY_CAMERA_HDR_ENABLE is not set

# Video subsystem
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
```

### ⚠️ 注意点

1. **設定を無効化する場合**:
   ```bash
   # 明示的に無効化
   # CONFIG_EXAMPLES_MY_APP is not set

   # ❌ 間違い
   CONFIG_EXAMPLES_MY_APP=n  # これは無効
   ```

2. **bool型とtristate型**:
   ```bash
   CONFIG_SOME_BOOL=y        # bool: y または未設定
   CONFIG_SOME_TRISTATE=m    # tristate: y, m, または未設定
   ```

---

## 6. 2重コンフィグ構造 (最重要!)

### ✅ 理解必須: 2つの .config ファイル

```
/home/ken/Spr_ws/spresense/
├── sdk/
│   └── .config              ← 小さい (295-400 bytes) - 使われない!
└── nuttx/
    └── .config              ← 大きい (60-70 KB) - 実際に使われる ✅
```

### 🔍 なぜ2つあるのか?

**SDK/.config**:
- SDK側のビルドメタ情報
- ビルドフローの制御のみ
- **アプリケーション設定には影響しない**

**NuttX/.config**:
- NuttX RTOS とアプリの実際の設定
- defconfig から生成される
- **これが実際にビルドで使われる**

### ✅ 正しい確認方法

```bash
# ❌ 間違い: SDK側を見る
cat /home/ken/Spr_ws/spresense/sdk/.config

# ✅ 正しい: NuttX側を見る
cat /home/ken/Spr_ws/spresense/nuttx/.config | grep SECURITY_CAMERA
```

### ✅ 設定の流れ

```
defconfig (設定の大元)
    ↓
  make (ビルド開始)
    ↓
NuttX/.config (生成・更新)
    ↓
ビルドシステム (これを読む)
    ↓
nuttx.spk (ファームウェア)
```

### ⚠️ 重要な教訓

**間違い**: SDK/.config を編集 → 反映されない
**正しい**: defconfig を編集 → make で NuttX/.config に反映される

---

## 7. ビルドコマンドルール

### ✅ 正しいビルド手順

```bash
# 1. ディレクトリ移動
cd /home/ken/Spr_ws/spresense/sdk

# 2. PATH 設定 (重要!)
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# 3. ビルド実行
make

# 4. 結果確認
ls -lh nuttx.spk
# または
ls -lh /home/ken/Spr_ws/spresense/nuttx/nuttx.spk
```

### ✅ クリーンビルド

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# アプリのみクリーン
make clean

# 完全クリーン (非推奨 - 時間がかかる)
make distclean
```

### ❌ よくある失敗

```bash
# ❌ 失敗1: PATH 未設定
cd /home/ken/Spr_ws/spresense/sdk
make  # → arm-none-eabi-gcc: not found

# ❌ 失敗2: 間違ったディレクトリ
cd /home/ken/Spr_ws/spresense
make  # → No targets specified and no makefile found

# ❌ 失敗3: 相対パス
cd /home/ken/Spr_ws
spresense/sdk/make  # → エラー
```

### ✅ ビルド成功の確認

**出力例**:
```
make[1]: Entering directory '/home/ken/Spr_ws/spresense/nuttx'
LD: nuttx
Generating: nuttx.spk
tools/cxd56/mkspk -c2 nuttx nuttx nuttx.spk;
File nuttx.spk is successfully created.
Done.
make[1]: Leaving directory '/home/ken/Spr_ws/spresense/nuttx'
```

**確認コマンド**:
```bash
# ファームウェアサイズ確認
ls -lh /home/ken/Spr_ws/spresense/nuttx/nuttx.spk
# 例: -rw-r--r-- 1 ken ken 175K Dec 16 10:30 nuttx.spk

# タイムスタンプ確認 (最新であること)
stat /home/ken/Spr_ws/spresense/nuttx/nuttx.spk
```

---

## 8. フラッシュルール

### ✅ 正しいフラッシュ手順

```bash
# 1. Spresense をブートローダーモードで接続
#    (BOOTボタンを押しながらUSB接続)

# 2. デバイス確認
ls -l /dev/ttyUSB0
# 例: crw-rw---- 1 root dialout 188, 0 Dec 16 10:35 /dev/ttyUSB0

# 3. フラッシュ実行
cd /home/ken/Spr_ws/spresense/sdk
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

### ✅ フラッシュ成功の確認

**出力例**:
```
>>> Install files ...
install -b 115200
>>> Install nuttx.spk
|0%-----------------------------50%------------------------------100%|
####################################################################

xxxxx bytes loaded.
Package validation is OK.
Saving package to "nuttx"
updater# sync
updater# Restarting the board ...
reboot
```

### ❌ よくあるエラー

```bash
# ❌ エラー1: デバイスが見つからない
# /dev/ttyUSB0: No such file or directory
# → BOOTボタンを押しながら再接続

# ❌ エラー2: パーミッションエラー
# Permission denied
# → sudo を使う、または dialout グループに追加

# ❌ エラー3: 既に使用中
# Device or resource busy
# → minicom などを終了
```

### ✅ パーミッション設定 (推奨)

```bash
# dialout グループに追加 (1回だけ実行)
sudo usermod -a -G dialout $USER

# ログアウト・ログインで反映

# 以降は sudo 不要
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 9. 検証ルール

### ✅ アプリ登録の確認

**手順**:
```bash
# 1. minicom で NuttShell に接続
minicom -D /dev/ttyUSB0 -b 115200

# 2. help コマンド実行
nsh> help

# 3. 期待される出力
Builtin Apps:
    security_camera    ← アプリ名が表示される ✅
    sh
    nsh
```

### ✅ アプリ起動確認

```bash
# バックグラウンドで起動
nsh> security_camera &

# フォアグラウンドで起動
nsh> security_camera

# プロセス確認
nsh> ps
  PID GROUP PRI POLICY   TYPE    NPX STATE    EVENT     SIGMASK  COMMAND
    0     0   0 FIFO     Kthread N-- Ready              00000000 Idle Task
    1     1 100 RR       Task    --- Running            00000000 init
    3     3 100 RR       Task    --- Waiting  Semaphore 00000000 security_camera
```

### ✅ USB CDC-ACM デバイス確認 (security_camera の場合)

**PC側で確認**:
```bash
# アプリ起動後、PC側で確認
ls -l /dev/ttyACM0
# 例: crw-rw---- 1 root dialout 166, 0 Dec 16 11:00 /dev/ttyACM0

# デバイス情報
lsusb | grep -i sony
# 例: Bus 001 Device 010: ID 054c:0bc2 Sony Corp.
```

### ❌ アプリが表示されない場合

**チェックリスト**:
1. Make.defs で `$(APPDIR)/` を使っていないか? → [Make.defs ルール](#4-makedefs-ルール-最重要)
2. defconfig に設定を追加したか? → [defconfig ルール](#5-defconfig-ルール)
3. NuttX/.config に設定が反映されているか? → `grep {APP_NAME} /home/ken/Spr_ws/spresense/nuttx/.config`
4. ビルドが成功したか? → `File nuttx.spk is successfully created.` が表示されたか
5. フラッシュが成功したか? → `Package validation is OK.` が表示されたか

---

## 📚 参考: 成功例のファイル

### BMI160 Orientation

```bash
# Kconfig
/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Kconfig

# Makefile
/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Makefile

# Make.defs (最重要!)
/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Make.defs
```

### Security Camera

```bash
# Kconfig
/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/Kconfig

# Makefile
/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/Makefile

# Make.defs (最重要!)
/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/Make.defs
```

---

## ✅ ルールのまとめ

| # | ルール | 重要度 |
|---|--------|-------|
| 1 | Make.defs で $(APPDIR)/ を使わない | 🔴 最重要 |
| 2 | NuttX/.config が実際に使われる | 🔴 最重要 |
| 3 | defconfig に設定を追加 | 🔴 最重要 |
| 4 | PATH を正しく設定してビルド | 🟠 重要 |
| 5 | examples/Kconfig に source 追加 | 🟠 重要 |
| 6 | Makefile で MAINSRC を分離 | 🟡 必須 |
| 7 | help でアプリ名を確認 | 🟡 必須 |

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース**: security_camera + bmi160_orientation プロジェクト

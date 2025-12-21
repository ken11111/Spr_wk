# Spresense ビルド クイックリファレンス

**最終更新**: 2025-12-16
**目的**: コマンドやパターンの即座の参照

---

## 🚀 即座にコピペできるコマンド集

### 新規アプリケーション作成

```bash
# 1. プロジェクト作成
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
mkdir -p my_app
cd my_app

# 2. 基本ファイル作成
touch Kconfig Makefile Make.defs my_app_main.c

# 3. 既存例からコピー (推奨)
cp ../bmi160_orientation/Kconfig ./Kconfig
cp ../bmi160_orientation/Makefile ./Makefile
cp ../bmi160_orientation/Make.defs ./Make.defs
# 上記ファイルを my_app 用に編集
```

---

### ビルド

```bash
# 基本ビルド (推奨)
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
./tools/config.py default  # 設定を再適用 (重要!)
make

# シンプルビルド (設定変更がない場合のみ)
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# ログ付きビルド
./tools/config.py default
make 2>&1 | tee /tmp/build.log

# クリーンビルド (アプリのみ)
make clean
./tools/config.py default
make
```

---

### フラッシュ

```bash
# 基本フラッシュ
cd /home/ken/Spr_ws/spresense/sdk
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# dialout グループ追加後 (sudo不要)
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

### NuttShell 接続

```bash
# minicom で接続
minicom -D /dev/ttyUSB0 -b 115200

# minicom 終了: Ctrl+A → X

# screen で接続 (代替)
screen /dev/ttyUSB0 115200
```

---

### 設定確認

```bash
# NuttX設定を確認 (実際に使われる)
grep "MY_APP" /home/ken/Spr_ws/spresense/nuttx/.config

# defconfig を確認 (設定の大元)
grep "MY_APP" /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig

# 2つのファイルサイズ比較 (理解用)
ls -lh /home/ken/Spr_ws/spresense/sdk/.config      # 小 (295-400B)
ls -lh /home/ken/Spr_ws/spresense/nuttx/.config    # 大 (60-70KB)
```

---

### デバイス確認

```bash
# シリアルポート確認
ls -l /dev/ttyUSB* /dev/ttyACM*

# USB デバイス確認
lsusb | grep -i sony

# デバイス使用状況
lsof /dev/ttyUSB0

# dmesg で接続ログ
dmesg | tail -20
```

---

## 📝 テンプレート集

### Kconfig テンプレート

```kconfig
config EXAMPLES_MY_APP
	tristate "My Application"
	default n
	---help---
		My application description.

if EXAMPLES_MY_APP

config EXAMPLES_MY_APP_PROGNAME
	string "Program name"
	default "my_app"
	---help---
		This is the name used in builtin apps.

config EXAMPLES_MY_APP_PRIORITY
	int "Task priority"
	default 100
	---help---
		Task scheduling priority.

config EXAMPLES_MY_APP_STACKSIZE
	int "Stack size"
	default 2048
	---help---
		Stack size in bytes.

endif # EXAMPLES_MY_APP
```

---

### Makefile テンプレート

```makefile
include $(APPDIR)/Make.defs

# Config from Kconfig
PROGNAME  = $(CONFIG_EXAMPLES_MY_APP_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_MY_APP_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MY_APP_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_MY_APP)

# Source files (implementation modules)
CSRCS  = module1.c
CSRCS += module2.c

# Main source (entry point)
MAINSRC = my_app_main.c

include $(APPDIR)/Application.mk
```

---

### Make.defs テンプレート (最重要!)

```makefile
############################################################################
# apps/examples/my_app/Make.defs
#
#   Copyright 2025 Your Name
#
############################################################################

ifneq ($(CONFIG_EXAMPLES_MY_APP),)
CONFIGURED_APPS += examples/my_app
endif
```

**⚠️ 絶対ルール**: `$(APPDIR)/` を使わない!

---

### メインファイル テンプレート (C)

```c
/****************************************************************************
 * apps/examples/my_app/my_app_main.c
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
    printf("Hello from my_app!\n");

    /* Your code here */

    return EXIT_SUCCESS;
}
```

---

### defconfig 設定テンプレート

```bash
# My Application
CONFIG_EXAMPLES_MY_APP=y
CONFIG_EXAMPLES_MY_APP_PROGNAME="my_app"
CONFIG_EXAMPLES_MY_APP_PRIORITY=100
CONFIG_EXAMPLES_MY_APP_STACKSIZE=2048

# Optional dependencies (例)
# CONFIG_VIDEO=y
# CONFIG_VIDEO_STREAM=y
# CONFIG_I2C=y
# CONFIG_SPI=y
```

---

## 🔍 検証コマンド集

### ビルド結果確認

```bash
# nuttx.spk 存在確認
ls -lh /home/ken/Spr_ws/spresense/nuttx/nuttx.spk

# ファイルサイズ確認 (通常 100KB 以上)
stat /home/ken/Spr_ws/spresense/nuttx/nuttx.spk

# タイムスタンプ確認 (最新であること)
ls -lt /home/ken/Spr_ws/spresense/nuttx/*.spk | head -1
```

---

### アプリ登録確認

```bash
# Make.defs が正しいか確認
cat /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Make.defs | grep CONFIGURED_APPS
# → $(APPDIR)/ が含まれていないことを確認 ✅

# examples/Kconfig に登録されているか
grep "my_app" /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig
# → source "examples/my_app/Kconfig" があること ✅

# defconfig に設定があるか
grep "MY_APP" /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
# → CONFIG_EXAMPLES_MY_APP=y があること ✅

# NuttX/.config に反映されているか
grep "MY_APP" /home/ken/Spr_ws/spresense/nuttx/.config
# → CONFIG_EXAMPLES_MY_APP=y があること ✅
```

---

### NuttShell での確認

```bash
# アプリがビルトインに登録されているか
nsh> help
# → my_app が表示されること ✅

# アプリを起動
nsh> my_app

# バックグラウンドで起動
nsh> my_app &

# プロセス確認
nsh> ps

# アプリ終了
nsh> kill <PID>
```

---

## 🛠️ トラブルシューティング クイックガイド

### アプリが help に表示されない

```bash
# 1. Make.defs を確認 (最頻出の原因!)
cat /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Make.defs
# → $(APPDIR)/ が含まれていたら削除

# 2. 修正後、再ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# 3. 再フラッシュ
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# 4. 確認
minicom -D /dev/ttyUSB0 -b 115200
# nsh> help
```

---

### ビルドエラー: arm-none-eabi-gcc not found

```bash
# PATH を設定
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# 確認
which arm-none-eabi-gcc
# → /home/ken/spresenseenv/usr/bin/arm-none-eabi-gcc
```

---

### フラッシュエラー: Permission denied

```bash
# 一時的: sudo を使う
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# 恒久的: グループに追加
sudo usermod -a -G dialout $USER
# → ログアウト・ログイン後、sudo 不要
```

---

### フラッシュエラー: Device busy

```bash
# 使用中のプロセスを確認
lsof /dev/ttyUSB0

# minicom を終了
killall minicom

# または、minicom 内で: Ctrl+A → X
```

---

## 📊 重要なファイルパス一覧

| 目的 | パス |
|-----|------|
| SDK ディレクトリ | `/home/ken/Spr_ws/spresense/sdk/` |
| アプリディレクトリ | `/home/ken/Spr_ws/spresense/sdk/apps/examples/{app}/` |
| defconfig (設定の大元) | `/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig` |
| examples/Kconfig | `/home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig` |
| NuttX/.config (実際の設定) | `/home/ken/Spr_ws/spresense/nuttx/.config` |
| SDK/.config (使われない) | `/home/ken/Spr_ws/spresense/sdk/.config` |
| nuttx.spk (ファームウェア) | `/home/ken/Spr_ws/spresense/nuttx/nuttx.spk` |
| flash.sh | `/home/ken/Spr_ws/spresense/sdk/tools/flash.sh` |
| config.py | `/home/ken/Spr_ws/spresense/sdk/tools/config.py` |

---

## 🎯 必須ファイル一覧 (新規アプリ)

| ファイル | 必須度 | 説明 |
|---------|-------|------|
| `Kconfig` | 必須 | 設定オプション定義 |
| `Makefile` | 必須 | ビルド定義 |
| `Make.defs` | 必須 | アプリ登録 ⭐最重要 |
| `{app}_main.c` | 必須 | エントリーポイント |
| `module1.c` | 任意 | 実装ファイル |
| `module1.h` | 任意 | ヘッダーファイル |
| `README.md` | 推奨 | ドキュメント |

---

## ⚡ よく使うコマンド組み合わせ

### 完全なビルド・フラッシュフロー

```bash
# 1. ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
./tools/config.py default  # 設定を再適用 (重要!)
make

# 2. フラッシュ (Spresense を BOOT モードで接続)
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# 3. 確認 (Spresense を通常モードで再接続)
minicom -D /dev/ttyUSB0 -b 115200
# nsh> help
# nsh> my_app
```

---

### 設定変更後のリビルド

```bash
# 1. defconfig を編集
nano /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
# CONFIG_XXX=y を追加

# 2. 設定を再適用
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py default

# 3. ビルド
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# 4. 確認
grep "CONFIG_XXX" /home/ken/Spr_ws/spresense/nuttx/.config
```

---

### 既存アプリのコピーから始める (推奨)

```bash
# 1. 成功例をコピー
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
cp -r bmi160_orientation my_app
cd my_app

# 2. 不要なファイルを削除
rm -f *.c *.h

# 3. 新しいソースファイルを作成
touch my_app_main.c

# 4. Kconfig, Makefile, Make.defs を編集
# - bmi160_orientation → my_app に置換
# - BMI160_ORIENTATION → MY_APP に置換

# 5. ビルド・フラッシュ
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 🔑 最重要ポイント (必ず守る!)

### Make.defs

```makefile
# ✅ 正しい
CONFIGURED_APPS += examples/my_app

# ❌ 間違い
CONFIGURED_APPS += $(APPDIR)/examples/my_app
CONFIGURED_APPS += apps/examples/my_app
CONFIGURED_APPS += /home/ken/.../examples/my_app
```

### 2重コンフィグ

```bash
# ❌ 間違い: SDK側を見る
cat /home/ken/Spr_ws/spresense/sdk/.config

# ✅ 正しい: NuttX側を見る
cat /home/ken/Spr_ws/spresense/nuttx/.config
```

### PATH 設定

```bash
# ✅ 正しい
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# ❌ 間違い (PATH未設定)
make  # → arm-none-eabi-gcc: not found
```

---

## 📚 関連ドキュメント

| ドキュメント | 用途 |
|------------|------|
| [00_INDEX.md](./00_INDEX.md) | 全体インデックス |
| [01_BUILD_RULES.md](./01_BUILD_RULES.md) | 詳細ルール参照 |
| [02_CHECKLIST.md](./02_CHECKLIST.md) | ステップバイステップ |
| [03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md) | エラー解決 |

---

## 🚀 1分でアプリを作成する最速フロー

```bash
# コピペで実行可能 (アプリ名を my_app から変更)
APP_NAME="my_app"
APP_UPPER="MY_APP"

cd /home/ken/Spr_ws/spresense/sdk/apps/examples
cp -r bmi160_orientation ${APP_NAME}
cd ${APP_NAME}

# ファイルを編集 (bmi160_orientation → my_app に置換)
sed -i "s/bmi160_orientation/${APP_NAME}/g" Kconfig Makefile Make.defs
sed -i "s/BMI160_ORIENTATION/${APP_UPPER}/g" Kconfig Makefile Make.defs

# メインファイルを作成
cat > ${APP_NAME}_main.c << 'EOF'
#include <nuttx/config.h>
#include <stdio.h>

int main(int argc, FAR char *argv[])
{
    printf("Hello from my_app!\n");
    return 0;
}
EOF

# 古いソースを削除
rm -f bmi160_orientation_main.c

# defconfig に追加 (手動編集が必要)
echo "CONFIG_EXAMPLES_${APP_UPPER}=y を defconfig に追加してください"
echo "/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig"

# ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース**: security_camera + bmi160_orientation プロジェクト

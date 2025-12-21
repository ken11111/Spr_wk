# Spresense ビルド前チェックリスト

**最終更新**: 2025-12-16
**目的**: ビルドエラーを未然に防ぐための完全チェックリスト

---

## 📋 使い方

- 新規アプリケーション作成時に、このチェックリストを**上から順番に**実行
- 各項目をチェック ✅ しながら進める
- エラーが出たら [03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md) を参照

---

## フェーズ1: プロジェクト構造作成

### 1.1 ディレクトリ作成

```bash
cd /home/ken/Spr_ws/spresense/sdk/apps/examples
mkdir -p {app_name}
cd {app_name}
```

- [ ] ディレクトリパスが正しい: `sdk/apps/examples/{app_name}/`
- [ ] アプリ名は小文字・アンダースコア (例: `my_app`, `sensor_reader`)

### 1.2 必須ファイル作成

```bash
touch Kconfig
touch Makefile
touch Make.defs
touch {app_name}_main.c
```

- [ ] `Kconfig` ファイルが存在
- [ ] `Makefile` ファイルが存在
- [ ] `Make.defs` ファイルが存在
- [ ] `{app_name}_main.c` ファイルが存在 (エントリーポイント)

---

## フェーズ2: Kconfig 設定

### 2.1 Kconfig ファイル編集

**最小構成**:
```kconfig
config EXAMPLES_{APP_NAME_UPPER}
	tristate "{App description}"
	default n
	---help---
		{Help text}

if EXAMPLES_{APP_NAME_UPPER}

config EXAMPLES_{APP_NAME_UPPER}_PROGNAME
	string "Program name"
	default "{app_name}"

config EXAMPLES_{APP_NAME_UPPER}_PRIORITY
	int "Task priority"
	default 100

config EXAMPLES_{APP_NAME_UPPER}_STACKSIZE
	int "Stack size"
	default 2048

endif
```

- [ ] `config EXAMPLES_{APP_NAME_UPPER}` が定義されている
- [ ] `default n` が設定されている
- [ ] `PROGNAME`, `PRIORITY`, `STACKSIZE` が定義されている
- [ ] 必要な依存関係を `select` で追加している

**依存関係の例**:
- カメラ: `select VIDEO`, `select VIDEO_STREAM`
- I2C: `select I2C`
- SPI: `select SPI`
- USB CDC-ACM: `select USBDEV`, `select CDCACM`

### 2.2 examples/Kconfig に登録

**ファイル**: `/home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig`

**追加**:
```kconfig
source "examples/{app_name}/Kconfig"
```

- [ ] `examples/Kconfig` を開いた
- [ ] `source "examples/{app_name}/Kconfig"` を追加した
- [ ] アルファベット順に挿入した (推奨)

**確認コマンド**:
```bash
grep "{app_name}" /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig
```

---

## フェーズ3: Makefile 作成

### 3.1 Makefile 編集

**テンプレート**:
```makefile
include $(APPDIR)/Make.defs

# Config
PROGNAME  = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE)
MODULE    = $(CONFIG_EXAMPLES_{APP_NAME_UPPER})

# Source files
CSRCS  = module1.c
CSRCS += module2.c

# Main source
MAINSRC = {app_name}_main.c

include $(APPDIR)/Application.mk
```

- [ ] `include $(APPDIR)/Make.defs` が先頭にある
- [ ] `PROGNAME`, `PRIORITY`, `STACKSIZE`, `MODULE` が定義されている
- [ ] 全ての実装ファイル (*.c) が `CSRCS` に列挙されている
- [ ] `MAINSRC` に main 関数を含むファイルを指定している
- [ ] `MAINSRC` を `CSRCS` に含めていない (重要!)
- [ ] `include $(APPDIR)/Application.mk` が末尾にある

**ファイル存在確認**:
```bash
# CSRCS と MAINSRC に指定した全ファイルが存在するか確認
ls -l *.c
```

---

## フェーズ4: Make.defs 作成 (最重要!)

### 4.1 Make.defs 編集

**正しいテンプレート**:
```makefile
############################################################################
# apps/examples/{app_name}/Make.defs
#
#   Copyright {YEAR} {Your Name}
#
############################################################################

ifneq ($(CONFIG_EXAMPLES_{APP_NAME_UPPER}),)
CONFIGURED_APPS += examples/{app_name}
endif
```

- [ ] `ifneq ($(CONFIG_EXAMPLES_{APP_NAME_UPPER}),)` が正しい
- [ ] `CONFIGURED_APPS += examples/{app_name}` が正しい
- [ ] ✅ **$(APPDIR)/ を使っていない** (最重要!)
- [ ] ✅ **apps/ から始めていない**
- [ ] ✅ **絶対パスを使っていない**
- [ ] ✅ **末尾にスラッシュがない**

### 4.2 Make.defs の検証

**正しい例**:
```makefile
CONFIGURED_APPS += examples/my_app              ✅
CONFIGURED_APPS += examples/security_camera     ✅
```

**間違った例**:
```makefile
CONFIGURED_APPS += $(APPDIR)/examples/my_app             ❌
CONFIGURED_APPS += apps/examples/my_app                  ❌
CONFIGURED_APPS += /home/ken/.../examples/my_app         ❌
CONFIGURED_APPS += examples/my_app/                      ❌
```

**確認コマンド**:
```bash
cat Make.defs | grep CONFIGURED_APPS
# → 出力に $(APPDIR)/ が含まれていないことを確認
```

---

## フェーズ5: defconfig 設定

### 5.1 defconfig に設定追加

**ファイル**: `/home/ken/Spr_ws/spresense/sdk/configs/default/defconfig`

**追加内容**:
```bash
# {App Name} Application
CONFIG_EXAMPLES_{APP_NAME_UPPER}=y
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME="{app_name}"
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY=100
CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE=2048
```

- [ ] `CONFIG_EXAMPLES_{APP_NAME_UPPER}=y` を追加した
- [ ] `CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME` を追加した
- [ ] その他の必要な CONFIG 変数を追加した
- [ ] 依存する機能の CONFIG も追加した (例: `CONFIG_VIDEO=y`)

**確認コマンド**:
```bash
grep "CONFIG_EXAMPLES_{APP_NAME_UPPER}" /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
```

### 5.2 必要な依存設定を追加

**例: カメラを使う場合**:
```bash
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
CONFIG_VIDEO_ISX012=y
```

**例: USB CDC-ACM を使う場合**:
```bash
CONFIG_USBDEV=y
CONFIG_CDCACM=y
```

---

## フェーズ6: ソースコード実装

### 6.1 メイン関数の実装

**ファイル**: `{app_name}_main.c`

**最小構成**:
```c
#include <nuttx/config.h>
#include <stdio.h>

int main(int argc, FAR char *argv[])
{
    printf("Hello from %s!\n", argv[0]);
    return 0;
}
```

- [ ] `#include <nuttx/config.h>` がある
- [ ] `main()` 関数が定義されている
- [ ] 引数は `int argc, FAR char *argv[]` (NuttX 形式)

### 6.2 その他のソースファイル

- [ ] 全ての `.c` ファイルに対応する `.h` ファイルがある (必要な場合)
- [ ] インクルードパスが正しい
- [ ] 必要なライブラリをリンクしている

---

## フェーズ7: ビルド実行

### 7.1 ビルド前の環境確認

```bash
# 1. SDK ディレクトリにいるか確認
pwd
# → /home/ken/Spr_ws/spresense/sdk

# 2. ツールチェインが存在するか確認
ls /home/ken/spresenseenv/usr/bin/arm-none-eabi-gcc
```

- [ ] 現在のディレクトリが `/home/ken/Spr_ws/spresense/sdk`
- [ ] arm-none-eabi-gcc が存在する

### 7.2 PATH 設定

```bash
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
```

- [ ] PATH を設定した

**確認**:
```bash
which arm-none-eabi-gcc
# → /home/ken/spresenseenv/usr/bin/arm-none-eabi-gcc
```

### 7.3 設定の再適用 (重要! ⭐)

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
./tools/config.py default
```

- [ ] `config.py default` を実行した
- [ ] エラーなく完了した

**なぜ必要か**:
- defconfig の設定を NuttX/.config に完全に反映するため
- この手順を省略すると、アプリが `help` に表示されない場合がある
- **必ず実行すること!**

### 7.4 ビルド実行

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

- [ ] ビルドコマンドを実行した
- [ ] エラーなく完了した
- [ ] 最後に `File nuttx.spk is successfully created.` が表示された
- [ ] ビルドログに `Register: {app_name}` が表示された (確認推奨)

**成功の出力例**:
```
make[1]: Entering directory '/home/ken/Spr_ws/spresense/nuttx'
LD: nuttx
Generating: nuttx.spk
File nuttx.spk is successfully created.
Done.
make[1]: Leaving directory '/home/ken/Spr_ws/spresense/nuttx'
```

### 7.5 ビルド結果確認

```bash
ls -lh /home/ken/Spr_ws/spresense/nuttx/nuttx.spk
```

- [ ] `nuttx.spk` ファイルが生成されている
- [ ] ファイルサイズが妥当 (通常 100KB 〜 数MB)
- [ ] タイムスタンプが最新

**オプション: アプリがバイナリに含まれているか確認**:
```bash
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
arm-none-eabi-nm /home/ken/Spr_ws/spresense/nuttx/nuttx | grep "{app_name}_main"
# → {app_name}_main のシンボルが表示されれば成功 ✅
```

---

## フェーズ8: NuttX/.config 確認 (重要!)

### 8.1 設定が反映されているか確認

```bash
grep "CONFIG_EXAMPLES_{APP_NAME_UPPER}" /home/ken/Spr_ws/spresense/nuttx/.config
```

**期待される出力**:
```
CONFIG_EXAMPLES_{APP_NAME_UPPER}=y
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME="{app_name}"
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY=100
CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE=2048
```

- [ ] `CONFIG_EXAMPLES_{APP_NAME_UPPER}=y` が表示される
- [ ] その他の CONFIG 変数も表示される
- [ ] ✅ **NuttX/.config を確認している** (SDK/.config ではない!)

### 8.2 2重コンフィグの理解

```bash
# ❌ 間違い: SDK側を見る (小さい)
ls -lh /home/ken/Spr_ws/spresense/sdk/.config
# → 295-400 bytes

# ✅ 正しい: NuttX側を見る (大きい)
ls -lh /home/ken/Spr_ws/spresense/nuttx/.config
# → 60-70 KB
```

- [ ] NuttX/.config が実際に使われることを理解している

---

## フェーズ9: フラッシュ

### 9.1 Spresense 接続

- [ ] Spresense をブートローダーモードで接続 (BOOT ボタン押しながら USB 接続)
- [ ] `/dev/ttyUSB0` デバイスが認識されている

**確認**:
```bash
ls -l /dev/ttyUSB0
# → crw-rw---- 1 root dialout 188, 0 ...
```

### 9.2 フラッシュ実行

```bash
cd /home/ken/Spr_ws/spresense/sdk
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

- [ ] フラッシュコマンドを実行した
- [ ] `Package validation is OK.` が表示された
- [ ] `Saving package to "nuttx"` が表示された
- [ ] `Restarting the board ...` が表示された

---

## フェーズ10: 動作確認

### 10.1 NuttShell 接続

```bash
# Spresense を通常モードで再接続 (BOOT ボタンなし)
minicom -D /dev/ttyUSB0 -b 115200
```

- [ ] minicom が起動した
- [ ] `nsh>` プロンプトが表示された

### 10.2 アプリ確認

```bash
nsh> help
```

**期待される出力**:
```
Builtin Apps:
    {app_name}     ← アプリ名が表示される ✅
    sh
    nsh
```

- [ ] ✅ **`help` でアプリ名が表示される** (最終目標!)
- [ ] アプリ名が正しい

### 10.3 アプリ起動確認

```bash
nsh> {app_name}
# または
nsh> {app_name} &
```

- [ ] アプリが起動した
- [ ] エラーが出ない
- [ ] 期待される動作をする

---

## ✅ 最終チェックリスト

### 必須項目

- [ ] Make.defs で `$(APPDIR)/` を使っていない
- [ ] NuttX/.config に設定が反映されている
- [ ] ビルドが成功した (`nuttx.spk` 生成)
- [ ] フラッシュが成功した
- [ ] `help` でアプリ名が表示される
- [ ] アプリが起動する

### トラブルシューティング

もし上記のいずれかで失敗した場合:
- [03_TROUBLESHOOTING.md](./03_TROUBLESHOOTING.md) を参照
- [01_BUILD_RULES.md](./01_BUILD_RULES.md) でルールを再確認

---

## 📊 チェックリスト進捗管理

### 完了したフェーズにチェック

- [ ] フェーズ1: プロジェクト構造作成
- [ ] フェーズ2: Kconfig 設定
- [ ] フェーズ3: Makefile 作成
- [ ] フェーズ4: Make.defs 作成
- [ ] フェーズ5: defconfig 設定
- [ ] フェーズ6: ソースコード実装
- [ ] フェーズ7: ビルド実行
- [ ] フェーズ8: NuttX/.config 確認
- [ ] フェーズ9: フラッシュ
- [ ] フェーズ10: 動作確認

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース**: security_camera + bmi160_orientation プロジェクト

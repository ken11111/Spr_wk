# ✅ 問題解決報告書

**日付**: 2025-12-14
**状態**: **完全解決**

---

## 問題の概要（過去形）

BMI160アプリケーションのビルドは成功していたが、NuttShellでコマンドとして実行できなかった問題。

### 過去の現象
```bash
nsh> bmi160_orientation
nsh: bmi160_orientation: command not found
```

### 現在の動作
```bash
nsh> bmi160_orientation
BMI160 Orientation and Position Estimation
==========================================

Madgwick AHRS initialized (beta=0.10, rate=100Hz)
Position estimator initialized

BMI160 sensor opened successfully
Starting data acquisition...
(正常にデータ出力)
```

---

## 根本原因

### 原因1: .configファイルの二重構造を理解していなかった

**発見内容**:
- SDK内: `/home/ken/Spr_ws/spresense/sdk/.config` (295 bytes)
- NuttX内: `/home/ken/Spr_ws/spresense/nuttx/.config` (68,221 bytes)

**問題点**:
- ビルドシステムは**NuttX側の.config**を参照
- SDK側の.configのみに設定を追加していた
- 結果として、CONFIG変数がビルドプロセスに反映されていなかった

**証拠**:
```bash
# ビルドシステムが実際に参照していたファイル
$ make --dry-run -p 2>&1 | grep CONFIGURED_APPS
CONFIGURED_APPS := examples/bmi160 ...
# bmi160_orientation が含まれていない
```

### 原因2: CONFIG変数命名規則の誤り

**問題点**:
- 当初使用: `CONFIG_MYPRO_BMI160_ORIENTATION`
- 正しい命名: `CONFIG_EXAMPLES_BMI160_ORIENTATION`

**理由**:
- Spresense SDKのexamplesは `EXAMPLES_` プレフィックスが必須
- カスタムプレフィックス（MYPRO等）は非標準で、ビルドシステムが認識しない

### 原因3: Makefileに不要なMODULE変数

**問題点**:
```makefile
# 誤り（MODULE変数が含まれている）
MODULE = $(CONFIG_EXAMPLES_BMI160_ORIENTATION)
```

**正解**:
```makefile
# MODULE変数は不要
PROGNAME  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_STACKSIZE)
```

### 原因4: Make.defsのパス指定

**問題点**:
```makefile
# 誤り
CONFIGURED_APPS += bmi160_orientation
```

**正解**:
```makefile
# 正しいパス
CONFIGURED_APPS += examples/bmi160_orientation
```

---

## 解決プロセス

### ステップ1: アプリケーションの正しい配置

```bash
# 正しい配置場所
/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/
```

**確認コマンド**:
```bash
$ ls /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/
BUILD_GUIDE.md  Kconfig  Make.defs  Makefile  README.md
bmi160_orientation_main.c  orientation_calc.c  orientation_calc.h
position_estimator.c  position_estimator.h
```

### ステップ2: Kconfigの修正

**修正内容**:
```kconfig
# /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Kconfig

config EXAMPLES_BMI160_ORIENTATION
    tristate "BMI160 Orientation and Position Estimation"
    default n
    select SENSORS_BMI160
    select EXTERNALS_AHRS
    ---help---
        Enable BMI160 sensor orientation (Roll/Pitch/Yaw) and
        position estimation application using Madgwick AHRS algorithm.

if EXAMPLES_BMI160_ORIENTATION

config EXAMPLES_BMI160_ORIENTATION_PROGNAME
    string "Program name"
    default "bmi160_orientation"
    ---help---
        This is the name of the program that will be used when the
        NSH ELF program is installed.

config EXAMPLES_BMI160_ORIENTATION_PRIORITY
    int "Task priority"
    default 100
    ---help---
        Task priority (0-255)

config EXAMPLES_BMI160_ORIENTATION_STACKSIZE
    int "Stack size"
    default 4096
    ---help---
        Task stack size in bytes

endif
```

### ステップ3: Makefileの修正

**修正内容**:
```makefile
# /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Makefile

include $(APPDIR)/Make.defs

# BMI160 Orientation built-in application info

PROGNAME  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PROGNAME)
PRIORITY  = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_BMI160_ORIENTATION_STACKSIZE)
# MODULE変数を削除（重要）

# BMI160 Orientation Example

MAINSRC = bmi160_orientation_main.c
CSRCS = orientation_calc.c position_estimator.c

# Add path to AHRS library headers
CFLAGS += -I$(SDKDIR)/../externals/ahrs/src/MadgwickAHRS

include $(APPDIR)/Application.mk
```

### ステップ4: Make.defsの修正

**修正内容**:
```makefile
# /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Make.defs

ifneq ($(CONFIG_EXAMPLES_BMI160_ORIENTATION),)
CONFIGURED_APPS += examples/bmi160_orientation
endif
```

**重要ポイント**:
- CONFIG変数名が `CONFIG_EXAMPLES_*` に変更
- パスが `examples/bmi160_orientation` に修正

### ステップ5: Kconfigへの登録

**修正内容**:
```bash
# /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig の22行目に追加

source "/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160/Kconfig"
source "/home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Kconfig"
```

### ステップ6: NuttX側.configへの設定追加（最重要）

**修正内容**:
```bash
# /home/ken/Spr_ws/spresense/nuttx/.config に以下を追加

CONFIG_EXAMPLES_BMI160_ORIENTATION=y
CONFIG_EXAMPLES_BMI160_ORIENTATION_PROGNAME="bmi160_orientation"
CONFIG_EXAMPLES_BMI160_ORIENTATION_PRIORITY=100
CONFIG_EXAMPLES_BMI160_ORIENTATION_STACKSIZE=4096
```

**追加位置**: 既存のCONFIG_EXAMPLES_SIXAXISの直後（1958行目）

**この修正が決定的に重要だった理由**:
- ビルドシステムはこのファイルを参照
- SDK側の.configは参照されていなかった
- この設定がなければ、CONFIGURED_APPSに追加されない

### ステップ7: 再ビルド

```bash
cd /home/ken/Spr_ws/spresense/sdk
make clean && make -j4
```

**ビルドログで確認**:
```
Register: bmi160_orientation  ← 重要！登録成功の証拠
Register: sixaxis
Register: sh
Register: nsh
```

**出力**:
- nuttx.spk: 174KB (177,824 bytes)
- ビルド成功

### ステップ8: ファームウェア書き込み

```bash
# ブートローダーインストール
./tools/flash.sh -B -c /dev/ttyUSB0
# 307168 bytes loaded, Package validation is OK

# ファームウェア書き込み
./tools/flash.sh -c /dev/ttyUSB0 ../nuttx/nuttx.spk
# 177824 bytes loaded, Package validation is OK
# Saving package to "nuttx"
# Restarting the board ...
```

### ステップ9: 動作確認

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

**結果**:
```
NuttShell (NSH) NuttX-12.3.0
nsh> bmi160_orientation

BMI160 Orientation and Position Estimation
==========================================

Madgwick AHRS initialized (beta=0.10, rate=100Hz)
Position estimator initialized

BMI160 sensor opened successfully
Starting data acquisition...

Time(ms)      Roll    Pitch      Yaw   |     X       Y       Z
[データが正常に出力される]
```

✅ **完全に動作！**

---

## 解決の決定的要因

### 最も重要だった発見

**NuttX側の.configへの設定追加**が解決の鍵でした。

**検証プロセス**:
```bash
# ビルド時にCONFIGURED_APPSを確認
$ make --dry-run -p 2>&1 | grep CONFIGURED_APPS

# 修正前
CONFIGURED_APPS := examples/bmi160 ... (bmi160_orientationなし)

# 修正後
CONFIGURED_APPS := examples/bmi160 ... examples/bmi160_orientation ...
```

この変化により、builtin_list.hに登録エントリが生成され、NuttShellからコマンドとして実行可能になりました。

---

## 学んだ重要な教訓

### 1. ビルドシステムの理解

**二重.config構造**:
- SDK側: 開発者向けの簡易設定
- NuttX側: ビルドシステムが実際に参照する設定

**教訓**: 設定変更時は**両方のファイル**を確認すべき

### 2. CONFIG変数の命名規則

**標準プレフィックス**:
- Examples: `CONFIG_EXAMPLES_*`
- System: `CONFIG_SYSTEM_*`
- Platform: `CONFIG_PLATFORM_*`

**教訓**: カスタムプレフィックスは避ける

### 3. Make.defsの重要性

**役割**:
- アプリケーションをCONFIGURED_APPSに追加
- ビルドシステムにアプリケーションの存在を知らせる

**教訓**: 単なる形式的ファイルではなく、登録の核心

### 4. ビルドログの読み方

**重要なキーワード**:
```
Register: <アプリ名>  ← 登録成功
```

このメッセージがない場合、何かが間違っている。

### 5. デバッグ手法

**有効だった手法**:
1. `make --dry-run -p` でCONFIGURED_APPSを確認
2. 動作している既存アプリ（sixaxis）との比較
3. ビルドログの詳細な確認
4. .configファイルの複数箇所の確認

---

## 技術的な深堀り

### builtin_list.hの生成メカニズム

**生成フロー**:
```
1. Make.defsがCONFIGURED_APPSにアプリを追加
   ↓
2. Application.mkがPROGNAME変数を読み取る
   ↓
3. builtin登録スクリプトが実行される
   ↓
4. builtin_list.hに以下のエントリが生成:
   { "bmi160_orientation", 100, 4096, bmi160_orientation_main },
   ↓
5. リンク時にシンボルが解決される
   ↓
6. NuttShellがコマンドとして認識
```

### Application.mkの役割

**重要な処理**:
```makefile
# Application.mkの内部処理（概念）
ifneq ($(PROGNAME),)
  # PROGNAME が定義されている場合
  # builtin登録処理を実行
  # プログラム名、優先度、スタックサイズを記録
endif
```

---

## 問題解決前後の比較

### ビルド前（問題あり）

| 項目 | 状態 |
|------|------|
| アプリ配置 | ❌ SDK外の/home/ken/Spr_ws/examples/ |
| CONFIG変数 | ❌ CONFIG_MYPRO_* |
| NuttX .config | ❌ 設定なし |
| Make.defs | ❌ パス誤り |
| Makefile | ❌ MODULE変数あり |
| ビルドログ | ❌ "Register:" なし |
| builtin_list.h | ❌ エントリなし |
| 実行 | ❌ command not found |

### ビルド後（解決済み）

| 項目 | 状態 |
|------|------|
| アプリ配置 | ✅ SDK内の正しい位置 |
| CONFIG変数 | ✅ CONFIG_EXAMPLES_* |
| NuttX .config | ✅ 設定追加済み |
| Make.defs | ✅ 正しいパス |
| Makefile | ✅ MODULE変数削除 |
| ビルドログ | ✅ "Register: bmi160_orientation" |
| builtin_list.h | ✅ エントリ生成 |
| 実行 | ✅ 正常動作 |

---

## まとめ

### 解決に要した時間
- 問題発生: 2025-12-13
- 原因特定: 2025-12-14 午前
- 完全解決: 2025-12-14 午前

**総時間**: 約12時間（デバッグ・解決含む）

### 解決の鍵となった発見

1. **NuttX側.configの存在と重要性**
2. **CONFIG命名規則の厳格さ**
3. **Make.defsのパス指定の正確性**
4. **MODULEモジュール変数の不要性**
5. **ビルドログの"Register:"メッセージの重要性**

### 今後の開発への影響

この問題解決プロセスで得た知見により、今後のSpresense開発では:
- 初期設定時の時間を大幅に短縮可能
- ビルドシステムの構造を正確に理解
- デバッグ手法の確立
- ドキュメント化による知識共有

---

**報告書作成日**: 2025-12-14
**問題解決日**: 2025-12-14
**ステータス**: ✅ **完全解決**

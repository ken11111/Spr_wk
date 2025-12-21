# Spresense ビルド トラブルシューティング

**最終更新**: 2025-12-16
**目的**: ビルドエラーと実行時エラーの迅速な解決

---

## 📖 目次

1. [アプリが help に表示されない](#1-アプリが-help-に表示されない)
2. [ビルドエラー](#2-ビルドエラー)
3. [リンカエラー](#3-リンカエラー)
4. [CONFIG エラー](#4-config-エラー)
5. [フラッシュエラー](#5-フラッシュエラー)
6. [実行時エラー](#6-実行時エラー)
7. [環境エラー](#7-環境エラー)

---

## 1. アプリが help に表示されない

### 症状

```bash
nsh> help
Builtin Apps:
    sh
    nsh
```

アプリ名が表示されない

### 原因と解決策

#### 原因1: Make.defs で $(APPDIR)/ を使用している ⭐ 最頻出!

**確認方法**:
```bash
cat /home/ken/Spr_ws/spresense/sdk/apps/examples/{app_name}/Make.defs
```

**間違った記述**:
```makefile
CONFIGURED_APPS += $(APPDIR)/examples/{app_name}
```

**解決策**:
```makefile
# $(APPDIR)/ を削除
CONFIGURED_APPS += examples/{app_name}
```

**修正後の手順**:
```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

**参考**: [01_BUILD_RULES.md - Make.defs ルール](./01_BUILD_RULES.md#4-makedefs-ルール-最重要)

---

#### 原因2: examples/Kconfig に source 行がない

**確認方法**:
```bash
grep "{app_name}" /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig
```

**解決策**:
```bash
# examples/Kconfig を編集
nano /home/ken/Spr_ws/spresense/sdk/apps/examples/Kconfig

# 以下を追加
source "examples/{app_name}/Kconfig"
```

---

#### 原因3: defconfig に設定がない

**確認方法**:
```bash
grep "CONFIG_EXAMPLES_{APP_NAME_UPPER}" /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig
```

**解決策**:
```bash
# defconfig を編集
nano /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig

# 以下を追加
CONFIG_EXAMPLES_{APP_NAME_UPPER}=y
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PROGNAME="{app_name}"
CONFIG_EXAMPLES_{APP_NAME_UPPER}_PRIORITY=100
CONFIG_EXAMPLES_{APP_NAME_UPPER}_STACKSIZE=2048
```

---

#### 原因4: NuttX/.config に設定が反映されていない

**確認方法**:
```bash
grep "CONFIG_EXAMPLES_{APP_NAME_UPPER}" /home/ken/Spr_ws/spresense/nuttx/.config
```

**解決策**:
```bash
# defconfig を編集後、再ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make

# 確認
grep "CONFIG_EXAMPLES_{APP_NAME_UPPER}" /home/ken/Spr_ws/spresense/nuttx/.config
```

**注意**: SDK/.config ではなく、NuttX/.config を確認すること!

---

#### 原因5: フラッシュしていない / 古いファームウェア

**解決策**:
```bash
# 最新のファームウェアをフラッシュ
cd /home/ken/Spr_ws/spresense/sdk
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# Spresense を再起動
# minicom で接続し直して help 確認
```

---

#### 原因6: config.py で設定を再適用していない ⭐ 重要!

**症状**:
- defconfig に設定を追加した
- Make.defs も正しい
- ビルドも成功した
- **でも `help` にアプリが表示されない**

**確認方法**:
```bash
# バイナリにアプリが含まれているか確認
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
arm-none-eabi-nm /home/ken/Spr_ws/spresense/nuttx/nuttx | grep "{app_name}_main"

# 何も表示されなければ、アプリがリンクされていない
```

**根本原因**:
defconfig と NuttX/.config の間で内部状態が不整合。単純な `make` だけでは設定が完全に反映されない場合がある。

**解決策**: `config.py default` を実行してから再ビルド

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# 1. 設定を完全に再適用 (重要!)
./tools/config.py default

# 2. ビルド
make

# 3. 確認: バイナリにアプリが含まれているか
arm-none-eabi-nm /home/ken/Spr_ws/spresense/nuttx/nuttx | grep "{app_name}_main"
# → {app_name}_main が表示されれば成功 ✅

# 4. フラッシュ
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

**ベストプラクティス**:
defconfig や .config を変更した後は、**必ず** `./tools/config.py default` を実行してから `make` すること!

**参考**: Security Camera プロジェクトで実際に発生したケース (2025-12-16)

---

## 2. ビルドエラー

### エラー: make: command not found

**症状**:
```bash
$ make
make: command not found
```

**原因**: PATH に make が含まれていない

**解決策**:
```bash
# 完全な PATH を設定
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# または絶対パスで実行
/usr/bin/make
```

---

### エラー: arm-none-eabi-gcc: not found

**症状**:
```
/bin/sh: 1: arm-none-eabi-gcc: not found
```

**原因**: ツールチェインが PATH に含まれていない

**解決策**:
```bash
# PATH にツールチェインを追加
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH

# 確認
which arm-none-eabi-gcc
# → /home/ken/spresenseenv/usr/bin/arm-none-eabi-gcc
```

**恒久的な対策** (オプション):
```bash
# ~/.bashrc に追加
echo 'export PATH=/home/ken/spresenseenv/usr/bin:$PATH' >> ~/.bashrc
source ~/.bashrc
```

---

### エラー: No targets specified and no makefile found

**症状**:
```
make: *** No targets specified and no makefile found.  Stop.
```

**原因**: 間違ったディレクトリで make を実行

**解決策**:
```bash
# 正しいディレクトリに移動
cd /home/ken/Spr_ws/spresense/sdk

# 確認
ls Makefile
# → Makefile が存在することを確認

# ビルド実行
make
```

---

### エラー: No rule to make target '/Make.defs'

**症状**:
```
tools/Unix.mk:33: /Make.defs: No such file or directory
make[1]: *** No rule to make target '/Make.defs'.  Stop.
```

**原因**: 設定が壊れている or クリーンビルドが必要

**解決策**:
```bash
cd /home/ken/Spr_ws/spresense/sdk

# 設定を再生成
./tools/config.py default

# ビルド
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

---

## 3. リンカエラー

### エラー: undefined reference to `function_name'

**症状**:
```
/home/ken/spresenseenv/usr/bin/../lib/gcc/arm-none-eabi/...
undefined reference to `my_function'
collect2: error: ld returned 1 exit status
```

**原因**: 関数の実装が見つからない

**解決策1**: ファイルが Makefile の CSRCS に含まれているか確認

```makefile
# Makefile を確認
CSRCS  = module1.c
CSRCS += module2.c  ← 実装ファイルが含まれているか?
```

**解決策2**: 関数宣言とプロトタイプが一致しているか確認

```c
// ヘッダー (.h)
void my_function(int param);

// 実装 (.c)
void my_function(int param)  // 引数の型が一致
{
    // ...
}
```

---

### エラー: multiple definition of `main'

**症状**:
```
multiple definition of `main'
```

**原因**: MAINSRC を CSRCS にも含めている

**解決策**:
```makefile
# ❌ 間違い
CSRCS = app_main.c module1.c
MAINSRC = app_main.c  # 重複!

# ✅ 正しい
CSRCS = module1.c     # main を含むファイルは除外
MAINSRC = app_main.c
```

---

## 4. CONFIG エラー

### エラー: 設定が反映されない

**症状**: defconfig に追加したのに、ビルド時に反映されない

**原因**: SDK/.config を見ている (間違い)

**解決策**: NuttX/.config を確認

```bash
# ❌ 間違い: SDK側
cat /home/ken/Spr_ws/spresense/sdk/.config

# ✅ 正しい: NuttX側
cat /home/ken/Spr_ws/spresense/nuttx/.config | grep MY_CONFIG
```

**再ビルドで反映**:
```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

---

### エラー: CONFIG_XXX is not set

**症状**: 必要な機能が無効になっている

```bash
# CONFIG_VIDEO is not set
```

**解決策**: defconfig に追加

```bash
# defconfig を編集
nano /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig

# 以下を追加
CONFIG_VIDEO=y
CONFIG_VIDEO_STREAM=y
```

**再ビルド**:
```bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py default  # 設定を再適用
make
```

---

## 5. フラッシュエラー

### エラー: /dev/ttyUSB0: No such file or directory

**症状**:
```
Cannot open /dev/ttyUSB0: No such file or directory
```

**原因**: Spresense が接続されていない or ブートローダーモードでない

**解決策**:

1. **BOOT ボタンを押しながら USB 接続**
2. **デバイス確認**:
   ```bash
   ls -l /dev/ttyUSB*
   # → /dev/ttyUSB0 が表示されるはず
   ```

3. **dmesg 確認**:
   ```bash
   dmesg | tail -20
   # → USB デバイスの接続ログを確認
   ```

---

### エラー: Permission denied

**症状**:
```
/dev/ttyUSB0: Permission denied
```

**原因**: パーミッション不足

**一時的な解決策**:
```bash
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

**恒久的な解決策**:
```bash
# dialout グループに追加
sudo usermod -a -G dialout $USER

# ログアウト・ログイン (または再起動)

# 確認
groups
# → dialout が含まれているはず

# 以降は sudo 不要
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

### エラー: Device or resource busy

**症状**:
```
/dev/ttyUSB0: Device or resource busy
```

**原因**: 別のプログラムがシリアルポートを使用中

**解決策**:

1. **使用中のプログラムを確認**:
   ```bash
   lsof /dev/ttyUSB0
   # または
   fuser /dev/ttyUSB0
   ```

2. **minicom を終了**:
   - minicom 内で `Ctrl+A` → `X`
   - または別の端末から `killall minicom`

3. **プロセスを強制終了**:
   ```bash
   sudo kill -9 <PID>
   ```

---

### エラー: Package validation failed

**症状**:
```
Package validation failed
```

**原因**: nuttx.spk ファイルが壊れている

**解決策**:
```bash
# 再ビルド
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make clean
make

# 確認
ls -lh nuttx.spk
# → ファイルサイズが妥当か確認 (通常 100KB 以上)

# 再フラッシュ
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 6. 実行時エラー

### エラー: アプリが起動しない

**症状**:
```bash
nsh> my_app
nsh: my_app: command not found
```

**原因**: アプリが登録されていない

**解決策**: [1. アプリが help に表示されない](#1-アプリが-help-に表示されない) を参照

---

### エラー: Segmentation Fault

**症状**:
```
Segmentation Fault
```

**原因**: メモリアクセス違反 (NULL ポインタ, 配列外アクセスなど)

**デバッグ方法**:

1. **printf デバッグ**:
   ```c
   printf("DEBUG: Checkpoint 1\n");
   // 問題の箇所
   printf("DEBUG: Checkpoint 2\n");
   ```

2. **スタックサイズを増やす**:
   ```bash
   # defconfig を編集
   CONFIG_EXAMPLES_MY_APP_STACKSIZE=8192  # 2048 → 8192
   ```

3. **コードレビュー**:
   - NULL ポインタチェック
   - 配列の境界チェック
   - メモリ初期化

---

### エラー: Stack Overflow

**症状**:
```
Stack overflow detected
```

**原因**: スタックサイズ不足

**解決策**:
```bash
# defconfig を編集
nano /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig

# スタックサイズを増やす
CONFIG_EXAMPLES_MY_APP_STACKSIZE=8192  # デフォルト: 2048

# 再ビルド・フラッシュ
cd /home/ken/Spr_ws/spresense/sdk
make
sudo tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

---

## 7. 環境エラー

### エラー: The system library `libudev` was not found

**症状** (Rust開発時):
```
error: failed to run custom build command for `libudev-sys v0.1.4`
The system library `libudev` required by crate `libudev-sys` was not found.
```

**原因**: libudev 開発パッケージがインストールされていない

**解決策**:
```bash
sudo apt-get update
sudo apt-get install -y libudev-dev pkg-config
```

---

### エラー: git not found

**症状**:
```
git: command not found
```

**解決策**:
```bash
sudo apt-get update
sudo apt-get install -y git
```

---

### エラー: python3 not found

**症状**:
```
python3: command not found
```

**解決策**:
```bash
sudo apt-get update
sudo apt-get install -y python3 python3-pip
```

---

## 🔧 デバッグテクニック

### 1. ビルドログの保存

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make 2>&1 | tee /tmp/build.log

# エラー箇所を検索
grep -i "error" /tmp/build.log
```

### 2. 設定の確認

```bash
# NuttX設定を確認
grep -i "MY_APP" /home/ken/Spr_ws/spresense/nuttx/.config

# defconfig と比較
diff /home/ken/Spr_ws/spresense/sdk/configs/default/defconfig \
     <(grep -i "MY_APP" /home/ken/Spr_ws/spresense/nuttx/.config)
```

### 3. 成功例との比較

```bash
# 成功しているアプリのファイルと比較
diff /home/ken/Spr_ws/spresense/sdk/apps/examples/bmi160_orientation/Make.defs \
     /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Make.defs
```

### 4. シンボルの確認 (リンカエラー時)

```bash
# オブジェクトファイルのシンボルを確認
arm-none-eabi-nm /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/module1.o

# 未定義シンボルを探す
arm-none-eabi-nm /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/*.o | grep "U "
```

---

## 📚 よくある質問

### Q1: SDK/.config と NuttX/.config の違いは?

**A**:
- SDK/.config: SDK側のメタ情報 (小さい: 295-400B) - **使われない**
- NuttX/.config: 実際の設定ファイル (大きい: 60-70KB) - **使われる**

詳細: [01_BUILD_RULES.md - 2重コンフィグ構造](./01_BUILD_RULES.md#6-2重コンフィグ構造-最重要)

### Q2: $(APPDIR)/ を使ってはいけない理由は?

**A**: ビルドシステムが内部で既に $(APPDIR)/ を補完するため、二重になってパスが壊れる

詳細: [01_BUILD_RULES.md - Make.defs ルール](./01_BUILD_RULES.md#4-makedefs-ルール-最重要)

### Q3: ビルドは成功するのにアプリが表示されない

**A**: ほぼ100%、Make.defs で `$(APPDIR)/` を使っているのが原因

**確認**:
```bash
cat /home/ken/Spr_ws/spresense/sdk/apps/examples/my_app/Make.defs | grep CONFIGURED_APPS
# → $(APPDIR)/ が含まれていたら削除
```

---

## 🚨 緊急時の対処

### 完全にビルドが壊れた場合

```bash
# 1. 完全クリーン
cd /home/ken/Spr_ws/spresense/sdk
make distclean

# 2. 設定を再生成
./tools/config.py default

# 3. 再ビルド
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin:$PATH
make
```

**警告**: `make distclean` は時間がかかります (10分程度)

---

**作成日**: 2025-12-16
**作成者**: Claude Code (Sonnet 4.5)
**ベース**: security_camera + bmi160_orientation トラブルシューティング経験

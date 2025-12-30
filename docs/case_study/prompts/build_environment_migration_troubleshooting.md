# ビルド環境移行トラブルシューティングガイド

## 概要

このドキュメントは、Spresense開発環境をGit管理下のディレクトリ（GH_wk_test）に移行する際に遭遇したトラブルと解決策をまとめたものです。

**対象バージョン**: Spresense SDK v3.3.0
**作成日**: 2025-12-28
**ケース**: Phase 1.5 VGAビルド環境の完全移行

---

## トラブル事例と解決策

### 1. kconfig-conf コマンドが見つからない

#### 症状
```
/usr/bin/bash: kconfig-conf: command not found
make[1]: *** [tools/Unix.mk:680: olddefconfig] Error 127
```

#### 原因
- NuttXビルドシステムの設定ツールが初期化されていない
- `.config`ファイルが存在しない状態で`make`を実行

#### 解決策
**推奨**: defconfigファイルから設定を開始する
```bash
cd /path/to/spresense/sdk
./tools/config.py examples/camera  # カメラサンプルの設定を適用
```

**代替案**: 既存の作業環境から必要なファイルをコピー
```bash
# Make.defsをコピー（ツールチェーン設定を含む）
cp /working/env/spresense/nuttx/Make.defs /new/env/spresense/nuttx/

# .configをコピー（後でカスタマイズ）
cp /working/env/spresense/nuttx/.config /new/env/spresense/nuttx/
```

#### 教訓
- **ゼロから構築しない**: 既存のdefconfigを活用する
- **段階的移行**: 動作する設定から始めて、必要な部分のみ変更

---

### 2. システムgccがクロスコンパイラの代わりに使用される

#### 症状
```
gcc: error: unrecognized command line option '-mfloat-abi=soft'
gcc: error: unrecognized argument in option '-mabi=aapcs'
```

#### 原因
- `Make.defs`に`CROSSDEV`変数が設定されていない
- システムのgcc（x86_64用）がARMコード用に使用されている

#### 解決策
`Make.defs`に以下を追加:
```makefile
# Toolchain configuration
CROSSDEV = arm-none-eabi-
```

**追加場所**: ToolchainのincludeディレクティブとArchitecture設定の間
```makefile
include $(TOPDIR)/.config
include $(TOPDIR)/tools/Config.mk
include $(TOPDIR)/tools/cxd56/Config.mk
include $(TOPDIR)/arch/arm/src/armv7-m/Toolchain.defs

# Toolchain configuration
CROSSDEV = arm-none-eabi-  # ← ここに追加

# Setup for the kind of memory that we are executing from
```

#### 検証方法
```bash
# ビルドログでarm-none-eabi-gccが使用されていることを確認
make 2>&1 | grep "arm-none-eabi-gcc"
```

---

### 3. アーキテクチャ固有ファイルが見つからない

#### 症状
```
make[2]: *** No rule to make target 'arm_exception.S', needed by '.depend'.  Stop.
```

#### 原因
- `.config`に`CONFIG_ARCH_ARMV7M`が設定されていない
- `ARCH_SUBDIR`が正しく設定されず、ビルドシステムがARMv7-Mディレクトリを見つけられない

#### アーキテクチャ依存関係
CXD56xxチップは以下の設定が必要:
```
CONFIG_ARCH_CORTEXM4=y    # Cortex-M4プロセッサ
CONFIG_ARCH_ARMV7M=y      # ARMv7-Mアーキテクチャ
```

#### 解決策
`.config`ファイルに追加:
```bash
echo "CONFIG_ARCH_CORTEXM4=y" >> /path/to/nuttx/.config
echo "CONFIG_ARCH_ARMV7M=y" >> /path/to/nuttx/.config
```

#### ビルドシステムの動作
`arch/arm/src/Makefile`の該当部分:
```makefile
ifeq ($(CONFIG_ARCH_ARMV7M),y)     # ARMv7-M
ARCH_SUBDIR = armv7-m
else ifeq ($(CONFIG_ARCH_ARMV8M),y)     # ARMv8-M
ARCH_SUBDIR = armv8-m
...
```

---

### 4. ライブラリ設定の欠落

#### 症状
```
error: 'CONFIG_LIBC_MAX_EXITFUNS' undeclared here (not in a function)
warning: CONFIG_TLS_NELEM is not defined [-Wcpp]
```

#### 原因
- 段階的に`.config`を構築する際、依存する設定値が抜け落ちる
- Kconfig の default 値が自動的に設定されない

#### 解決策
欠落した設定を追加:
```bash
cat >> /path/to/nuttx/.config <<'EOF'
CONFIG_LIBC_MAX_EXITFUNS=0
CONFIG_TLS_NELEM=0
EOF
```

#### デフォルト値の確認方法
```bash
# Kconfigファイルから default 値を探す
grep -A 5 "CONFIG_LIBC_MAX_EXITFUNS" nuttx/libs/libc/stdlib/Kconfig
```

出力例:
```
config LIBC_MAX_EXITFUNS
	int "Maximum amount of exit functions"
	default 0
	---help---
		Configure the amount of exit functions for atexit/on_exit.
```

---

### 5. SDIO/Work Queue設定の連鎖的依存関係

#### 症状
```
error: #error "This driver requires CONFIG_SDIO_BLOCKSETUP"
error: #error "Callback support requires CONFIG_SCHED_WORKQUEUE and CONFIG_SCHED_HPWORK"
error: #error Work queue support is required (CONFIG_SCHED_WORKQUEUE)
```

#### 原因
- SDカードサポートには複数の設定が必要
- Work queueはファイルシステムの自動マウントにも必要
- 依存関係が複雑で、手動での設定が困難

#### 解決策の変遷

**失敗したアプローチ**: 個別の設定を段階的に追加
```bash
# これでは依存関係を全て解決できない
echo "CONFIG_SCHED_WORKQUEUE=y" >> .config
echo "CONFIG_SDIO_BLOCKSETUP=y" >> .config
# ... 他にも多数の設定が必要
```

**成功したアプローチ**: defconfigを使用
```bash
# 完全な設定から始める
./tools/config.py examples/camera

# 必要な部分のみカスタマイズ
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config

# カスタムアプリの設定を追加
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF
```

#### 教訓
**完全な設定から開始 → 差分でカスタマイズ** の方が、**ゼロから構築** より確実

---

### 6. スタックサイズ警告オプションの値が空

#### 症状
```
arm-none-eabi-gcc: error: missing argument to '-Wstack-usage='
```

#### 原因
- `.config`に`CONFIG_STACK_USAGE_WARNING`が定義されていない
- コンパイラオプション`-Wstack-usage=`に値が渡されない

#### 解決策
```bash
echo "CONFIG_STACK_USAGE_WARNING=0" >> /path/to/nuttx/.config
```

値の意味:
- `0`: スタック使用量警告を無効化
- `> 0`: 指定バイト数以上のスタック使用で警告

---

## ベストプラクティス

### 1. 設定ファイルの管理

#### defconfigからの開始
```bash
# 最も近い機能のdefconfigを探す
find sdk/configs -name "defconfig" | grep camera

# 適用
cd sdk
./tools/config.py examples/camera
```

#### カスタマイズの記録
変更内容をスクリプトとして保存:
```bash
#!/bin/bash
# customize_config.sh

cd /path/to/nuttx

# 標準カメラアプリを無効化
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config

# カスタムアプリを有効化
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF
```

### 2. ビルドの検証

#### シンボルテーブルの確認
```bash
# アプリケーションのエントリポイントを確認
arm-none-eabi-nm nuttx/nuttx | grep "security_camera_main"

# 期待する関数が含まれているか確認
arm-none-eabi-nm nuttx/nuttx | grep -E "perf_logger|camera_manager"
```

#### ビルドログの保存
```bash
# エラーの詳細を後で確認できるようログを保存
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make 2>&1 | tee build.log

# エラーのみを抽出
grep -i "error:" build.log
```

### 3. PATH設定

#### 一貫したPATH設定
```bash
# ビルド環境で常に同じPATHを使用
export PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin

# または毎回指定
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make
```

#### PATHの検証
```bash
# arm-none-eabi-gccが見つかることを確認
which arm-none-eabi-gcc

# バージョン確認
arm-none-eabi-gcc --version
```

---

## クリーンビルドの手順

以下は、今回の経験から導かれた推奨手順です:

### ステップ1: 環境の準備
```bash
cd /path/to/GH_wk_test/spresense/sdk

# クリーンな状態から開始
make distclean
```

### ステップ2: 基本設定の適用
```bash
# 最も近い機能のdefconfigを使用
./tools/config.py examples/camera
```

### ステップ3: カスタマイズ
```bash
cd ../nuttx

# 標準アプリを無効化
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config

# カスタムアプリを有効化
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF
```

### ステップ4: ビルド
```bash
cd ../sdk

# PATH指定でビルド
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make 2>&1 | tee build.log
```

### ステップ5: 検証
```bash
# ファームウェアが生成されたか確認
ls -lh ../nuttx/nuttx.spk

# シンボルを確認
arm-none-eabi-nm ../nuttx/nuttx | grep "security_camera_main"
arm-none-eabi-nm ../nuttx/nuttx | grep -E "perf_logger|camera_manager"
```

---

## トラブルシューティングチェックリスト

ビルドエラーが発生した場合、以下を順番に確認:

- [ ] **1. PATH設定**: arm-none-eabi-gccにパスが通っているか
  ```bash
  which arm-none-eabi-gcc
  ```

- [ ] **2. Make.defs**: CROSSDEVが設定されているか
  ```bash
  grep "CROSSDEV" nuttx/Make.defs
  ```

- [ ] **3. アーキテクチャ設定**: CONFIG_ARCH_CORTEXM4とCONFIG_ARCH_ARMV7Mが設定されているか
  ```bash
  grep -E "CONFIG_ARCH_CORTEXM4|CONFIG_ARCH_ARMV7M" nuttx/.config
  ```

- [ ] **4. 基本ライブラリ設定**: CONFIG_LIBC_MAX_EXITFUNSとCONFIG_TLS_NELEMが設定されているか
  ```bash
  grep -E "CONFIG_LIBC_MAX_EXITFUNS|CONFIG_TLS_NELEM" nuttx/.config
  ```

- [ ] **5. Work Queue設定**: CONFIG_SCHED_WORKQUEUEが設定されているか
  ```bash
  grep "CONFIG_SCHED_WORKQUEUE" nuttx/.config
  ```

- [ ] **6. SDIO設定**: CONFIG_SDIO_BLOCKSETUPが設定されているか
  ```bash
  grep "CONFIG_SDIO_BLOCKSETUP" nuttx/.config
  ```

- [ ] **7. スタック警告設定**: CONFIG_STACK_USAGE_WARNINGが設定されているか
  ```bash
  grep "CONFIG_STACK_USAGE_WARNING" nuttx/.config
  ```

---

## 設定の依存関係マップ

### 📊 視覚的な依存関係図

**PlantUML図で詳細を確認**: [config_dependencies.puml](../../diagrams/config_dependencies.puml)

より詳しい依存関係は、PlantUML図で視覚的に確認できます。図には以下の情報が含まれています：
- 自動選択される設定と手動設定が必要な設定の区別
- 必須依存とオプション依存の区別
- ツールチェーン設定への影響
- アプリケーション設定とシステム設定の関係

### テキスト版の依存関係マップ

```
CXD56xx (Spresense)
  │
  ├─ CONFIG_ARCH_CHIP_CXD56XX=y
  │   └─ 自動的に選択: CONFIG_ARCH_CORTEXM4=y
  │
  ├─ CONFIG_ARCH_CORTEXM4=y
  │   └─ ツールチェーン: armv7e-m, cortex-m4
  │
  ├─ CONFIG_ARCH_ARMV7M=y
  │   └─ ARCH_SUBDIR = armv7-m (ソースディレクトリ)
  │
  ├─ SDカードサポート
  │   ├─ CONFIG_CXD56_SDIO=y
  │   ├─ CONFIG_SDIO_BLOCKSETUP=y
  │   ├─ CONFIG_SCHED_WORKQUEUE=y
  │   └─ CONFIG_SCHED_HPWORK=y
  │
  ├─ ファイルシステム自動マウント
  │   ├─ CONFIG_FS_AUTOMOUNTER=y
  │   └─ CONFIG_SCHED_WORKQUEUE=y (上記と共通)
  │
  └─ カメラサポート
      ├─ CONFIG_VIDEO=y
      ├─ CONFIG_VIDEO_STREAM=y
      ├─ CONFIG_VIDEO_ISX012=y または CONFIG_VIDEO_ISX019=y
      ├─ CONFIG_CXD56_CISIF=y
      └─ CONFIG_CXD56_I2C2=y
```

---

## 参考情報

### 📊 PlantUML図（視覚的理解）
- [config_dependencies.puml](../../diagrams/config_dependencies.puml) - CONFIG依存関係図
- [build_system_components.puml](../../diagrams/build_system_components.puml) - ビルドシステムコンポーネント図
- [build_flow.puml](../../diagrams/build_flow.puml) - ビルドフロー図
- [troubleshooting_flow.puml](../../diagrams/troubleshooting_flow.puml) - トラブルシューティングフロー図
- [directory_structure.puml](../../diagrams/directory_structure.puml) - ディレクトリ構造図
- [diagrams/README.md](../../diagrams/README.md) - 図の使い方と説明

### 関連ドキュメント
- [camera_lessons_learned.md](camera_lessons_learned.md) - カメラアプリケーション開発の教訓
- [camera_config_reference.md](camera_config_reference.md) - カメラ設定リファレンス
- [build_troubleshooting_cheatsheet.md](build_troubleshooting_cheatsheet.md) - クイックリファレンス
- [build_rules/](build_rules/) - ビルドルールの詳細

### Spresense公式ドキュメント
- [Spresense SDK チュートリアル](https://developer.sony.com/develop/spresense/docs/sdk_tutorials_ja.html)
- [NuttX ビルドシステム](https://nuttx.apache.org/docs/latest/components/build.html)

### デバッグコマンド
```bash
# 設定の差分確認
diff -u /working/env/.config /new/env/.config

# 有効な設定のみ表示
grep "^CONFIG_" .config | grep "=y"

# コメントアウトされた設定を表示
grep "^# CONFIG_" .config

# 特定の機能の設定を検索
grep -r "CONFIG_VIDEO" sdk/configs/examples/camera/
```

---

## まとめ

### 重要な教訓

1. **defconfigから始める**: ゼロから設定を構築するのではなく、既存の動作する設定をベースにする
2. **依存関係を理解する**: 一つの機能が複数の設定に依存することを認識する
3. **段階的に検証**: 大きな変更を一度に行わず、小さなステップで確認しながら進める
4. **ログを保存**: トラブル時の調査のため、ビルドログを必ず保存する
5. **検証を自動化**: シンボルテーブルの確認などを自動化して、ビルド結果を検証する

### 時間節約のポイント

- **defconfig使用**: 個別設定の追加で30分以上 → defconfigベースで5分以内
- **完全なログ**: エラー時の再現に必要な情報を初回から取得
- **検証スクリプト**: ビルド成功後の確認作業を自動化

### 次のステップ

1. この手順を自動化スクリプト化
2. CI/CDパイプラインへの統合
3. 他の構成（例: Phase 2.0）への適用

---

**更新履歴**:
- 2025-12-28: 初版作成（Phase 1.5 VGAビルド環境移行の経験から）

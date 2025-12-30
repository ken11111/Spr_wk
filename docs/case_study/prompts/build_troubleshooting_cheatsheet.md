# Spresenseビルド トラブルシューティング チートシート

## クイックリファレンス

### 🚀 クリーンビルド（5分手順）

```bash
# 1. クリーン
cd /path/to/spresense/sdk
make distclean

# 2. 設定適用
./tools/config.py examples/camera

# 3. カスタマイズ
cd ../nuttx
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF

# 4. ビルド
cd ../sdk
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make 2>&1 | tee build.log

# 5. 検証
ls -lh ../nuttx/nuttx.spk
arm-none-eabi-nm ../nuttx/nuttx | grep "security_camera_main"
```

---

## 🔥 よくあるエラーと即座の解決策

### エラー: `kconfig-conf: command not found`

```bash
# 解決策: defconfigを使用
./tools/config.py examples/camera
```

### エラー: `gcc: error: unrecognized command line option '-mfloat-abi=soft'`

```bash
# 解決策: Make.defsにCROSSDEVを追加
cat >> nuttx/Make.defs <<'EOF'

# Toolchain configuration
CROSSDEV = arm-none-eabi-
EOF
```

### エラー: `No rule to make target 'arm_exception.S'`

```bash
# 解決策: アーキテクチャ設定を追加
cat >> nuttx/.config <<'EOF'
CONFIG_ARCH_CORTEXM4=y
CONFIG_ARCH_ARMV7M=y
EOF
```

### エラー: `CONFIG_LIBC_MAX_EXITFUNS' undeclared`

```bash
# 解決策: ライブラリ設定を追加
cat >> nuttx/.config <<'EOF'
CONFIG_LIBC_MAX_EXITFUNS=0
CONFIG_TLS_NELEM=0
EOF
```

### エラー: `This driver requires CONFIG_SDIO_BLOCKSETUP`

```bash
# 解決策: SDIO/Work Queue設定を追加
cat >> nuttx/.config <<'EOF'
CONFIG_SCHED_WORKQUEUE=y
CONFIG_SCHED_HPWORK=y
CONFIG_SDIO_BLOCKSETUP=y
EOF
```

### エラー: `missing argument to '-Wstack-usage='`

```bash
# 解決策: スタック警告設定を追加
echo "CONFIG_STACK_USAGE_WARNING=0" >> nuttx/.config
```

---

## 🔍 診断コマンド

### ツールチェーン確認
```bash
# ARM GCCが見つかるか
which arm-none-eabi-gcc

# バージョン確認
arm-none-eabi-gcc --version

# PATH確認
echo $PATH | tr ':' '\n' | grep spresense
```

### 設定確認
```bash
# 必須設定が揃っているか
grep -E "CONFIG_ARCH_CORTEXM4|CONFIG_ARCH_ARMV7M|CONFIG_SCHED_WORKQUEUE|CONFIG_SDIO_BLOCKSETUP" nuttx/.config

# アプリ設定確認
grep "CONFIG_EXAMPLES_SECURITY_CAMERA" nuttx/.config

# Make.defsのCROSSDEV確認
grep "CROSSDEV" nuttx/Make.defs
```

### ビルド結果確認
```bash
# ファームウェア生成確認
ls -lh nuttx/nuttx.spk

# アプリがビルドされたか
arm-none-eabi-nm nuttx/nuttx | grep "security_camera_main"

# 機能が含まれているか
arm-none-eabi-nm nuttx/nuttx | grep -E "perf_logger|camera_manager"

# エラーのみ抽出
grep -i "error:" build.log | head -20
```

---

## 📋 設定チェックリスト

コピー＆ペーストで実行できる完全チェック:

```bash
#!/bin/bash
echo "=== Spresenseビルド環境チェック ==="

echo -n "1. ARM GCC: "
which arm-none-eabi-gcc && echo "✓ OK" || echo "✗ NG"

echo -n "2. CROSSDEV設定: "
grep -q "CROSSDEV.*arm-none-eabi" nuttx/Make.defs && echo "✓ OK" || echo "✗ NG"

echo -n "3. Cortex-M4設定: "
grep -q "CONFIG_ARCH_CORTEXM4=y" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "4. ARMv7-M設定: "
grep -q "CONFIG_ARCH_ARMV7M=y" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "5. LIBC設定: "
grep -q "CONFIG_LIBC_MAX_EXITFUNS" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "6. TLS設定: "
grep -q "CONFIG_TLS_NELEM" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "7. Work Queue設定: "
grep -q "CONFIG_SCHED_WORKQUEUE=y" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "8. SDIO設定: "
grep -q "CONFIG_SDIO_BLOCKSETUP=y" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "9. スタック警告設定: "
grep -q "CONFIG_STACK_USAGE_WARNING" nuttx/.config && echo "✓ OK" || echo "✗ NG"

echo -n "10. アプリ設定: "
grep -q "CONFIG_EXAMPLES_SECURITY_CAMERA=y" nuttx/.config && echo "✓ OK" || echo "✗ NG"
```

---

## 🛠️ ワンライナー修正集

### 完全リセット＋再設定
```bash
cd sdk && make distclean && ./tools/config.py examples/camera && cd ../nuttx && sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config && sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config && cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF
```

### 必須設定を全て追加
```bash
cat >> nuttx/.config <<'EOF'
CONFIG_ARCH_CORTEXM4=y
CONFIG_ARCH_ARMV7M=y
CONFIG_LIBC_MAX_EXITFUNS=0
CONFIG_TLS_NELEM=0
CONFIG_SCHED_WORKQUEUE=y
CONFIG_SCHED_HPWORK=y
CONFIG_SDIO_BLOCKSETUP=y
CONFIG_STACK_USAGE_WARNING=0
EOF
```

### PATH付きビルド＋ログ保存＋検証
```bash
cd sdk && PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make 2>&1 | tee build.log && ls -lh ../nuttx/nuttx.spk && arm-none-eabi-nm ../nuttx/nuttx | grep "security_camera_main"
```

---

## 📊 設定値リファレンス

### アーキテクチャ設定
| 設定 | 値 | 説明 |
|------|-----|------|
| CONFIG_ARCH | "arm" | ARMアーキテクチャ |
| CONFIG_ARCH_CHIP | "cxd56xx" | CXD56xxチップ |
| CONFIG_ARCH_CHIP_CXD56XX | y | Spresense |
| CONFIG_ARCH_CORTEXM4 | y | Cortex-M4コア |
| CONFIG_ARCH_ARMV7M | y | ARMv7-M命令セット |

### ツールチェーン設定
| 設定 | 値 | 説明 |
|------|-----|------|
| CROSSDEV | arm-none-eabi- | クロスコンパイラプレフィックス |
| ARCH_SUBDIR | armv7-m | アーキテクチャソースディレクトリ |

### アプリケーション設定（例: security_camera）
| 設定 | 値 | 説明 |
|------|-----|------|
| CONFIG_EXAMPLES_SECURITY_CAMERA | y | アプリを有効化 |
| CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME | "security_camera" | 実行ファイル名 |
| CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY | 100 | タスク優先度 |
| CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE | 8192 | スタックサイズ（バイト）|

---

## 🚨 緊急時の対処

### ビルドが完全に壊れた場合

```bash
# 1. 作業環境から完全コピー
cp -r /home/ken/Spr_ws/spresense/nuttx/.config /path/to/GH_wk_test/spresense/nuttx/
cp -r /home/ken/Spr_ws/spresense/nuttx/Make.defs /path/to/GH_wk_test/spresense/nuttx/

# 2. アプリ設定のみ変更
cd /path/to/GH_wk_test/spresense/nuttx
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF

# 3. ビルド実行
cd ../sdk
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make
```

---

## 💡 プロのヒント

### 1. ビルド時間短縮
```bash
# 並列ビルド（CPUコア数に応じて調整）
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make -j$(nproc)
```

### 2. 設定差分の確認
```bash
# 作業環境と新環境の設定差分
diff -u /working/.config /new/.config | less
```

### 3. エラーを絞り込む
```bash
# 最初のエラーのみ表示
make 2>&1 | grep -i "error:" | head -1

# エラー前後の文脈も表示
make 2>&1 | grep -B 3 -A 3 -i "error:" | head -20
```

### 4. クリーンビルドの自動化
```bash
# スクリプト化して保存
cat > rebuild.sh <<'SCRIPT'
#!/bin/bash
set -e
cd $(dirname $0)/sdk
make distclean
./tools/config.py examples/camera
cd ../nuttx
sed -i 's/^CONFIG_EXAMPLES_CAMERA=y$/# CONFIG_EXAMPLES_CAMERA is not set/' .config
sed -i '/^CONFIG_EXAMPLES_CAMERA_/d' .config
cat >> .config <<'EOF'
CONFIG_EXAMPLES_SECURITY_CAMERA=y
CONFIG_EXAMPLES_SECURITY_CAMERA_PROGNAME="security_camera"
CONFIG_EXAMPLES_SECURITY_CAMERA_PRIORITY=100
CONFIG_EXAMPLES_SECURITY_CAMERA_STACKSIZE=8192
EOF
cd ../sdk
PATH=$HOME/spresenseenv/usr/bin:/usr/bin:/bin make -j$(nproc)
echo "✓ Build complete: $(ls -lh ../nuttx/nuttx.spk)"
SCRIPT
chmod +x rebuild.sh
```

---

## 📖 関連リソース

### ドキュメント
- [詳細トラブルシューティングガイド](build_environment_migration_troubleshooting.md) - 詳細な解説とベストプラクティス
- [カメラ設定リファレンス](camera_config_reference.md) - カメラ固有の設定
- [ビルドシステムの教訓](camera_lessons_learned.md) - 開発の教訓集

### 📊 PlantUML図（視覚的理解）
- [config_dependencies.puml](../../diagrams/config_dependencies.puml) - CONFIG依存関係が一目で分かる
- [build_flow.puml](../../diagrams/build_flow.puml) - 推奨ビルド手順のフローチャート
- [troubleshooting_flow.puml](../../diagrams/troubleshooting_flow.puml) - エラー診断フロー
- [build_system_components.puml](../../diagrams/build_system_components.puml) - ファイル間の関係図
- [directory_structure.puml](../../diagrams/directory_structure.puml) - ディレクトリ構造
- [diagrams/README.md](../../diagrams/README.md) - 図の使い方詳細

**💡 ヒント**: PlantUML図を見ると、テキストでは分かりにくい依存関係が視覚的に理解できます。

---

**最終更新**: 2025-12-28

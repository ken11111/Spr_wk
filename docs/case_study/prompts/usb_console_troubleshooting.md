# USB シリアルコンソール接続トラブルシューティングガイド

**作成日**: 2025-12-28
**対象**: Spresense NuttX USB シリアルコンソール接続問題
**ビルド環境**: Phase 1.5 VGA以降

---

## 目次

- [問題の概要](#問題の概要)
- [原因の特定](#原因の特定)
- [解決策](#解決策)
- [設定の詳細](#設定の詳細)
- [検証方法](#検証方法)
- [関連ドキュメント](#関連ドキュメント)

---

## 問題の概要

### 症状

ファームウェア(nuttx.spk)を書き込んだ後、以下の問題が発生：

```
nsh> sercon
sh: sercon: command not found
```

### 影響

- **USB CDC-ACM経由でのシリアルコンソール接続ができない**
- カメラアプリケーションが起動しても、PC側からデータを受信できない
- `/dev/ttyACM0`が利用できない

### 発生条件

- defconfigから新規ビルド環境を構築した場合
- `CONFIG_SYSTEM_CDCACM`が無効化されている場合
- `CONFIG_NSH_USBCONSOLE`が無効化されている場合

---

## 原因の特定

### 原因1: serconコマンドが登録されていない

`sercon`コマンドはUSB CDC-ACMシリアルドライバーを有効化するNuttShellのビルトインコマンドです。

**必要なCONFIG設定**:
```bash
CONFIG_SYSTEM_CDCACM=y
```

この設定がないと、`sercon`コマンドがビルドシステムに登録されません。

### 原因2: USB CDC基本機能が無効

USB CDC-ACM自体が有効になっていても、システムレベルのコマンドが登録されていない場合があります。

**確認方法**:
```bash
grep CONFIG_SYSTEM_CDCACM /home/ken/Spr_ws/GH_wk_test/spresense/nuttx/.config
```

**期待される出力**:
```
CONFIG_SYSTEM_CDCACM=y
```

**問題がある場合の出力**:
```
# CONFIG_SYSTEM_CDCACM is not set
```

### 原因3: 自動USBコンソール起動が無効

NuttShell起動時に自動的にUSBコンソールを有効化する設定がない。

**確認方法**:
```bash
grep CONFIG_NSH_USBCONSOLE /home/ken/Spr_ws/GH_wk_test/spresense/nuttx/.config
```

**問題がある場合の出力**:
```
# CONFIG_NSH_USBCONSOLE is not set
```

---

## 解決策

### 解決策A: serconコマンドを復活させる（従来の手動方式）

**用途**: 従来通り`sercon`コマンドを手動で実行する方式

**手順**:

1. **CONFIG設定を追加**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
cat >> .config <<'EOF'
CONFIG_SYSTEM_CDCACM=y
EOF
```

2. **古い無効化設定を削除**:
```bash
sed -i '/^# CONFIG_SYSTEM_CDCACM is not set$/d' .config
```

3. **再ビルド**:
```bash
cd ../sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin
make clean
make -j4
```

4. **検証**:
```bash
grep "Register: sercon" build.log
```

**期待される出力**:
```
Register: sercon
```

5. **動作確認**:
```
nsh> sercon
CDC/ACM serial driver registered
```

---

### 解決策B: USB自動起動を有効化（推奨方式）

**用途**: NuttShell起動時に自動的にUSBコンソールを有効化（sercon不要）

**手順**:

1. **CONFIG設定を追加**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
cat >> .config <<'EOF'
CONFIG_NSH_USBCONSOLE=y
EOF
```

2. **古い無効化設定を削除**:
```bash
sed -i '/^# CONFIG_NSH_USBCONSOLE is not set$/d' .config
```

3. **再ビルド** (解決策Aと同じ手順)

4. **動作確認**:

NuttShell起動時、自動的に`/dev/ttyACM0`が有効化され、USB経由でアクセス可能になります。

```
NuttShell (NSH) NuttX-10.x.x
nsh>  ← USB経由で自動的に接続される
```

---

### 解決策C: 両方を有効化（最も柔軟）

**用途**: 自動起動 + 手動serconコマンドの両方をサポート

**手順**:

1. **両方のCONFIG設定を追加**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/nuttx
cat >> .config <<'EOF'
CONFIG_SYSTEM_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
EOF
```

2. **古い無効化設定を削除**:
```bash
sed -i '/^# CONFIG_NSH_USBCONSOLE is not set$/d' .config
sed -i '/^# CONFIG_SYSTEM_CDCACM is not set$/d' .config
```

3. **再ビルド**:
```bash
cd ../sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin
make clean
make -j4
```

4. **検証**:
```bash
# serconコマンドが登録されているか確認
grep "Register: sercon" build.log

# CONFIG設定を確認
grep -E "CONFIG_SYSTEM_CDCACM|CONFIG_NSH_USBCONSOLE" ../nuttx/.config
```

**期待される出力**:
```
Register: sercon
CONFIG_SYSTEM_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
```

**メリット**:
- 自動起動により、通常はsercon不要
- トラブル時に手動でsercon/serdisコマンドを使って制御可能
- 最も柔軟な構成

---

## 設定の詳細

### CONFIG_SYSTEM_CDCACM

**説明**: USBシリアルコンソールのシステムコマンド（sercon/serdis）を有効化

**依存関係**:
- `CONFIG_CDCACM=y` (USB CDC-ACM基本機能)

**提供されるコマンド**:
- `sercon`: CDC-ACMシリアルドライバーを有効化
- `serdis`: CDC-ACMシリアルドライバーを無効化

**Kconfigパス**:
```
Application Configuration
  └─ System NSH Add-Ons
      └─ [*] USB CDC/ACM Device Commands
```

---

### CONFIG_NSH_USBCONSOLE

**説明**: NuttShell起動時に自動的にUSBコンソールを有効化

**依存関係**:
- `CONFIG_CDCACM=y` (USB CDC-ACM基本機能)

**動作**:
- NuttShell初期化時に自動的に`/dev/ttyACM0`を有効化
- 手動でserconコマンドを実行する必要がない

**Kconfigパス**:
```
Application Configuration
  └─ NSH Library
      └─ Console Configuration
          └─ [*] Use USB console
```

---

## 検証方法

### 1. ビルド時の検証

**builtin登録の確認**:
```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/sdk
grep "Register:" build.log | head -10
```

**期待される出力**:
```
Register: security_camera
Register: nsh
Register: sercon          ← これが重要
Register: sh
Register: serdis
```

**CONFIG設定の確認**:
```bash
grep -E "CONFIG_SYSTEM_CDCACM|CONFIG_NSH_USBCONSOLE|CONFIG_CDCACM=" ../nuttx/.config
```

**期待される出力**:
```
CONFIG_CDCACM=y
CONFIG_SYSTEM_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
```

---

### 2. 実機での検証

#### 検証A: serconコマンドの存在確認

```
nsh> ?
```

**期待される出力**（一部抜粋）:
```
Builtin Apps:
  nsh
  security_camera
  sercon            ← これが表示されればOK
  serdis
  sh
```

#### 検証B: USB自動起動の確認

**CONFIG_NSH_USBCONSOLE=y の場合**:

NuttShell起動直後に`/dev/ttyACM0`が自動的に有効化されます。

**確認方法**:
1. ファームウェアを書き込む
2. Spresenseをリセット
3. PC側で`/dev/ttyACM0`の出現を確認

```bash
# Linux側 (WSL2)
ls /dev/ttyACM0
# 期待: /dev/ttyACM0 が存在
```

4. USB経由でデータ受信テスト

```bash
cat /dev/ttyACM0 > test.bin
```

---

### 3. トラブルシューティング時の確認

**問題**: `sercon`を実行しても`/dev/ttyACM0`が出現しない

**原因の可能性**:
1. Spresense側の`CONFIG_CDCACM`が無効
2. Linux側のドライバー未ロード
3. WSL2へのUSB接続が確立されていない

**確認手順**:

```bash
# 1. Spresense側のCONFIG確認
grep CONFIG_CDCACM= /home/ken/Spr_ws/GH_wk_test/spresense/nuttx/.config
# 期待: CONFIG_CDCACM=y

# 2. Linux側ドライバー確認
lsmod | grep cdc_acm
# 期待: cdc_acm が表示される

# 3. ドライバーロード（必要な場合）
sudo modprobe cdc-acm

# 4. WSL2へのUSB接続確認（Windows側PowerShellで）
usbipd list
# 期待: Sony 054c:0bc2 が "Attached" 状態
```

---

## 関連ドキュメント

### case_study内のドキュメント

- [build_environment_migration_troubleshooting.md](./build_environment_migration_troubleshooting.md) - ビルド環境全般のトラブルシューティング
- [build_troubleshooting_cheatsheet.md](./build_troubleshooting_cheatsheet.md) - ビルドエラー即座解決
- [01_environment_check.md](./01_environment_check.md) - Phase 1: 環境構築
- [02_build_system_integration.md](./02_build_system_integration.md) - Phase 2: アプリ統合
- [INDEX.md](./INDEX.md) - ドキュメント一覧

### セキュリティカメラプロジェクト内のドキュメント

- [04_TEST_PROCEDURE_FLOW.md](../../security_camera/04_test_results/04_TEST_PROCEDURE_FLOW.md) - テスト手順フロー
- [03_USB_CDC_SETUP.md](../../security_camera/03_manuals/03_USB_CDC_SETUP.md) - USB CDC詳細セットアップ
- [04_TROUBLESHOOTING.md](../../security_camera/03_manuals/04_TROUBLESHOOTING.md) - 総合トラブルシューティング

### PlantUML図

- [usb_console_troubleshooting_flow.puml](../diagrams/usb_console_troubleshooting_flow.puml) - USB接続トラブルシューティングフロー

---

## 付録: CONFIG設定の全体像

### USB CDC-ACM関連の設定階層

```
CONFIG_CDCACM=y                    # USB CDC-ACM基本機能（必須）
├─ CONFIG_SYSTEM_CDCACM=y          # sercon/serdisコマンド（オプション）
└─ CONFIG_NSH_USBCONSOLE=y         # USB自動起動（オプション）
```

### 推奨設定パターン

#### パターン1: 従来の手動方式
```bash
CONFIG_CDCACM=y
CONFIG_SYSTEM_CDCACM=y
```

**使い方**: `nsh> sercon` を手動実行

---

#### パターン2: 自動起動方式（推奨）
```bash
CONFIG_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
```

**使い方**: NuttShell起動時に自動有効化

---

#### パターン3: ハイブリッド方式（最も柔軟）
```bash
CONFIG_CDCACM=y
CONFIG_SYSTEM_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
```

**使い方**:
- 通常: 自動有効化
- トラブル時: `sercon`/`serdis`で手動制御可能

---

## よくある質問（FAQ）

### Q1: CONFIG_NSH_USBCONSOLEとCONFIG_SYSTEM_CDCACM、どちらを使うべきか？

**A**: 両方有効化することを推奨します（パターン3）。

- `CONFIG_NSH_USBCONSOLE=y`: 通常動作時に便利（自動起動）
- `CONFIG_SYSTEM_CDCACM=y`: トラブル時に手動制御可能

---

### Q2: serconを実行したのに/dev/ttyACM0が出現しない

**A**: 以下を確認してください：

1. **Linux側ドライバー**:
   ```bash
   sudo modprobe cdc-acm
   lsmod | grep cdc_acm
   ```

2. **WSL2へのUSB接続**（Windows側PowerShell）:
   ```powershell
   usbipd list
   # Sony 054c:0bc2 が "Attached" であることを確認

   # Attachedでない場合:
   usbipd attach --wsl --busid <BUSID>
   ```

3. **Spresense側のCONFIG**:
   ```bash
   grep CONFIG_CDCACM= ../nuttx/.config
   # CONFIG_CDCACM=y であることを確認
   ```

---

### Q3: 既存のdefconfigから移行する場合の手順は？

**A**:

```bash
cd /home/ken/Spr_ws/GH_wk_test/spresense/sdk

# 1. defconfigを適用
./tools/config.py examples/camera

# 2. USB CDC設定を追加
cd ../nuttx
cat >> .config <<'EOF'
CONFIG_SYSTEM_CDCACM=y
CONFIG_NSH_USBCONSOLE=y
EOF

# 3. 古い無効化設定を削除
sed -i '/^# CONFIG_SYSTEM_CDCACM is not set$/d' .config
sed -i '/^# CONFIG_NSH_USBCONSOLE is not set$/d' .config

# 4. 再ビルド
cd ../sdk
export PATH=/home/ken/spresenseenv/usr/bin:/usr/bin:/bin
make clean
make -j4
```

---

**作成者**: Claude Code (Sonnet 4.5)
**最終更新**: 2025-12-28

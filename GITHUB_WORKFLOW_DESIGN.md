# GitHub 運用のための構成設計

**作成日**: 2025-12-21
**目的**: Spresense セキュリティカメラプロジェクトのGitHub運用環境構築

---

## 現環境分析

### 現在の開発環境 (`/home/ken/Spr_ws/spresense/`)

```
spresense/
├── nuttx/                              # NuttX RTOS
├── sdk/                                # Spresense SDK
│   ├── apps/
│   │   └── examples/
│   │       └── security_camera/        # ★ アプリケーションコード
│   │           ├── camera_app_main.c
│   │           ├── camera_manager.c/h
│   │           ├── encoder_manager.c/h
│   │           ├── protocol_handler.c/h
│   │           ├── usb_transport.c/h
│   │           ├── mjpeg_protocol.c/h
│   │           ├── config.h
│   │           ├── Kconfig
│   │           ├── Makefile
│   │           └── README.md
│   ├── build.sh                        # ★ ビルドスクリプト
│   ├── .config                         # ★ SDK設定
│   └── tools/
│       ├── config.py
│       └── flash.sh
├── security_camera/                    # ★ プロジェクトドキュメント
│   ├── 01_specifications/
│   ├── 02_implementation/
│   ├── 03_manuals/
│   ├── 04_test_results/
│   ├── 05_project/
│   └── README.md
└── spresense_env.sh
```

---

## GitHub 環境設計

### 構成概要

```
GH_wk_test/                             # メインリポジトリ (Spr_wk)
├── spresense/                          # サブモジュール (forked spresense SDK)
│   ├── nuttx/
│   └── sdk/
│       ├── apps/
│       │   └── examples/
│       │       └── security_camera/    # アプリケーションコード (Fork先で管理)
│       ├── build.sh
│       ├── .config
│       └── tools/
├── docs/                               # プロジェクトドキュメント (メインリポジトリ)
│   ├── 01_specifications/
│   ├── 02_implementation/
│   ├── 03_manuals/
│   ├── 04_test_results/
│   └── 05_project/
├── scripts/                            # プロジェクト固有スクリプト
│   ├── build.sh                        # ラッパービルドスクリプト
│   ├── flash.sh                        # ラッパーフラッシュスクリプト
│   └── setup_env.sh                    # 環境セットアップ
├── README.md                           # プロジェクトREADME
├── .gitignore
└── .gitmodules                         # サブモジュール定義
```

### 2つのリポジトリの役割分担

#### 1. **Spresense Fork (サブモジュール)**: `https://github.com/ken11111/spresense`

**管理対象**:
- Spresense SDK全体 (Sony公式SDKのFork)
- カスタムアプリケーション: `sdk/apps/examples/security_camera/`
- SDK関連の設定・ビルドスクリプト

**ブランチ戦略**:
- `master`: Sony公式SDKとの同期用 (upstream tracking)
- `develop`: 開発用ブランチ
- `feature/security-camera`: セキュリティカメラ機能ブランチ

**利点**:
- 公式SDKの更新を取り込みやすい
- アプリケーションコードをSDK内で管理
- 将来的にupstream (Sony公式) へのプルリクエストが可能

#### 2. **Spr_wk (メインリポジトリ)**: `https://github.com/ken11111/Spr_wk`

**管理対象**:
- プロジェクト全体のドキュメント
- ビルド・テスト用スクリプト (ラッパー)
- プロジェクト固有の設定
- PC側アプリケーション (Rust viewer等)

**ブランチ戦略**:
- `main`: メインブランチ
- `develop`: 開発用
- `feature/*`: 機能別ブランチ

**利点**:
- プロジェクト全体の管理
- SDKとプロジェクトドキュメントを分離
- CI/CD設定の管理

---

## ファイル配置計画

### Phase 1: アプリケーションコードの配置

**コピー元**: `/home/ken/Spr_ws/spresense/sdk/apps/examples/security_camera/`
**コピー先**: `/home/ken/Spr_ws/GH_wk_test/spresense/sdk/apps/examples/security_camera/`

**対象ファイル** (ソースコードのみ):
```
security_camera/
├── camera_app_main.c
├── camera_manager.c
├── camera_manager.h
├── encoder_manager.c
├── encoder_manager.h
├── protocol_handler.c
├── protocol_handler.h
├── usb_transport.c
├── usb_transport.h
├── mjpeg_protocol.c
├── mjpeg_protocol.h
├── config.h
├── Kconfig
├── Makefile
└── README.md
```

**除外ファイル** (ビルド成果物):
- `*.o`
- `.built`
- `.depend`
- `Make.dep`

### Phase 2: ドキュメントの配置

**コピー元**: `/home/ken/Spr_ws/spresense/security_camera/`
**コピー先**: `/home/ken/Spr_ws/GH_wk_test/docs/`

**対象ディレクトリ**:
```
docs/
├── 01_specifications/
├── 02_implementation/
├── 03_manuals/
├── 04_test_results/
├── 05_project/
└── README.md
```

### Phase 3: ビルドスクリプト・設定の配置

**SDK側** (submodule内):
- `spresense/sdk/build.sh` (既存または現環境からコピー)
- `spresense/sdk/.config` (設定ファイル)

**プロジェクト側** (main repo):
- `scripts/build.sh` (サブモジュールを考慮したラッパー)
- `scripts/flash.sh` (フラッシュ用ラッパー)
- `scripts/setup_env.sh` (初回環境セットアップ)

---

## Git設定

### .gitignore (Spresense fork)

```gitignore
# Build artifacts
*.o
*.a
*.so
*.spk
*.elf
*.bin
.built
.depend
Make.dep
*.lock

# Editor
.vscode/
*.swp
*~

# Config backups
*.backup
.config.old
```

### .gitignore (Spr_wk main repo)

```gitignore
# Build artifacts
*.spk
*.bin
*.log

# Temporary files
*.tmp
*~

# IDE
.vscode/
.idea/
```

---

## 運用フロー

### 初回セットアップ

```bash
# 1. メインリポジトリのクローン
git clone https://github.com/ken11111/Spr_wk.git
cd Spr_wk

# 2. サブモジュールの初期化
git submodule update --init --recursive

# 3. 環境セットアップ
./scripts/setup_env.sh

# 4. ビルド
./scripts/build.sh
```

### 開発フロー

```bash
# 1. アプリケーションコードの変更
cd spresense/sdk/apps/examples/security_camera/
# コード編集...

# 2. ビルド (プロジェクトルートから)
cd /home/ken/Spr_ws/GH_wk_test
./scripts/build.sh

# 3. テスト
./scripts/flash.sh

# 4. コミット (サブモジュール側)
cd spresense
git add sdk/apps/examples/security_camera/
git commit -m "feat: add new camera feature"
git push origin feature/security-camera

# 5. メインリポジトリ更新
cd ..
git add spresense
git commit -m "chore: update spresense submodule"
git push origin develop
```

---

## 移行手順

### Step 1: 現環境の保全

- 現環境 (`/home/ken/Spr_ws/spresense/`) は変更しない
- GitHub環境で動作確認が取れるまで保持

### Step 2: ファイルコピー

1. アプリケーションコード → Spresense fork
2. ドキュメント → メインリポジトリ
3. ビルドスクリプト → 両方に適切に配置

### Step 3: Git設定

1. `.gitignore` 設定
2. サブモジュール参照の確認
3. ブランチ作成

### Step 4: ビルド動作確認

1. GitHub環境でビルド実行
2. nuttx.spk の生成確認
3. フラッシュ・実行テスト

### Step 5: 運用開始判断

以下が確認できれば運用開始可能:
- ✅ ビルドが成功する
- ✅ フラッシュが成功する
- ✅ アプリケーションが動作する
- ✅ Git操作が問題なく行える

---

## 利点・考慮事項

### 利点

1. **バージョン管理**:
   - SDK更新とアプリ開発を独立管理
   - 公式SDKの更新を容易に取り込める

2. **コラボレーション**:
   - GitHubでのPull Request / Issue管理
   - コードレビュー・CI/CD統合

3. **ドキュメント管理**:
   - プロジェクトドキュメントとコードの分離
   - ドキュメント更新がSDKに影響しない

4. **再現性**:
   - サブモジュールで特定のSDKバージョンを固定
   - 環境セットアップの自動化

### 考慮事項

1. **サブモジュール管理**:
   - サブモジュール更新時は明示的に `git add spresense` が必要
   - クローン時に `--recursive` オプションが必要

2. **ブランチ管理**:
   - メインリポジトリとサブモジュールで別々のブランチ管理
   - 整合性の維持に注意

3. **ビルドパス**:
   - 絶対パスから相対パスへの変更が必要な場合あり
   - 環境変数の調整

---

**次のステップ**: この設計に基づいてファイルコピーと設定を実行します

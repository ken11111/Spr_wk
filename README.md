# Spresense Security Camera Project

防犯カメラシステムのGitHub開発環境

## プロジェクト構成

```
Spr_wk/
├── spresense/              # Spresense SDK (submodule - forked from Sony)
│   └── sdk/
│       └── apps/examples/
│           └── security_camera/    # アプリケーションコード
├── docs/                   # プロジェクトドキュメント
│   ├── 01_specifications/  # 仕様書
│   ├── 02_implementation/  # 実装詳細
│   ├── 03_manuals/         # マニュアル
│   ├── 04_test_results/    # テスト結果
│   └── 05_project/         # プロジェクト管理
├── scripts/                # ビルド・テスト用スクリプト
│   ├── setup_env.sh        # 環境セットアップ
│   ├── build.sh            # ビルド
│   └── flash.sh            # フラッシュ
└── README.md               # このファイル
```

## クイックスタート

### 1. リポジトリのクローン

```bash
git clone https://github.com/ken11111/Spr_wk.git
cd Spr_wk
```

### 2. サブモジュールの初期化

```bash
git submodule update --init --recursive
```

### 3. 環境セットアップ

```bash
./scripts/setup_env.sh
```

このスクリプトは以下を確認・実行します:
- 必要なパッケージのインストール
- Spresense ツールチェーンの確認
- USB ドライバーのロード

### 4. ビルド

```bash
./scripts/build.sh
```

### 5. フラッシュ

```bash
./scripts/flash.sh [デバイス]
# デフォルト: /dev/ttyUSB0
```

### 6. 実行

```bash
# minicom でコンソールに接続
minicom

# NuttShell プロンプトで
nsh> sercon
nsh> security_camera
```

## 開発フロー

### アプリケーションコードの変更

```bash
# 1. アプリケーションコードを編集
cd spresense/sdk/apps/examples/security_camera/
# ファイル編集...

# 2. ビルド (プロジェクトルートから)
cd /path/to/Spr_wk
./scripts/build.sh

# 3. テスト
./scripts/flash.sh
```

### Git ワークフロー

#### アプリケーションコードの変更をコミット (サブモジュール側)

```bash
cd spresense
git checkout -b feature/new-feature
git add sdk/apps/examples/security_camera/
git commit -m "feat: add new camera feature"
git push origin feature/new-feature
```

#### メインリポジトリの更新

```bash
cd ..
git add spresense
git commit -m "chore: update spresense submodule to include new feature"
git push origin develop
```

#### ドキュメントの変更

```bash
# メインリポジトリで直接編集
git add docs/
git commit -m "docs: update test results"
git push origin develop
```

## ブランチ戦略

### メインリポジトリ (Spr_wk)
- `main`: 本番環境用
- `develop`: 開発統合ブランチ
- `feature/*`: 機能開発ブランチ

### サブモジュール (spresense fork)
- `master`: Sony公式SDKとの同期用
- `develop`: 開発用
- `feature/security-camera`: セキュリティカメラ機能ブランチ

## 詳細ドキュメント

- **プロジェクト概要**: [docs/README.md](docs/README.md)
- **仕様書**: [docs/01_specifications/](docs/01_specifications/)
- **実装詳細**: [docs/02_implementation/](docs/02_implementation/)
- **マニュアル**: [docs/03_manuals/](docs/03_manuals/)
- **テスト結果**: [docs/04_test_results/](docs/04_test_results/)
- **プロジェクト管理**: [docs/05_project/](docs/05_project/)

## システム要件

### ハードウェア
- Spresense メインボード
- Spresense 拡張ボード
- Spresense HDR Camera Board

### ソフトウェア
- Ubuntu 20.04+ (WSL2 推奨)
- Spresense ツールチェーン (arm-none-eabi-gcc)
- Python 3.x
- Git

### 必要なパッケージ
```bash
sudo apt-get install -y \
    build-essential \
    python3 python3-serial \
    git kconfig-frontends gperf \
    libncurses5-dev flex bison \
    genromfs xxd minicom
```

## トラブルシューティング

### サブモジュールが初期化されていない

```bash
git submodule update --init --recursive
```

### ツールチェーンが見つからない

```bash
export PATH="${HOME}/spresenseenv/usr/bin:${PATH}"
```

### USB デバイスが見つからない (WSL2)

```bash
# Windows PowerShell (管理者権限)
usbipd list
usbipd attach --wsl --busid <BUSID>

# WSL2 Ubuntu
sudo modprobe cp210x
sudo modprobe cdc_acm
```

詳細は [docs/03_manuals/04_TROUBLESHOOTING.md](docs/03_manuals/04_TROUBLESHOOTING.md) を参照

## ライセンス

BSD 3-Clause License

## 参考リンク

- [Spresense 公式ドキュメント](https://developer.sony.com/develop/spresense/)
- [GitHub Workflow Design](GITHUB_WORKFLOW_DESIGN.md)

## 作成者

- Project: Security Camera Application
- Powered by: Spresense SDK (Sony)
- Documentation: Claude Code (Sonnet 4.5)

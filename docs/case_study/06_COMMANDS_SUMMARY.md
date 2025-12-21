# コマンド実行サマリー

このドキュメントは、SDKアップデートとBMI160アプリケーション開発で実行した全コマンドをまとめたものです。

## 1. SDK アップデート

### 1.1 既存SDK確認
```bash
cd /home/ken/Spr_ws/spresense/sdk

# バージョン確認
if [ -f /home/ken/Spr_ws/spresense/sdk/package.json ]; then
  cat /home/ken/Spr_ws/spresense/sdk/package.json
elif [ -f /home/ken/Spr_ws/spresense/sdk/.version ]; then
  cat /home/ken/Spr_ws/spresense/sdk/.version
else
  echo "Version file not found"
fi
```

### 1.2 新しいSDKのダウンロード
```bash
cd /home/ken/Spr_ws
git clone --recursive https://github.com/sonydevworld/spresense.git spresense_new
mv spresense spresense_old
mv spresense_new spresense

cd /home/ken/Spr_ws/spresense/sdk
git describe --tags
# 出力: v3.4.5-28-gb0d63b7a6
```

### 1.3 開発ツールのインストール
```bash
cd /home/ken/Spr_ws/spresense/sdk
bash tools/install-tools.sh

# PATHの設定（.bashrcに追加推奨）
export PATH=$HOME/spresenseenv/usr/bin:$PATH

# ツールの確認
arm-none-eabi-gcc --version
# arm-none-eabi-gcc (Based on GCC 12.2 Package) 12.2.1 20221205
```

### 1.4 動作確認
```bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py examples/hello
make -j4

# ビルド成功確認
ls -lh nuttx.spk
# -rw-r--r-- 1 ken ken 142K ...
```

## 2. BMI160アプリケーション開発

### 2.1 アプリケーションファイルの準備

アプリケーションは既に `/home/ken/Spr_ws/Mypro/bmi160_orientation/` に作成済み

ファイル構成:
```
bmi160_orientation/
├── bmi160_orientation_main.c
├── orientation_calc.c
├── orientation_calc.h
├── position_estimator.c
├── position_estimator.h
├── Makefile
├── Kconfig
├── Make.defs
├── BUILD_GUIDE.md
└── README.md
```

### 2.2 アプリケーションのコピー
```bash
cd /home/ken/Spr_ws/spresense
cp -r /home/ken/Spr_ws/Mypro/bmi160_orientation examples/

# 確認
ls -la examples/bmi160_orientation/
```

### 2.3 Kconfigへの追加
```bash
# examples/Kconfig の38行目に追加
# テキストエディタで編集、または:
sed -i '38i source "/home/ken/Spr_ws/spresense/examples/bmi160_orientation/Kconfig"' \
  /home/ken/Spr_ws/spresense/examples/Kconfig

# 確認
grep bmi160_orientation /home/ken/Spr_ws/spresense/examples/Kconfig
```

## 3. SDK設定とビルド

### 3.1 基本設定の適用
```bash
cd /home/ken/Spr_ws/spresense/sdk
./tools/config.py examples/sixaxis feature/ahrs
```

### 3.2 追加設定の記述
```bash
cd /home/ken/Spr_ws/spresense/sdk

# .configに追加
cat >> .config <<'EOF'
CONFIG_MYPRO_BMI160_ORIENTATION=y
CONFIG_MYPRO_BMI160_ORIENTATION_PROGNAME="bmi160_orientation"
CONFIG_MYPRO_BMI160_ORIENTATION_PRIORITY=100
CONFIG_MYPRO_BMI160_ORIENTATION_STACKSIZE=4096
CONFIG_EXTERNALS_AHRS=y
CONFIG_EXTERNALS_AHRS_MADGWICK=y
CONFIG_SENSORS_BMI160=y
CONFIG_LIBM=y
EOF

# 確認
grep -E "BMI160|AHRS|LIBM" .config
```

### 3.3 ビルド実行
```bash
cd /home/ken/Spr_ws/spresense/sdk

# PATH設定（セッションごとに必要）
export PATH=$HOME/spresenseenv/usr/bin:$PATH

# クリーンビルド
make clean
make -j4

# 結果確認
ls -lh nuttx.spk
# -rw-r--r-- 1 ken ken 170K ...
```

## 4. ハードウェアセットアップ（WSL2環境）

### 4.1 USBデバイス接続確認
```bash
# WSL2でのデバイス確認
ls -l /dev/ttyUSB*

# デバイスが見つからない場合:
# 1. Windowsホスト側でusbipd-winを使用してデバイスをアタッチ
#    PowerShellで: usbipd wsl attach --busid X-X

# 2. WSL2でCP210xドライバをロード
sudo modprobe cp210x

# 3. 再確認
ls -l /dev/ttyUSB0
# crw------- 1 root root 188, 0 Dec 13 21:17 /dev/ttyUSB0

# 4. ユーザーグループ確認
whoami && groups
# ken
# ken adm dialout cdrom ...
```

### 4.2 ブートローダのインストール
```bash
cd /home/ken/Spr_ws/spresense/sdk

# ブートローダファイルの確認
ls -lh ../firmware/spresense/loader.espk
# -rw-r--r-- 1 ken ken 128K ...

# ブートローダ書き込み（-B オプション使用）
sudo -E ./tools/flash.sh -B -c /dev/ttyUSB0 -b 115200

# 成功メッセージを確認
# >>> Install files ...
# >>> Install completed successfully <<<
```

## 5. ファームウェア書き込み

### 5.1 nuttx.spkの書き込み
```bash
cd /home/ken/Spr_ws/spresense/sdk

# 書き込み実行（sudoパスワード入力が必要）
sudo -E ./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# 成功メッセージを確認
# >>> Install files ...
# >>> Install completed successfully <<<
```

### 5.2 シリアルコンソール接続
```bash
# minicomを使用（WSL2環境）
minicom -D /dev/ttyUSB0

# または screen
screen /dev/ttyUSB0 115200

# NuttShell プロンプトが表示されることを確認
# nsh>
```

### 5.3 NuttShellでの動作確認
```bash
# nsh> プロンプトで

# ビルトインコマンド一覧
help

# デバイス確認
ls /dev/

# センサーデバイス確認
ls /dev/accel*

# アプリケーション実行（※現在は未登録）
bmi160_orientation
```

## 6. トラブルシューティングコマンド

### 6.1 ビルド関連
```bash
# 設定確認
cd /home/ken/Spr_ws/spresense/sdk
grep "CONFIG_MYPRO_BMI160" .config

# ビルトインアプリ一覧確認
cat ./apps/builtin/builtin_list.h | grep -i bmi160

# Makefile構文確認
cd ../examples/bmi160_orientation
cat Makefile
```

### 6.2 デバイス関連
```bash
# USB デバイス確認
lsusb
# Bus 001 Device 003: ID 1004:631c Sony Corp. CP210x UART Bridge

# カーネルモジュール確認
lsmod | grep cp210x

# デバイスメッセージ確認
dmesg | tail -20

# デバイスパーミッション変更（一時的）
sudo chmod 666 /dev/ttyUSB0
```

### 6.3 ビルドエラー対策
```bash
# 完全クリーン
cd /home/ken/Spr_ws/spresense/sdk
make distclean

# 設定再適用
./tools/config.py examples/sixaxis feature/ahrs
# .configを手動で再編集

# 再ビルド
export PATH=$HOME/spresenseenv/usr/bin:$PATH
make -j4
```

## 7. menuconfigを使った設定（代替方法）

```bash
cd /home/ken/Spr_ws/spresense/sdk

# menuconfig起動
make menuconfig

# 操作:
# 1. Application Configuration を選択
# 2. Spresense SDK を選択
# 3. Examples を選択
# 4. BMI160 Orientation and Position Estimation を選択し、<Y>で有効化
# 5. <Save> で保存
# 6. <Exit> で終了

# 設定確認
grep MYPRO_BMI160 .config

# ビルド
make clean && make -j4
```

## 8. よく使うコマンドのエイリアス設定

```bash
# .bashrc に追加推奨
cat >> ~/.bashrc <<'EOF'

# Spresense development
export PATH=$HOME/spresenseenv/usr/bin:$PATH
export SPRESENSE_HOME=/home/ken/Spr_ws/spresense

alias sprsdk='cd $SPRESENSE_HOME/sdk'
alias sprflash='sudo -E $SPRESENSE_HOME/sdk/tools/flash.sh'
alias sprbuild='export PATH=$HOME/spresenseenv/usr/bin:$PATH && make -j4'
alias sprmenu='make menuconfig'
alias sprserial='minicom -D /dev/ttyUSB0'
EOF

# 反映
source ~/.bashrc
```

## 9. 環境変数まとめ

```bash
# 必須環境変数
export PATH=$HOME/spresenseenv/usr/bin:$PATH

# 任意（便利）
export SPRESENSE_HOME=/home/ken/Spr_ws/spresense
export SPRESENSE_SDK=$SPRESENSE_HOME/sdk

# ARM GCC設定（通常不要、自動設定される）
export CROSS_COMPILE=arm-none-eabi-
```

## 10. ファイルパス早見表

```bash
# SDK
/home/ken/Spr_ws/spresense/sdk/

# ビルド設定
/home/ken/Spr_ws/spresense/sdk/.config

# ファームウェア
/home/ken/Spr_ws/spresense/sdk/nuttx.spk

# アプリケーション（統合後）
/home/ken/Spr_ws/spresense/examples/bmi160_orientation/

# アプリケーション（開発元）
/home/ken/Spr_ws/Mypro/bmi160_orientation/

# ビルトインアプリリスト
/home/ken/Spr_ws/spresense/sdk/apps/builtin/builtin_list.h

# ブートローダ
/home/ken/Spr_ws/spresense/firmware/spresense/loader.espk

# ツールチェーン
$HOME/spresenseenv/usr/bin/arm-none-eabi-*
```

---

**注意事項**:
- すべてのビルドコマンドは `/home/ken/Spr_ws/spresense/sdk` から実行
- `export PATH=$HOME/spresenseenv/usr/bin:$PATH` は各シェルセッションで必要
- `sudo` を使用するコマンドはパスワード入力が必要
- USBデバイスはWSL2環境では自動マウントされないため、usbipd設定が必要

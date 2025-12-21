# BMI160 Orientation - ビルドガイド

## 前提条件

- Spresense SDK v3.4.5以上
- BMI160センサモジュール（I2CまたはSPI接続）
- 開発環境がセットアップ済み

## ビルド手順

### ステップ1: 環境変数の設定

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=$HOME/spresenseenv/usr/bin:$PATH
```

### ステップ2: SDK設定の準備

既存のBMI160サンプル設定をベースにします：

```bash
./tools/config.py examples/sixaxis
```

### ステップ3: 必要な機能を有効化

menuconfigで追加設定を行います：

```bash
make menuconfig
```

以下の項目を設定してください：

#### 1. BMI160センサドライバ

```
Device Drivers
  -> Sensors
    [*] BMI160 (Accelerometer/Gyroscope)
        BMI160 Interface (I2C)  または (SPI)
```

#### 2. AHRSライブラリ

```
Application Configuration
  -> Spresense SDK
    -> External Libraries
      [*] AHRS (Attitude Heading Reference System)
        [*] Madgwick AHRS
```

#### 3. 数学ライブラリ

```
Application Configuration
  -> Library Routines
    [*] Math library
```

設定を保存して終了します（Save -> Exit）。

### ステップ4: アプリケーションの追加

#### オプション A: examples ディレクトリにコピー（推奨）

```bash
cd /home/ken/Spr_ws/spresense
cp -r /home/ken/Spr_ws/Mypro/bmi160_orientation examples/

# Kconfigに追加（手動編集が必要）
# examples/Kconfig に以下を追加：
# source "examples/bmi160_orientation/Kconfig"
```

#### オプション B: Myproディレクトリから直接ビルド

Myproディレクトリの構造を利用する場合は、追加の設定が必要です。

### ステップ5: ビルド

```bash
cd /home/ken/Spr_ws/spresense/sdk
make
```

成功すると `nuttx.spk` ファイルが生成されます。

### ステップ6: 書き込み

```bash
./tools/flash.sh -c /dev/ttyUSB0 nuttx.spk
```

## トラブルシューティング

### エラー: "MadgwickAHRS.h: No such file or directory"

AHRSライブラリが有効化されていません。menuconfigで以下を確認：
- `EXTERNALS_AHRS` が有効
- `EXTERNALS_AHRS_MADGWICK` が有効

### エラー: "bmi160.h: No such file or directory"

BMI160ドライバが有効化されていません。menuconfigで以下を確認：
- `SENSORS_BMI160` または `SENSORS_BMI160_SCU` が有効

### エラー: リンクエラー (undefined reference to 'sinf', 'cosf', etc.)

数学ライブラリがリンクされていません。menuconfigで以下を確認：
- `LIBM` が有効

### センサーが見つからない

デバイスファイル `/dev/accel0` が存在するか確認：

```bash
nsh> ls /dev/
```

BMI160が正しく初期化されているか確認：

```bash
nsh> i2cdetect   # I2Cの場合
```

## ハードウェア接続

### I2C接続の場合

| Spresense | BMI160 |
|-----------|--------|
| SCL0      | SCL    |
| SDA0      | SDA    |
| 3.3V      | VCC    |
| GND       | GND    |

### SPI接続の場合

| Spresense | BMI160 |
|-----------|--------|
| SCK       | SCK    |
| MOSI      | SDI    |
| MISO      | SDO    |
| CS        | CS     |
| 3.3V      | VCC    |
| GND       | GND    |

## デバッグ情報の有効化

より詳細なログを出力する場合：

```bash
make menuconfig

# デバッグオプション
Build Setup
  -> Debug Options
    [*] Enable Debug Features
      [*] Sensor Debug Features
```

## 実行例

```bash
nsh> bmi160_orientation
BMI160 Orientation and Position Estimation
==========================================

Madgwick AHRS initialized (beta=0.10, rate=100Hz)
Position estimator initialized

BMI160 sensor opened successfully
Starting data acquisition...

Time(ms)      Roll    Pitch      Yaw   |     X       Y       Z
                [deg]   [deg]    [deg]  |    [m]     [m]     [m]
---------------------------------------------------------------
[    100000] Roll:   0.12° Pitch:  -0.34° Yaw:  89.56° | X:  0.000m Y:  0.000m Z:  0.000m
...
```

## パラメータの調整

`bmi160_orientation_main.c` で以下のパラメータを調整できます：

```c
#define SAMPLE_RATE_HZ      100.0f    // サンプリングレート
#define MADGWICK_BETA       0.1f      // フィルタパラメータ（0.01-0.5）
#define GYRO_SCALE          ...       // ジャイロスコープのスケール
#define ACCEL_SCALE         ...       // 加速度計のスケール
```

## よくある質問

**Q: 位置推定の精度が悪い**

A: 加速度の二重積分は誤差が累積しやすいです。以下を試してください：
- センサーのキャリブレーション
- サンプリングレートの向上
- 外部センサー（GPS等）との統合

**Q: Yawがドリフトする**

A: 磁力計がないため、Yawは時間とともにドリフトします。
正確なYaw推定には3軸磁力計（例：BMX160）が必要です。

**Q: Rollが反転している**

A: センサーの取り付け方向により軸が反転することがあります。
`convert_sensor_data()` 関数で符号を調整してください。

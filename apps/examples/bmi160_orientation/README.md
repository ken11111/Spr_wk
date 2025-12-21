# BMI160 Orientation and Position Estimation

BMI160 6軸IMUセンサを使用したRoll/Pitch/Yaw角度推定と位置推定のサンプルアプリケーションです。

## 機能

- **姿勢推定**: Madgwick AHRSアルゴリズムを使用してRoll/Pitch/Yaw角度を計算
- **位置推定**: 加速度データの積分により相対位置を推定
- **リアルタイム出力**: 10Hzでシリアルコンソールに結果を出力

## 必要なハードウェア

- Spresense メインボード
- BMI160センサ (I2C または SPI接続)

## ビルド方法

### 1. SDKの設定

```bash
cd /home/ken/Spr_ws/spresense/sdk
export PATH=$HOME/spresenseenv/usr/bin:$PATH

# BMI160とAHRSライブラリを有効化する設定例
./tools/config.py feature/sensor examples/bmi160
```

### 2. menuconfig で機能を有効化

```bash
make menuconfig
```

以下を有効にしてください：
- `Sensor drivers` -> `BMI160 Sensor`
- `External libraries` -> `AHRS Library`
- `User applications` -> `BMI160 Orientation and Position Estimation`

### 3. ビルド

```bash
make
```

## 実行方法

```bash
# Spresenseボードに書き込み
tools/flash.sh -c /dev/ttyUSB0 nuttx.spk

# シリアルコンソールに接続
minicom -D /dev/ttyUSB0

# NuttShellで実行
nsh> bmi160_orientation
```

## 出力例

```
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
[    100100] Roll:   0.15° Pitch:  -0.32° Yaw:  89.61° | X:  0.001m Y:  0.000m Z:  0.000m
...
```

## 注意事項

### 位置推定の精度

- 加速度の二重積分により位置を推定するため、時間とともに誤差が累積します
- 短時間（数秒程度）の相対位置推定にのみ使用してください
- 長時間の追跡にはGPSなどの外部センサーとの統合が必要です

### キャリブレーション

- 起動時に静止状態で100サンプルのキャリブレーションを行います
- センサーを静止させた状態でプログラムを起動してください

### センサー設定

- サンプリングレート: 100Hz
- ジャイロレンジ: ±2000 deg/s
- 加速度レンジ: ±16g

これらの値は `bmi160_orientation_main.c` の定数で調整できます。

## ファイル構成

```
bmi160_orientation/
├── bmi160_orientation_main.c   # メインプログラム
├── orientation_calc.c           # 姿勢計算ロジック
├── orientation_calc.h           # 姿勢計算ヘッダー
├── position_estimator.c         # 位置推定ロジック
├── position_estimator.h         # 位置推定ヘッダー
├── Makefile                     # ビルド設定
├── Kconfig                      # 設定オプション
├── Make.defs                    # ビルド定義
└── README.md                    # このファイル
```

## アルゴリズム

### Madgwick AHRS

Sebastian Madgwickによって開発されたセンサーフュージョンアルゴリズムを使用しています。
ジャイロスコープと加速度計のデータを融合して、高精度な姿勢推定を実現します。

### 座標系

- センサー座標系: BMI160の物理軸
- 世界座標系: Z軸が上向き、重力方向が-Z

## ライセンス

Copyright 2025 Sony Semiconductor Solutions Corporation

BSD 3-Clause License

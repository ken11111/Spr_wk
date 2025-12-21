# BMI160 Orientation and Position Estimation - ソフトウェア仕様書

## 1. 概要

### 1.1 システム概要

BMI160 6軸慣性センサー（加速度計+ジャイロスコープ）を使用して、デバイスの姿勢（Roll/Pitch/Yaw）と位置（X/Y/Z）をリアルタイムで推定するアプリケーション。

**主な機能**:
- ✅ 6軸センサーデータの取得（加速度・角速度）
- ✅ Madgwick AHRS アルゴリズムによる姿勢推定
- ✅ Quaternion（四元数）から Euler角（Roll/Pitch/Yaw）への変換
- ✅ 座標変換による位置推定（積分ベース）
- ✅ センサーバイアス補正とキャリブレーション
- ✅ リアルタイム出力（10Hz）

### 1.2 開発環境

- **OS**: NuttX RTOS
- **ハードウェア**: Sony Spresense
- **センサー**: BMI160 (I2C接続)
- **外部ライブラリ**: MadgwickAHRS
- **プログラミング言語**: C
- **ビルドシステム**: Kconfig + Make

---

## 2. システムアーキテクチャ

### 2.1 コンポーネント図

\`\`\`plantuml
@startuml
package "BMI160 Orientation Application" {
  [Main Application] as Main
  [Orientation Calculator] as OrientCalc
  [Position Estimator] as PosEst
}

package "External Libraries" {
  [MadgwickAHRS] as Madgwick
}

package "NuttX System" {
  [BMI160 Driver] as BMI160Drv
  [Sensor Framework] as SensorFW
}

package "Hardware" {
  [BMI160 Sensor] as BMI160HW
}

Main --> OrientCalc : quaternion_to_euler()
Main --> PosEst : position_update()
Main --> Madgwick : MadgwickAHRSupdateIMU()
Main --> BMI160Drv : read()

OrientCalc --> Madgwick : quaternion2euler()
OrientCalc ..> Madgwick : quaternion_to_dcm()

PosEst --> OrientCalc : quaternion_to_dcm()

BMI160Drv --> SensorFW : sensor_ops
SensorFW --> BMI160HW : I2C

note right of Main
  メインループ:
  1. センサーデータ読み取り
  2. 姿勢更新（AHRS）
  3. 位置推定更新
  4. 結果表示
end note

note right of PosEst
  積分ベースの位置推定:
  - 加速度 → 速度
  - 速度 → 位置
  バイアス補正とドリフト対策
end note

@enduml
\`\`\`

### 2.2 レイヤー構造

\`\`\`
┌─────────────────────────────────────────┐
│  Application Layer                      │
│  ┌─────────────────────────────────┐   │
│  │ bmi160_orientation_main.c       │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  Business Logic Layer                   │
│  ┌──────────────┐  ┌──────────────┐    │
│  │orientation_  │  │position_     │    │
│  │calc.c/h      │  │estimator.c/h │    │
│  └──────────────┘  └──────────────┘    │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  Algorithm Library Layer                │
│  ┌─────────────────────────────────┐   │
│  │ MadgwickAHRS (External)         │   │
│  │ - Quaternion computation        │   │
│  │ - Sensor fusion (IMU)           │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  Driver Layer (NuttX)                   │
│  ┌─────────────────────────────────┐   │
│  │ BMI160 Driver                   │   │
│  │ (/dev/accel0)                   │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  Hardware Layer                         │
│  ┌─────────────────────────────────┐   │
│  │ BMI160 Sensor (I2C)             │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
\`\`\`

---

## 3. 詳細設計

### 3.1 データフロー図

\`\`\`plantuml
@startuml
skinparam activityShape octagon

start

:BMI160センサーから<
6軸データ読み取り;
note right
  加速度: ax, ay, az
  角速度: gx, gy, gz
  (Raw 16bit値)
end note

:センサーデータ変換;
note right
  - ジャイロ: rad/s
  - 加速度: m/s²
  スケール係数適用
end note

partition "姿勢推定" {
  :Madgwick AHRS更新;
  note right
    入力: gx,gy,gz, ax,ay,az
    出力: Quaternion q[4]
    フィルタパラメータ: β=0.1
  end note

  :Quaternion → Euler角変換;
  note right
    出力:
    - Roll (度)
    - Pitch (度)
    - Yaw (度)
  end note
}

partition "位置推定" {
  if (キャリブレーション完了?) then (no)
    :バイアス蓄積;
    note right
      100サンプル平均
    end note
  else (yes)
    :バイアス除去;

    :Quaternion → DCM変換;
    note right
      方向余弦行列
      3×3 回転行列
    end note

    :センサー座標系 →
    ワールド座標系変換;
    note right
      DCMによる座標回転
      重力成分除去
    end note

    :加速度積分 → 速度;
    note right
      v = v + a·dt
      ドリフト対策:
      |v| < 0.01m/s → v=0
    end note

    :速度積分 → 位置;
    note right
      x = x + v·dt
    end note
  endif
}

:結果表示;
note right
  10Hzで出力:
  - Roll/Pitch/Yaw
  - X/Y/Z 位置
end note

stop

@enduml
\`\`\`

### 3.2 シーケンス図

#### 3.2.1 初期化シーケンス

\`\`\`plantuml
@startuml
actor User
participant "main()" as Main
participant "AHRS" as AHRS
participant "Position\nEstimator" as PosEst
participant "BMI160\nDriver" as Driver

User -> Main: アプリ起動
activate Main

Main -> AHRS: INIT_AHRS(&g_ahrs,\n  beta=0.1, rate=100Hz)
activate AHRS
AHRS -> AHRS: 内部状態初期化
AHRS --> Main: 初期化完了
deactivate AHRS

Main -> PosEst: position_estimator_init(\n  &g_pos_state)
activate PosEst
PosEst -> PosEst: メモリクリア\n位置・速度・バイアス = 0
PosEst --> Main: 初期化完了
deactivate PosEst

Main -> Driver: open("/dev/accel0",\n  O_RDONLY)
activate Driver
Driver -> Driver: デバイスオープン\nセンサー設定確認
Driver --> Main: fd (成功)\nまたは -1 (失敗)
deactivate Driver

alt 成功
  Main -> Main: メインループ開始
else 失敗
  Main -> User: エラーメッセージ
  Main -> Main: プログラム終了
end

deactivate Main

@enduml
\`\`\`

#### 3.2.2 メインループ処理シーケンス

\`\`\`plantuml
@startuml
participant "main()" as Main
participant "BMI160\nDriver" as Driver
participant "MadgwickAHRS" as Madgwick
participant "Orientation\nCalculator" as Orient
participant "Position\nEstimator" as PosEst

loop メインループ (無限)
  Main -> Driver: read(fd, &raw_data,\n  sizeof(accel_gyro_st_s))
  activate Driver
  Driver -> Driver: センサーデータ取得\n(I2C経由)
  Driver --> Main: センサーデータ\n(Raw 16bit)
  deactivate Driver

  alt タイムスタンプ変化あり
    Main -> Main: convert_sensor_data()
    note right
      Raw値 → 物理単位変換
      - ジャイロ: rad/s
      - 加速度: m/s²
    end note

    Main -> Madgwick: MadgwickAHRSupdateIMU(\n  &g_ahrs, gx,gy,gz,\n  ax,ay,az, dt)
    activate Madgwick
    Madgwick -> Madgwick: センサーフュージョン\nQuaternion更新
    Madgwick --> Main: 更新されたQuaternion
    deactivate Madgwick

    Main -> Orient: orientation_get_euler(\n  &g_ahrs, &roll,\n  &pitch, &yaw)
    activate Orient
    Orient -> Orient: Quaternion →\nEuler角変換
    Orient --> Main: Roll, Pitch, Yaw\n(度)
    deactivate Orient

    Main -> PosEst: position_estimator_update(\n  &g_pos_state, &g_ahrs,\n  ax, ay, az, dt)
    activate PosEst

    alt キャリブレーション中
      PosEst -> PosEst: バイアス蓄積\n(100サンプル)
    else キャリブレーション完了
      PosEst -> Orient: quaternion_to_dcm(\n  g_ahrs.q, dcm)
      activate Orient
      Orient --> PosEst: DCM (3×3行列)
      deactivate Orient

      PosEst -> PosEst: 座標変換\n(Sensor → World)
      PosEst -> PosEst: 重力除去
      PosEst -> PosEst: 積分: a→v→x
      PosEst -> PosEst: ドリフト対策
    end

    PosEst --> Main: 更新された位置\n(X, Y, Z)
    deactivate PosEst

    alt サンプルカウント >= 10
      Main -> Main: print_orientation_\n  position()
      note right
        10Hz出力:
        Roll/Pitch/Yaw
        X/Y/Z
      end note
    end
  end

  Main -> Main: usleep(1000)\n(1ms待機)
end

@enduml
\`\`\`

#### 3.2.3 位置推定詳細シーケンス

\`\`\`plantuml
@startuml
participant "Position\nEstimator" as PosEst
participant "Orientation\nCalculator" as Orient

-> PosEst: position_estimator_update(\n  state, ahrs, ax,ay,az, dt)
activate PosEst

alt !calibrated
  PosEst -> PosEst: bias_x += ax\nbias_y += ay\nbias_z += az\ncalib_samples++

  alt calib_samples >= 100
    PosEst -> PosEst: bias = sum / 100\ncalibrated = 1
  end

  <-- PosEst: return (キャリブレーション中)
else calibrated
  PosEst -> PosEst: バイアス除去\nax -= bias_x\nay -= bias_y\naz -= bias_z

  PosEst -> Orient: orientation_quaternion_to_dcm(\n  ahrs->q, dcm)
  activate Orient
  Orient -> Orient: Quaternion → DCM計算\n9要素計算
  Orient --> PosEst: dcm[3][3]
  deactivate Orient

  PosEst -> PosEst: 座標変換\nax_world = dcm[0][0]*ax + ...\nay_world = dcm[1][0]*ax + ...\naz_world = dcm[2][0]*ax + ...

  PosEst -> PosEst: 重力除去\naz_world -= 9.80665

  PosEst -> PosEst: 速度更新\nvx += ax_world * dt\nvy += ay_world * dt\nvz += az_world * dt

  PosEst -> PosEst: ドリフト対策\nif |v| < 0.01: v = 0

  PosEst -> PosEst: 位置更新\nx += vx * dt\ny += vy * dt\nz += vz * dt

  <-- PosEst: return (位置更新完了)
end

deactivate PosEst

@enduml
\`\`\`

---

## 4. モジュール仕様

### 4.1 bmi160_orientation_main.c

**責務**: アプリケーションのメインエントリーポイント、全体制御

**主要関数**:

| 関数名 | 説明 | 入力 | 出力 |
|--------|------|------|------|
| `main()` | メインループ | argc, argv | 0 (正常) / -1 (エラー) |
| `convert_sensor_data()` | センサーデータ変換 | raw (16bit) | gx,gy,gz, ax,ay,az (物理単位) |
| `print_orientation_position()` | 結果表示 | timestamp, Roll/Pitch/Yaw, X/Y/Z | - |

**主要定数**:

\`\`\`c
#define BMI160_DEVPATH      "/dev/accel0"
#define SAMPLE_RATE_HZ      100.0f      // サンプリングレート
#define MADGWICK_BETA       0.1f        // AHRSフィルタパラメータ
#define GYRO_SCALE          (2000.0f / 32768.0f * M_PI / 180.0f)
#define ACCEL_SCALE         (16.0f / 32768.0f)
#define GRAVITY             9.80665f
\`\`\`

**データフロー**:
1. BMI160デバイスオープン → `/dev/accel0`
2. AHRSとPosition Estimator初期化
3. 無限ループ:
   - センサーデータ読み取り
   - 物理単位変換
   - AHRS更新
   - Euler角取得
   - 位置推定更新
   - 10サンプルごとに表示（10Hz）

### 4.2 orientation_calc.c/h

**責務**: Quaternionから姿勢角への変換、座標変換行列計算

**主要関数**:

| 関数名 | 説明 | 入力 | 出力 |
|--------|------|------|------|
| `orientation_get_euler()` | Quaternion → Euler角 | ahrs (Quaternion) | roll, pitch, yaw (度) |
| `orientation_quaternion_to_dcm()` | Quaternion → DCM | q[4] | dcm[3][3] |

**アルゴリズム**:

**Euler角変換**:
\`\`\`c
// Madgwickライブラリのquaternion2euler()を使用
quaternion2euler(ahrs->q, euler);
roll  = euler[0] * RAD_TO_DEG;
pitch = euler[1] * RAD_TO_DEG;
yaw   = euler[2] * RAD_TO_DEG;
\`\`\`

**DCM変換**:
\`\`\`
DCM = | 1-2(q2²+q3²)   2(q1q2-q0q3)   2(q1q3+q0q2) |
      | 2(q1q2+q0q3)   1-2(q1²+q3²)   2(q2q3-q0q1) |
      | 2(q1q3-q0q2)   2(q2q3+q0q1)   1-2(q1²+q2²) |
\`\`\`

### 4.3 position_estimator.c/h

**責務**: 加速度データから位置推定、バイアス補正

**データ構造**:

\`\`\`c
struct position_state_s
{
  float x, y, z;         // 位置 (m)
  float vx, vy, vz;      // 速度 (m/s)
  float bias_x, bias_y, bias_z;  // バイアス
  int calibrated;        // キャリブレーション完了フラグ
  int calib_samples;     // キャリブレーションサンプル数
};
\`\`\`

**主要関数**:

| 関数名 | 説明 | 入力 | 出力 |
|--------|------|------|------|
| `position_estimator_init()` | 初期化 | state | - |
| `position_estimator_update()` | 位置推定更新 | state, ahrs, ax/ay/az, dt | - |
| `position_estimator_reset()` | 位置・速度リセット | state | - |

**アルゴリズム**:

1. **キャリブレーション** (最初の100サンプル):
   \`\`\`
   bias = Σ(accel) / 100
   \`\`\`

2. **座標変換** (Sensor → World):
   \`\`\`
   a_world = DCM × a_sensor
   \`\`\`

3. **重力除去**:
   \`\`\`
   az_world -= 9.80665 m/s²
   \`\`\`

4. **積分**:
   \`\`\`
   v(t+dt) = v(t) + a(t)·dt
   x(t+dt) = x(t) + v(t)·dt
   \`\`\`

5. **ドリフト対策**:
   \`\`\`
   if |v| < 0.01 m/s: v = 0
   \`\`\`

---

## 5. 外部依存関係

### 5.1 MadgwickAHRS ライブラリ

**機能**: センサーフュージョンによる姿勢推定

**主要API**:

\`\`\`c
// 初期化マクロ
INIT_AHRS(ahrs, beta, sample_rate);

// IMU更新（加速度+ジャイロ）
void MadgwickAHRSupdateIMU(struct ahrs_out_s *ahrs,
                           float gx, float gy, float gz,
                           float ax, float ay, float az,
                           float dt);

// Quaternion → Euler角変換
void quaternion2euler(const float q[4], float euler[3]);
\`\`\`

**出力データ構造**:

\`\`\`c
struct ahrs_out_s
{
  float q[4];    // Quaternion [q0, q1, q2, q3]
  // その他の内部状態...
};
\`\`\`

### 5.2 BMI160 ドライバー (NuttX)

**デバイスパス**: `/dev/accel0`

**データ構造**:

\`\`\`c
struct accel_gyro_st_s
{
  struct sensor_accel accel;  // 加速度データ (x,y,z: int16_t)
  struct sensor_gyro gyro;    // ジャイロデータ (x,y,z: int16_t)
  uint32_t sensor_time;       // タイムスタンプ
};
\`\`\`

**操作**:

\`\`\`c
// デバイスオープン
int fd = open("/dev/accel0", O_RDONLY);

// データ読み取り
read(fd, &raw_data, sizeof(struct accel_gyro_st_s));

// クローズ
close(fd);
\`\`\`

---

## 6. ビルド設定

### 6.1 Kconfig 設定

**依存関係**:
- `SENSORS_BMI160`: BMI160センサードライバー
- `EXTERNALS_AHRS`: Madgwick AHRSライブラリ

**設定パラメータ**:
- `EXAMPLES_BMI160_ORIENTATION_PROGNAME`: プログラム名 (デフォルト: "bmi160_orientation")
- `EXAMPLES_BMI160_ORIENTATION_PRIORITY`: タスク優先度 (デフォルト: 100)
- `EXAMPLES_BMI160_ORIENTATION_STACKSIZE`: スタックサイズ (デフォルト: 4096 bytes)

### 6.2 Makefile

\`\`\`makefile
# ソースファイル
CSRCS = bmi160_orientation_main.c
CSRCS += orientation_calc.c
CSRCS += position_estimator.c

# プログラム名
PROGNAME = bmi160_orientation

# 優先度とスタックサイズ
PRIORITY = 100
STACKSIZE = 4096
\`\`\`

---

## 7. 動作仕様

### 7.1 サンプリングレート

- **センサー読み取り**: 100 Hz (10ms間隔)
- **AHRS更新**: 100 Hz
- **位置推定更新**: 100 Hz
- **画面出力**: 10 Hz (100サンプルごと)

### 7.2 キャリブレーション

- **期間**: 最初の100サンプル (1秒間)
- **目的**: 加速度計のバイアス推定
- **条件**: デバイスを静止状態に保つ
- **処理**: 加速度データの平均を計算

### 7.3 出力フォーマット

\`\`\`
Time(ms)      Roll    Pitch      Yaw   |     X       Y       Z
              [deg]   [deg]    [deg]  |    [m]     [m]     [m]
---------------------------------------------------------------
[   1000]   Roll:  0.23° Pitch: -1.45° Yaw:  89.12° | X:  0.001m Y: -0.002m Z:  0.000m
[   1100]   Roll:  0.45° Pitch: -1.38° Yaw:  89.34° | X:  0.002m Y: -0.003m Z:  0.001m
...
\`\`\`

### 7.4 座標系

**センサー座標系** (BMI160):
- X軸: 前方
- Y軸: 左方
- Z軸: 上方

**ワールド座標系**:
- X軸: 北方向（または初期Yaw方向）
- Y軸: 西方向
- Z軸: 上方（重力と逆方向）

### 7.5 精度と制限

**姿勢推定**:
- Roll/Pitch精度: ±1-2°（静止時）
- Yaw精度: ±5-10°（磁力計なしのため）
- 更新レート: 100 Hz

**位置推定**:
- 短期精度: ±数cm（数秒間）
- ドリフト: 時間経過とともに累積
- 制限: 積分ベースのため長期使用に不向き
- 改善策: カルマンフィルタや外部位置情報との融合が必要

---

## 8. 処理フロー図

### 8.1 状態遷移図

\`\`\`plantuml
@startuml
[*] --> 初期化

初期化 : AHRS初期化
初期化 : Position Estimator初期化
初期化 : デバイスオープン

初期化 --> キャリブレーション中 : 成功
初期化 --> [*] : 失敗

キャリブレーション中 : バイアス蓄積
キャリブレーション中 : サンプルカウント

キャリブレーション中 --> 通常動作 : 100サンプル完了

通常動作 : センサー読み取り
通常動作 : AHRS更新
通常動作 : 位置推定
通常動作 : 結果表示

通常動作 --> 通常動作 : ループ継続
通常動作 --> [*] : エラーまたは終了

@enduml
\`\`\`

### 8.2 クラス図（データ構造）

\`\`\`plantuml
@startuml
class "position_state_s" {
  +float x, y, z
  +float vx, vy, vz
  +float bias_x, bias_y, bias_z
  +int calibrated
  +int calib_samples
}

class "ahrs_out_s" {
  +float q[4]
}

class "accel_gyro_st_s" {
  +sensor_accel accel
  +sensor_gyro gyro
  +uint32_t sensor_time
}

class "sensor_accel" {
  +int16_t x, y, z
}

class "sensor_gyro" {
  +int16_t x, y, z
}

accel_gyro_st_s *-- sensor_accel
accel_gyro_st_s *-- sensor_gyro

note right of position_state_s
  位置推定状態
  - 位置・速度
  - バイアス補正値
  - キャリブレーション状態
end note

note right of ahrs_out_s
  AHRS出力
  - Quaternion (姿勢)
end note

@enduml
\`\`\`

---

## 9. パフォーマンス要件

### 9.1 リソース使用量

- **スタックサイズ**: 4096 bytes
- **ヒープ使用**: なし（静的メモリのみ）
- **タスク優先度**: 100（中優先度）
- **CPU使用率**: ~5-10%（100Hz動作時）

### 9.2 応答時間

- **センサー読み取り**: <1ms
- **AHRS更新**: <2ms
- **位置推定更新**: <1ms
- **総処理時間**: <5ms/サンプル

### 9.3 メモリ配置

\`\`\`
グローバル変数:
├── g_ahrs          : struct ahrs_out_s    (~100 bytes)
└── g_pos_state     : position_state_s     (~40 bytes)

スタック:
├── main()ローカル変数                     (~100 bytes)
├── センサーデータバッファ                  (~50 bytes)
└── 関数呼び出しオーバーヘッド              (~200 bytes)
\`\`\`

---

## 10. エラーハンドリング

### 10.1 エラーケース

| エラー | 検出方法 | 対処 |
|--------|---------|------|
| デバイスオープン失敗 | `open()` < 0 | エラーメッセージ表示、終了 |
| センサー読み取り失敗 | `read()` != sizeof | エラーメッセージ表示、ループ脱出 |
| キャリブレーション不良 | - | 自動的に再キャリブレーション |

### 10.2 異常検知

- タイムスタンプ変化なし → データスキップ
- 加速度異常値 → フィルタリング（閾値チェック）
- 位置ドリフト → 速度閾値による抑制

---

## 11. 今後の拡張

### 11.1 改善案

1. **磁力計統合**: Yaw精度向上（9軸AHRS）
2. **カルマンフィルタ**: 位置推定精度向上
3. **ゼロ速度更新**: ドリフト補正
4. **外部位置情報**: GPS/UWB統合
5. **ログ記録**: SDカードへのデータ保存
6. **リアルタイム可視化**: グラフ表示

### 11.2 応用例

- ドローンの姿勢制御
- ロボットのナビゲーション
- VR/ARトラッキング
- 人間の動作解析
- 振動モニタリング

---

## 12. 参考資料

### 12.1 アルゴリズム

- Madgwick, S. O. H. "An efficient orientation filter for inertial and inertial/magnetic sensor arrays" (2010)
- Quaternion mathematics and operations
- Direction Cosine Matrix (DCM) theory

### 12.2 センサー仕様

- BMI160 Datasheet (Bosch Sensortec)
- I2C通信プロトコル
- センサー座標系定義

### 12.3 NuttX API

- Sensor Framework API
- File I/O operations
- POSIX threading

---

**文書バージョン**: 1.0
**作成日**: 2025-12-14
**対象システム**: Spresense BMI160 Orientation Example
**作成者**: Automated Documentation

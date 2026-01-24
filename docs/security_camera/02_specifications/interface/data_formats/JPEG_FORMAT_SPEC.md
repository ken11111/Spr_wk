# JPEG フォーマット仕様

**バージョン**: 2.0 (Phase 1.5対応)
**日付**: 2026-01-22
**エンコーダー**: ISX012 ハードウェアJPEGエンコーダー

## 概要

SpresenseカメラシステムにおけるJPEGフォーマット仕様。ISX012ハードウェアエンコーダーの特性とPhase 1.5で判明した性能特性を詳細に規定。

## サポート解像度

### QVGA (320×240) - Phase 1.0
```
解像度: 320×240 pixels
アスペクト比: 4:3
総ピクセル数: 76,800 pixels
用途: 初期開発・デバッグ
フレームレート: 30+ fps達成
```

### VGA (640×480) - Phase 1.5 ⭐
```
解像度: 640×480 pixels
アスペクト比: 4:3
総ピクセル数: 307,200 pixels (QVGA の 4倍)
用途: 本格運用
フレームレート: 11 fps安定
```

## JPEG圧縮特性

### 基本仕様
- **フォーマット**: JPEG (ITU-T T.81 / ISO 10918-1)
- **色空間**: YUV 4:2:0 (Y:Cb:Cr = 4:1:1サブサンプリング)
- **エンコーディング**: ベースライン DCT
- **ハフマン符号化**: 標準テーブル使用
- **量子化**: ISX012内部テーブル (品質固定)

### VGAサイズ分析 (Phase 1.5実測) ⭐

#### ファイルサイズ分布
```
シーン複雑度別JPEG圧縮結果:

【シンプルシーン】(壁・単色背景)
- 最小: 36KB
- 典型: 38-42KB
- 圧縮率: 1:18.4 (640×480×24bit → 38KB)

【標準シーン】(室内・一般的な被写体)
- 典型: 45-55KB
- 圧縮率: 1:14.0

【複雑シーン】(多数オブジェクト・高周波成分)
- 最大: 65KB
- 圧縮率: 1:11.8
- エンコード負荷: 高 (レイテンシ急増)
```

#### サイズ変動要因
1. **エッジ密度**: エッジ多 → ファイルサイズ大
2. **色変化**: グラデーション・ノイズ → 圧縮効率悪化
3. **照明条件**: 暗所ノイズ → サイズ増加
4. **動きブラー**: 手ブレ → 高周波成分増加

### エンコード性能特性

#### レイテンシ変動 (Phase 1.5重大発見) 🔴

```
ISX012 JPEG エンコードレイテンシ:

【正常動作】
- 最短: 0.05ms (シンプルシーン)
- 典型: 2-6ms (標準シーン)

【高負荷動作】
- 最長: 265ms (複雑シーン)
- 変動倍率: 5,300x (0.05ms → 265ms)

⚠️ Critical Finding:
シーン複雑度により性能が5,300倍変動
→ 固定シーンでのテスト必須
```

#### 負荷分析
```c
// camera_threads.c での実測値
typedef struct {
    uint64_t encode_start_us;
    uint64_t encode_end_us;
    uint32_t jpeg_size_bytes;
    uint8_t  scene_complexity;  // 0-255
} jpeg_encode_stats_t;

// 実測例:
// Simple scene: encode_time = 50μs,  jpeg_size = 38KB
// Complex scene: encode_time = 265ms, jpeg_size = 65KB
```

## V4L2設定パラメータ

### ISX012固有設定
```c
// ISX012 JPEG設定
struct v4l2_control ctrl;

// 解像度設定
struct v4l2_format fmt;
fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
fmt.fmt.pix.width = 640;   // VGA
fmt.fmt.pix.height = 480;
fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
fmt.fmt.pix.field = V4L2_FIELD_NONE;

// JPEG品質設定 (ISX012固定)
ctrl.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
ctrl.value = 80;  // 0-100, ISX012では固定値
```

### バッファ設定 (Phase 1.5最適化)
```c
// V4L2バッファ要求
struct v4l2_requestbuffers reqbuf;
reqbuf.count = 3;  // 重要: ISX012最大値 (4以上は性能劣化)
reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
reqbuf.memory = V4L2_MEMORY_MMAP;

// Phase 1.5発見: CAMERA_BUFFER_NUM=3が最適
#define CAMERA_BUFFER_NUM  3  // V4L2ドライバー制限
```

## メモリ管理

### バッファサイズ設計

#### JPEG出力バッファ
```c
// 最大サイズ設計 (Phase 1.5実測ベース)
#define MAX_JPEG_SIZE_VGA    (96 * 1024)   // 96KB (安全マージン47%)
#define MAX_JPEG_SIZE_QVGA   (32 * 1024)   // 32KB

// 実測最大値: 65KB (VGA complex scene)
// 安全率: (96-65)/65 = 47.7%
```

#### バッファプール設計
```c
typedef struct {
    uint8_t  jpeg_data[MAX_JPEG_SIZE_VGA];
    uint32_t jpeg_size;
    uint64_t timestamp_us;
    uint32_t sequence;
    bool     in_use;
} jpeg_buffer_t;

// トリプルバッファリング (V4L2制限対応)
static jpeg_buffer_t g_jpeg_buffers[3];
```

### メモリ最適化実績 (Phase 1.5)
```
メモリ使用量削減:
- Phase 1.0: 1.5MB (過大設計)
- Phase 1.5: 192KB (実測ベース設計)
- 削減率: 87.2%
```

## 品質パラメータ

### ISX012 JPEG品質制御

#### 固定パラメータ (変更不可)
- **量子化テーブル**: ISX012内蔵 (ITU-T T.81標準ベース)
- **ハフマンテーブル**: ISX012内蔵標準テーブル
- **サブサンプリング**: 4:2:0固定
- **品質数値**: 概算80相当 (推定)

#### 動的制御 (将来検討)
```c
// 将来の動的品質制御案
typedef struct {
    uint8_t target_quality;     // 目標品質 (50-95)
    uint32_t target_size_kb;    // 目標ファイルサイズ
    uint32_t max_encode_ms;     // 最大エンコード時間
} jpeg_quality_control_t;

// 帯域制限時の品質自動調整
int jpeg_auto_adjust_quality(uint32_t available_bandwidth_kbps)
{
    if (available_bandwidth_kbps < 2000) {
        return 60;  // 低品質・小サイズ
    } else if (available_bandwidth_kbps < 5000) {
        return 75;  // 中品質
    } else {
        return 80;  // 高品質 (現在の固定値)
    }
}
```

## 性能ベンチマーク

### フレームレート実測 (Phase 1.5)

#### VGA性能プロファイル
```
Test Environment:
- Resolution: 640×480 VGA
- Scene: Fixed complexity (標準室内シーン)
- Duration: 300フレーム連続キャプチャ
- Buffer: CAMERA_BUFFER_NUM=3

Results:
- Target FPS: 30 fps
- Achieved FPS: 11.0 fps (安定)
- Success Rate: 100% (300/300フレーム)
- Frame Drop: 0%
```

#### レイテンシ分析 (ms)
| 処理段階 | 最小 | 典型 | 最大 | 割合 |
|---------|------|------|------|------|
| **V4L2キャプチャ** | 4.5 | 5.5 | 7.2 | 12% |
| **JPEG エンコード** | 0.05 | 5.8 | 265 | 13%* |
| **パケット化** | 1.8 | 2.1 | 2.8 | 5% |
| **CRC計算** | 8.2 | 8.7 | 9.4 | 20% |
| **USB転送** | 28 | 30 | 33 | 68% |
| **合計** | 44 | 48 | 317* | 100% |

*複雑シーン時の異常値

### 制約・ボトルネック

#### Hardware制約
1. **V4L2バッファ制限**: 最大3バッファ (ドライバー制限)
2. **USB帯域制約**: 12Mbps Full Speed (30fps困難)
3. **JPEG負荷変動**: シーン依存5,300倍変動

#### Software最適化余地
1. **CRC最適化**: 8.7ms → <5ms可能 (ハードウェアCRC)
2. **パケット化**: 2.1ms → <1ms可能 (ゼロコピー)
3. **USB転送**: DMA使用で-10%可能

## エラーハンドリング

### JPEG エンコードエラー
```c
typedef enum {
    JPEG_ERROR_NONE = 0,
    JPEG_ERROR_TIMEOUT = 1,        // エンコードタイムアウト
    JPEG_ERROR_BUFFER_FULL = 2,    // 出力バッファ不足
    JPEG_ERROR_V4L2_FAILURE = 3,   // V4L2ドライバーエラー
    JPEG_ERROR_SIZE_OVERFLOW = 4,  // 予想サイズ超過
} jpeg_error_t;

int handle_jpeg_encode_error(jpeg_error_t error)
{
    switch (error) {
        case JPEG_ERROR_TIMEOUT:
            // 複雑シーン検出 → 品質下げて再試行
            LOG_WARN("JPEG encode timeout - complex scene detected");
            return retry_with_lower_quality();

        case JPEG_ERROR_SIZE_OVERFLOW:
            // バッファサイズ不足 → 緊急対応
            LOG_ERROR("JPEG size exceeded buffer: %d > %d",
                     actual_size, MAX_JPEG_SIZE_VGA);
            return enlarge_buffer_or_reduce_quality();

        default:
            return handle_generic_error(error);
    }
}
```

### サイズ検証
```c
bool validate_jpeg_size(uint32_t jpeg_size)
{
    if (jpeg_size < MIN_JPEG_SIZE_VGA) {
        LOG_WARN("Suspiciously small JPEG: %d bytes", jpeg_size);
        return false;  // 破損の可能性
    }

    if (jpeg_size > MAX_JPEG_SIZE_VGA) {
        LOG_ERROR("JPEG too large: %d > %d bytes", jpeg_size, MAX_JPEG_SIZE_VGA);
        return false;  // バッファオーバーフロー危険
    }

    return true;
}
```

## テスト・検証

### 標準テストシーン
```c
// テストシーン定義 (再現可能性確保)
typedef struct {
    const char *scene_name;
    uint32_t   expected_size_kb;
    uint32_t   max_encode_ms;
    const char *description;
} test_scene_t;

static const test_scene_t test_scenes[] = {
    {"simple_wall", 38, 10, "白壁・最小複雑度"},
    {"standard_room", 48, 50, "標準室内・中程度複雑度"},
    {"complex_outdoor", 62, 200, "屋外・高複雑度"},
    {"worst_case", 65, 265, "最悪ケース・最大複雑度"},
};
```

### 性能回帰テスト
```c
void test_jpeg_performance_regression(void)
{
    for (int i = 0; i < ARRAY_SIZE(test_scenes); i++) {
        const test_scene_t *scene = &test_scenes[i];

        // シーン設定
        setup_test_scene(scene->scene_name);

        // 10回測定の平均
        uint32_t total_size = 0;
        uint32_t total_time_ms = 0;

        for (int j = 0; j < 10; j++) {
            uint64_t start_us = get_timestamp_us();
            uint32_t jpeg_size = capture_jpeg_frame();
            uint64_t end_us = get_timestamp_us();

            total_size += jpeg_size;
            total_time_ms += (end_us - start_us) / 1000;
        }

        uint32_t avg_size = total_size / 10;
        uint32_t avg_time_ms = total_time_ms / 10;

        // 性能検証
        assert(avg_size <= scene->expected_size_kb * 1024 * 1.1);  // +10%許容
        assert(avg_time_ms <= scene->max_encode_ms * 1.2);         // +20%許容

        LOG_INFO("Scene '%s': size=%dKB, time=%dms",
                scene->scene_name, avg_size/1024, avg_time_ms);
    }
}
```

## 将来拡張

### 動的品質制御 (Phase 2.0)
```c
// 帯域適応品質制御
typedef struct {
    uint32_t current_bandwidth_kbps;
    uint32_t target_fps;
    uint8_t  adaptive_quality;        // 動的品質値
    bool     quality_control_enabled;
} adaptive_jpeg_t;

int jpeg_adaptive_quality_update(uint32_t available_bandwidth)
{
    // 帯域に応じた品質自動調整
    // 低帯域 → 品質下げてフレームレート優先
    // 高帯域 → 品質上げて画質優先
}
```

### マルチ解像度対応
```c
// 動的解像度切り替え
typedef enum {
    RESOLUTION_QVGA = 0,    // 320×240
    RESOLUTION_VGA = 1,     // 640×480
    RESOLUTION_HD = 2,      // 1280×720 (将来)
} resolution_mode_t;

int jpeg_change_resolution(resolution_mode_t mode)
{
    // 実行時解像度変更
    // V4L2フォーマット再設定
    // バッファサイズ動的調整
}
```

### ISX019対応 (将来)
```c
// 次世代カメラセンサー対応
#ifdef CAMERA_ISX019
#define JPEG_QUALITY_CONTROL_AVAILABLE  1
#define DYNAMIC_QUALITY_RANGE           30-95
#define HARDWARE_QUALITY_ADJUSTMENT     1
#endif
```

**Phase 1.5実測データに基づくJPEGフォーマット仕様の完全規定** ✅
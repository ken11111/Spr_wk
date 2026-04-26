/****************************************************************************
 * security_camera/frame_statistics.h
 *
 *   Copyright 2025 Security Camera Project - Phase 11
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#ifndef __SECURITY_CAMERA_FRAME_STATISTICS_H
#define __SECURITY_CAMERA_FRAME_STATISTICS_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Frame Statistics Configuration (Phase 11) */
#define FRAME_STATISTICS_WINDOW_SIZE    10      /* フレーム統計窓サイズ */
#define COMPLEXITY_THRESHOLD            0.5f    /* 複雑度閾値 */
#define MIN_FRAME_SIZE_BYTES           1024     /* 最小フレームサイズ (1KB) */
#define MAX_FRAME_SIZE_BYTES          (128*1024) /* 最大フレームサイズ (128KB) */

/* 統計計算パラメータ */
#define STATISTICS_UPDATE_INTERVAL_MS   100     /* 統計更新間隔 */
#define COMPLEXITY_SMOOTHING_FACTOR     0.8f    /* 複雑度平滑化係数 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* フレーム統計構造体 */
typedef struct frame_statistics_s
{
  /* 基本統計 */
  uint32_t frame_count;               /* 総フレーム数 */
  uint32_t window_size;               /* 統計窓サイズ（通常10） */
  uint32_t size_history[FRAME_STATISTICS_WINDOW_SIZE]; /* サイズ履歴 */
  uint8_t history_index;              /* 履歴インデックス（循環バッファ） */

  /* 計算済み統計 */
  uint32_t total_size_bytes;          /* 窓内総サイズ */
  uint32_t avg_size_bytes;            /* 平均サイズ */
  uint32_t min_size_bytes;            /* 最小サイズ */
  uint32_t max_size_bytes;            /* 最大サイズ */
  uint64_t size_variance;             /* サイズ分散（固定小数点） */

  /* 複雑度指数 */
  float complexity_index;             /* シーン複雑度指数（0.0-1.0+） */
  float smoothed_complexity;          /* 平滑化された複雑度指数 */

  /* タイムスタンプ */
  uint64_t last_update_us;            /* 最終更新時刻（マイクロ秒） */
  uint64_t first_frame_us;            /* 最初のフレーム時刻 */

  /* 制御情報 */
  bool initialized;                   /* 初期化フラグ */
  bool window_full;                   /* 窓が満杯かどうか */

} frame_statistics_t;

/* フレームサイズトレンド情報 */
typedef struct frame_trend_s
{
  float trend_coefficient;            /* トレンド係数（線形回帰） */
  float correlation;                  /* 相関係数 */
  bool trend_increasing;              /* 増加トレンドかどうか */
  bool trend_valid;                   /* トレンドが有効かどうか */
} frame_trend_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief フレーム統計システムを初期化
 * @param stats 統計構造体
 * @return 0: success, <0: error
 */
int frame_statistics_init(frame_statistics_t *stats);

/**
 * @brief フレームサイズで統計を更新
 * @param stats 統計構造体
 * @param frame_size フレームサイズ（バイト）
 * @param timestamp_us タイムスタンプ（マイクロ秒）
 * @return 0: success, <0: error
 */
int frame_statistics_update(frame_statistics_t *stats, uint32_t frame_size, uint64_t timestamp_us);

/**
 * @brief 複雑度指数を計算
 * @param stats 統計構造体
 * @return 複雑度指数（0.0-1.0+）
 */
float frame_statistics_calculate_complexity(frame_statistics_t *stats);

/**
 * @brief フレームサイズトレンドを分析
 * @param stats 統計構造体
 * @param trend トレンド情報出力
 * @return 0: success, <0: error
 */
int frame_statistics_analyze_trend(frame_statistics_t *stats, frame_trend_t *trend);

/**
 * @brief 平均フレームサイズを取得（KB単位）
 * @param stats 統計構造体
 * @return 平均フレームサイズ（KB）
 */
float frame_statistics_get_avg_size_kb(frame_statistics_t *stats);

/**
 * @brief 正規化されたサイズ分散を取得
 * @param stats 統計構造体
 * @return 正規化された分散（0.0-1.0）
 */
float frame_statistics_get_normalized_variance(frame_statistics_t *stats);

/**
 * @brief 統計をリセット
 * @param stats 統計構造体
 */
void frame_statistics_reset(frame_statistics_t *stats);

/**
 * @brief デバッグ情報を出力
 * @param stats 統計構造体
 */
void frame_statistics_debug_print(frame_statistics_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_FRAME_STATISTICS_H */
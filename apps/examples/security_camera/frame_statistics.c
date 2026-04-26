/****************************************************************************
 * security_camera/frame_statistics.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "frame_statistics.h"
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <syslog.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/**
 * @brief 現在時刻をマイクロ秒で取得
 */
static uint64_t get_current_time_us(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * @brief 配列の平均値を計算
 */
static uint32_t calculate_average(const uint32_t *array, uint32_t count)
{
  if (count == 0) return 0;

  uint64_t sum = 0;
  for (uint32_t i = 0; i < count; i++)
    {
      sum += array[i];
    }

  return (uint32_t)(sum / count);
}

/**
 * @brief 配列の分散を計算（固定小数点）
 */
static uint64_t calculate_variance(const uint32_t *array, uint32_t count, uint32_t average)
{
  if (count <= 1) return 0;

  uint64_t sum_squared_diff = 0;
  for (uint32_t i = 0; i < count; i++)
    {
      int64_t diff = (int64_t)array[i] - (int64_t)average;
      sum_squared_diff += (uint64_t)(diff * diff);
    }

  return sum_squared_diff / (count - 1);
}

/**
 * @brief 配列の最小値と最大値を取得
 */
static void find_min_max(const uint32_t *array, uint32_t count,
                        uint32_t *min_val, uint32_t *max_val)
{
  if (count == 0)
    {
      *min_val = *max_val = 0;
      return;
    }

  *min_val = *max_val = array[0];
  for (uint32_t i = 1; i < count; i++)
    {
      if (array[i] < *min_val) *min_val = array[i];
      if (array[i] > *max_val) *max_val = array[i];
    }
}

/**
 * @brief 線形回帰のトレンド係数を計算
 */
static float calculate_trend_coefficient(const uint32_t *array, uint32_t count)
{
  if (count < 2) return 0.0f;

  float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_xx = 0.0f;

  for (uint32_t i = 0; i < count; i++)
    {
      float x = (float)i;
      float y = (float)array[i];
      sum_x += x;
      sum_y += y;
      sum_xy += x * y;
      sum_xx += x * x;
    }

  float n = (float)count;
  float denominator = n * sum_xx - sum_x * sum_x;

  if (fabsf(denominator) < 1e-6f) return 0.0f;

  return (n * sum_xy - sum_x * sum_y) / denominator;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/**
 * @brief フレーム統計システムを初期化
 */
int frame_statistics_init(frame_statistics_t *stats)
{
  if (!stats) return -1;

  memset(stats, 0, sizeof(frame_statistics_t));

  stats->window_size = FRAME_STATISTICS_WINDOW_SIZE;
  stats->initialized = true;
  stats->first_frame_us = get_current_time_us();
  stats->last_update_us = stats->first_frame_us;

  /* 初期値設定 */
  stats->min_size_bytes = MAX_FRAME_SIZE_BYTES;
  stats->max_size_bytes = MIN_FRAME_SIZE_BYTES;

  return 0;
}

/**
 * @brief フレームサイズで統計を更新
 */
int frame_statistics_update(frame_statistics_t *stats, uint32_t frame_size, uint64_t timestamp_us)
{
  if (!stats || !stats->initialized) return -1;

  /* サイズ範囲チェック */
  if (frame_size < MIN_FRAME_SIZE_BYTES || frame_size > MAX_FRAME_SIZE_BYTES)
    {
      syslog(LOG_WARNING, "Frame size %u out of range [%d-%d]",
             frame_size, MIN_FRAME_SIZE_BYTES, MAX_FRAME_SIZE_BYTES);
      return -1;
    }

  /* タイムスタンプ更新 */
  if (timestamp_us == 0)
    {
      timestamp_us = get_current_time_us();
    }
  stats->last_update_us = timestamp_us;

  /* 循環バッファに追加 */
  stats->size_history[stats->history_index] = frame_size;
  stats->history_index = (stats->history_index + 1) % stats->window_size;

  /* フレームカウント更新 */
  stats->frame_count++;

  /* 窓が満杯になったかチェック */
  if (stats->frame_count >= stats->window_size && !stats->window_full)
    {
      stats->window_full = true;
    }

  /* 統計を再計算 */
  uint32_t active_count = stats->window_full ? stats->window_size : stats->frame_count;

  /* 平均値計算 */
  stats->avg_size_bytes = calculate_average(stats->size_history, active_count);

  /* 最小値・最大値計算 */
  find_min_max(stats->size_history, active_count,
               &stats->min_size_bytes, &stats->max_size_bytes);

  /* 分散計算 */
  stats->size_variance = calculate_variance(stats->size_history, active_count,
                                           stats->avg_size_bytes);

  /* 総サイズ計算 */
  stats->total_size_bytes = 0;
  for (uint32_t i = 0; i < active_count; i++)
    {
      stats->total_size_bytes += stats->size_history[i];
    }

  /* 複雑度指数計算 */
  stats->complexity_index = frame_statistics_calculate_complexity(stats);

  /* 平滑化された複雑度指数更新 */
  if (stats->frame_count == 1)
    {
      stats->smoothed_complexity = stats->complexity_index;
    }
  else
    {
      stats->smoothed_complexity = COMPLEXITY_SMOOTHING_FACTOR * stats->smoothed_complexity +
                                  (1.0f - COMPLEXITY_SMOOTHING_FACTOR) * stats->complexity_index;
    }

  return 0;
}

/**
 * @brief 複雑度指数を計算
 */
float frame_statistics_calculate_complexity(frame_statistics_t *stats)
{
  if (!stats || !stats->initialized || stats->frame_count == 0) return 0.0f;

  /* 分散ベースの複雑度指数 */
  if (stats->avg_size_bytes == 0) return 0.0f;

  float normalized_variance = sqrtf((float)stats->size_variance) / (float)stats->avg_size_bytes;

  /* 0.0-1.0+の範囲にクランプ */
  if (normalized_variance < 0.0f) normalized_variance = 0.0f;
  if (normalized_variance > 2.0f) normalized_variance = 2.0f;  /* 理論的最大値 */

  return normalized_variance;
}

/**
 * @brief フレームサイズトレンドを分析
 */
int frame_statistics_analyze_trend(frame_statistics_t *stats, frame_trend_t *trend)
{
  if (!stats || !trend || !stats->initialized) return -1;

  memset(trend, 0, sizeof(frame_trend_t));

  /* 最小限のデータ点が必要 */
  uint32_t active_count = stats->window_full ? stats->window_size : stats->frame_count;
  if (active_count < 3)
    {
      trend->trend_valid = false;
      return 0;
    }

  /* トレンド係数計算 */
  trend->trend_coefficient = calculate_trend_coefficient(stats->size_history, active_count);

  /* 相関係数計算（簡易版） */
  float avg_size = (float)stats->avg_size_bytes;
  float avg_x = (float)(active_count - 1) / 2.0f;

  float numerator = 0.0f, denom_x = 0.0f, denom_y = 0.0f;
  for (uint32_t i = 0; i < active_count; i++)
    {
      float x_diff = (float)i - avg_x;
      float y_diff = (float)stats->size_history[i] - avg_size;
      numerator += x_diff * y_diff;
      denom_x += x_diff * x_diff;
      denom_y += y_diff * y_diff;
    }

  float denominator = sqrtf(denom_x * denom_y);
  trend->correlation = (denominator > 1e-6f) ? numerator / denominator : 0.0f;

  /* トレンド方向判定 */
  trend->trend_increasing = (trend->trend_coefficient > 0.0f);

  /* トレンド有効性判定 */
  trend->trend_valid = (fabsf(trend->correlation) > 0.3f);  /* 最小相関閾値 */

  return 0;
}

/**
 * @brief 平均フレームサイズを取得（KB単位）
 */
float frame_statistics_get_avg_size_kb(frame_statistics_t *stats)
{
  if (!stats || !stats->initialized) return 0.0f;

  return (float)stats->avg_size_bytes / 1024.0f;
}

/**
 * @brief 正規化されたサイズ分散を取得
 */
float frame_statistics_get_normalized_variance(frame_statistics_t *stats)
{
  if (!stats || !stats->initialized || stats->avg_size_bytes == 0) return 0.0f;

  float variance = (float)stats->size_variance;
  float avg_squared = (float)stats->avg_size_bytes * (float)stats->avg_size_bytes;

  /* 変動係数として正規化（0.0-1.0範囲） */
  float normalized = sqrtf(variance) / (float)stats->avg_size_bytes;

  /* 最大1.0にクランプ */
  return (normalized > 1.0f) ? 1.0f : normalized;
}

/**
 * @brief 統計をリセット
 */
void frame_statistics_reset(frame_statistics_t *stats)
{
  if (!stats) return;

  uint64_t current_time = get_current_time_us();

  memset(stats->size_history, 0, sizeof(stats->size_history));
  stats->frame_count = 0;
  stats->history_index = 0;
  stats->window_full = false;
  stats->total_size_bytes = 0;
  stats->avg_size_bytes = 0;
  stats->min_size_bytes = MAX_FRAME_SIZE_BYTES;
  stats->max_size_bytes = MIN_FRAME_SIZE_BYTES;
  stats->size_variance = 0;
  stats->complexity_index = 0.0f;
  stats->smoothed_complexity = 0.0f;
  stats->first_frame_us = current_time;
  stats->last_update_us = current_time;
}

/**
 * @brief デバッグ情報を出力
 */
void frame_statistics_debug_print(frame_statistics_t *stats)
{
  if (!stats || !stats->initialized)
    {
      syslog(LOG_INFO, "Frame Statistics: Not initialized");
      return;
    }

  syslog(LOG_INFO, "=== Frame Statistics Debug ===");
  syslog(LOG_INFO, "Frame Count: %u (window: %s)",
         stats->frame_count, stats->window_full ? "full" : "partial");
  syslog(LOG_INFO, "Avg Size: %u bytes (%.2f KB)",
         stats->avg_size_bytes, frame_statistics_get_avg_size_kb(stats));
  syslog(LOG_INFO, "Size Range: %u - %u bytes",
         stats->min_size_bytes, stats->max_size_bytes);
  syslog(LOG_INFO, "Variance: %llu, Complexity: %.3f (smoothed: %.3f)",
         (unsigned long long)stats->size_variance,
         stats->complexity_index, stats->smoothed_complexity);
  syslog(LOG_INFO, "Total Size: %u bytes, Window: %u",
         stats->total_size_bytes, stats->window_size);

  /* トレンド分析 */
  frame_trend_t trend;
  if (frame_statistics_analyze_trend(stats, &trend) == 0 && trend.trend_valid)
    {
      syslog(LOG_INFO, "Trend: %s (coeff: %.3f, corr: %.3f)",
             trend.trend_increasing ? "increasing" : "decreasing",
             trend.trend_coefficient, trend.correlation);
    }
  else
    {
      syslog(LOG_INFO, "Trend: insufficient data or not significant");
    }

  uint64_t elapsed_us = stats->last_update_us - stats->first_frame_us;
  float elapsed_seconds = (float)elapsed_us / 1000000.0f;
  syslog(LOG_INFO, "Runtime: %.1f seconds", elapsed_seconds);
}
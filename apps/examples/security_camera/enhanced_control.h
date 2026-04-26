/****************************************************************************
 * security_camera/enhanced_control.h
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

#ifndef __SECURITY_CAMERA_ENHANCED_CONTROL_H
#define __SECURITY_CAMERA_ENHANCED_CONTROL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "frame_statistics.h"
#include "fps_controller.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Phase 11 Enhanced Control Configuration */
#define ENHANCED_CONTROL_ENABLED            true
#define ENHANCED_CONTROL_VERSION            "11.1"

/* 多変数制御パラメータ */
#define MULTI_VARIABLE_INPUT_COUNT          6       /* 入力変数数 */
#define CONTROL_WEIGHT_QUEUE                0.6f    /* キュー深度重み */
#define CONTROL_WEIGHT_SIZE                 0.2f    /* フレームサイズ重み */
#define CONTROL_WEIGHT_TRANSMISSION         0.1f    /* 伝送時間重み */
#define CONTROL_WEIGHT_PREDICTION           0.1f    /* 予測重み */

/* 制御システム制限 */
#define ENHANCED_CONTROL_CYCLE_MS           100     /* 制御周期 */
#define ENHANCED_CONTROL_TIMEOUT_MS         500     /* 制御タイムアウト */
#define MAX_CONTROL_CYCLES_WITHOUT_UPDATE   5       /* 更新なし最大サイクル数 */

/* 正規化パラメータ */
#define NORMALIZE_QUEUE_DEPTH_MAX           10.0f   /* キュー深度最大値 */
#define NORMALIZE_FRAME_SIZE_MAX_KB         128.0f  /* フレームサイズ最大値(KB) */
#define NORMALIZE_TRANSMISSION_MAX_MS       200.0f  /* 伝送時間最大値(ms) */
#define NORMALIZE_COMPLEXITY_MAX            2.0f    /* 複雑度最大値 */

/* 安定性チェックパラメータ */
#define STABILITY_VARIANCE_THRESHOLD        0.5f    /* 安定性分散閾値 */
#define STABILITY_HISTORY_SIZE              5       /* 安定性履歴サイズ */
#define FALLBACK_TRIGGER_COUNT              3       /* フォールバック発動回数 */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* 多変数制御入力構造体 */
typedef struct multi_variable_input_s
{
  /* 正規化された制御入力 (0.0-1.0範囲) */
  float queue_depth;                    /* 正規化キュー深度 */
  float avg_frame_size_kb;             /* 正規化平均フレームサイズ */
  float frame_size_variance;           /* 正規化サイズ分散 */
  float transmission_time_ms;          /* 正規化伝送時間 */
  float predicted_queue_load;          /* 予測キュー負荷 */
  float complexity_index;              /* シーン複雑度指数 */

  /* 入力メタ情報 */
  uint64_t timestamp_us;               /* 入力タイムスタンプ */
  bool valid;                          /* 入力妥当性フラグ */
  uint32_t input_source_mask;          /* 入力ソースマスク */

  /* デバッグ情報 */
  float raw_queue_depth;               /* 生キュー深度値 */
  float raw_frame_size_bytes;          /* 生フレームサイズ値 */
  float raw_transmission_time;         /* 生伝送時間値 */

} multi_variable_input_t;

/* 適応PIDコントローラ構造体 */
typedef struct adaptive_pid_controller_s
{
  /* 基本PID係数 */
  float base_kp, base_ki, base_kd;

  /* 適応PID係数 */
  float current_kp, current_ki, current_kd;

  /* 適応制御パラメータ */
  float complexity_threshold;          /* 複雑度閾値 */
  float adaptation_factor;             /* 適応係数 */
  bool adaptation_enabled;             /* 適応制御有効フラグ */

  /* PID制御状態 */
  float setpoint;                      /* 目標値 */
  float integral;                      /* 積分項 */
  float previous_error;                /* 前回誤差 */
  float output_min, output_max;        /* 出力範囲 */
  uint64_t last_update_us;            /* 最終更新時刻 */

  /* 統計・デバッグ情報 */
  uint32_t adaptation_count;           /* 適応実行回数 */
  float avg_complexity;                /* 平均複雑度 */
  float last_output;                   /* 前回出力値 */
  float output_history[STABILITY_HISTORY_SIZE]; /* 出力履歴（安定性チェック用） */
  uint8_t output_history_index;        /* 出力履歴インデックス */

} adaptive_pid_controller_t;

/* 予測制御構造体 */
typedef struct predictive_controller_s
{
  /* 予測パラメータ */
  uint32_t prediction_window;          /* 予測窓サイズ */
  float size_history[5];               /* サイズ履歴（予測用） */
  uint8_t history_index;               /* 履歴インデックス */

  /* トレンド分析 */
  float trend_coefficient;             /* トレンド係数 */
  float correlation;                   /* 相関係数 */
  bool trend_increasing;               /* 増加トレンドフラグ */
  bool trend_valid;                    /* トレンド有効性フラグ */

  /* 予測精度 */
  uint32_t prediction_count;           /* 予測実行回数 */
  uint32_t correct_predictions;        /* 正確な予測回数 */
  float prediction_accuracy;           /* 予測精度 (0.0-1.0) */
  float prediction_threshold;          /* 予測精度閾値 */

  /* 制御状態 */
  bool prediction_enabled;             /* 予測制御有効フラグ */
  uint64_t last_prediction_us;         /* 最終予測時刻 */

} predictive_controller_t;

/* 動的バッファ管理構造体 */
typedef struct buffer_manager_s
{
  /* バッファ設定 */
  uint32_t base_buffer_size;           /* 基本バッファサイズ */
  uint32_t current_buffer_size;        /* 現在バッファサイズ */
  uint32_t min_buffer_size;            /* 最小バッファサイズ */
  uint32_t max_buffer_size;            /* 最大バッファサイズ */
  float size_multiplier;               /* サイズ乗数 */

  /* 制御統計 */
  uint32_t resize_count;               /* リサイズ実行回数 */
  uint32_t resize_success_count;       /* リサイズ成功回数 */
  bool auto_resize_enabled;            /* 自動リサイズ有効フラグ */
  uint64_t last_resize_us;             /* 最終リサイズ時刻 */

  /* パフォーマンス統計 */
  float buffer_utilization;            /* バッファ使用率 */
  float avg_utilization;               /* 平均使用率 */
  uint32_t overflow_count;             /* オーバーフロー回数 */

} buffer_manager_t;

/* 総合制御システム構造体 */
typedef struct enhanced_control_system_s
{
  /* コンポーネント */
  frame_statistics_t stats;            /* フレーム統計 */
  adaptive_pid_controller_t pid;       /* 適応PID制御 */
  predictive_controller_t predictor;   /* 予測制御 */
  buffer_manager_t buffer_mgr;         /* バッファ管理 */
  multi_variable_input_t input;        /* 多変数入力 */

  /* システム状態 */
  bool system_enabled;                 /* システム有効フラグ */
  bool fallback_mode;                  /* フォールバックモード */
  uint32_t control_cycles;             /* 制御サイクル回数 */
  uint32_t fallback_count;             /* フォールバック回数 */

  /* パフォーマンス統計 */
  uint64_t total_control_time_us;      /* 総制御時間 */
  uint64_t last_cycle_time_us;         /* 前回サイクル時間 */
  float avg_cycle_time_ms;             /* 平均サイクル時間 */
  uint32_t control_failures;          /* 制御失敗回数 */

  /* 統合制御出力 */
  float fps_output;                    /* FPS出力値 */
  uint32_t buffer_size_output;         /* バッファサイズ出力値 */
  bool outputs_valid;                  /* 出力妥当性フラグ */

} enhanced_control_system_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* === 多変数入力管理 === */

/**
 * @brief 多変数入力を初期化
 * @param input 入力構造体
 * @return 0: success, <0: error
 */
int multi_variable_input_init(multi_variable_input_t *input);

/**
 * @brief 制御入力を構築
 * @param input 入力構造体
 * @param stats フレーム統計
 * @param queue_depth 現在のキュー深度
 * @param transmission_time 伝送時間(ms)
 * @return 0: success, <0: error
 */
int build_control_input(multi_variable_input_t *input,
                       frame_statistics_t *stats,
                       float queue_depth,
                       float transmission_time);

/**
 * @brief 重み付き制御入力を計算
 * @param input 多変数入力
 * @return 重み付き制御入力値
 */
float weighted_control_input(multi_variable_input_t *input);

/**
 * @brief 入力値を正規化
 * @param input 入力構造体
 * @return 0: success, <0: error
 */
int normalize_control_inputs(multi_variable_input_t *input);

/* === 適応PID制御 === */

/**
 * @brief 適応PIDコントローラを初期化
 * @param controller コントローラ構造体
 * @param base_kp 基本Kpゲイン
 * @param base_ki 基本Kiゲイン
 * @param base_kd 基本Kdゲイン
 * @return 0: success, <0: error
 */
int adaptive_pid_init(adaptive_pid_controller_t *controller,
                      float base_kp, float base_ki, float base_kd);

/**
 * @brief 複雑度に応じてゲインを適応調整
 * @param controller コントローラ構造体
 * @param complexity_index 複雑度指数
 * @return 0: success, <0: error
 */
int adaptive_pid_adapt_gains(adaptive_pid_controller_t *controller,
                             float complexity_index);

/**
 * @brief 適応PID制御計算
 * @param controller コントローラ構造体
 * @param control_error 制御誤差
 * @return 制御出力値
 */
float adaptive_pid_update(adaptive_pid_controller_t *controller,
                         float control_error);

/**
 * @brief ゲインを基本値にリセット
 * @param controller コントローラ構造体
 */
void adaptive_pid_reset_to_base_gains(adaptive_pid_controller_t *controller);

/* === 予測制御 === */

/**
 * @brief 予測コントローラを初期化
 * @param predictor 予測制御構造体
 * @return 0: success, <0: error
 */
int predictive_controller_init(predictive_controller_t *predictor);

/**
 * @brief 予測制御を更新
 * @param predictor 予測制御構造体
 * @param input 多変数入力
 * @return 予測キュー負荷
 */
float predictive_control_update(predictive_controller_t *predictor,
                               multi_variable_input_t *input);

/**
 * @brief 予測精度を更新
 * @param predictor 予測制御構造体
 * @param actual_size 実際のフレームサイズ
 * @return 0: success, <0: error
 */
int update_prediction_accuracy(predictive_controller_t *predictor,
                              float actual_size);

/* === バッファ管理 === */

/**
 * @brief バッファマネージャを初期化
 * @param manager バッファマネージャ構造体
 * @param base_size 基本バッファサイズ
 * @return 0: success, <0: error
 */
int buffer_manager_init(buffer_manager_t *manager, uint32_t base_size);

/**
 * @brief 最適バッファサイズを計算
 * @param manager バッファマネージャ構造体
 * @param stats フレーム統計
 * @return 最適バッファサイズ
 */
uint32_t buffer_manager_calculate_size(buffer_manager_t *manager,
                                      frame_statistics_t *stats);

/**
 * @brief バッファサイズを調整
 * @param manager バッファマネージャ構造体
 * @param new_size 新しいバッファサイズ
 * @return 0: success, <0: error
 */
int buffer_manager_resize(buffer_manager_t *manager, uint32_t new_size);

/* === 総合制御システム === */

/**
 * @brief 拡張制御システムを初期化
 * @param system 制御システム構造体
 * @return 0: success, <0: error
 */
int enhanced_control_system_init(enhanced_control_system_t *system);

/**
 * @brief 拡張制御サイクルを実行
 * @param system 制御システム構造体
 * @param frame_size 現在のフレームサイズ
 * @param queue_depth 現在のキュー深度
 * @param transmission_time 伝送時間
 * @return 0: success, <0: error
 */
int execute_enhanced_control_cycle(enhanced_control_system_t *system,
                                  uint32_t frame_size,
                                  float queue_depth,
                                  float transmission_time);

/**
 * @brief システム安定性チェック
 * @param system 制御システム構造体
 * @return true: 安定, false: 不安定
 */
bool system_stability_check(enhanced_control_system_t *system);

/**
 * @brief Phase 10制御にフォールバック
 * @param system 制御システム構造体
 * @return 0: success, <0: error
 */
int trigger_fallback_to_phase10(enhanced_control_system_t *system);

/**
 * @brief 制御システム統計をリセット
 * @param system 制御システム構造体
 */
void enhanced_control_system_reset(enhanced_control_system_t *system);

/**
 * @brief 制御システムデバッグ出力
 * @param system 制御システム構造体
 */
void enhanced_control_debug_print(enhanced_control_system_t *system);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_ENHANCED_CONTROL_H */
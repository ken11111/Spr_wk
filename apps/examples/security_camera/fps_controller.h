/****************************************************************************
 * security_camera/fps_controller.h
 *
 *   Copyright 2025 Security Camera Project - Phase 10
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

#ifndef __SECURITY_CAMERA_FPS_CONTROLLER_H
#define __SECURITY_CAMERA_FPS_CONTROLLER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* PID Controller Configuration (Phase 10) */
#define FPS_CONTROLLER_KP             0.15f    /* Proportional gain */
#define FPS_CONTROLLER_KI             0.02f    /* Integral gain */
#define FPS_CONTROLLER_KD             0.0f     /* Derivative gain (disabled) */

/* Control Parameters */
#define FPS_CONTROLLER_SETPOINT       3.5f     /* Target queue depth */
#define FPS_CONTROLLER_OUTPUT_MIN     5.0f     /* Minimum FPS */
#define FPS_CONTROLLER_OUTPUT_MAX     30.0f    /* Maximum FPS */
#define FPS_CONTROLLER_INTEGRAL_LIMIT 5.0f     /* Integral clamp limit */

/* Control Loop Timing */
#define FPS_CONTROLLER_PERIOD_MS      100      /* 100ms = 10Hz control rate */
#define FPS_CONTROLLER_PERIOD_US      100000   /* 100ms in microseconds */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* FPS Controller state structure */
typedef struct fps_controller_s
{
  /* PID Parameters */
  float kp;                    /* Proportional gain */
  float ki;                    /* Integral gain */
  float kd;                    /* Derivative gain (unused) */

  /* Control Variables */
  float setpoint;              /* Target queue depth */
  float output_min;            /* Minimum FPS output */
  float output_max;            /* Maximum FPS output */

  /* PID State */
  float integral;              /* Accumulated integral term */
  float previous_error;        /* Previous error for derivative */
  uint64_t last_update_us;     /* Last control update timestamp */

  /* System State */
  float current_output;        /* Current FPS output */
  bool enabled;                /* Controller enable flag */

  /* Statistics */
  uint32_t update_count;       /* Number of control updates */
  float avg_error;             /* Running average error */
  float max_error;             /* Maximum error recorded */

} fps_controller_t;

/* Control metrics for monitoring */
typedef struct control_metrics_s
{
  float queue_depth;           /* Primary process variable */
  float error;                 /* Setpoint error */
  float control_output;        /* PID output (FPS) */
  float proportional_term;     /* P contribution */
  float integral_term;         /* I contribution */
  uint64_t timestamp_us;       /* Sample timestamp */
} control_metrics_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize FPS PID controller
 * @param controller Controller instance
 * @param setpoint Target queue depth
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 * @return 0: success, <0: error
 */
int fps_controller_init(fps_controller_t *controller, float setpoint,
                       float kp, float ki, float kd);

/**
 * @brief Update PID controller with new measurement
 * @param controller Controller instance
 * @param queue_depth Current queue depth measurement
 * @return New FPS output, <0: error
 */
float fps_controller_update(fps_controller_t *controller, float queue_depth);

/**
 * @brief Get current controller output
 * @param controller Controller instance
 * @return Current FPS output, <0: error
 */
float fps_controller_get_output(fps_controller_t *controller);

/**
 * @brief Enable/disable controller
 * @param controller Controller instance
 * @param enabled Enable flag
 */
void fps_controller_enable(fps_controller_t *controller, bool enabled);

/**
 * @brief Reset controller state
 * @param controller Controller instance
 */
void fps_controller_reset(fps_controller_t *controller);

/**
 * @brief Get controller metrics for monitoring
 * @param controller Controller instance
 * @param metrics Output metrics structure
 */
void fps_controller_get_metrics(fps_controller_t *controller,
                               control_metrics_t *metrics);

/**
 * @brief Check if controller output is stable (for fallback detection)
 * @param controller Controller instance
 * @return true if stable, false if oscillating
 */
bool fps_controller_is_stable(fps_controller_t *controller);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_FPS_CONTROLLER_H */
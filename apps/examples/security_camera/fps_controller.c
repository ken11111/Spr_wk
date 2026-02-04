/****************************************************************************
 * security_camera/fps_controller.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <syslog.h>

#include "fps_controller.h"
#include "config.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Stability check parameters */
#define STABILITY_WINDOW_SIZE    10     /* Number of samples for stability check */
#define STABILITY_VARIANCE_LIMIT 0.5f   /* Maximum variance for stable operation */

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_timestamp_us
 *
 * Description:
 *   Get current timestamp in microseconds
 *
 ****************************************************************************/

static uint64_t get_timestamp_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/****************************************************************************
 * Name: clamp_float
 *
 * Description:
 *   Clamp float value to specified range
 *
 ****************************************************************************/

static float clamp_float(float value, float min_val, float max_val)
{
  if (value < min_val) return min_val;
  if (value > max_val) return max_val;
  return value;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: fps_controller_init
 *
 * Description:
 *   Initialize FPS PID controller
 *
 ****************************************************************************/

int fps_controller_init(fps_controller_t *controller, float setpoint,
                       float kp, float ki, float kd)
{
  if (controller == NULL)
    {
      LOG_ERROR("Invalid controller pointer");
      return -EINVAL;
    }

  /* Clear controller structure */
  memset(controller, 0, sizeof(fps_controller_t));

  /* Set PID parameters */
  controller->kp = kp;
  controller->ki = ki;
  controller->kd = kd;

  /* Set control parameters */
  controller->setpoint = setpoint;
  controller->output_min = FPS_CONTROLLER_OUTPUT_MIN;
  controller->output_max = FPS_CONTROLLER_OUTPUT_MAX;

  /* Initialize state */
  controller->integral = 0.0f;
  controller->previous_error = 0.0f;
  controller->last_update_us = get_timestamp_us();
  controller->current_output = 7.0f; /* Start at target FPS */
  controller->enabled = false; /* Start disabled */

  /* Initialize statistics */
  controller->update_count = 0;
  controller->avg_error = 0.0f;
  controller->max_error = 0.0f;

  LOG_INFO("FPS Controller initialized: Kp=%.3f Ki=%.3f Setpoint=%.1f",
           kp, ki, setpoint);

  return 0;
}

/****************************************************************************
 * Name: fps_controller_update
 *
 * Description:
 *   Update PID controller with new measurement
 *
 ****************************************************************************/

float fps_controller_update(fps_controller_t *controller, float queue_depth)
{
  uint64_t current_time_us;
  float dt_seconds;
  float error;
  float proportional;
  float derivative;
  float output;

  if (controller == NULL)
    {
      LOG_ERROR("Invalid controller pointer");
      return -1.0f;
    }

  if (!controller->enabled)
    {
      return controller->current_output;
    }

  /* Calculate time delta */
  current_time_us = get_timestamp_us();
  dt_seconds = (current_time_us - controller->last_update_us) / 1000000.0f;

  /* Ensure reasonable time delta (10ms - 1s) */
  if (dt_seconds < 0.01f || dt_seconds > 1.0f)
    {
      LOG_WARN("Unusual control period: %.3f seconds", dt_seconds);
      dt_seconds = FPS_CONTROLLER_PERIOD_US / 1000000.0f; /* Use nominal period */
    }

  /* Calculate error (setpoint - measurement) */
  error = controller->setpoint - queue_depth;

  /* Proportional term */
  proportional = controller->kp * error;

  /* Integral term with clamping */
  controller->integral += error * dt_seconds;
  controller->integral = clamp_float(controller->integral,
                                   -FPS_CONTROLLER_INTEGRAL_LIMIT,
                                    FPS_CONTROLLER_INTEGRAL_LIMIT);

  /* Derivative term (currently disabled: kd = 0.0) */
  derivative = controller->kd * (error - controller->previous_error) / dt_seconds;

  /* PID output */
  output = proportional + (controller->ki * controller->integral) + derivative;

  /* Add to base FPS (7.0) and clamp to valid range */
  output = 7.0f + output;
  output = clamp_float(output, controller->output_min, controller->output_max);

  /* Update controller state */
  controller->current_output = output;
  controller->previous_error = error;
  controller->last_update_us = current_time_us;
  controller->update_count++;

  /* Update statistics */
  float abs_error = (error < 0) ? -error : error;
  controller->avg_error = ((controller->avg_error * (controller->update_count - 1)) + abs_error) / controller->update_count;
  if (abs_error > controller->max_error)
    {
      controller->max_error = abs_error;
    }

  LOG_DEBUG("PID Update: depth=%.1f error=%.2f output=%.1f (P=%.2f I=%.2f)",
           queue_depth, error, output, proportional, controller->ki * controller->integral);

  return output;
}

/****************************************************************************
 * Name: fps_controller_get_output
 *
 * Description:
 *   Get current controller output
 *
 ****************************************************************************/

float fps_controller_get_output(fps_controller_t *controller)
{
  if (controller == NULL)
    {
      return -1.0f;
    }

  return controller->current_output;
}

/****************************************************************************
 * Name: fps_controller_enable
 *
 * Description:
 *   Enable/disable controller
 *
 ****************************************************************************/

void fps_controller_enable(fps_controller_t *controller, bool enabled)
{
  if (controller == NULL)
    {
      return;
    }

  if (enabled && !controller->enabled)
    {
      /* Reset state when enabling */
      fps_controller_reset(controller);
      LOG_INFO("FPS Controller enabled");
    }
  else if (!enabled && controller->enabled)
    {
      LOG_INFO("FPS Controller disabled");
    }

  controller->enabled = enabled;
}

/****************************************************************************
 * Name: fps_controller_reset
 *
 * Description:
 *   Reset controller state
 *
 ****************************************************************************/

void fps_controller_reset(fps_controller_t *controller)
{
  if (controller == NULL)
    {
      return;
    }

  controller->integral = 0.0f;
  controller->previous_error = 0.0f;
  controller->last_update_us = get_timestamp_us();
  controller->current_output = 7.0f; /* Reset to target FPS */

  /* Don't reset statistics - keep for monitoring */

  LOG_INFO("FPS Controller state reset");
}

/****************************************************************************
 * Name: fps_controller_get_metrics
 *
 * Description:
 *   Get controller metrics for monitoring
 *
 ****************************************************************************/

void fps_controller_get_metrics(fps_controller_t *controller,
                               control_metrics_t *metrics)
{
  if (controller == NULL || metrics == NULL)
    {
      return;
    }

  /* Calculate current error based on last update */
  float current_error = controller->previous_error; /* Last calculated error */

  metrics->queue_depth = controller->setpoint - current_error; /* Reconstruct measurement */
  metrics->error = current_error;
  metrics->control_output = controller->current_output;
  metrics->proportional_term = controller->kp * current_error;
  metrics->integral_term = controller->ki * controller->integral;
  metrics->timestamp_us = controller->last_update_us;
}

/****************************************************************************
 * Name: fps_controller_is_stable
 *
 * Description:
 *   Check if controller output is stable (for fallback detection)
 *
 ****************************************************************************/

bool fps_controller_is_stable(fps_controller_t *controller)
{
  if (controller == NULL)
    {
      return false;
    }

  /* Simple stability check: average error should be reasonable */
  if (controller->update_count < STABILITY_WINDOW_SIZE)
    {
      return true; /* Not enough data yet */
    }

  /* Check if average error is within acceptable range */
  if (controller->avg_error > STABILITY_VARIANCE_LIMIT)
    {
      LOG_WARN("Controller unstable: avg_error=%.3f max_error=%.3f",
               controller->avg_error, controller->max_error);
      return false;
    }

  return true;
}
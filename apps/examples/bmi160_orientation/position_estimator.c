/****************************************************************************
 * bmi160_orientation/position_estimator.c
 *
 * Position estimation from accelerometer data
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <math.h>
#include <MadgwickAHRS.h>
#include "position_estimator.h"
#include "orientation_calc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define GRAVITY           9.80665f      /* m/s^2 */
#define CALIBRATION_COUNT 100           /* Number of samples for calibration */
#define VELOCITY_THRESHOLD 0.01f        /* m/s - Ignore small velocities */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: position_estimator_init
 ****************************************************************************/

void position_estimator_init(struct position_state_s *state)
{
  memset(state, 0, sizeof(struct position_state_s));
  state->calibrated = 0;
  state->calib_samples = 0;
}

/****************************************************************************
 * Name: position_estimator_reset
 ****************************************************************************/

void position_estimator_reset(struct position_state_s *state)
{
  state->x = 0.0f;
  state->y = 0.0f;
  state->z = 0.0f;
  state->vx = 0.0f;
  state->vy = 0.0f;
  state->vz = 0.0f;
}

/****************************************************************************
 * Name: position_estimator_update
 ****************************************************************************/

void position_estimator_update(struct position_state_s *state,
                               struct ahrs_out_s *ahrs,
                               float ax, float ay, float az,
                               float dt)
{
  float dcm[3][3];
  float ax_world, ay_world, az_world;

  /* Calibration phase: collect bias samples while stationary */
  if (!state->calibrated)
    {
      state->bias_x += ax;
      state->bias_y += ay;
      state->bias_z += az;
      state->calib_samples++;

      if (state->calib_samples >= CALIBRATION_COUNT)
        {
          state->bias_x /= CALIBRATION_COUNT;
          state->bias_y /= CALIBRATION_COUNT;
          state->bias_z /= CALIBRATION_COUNT;
          state->calibrated = 1;
        }
      return;
    }

  /* Remove bias */
  ax -= state->bias_x;
  ay -= state->bias_y;
  az -= state->bias_z;

  /* Transform acceleration from sensor frame to world frame using DCM */
  orientation_quaternion_to_dcm(ahrs->q, dcm);

  ax_world = dcm[0][0] * ax + dcm[0][1] * ay + dcm[0][2] * az;
  ay_world = dcm[1][0] * ax + dcm[1][1] * ay + dcm[1][2] * az;
  az_world = dcm[2][0] * ax + dcm[2][1] * ay + dcm[2][2] * az;

  /* Remove gravity (assuming Z-axis is up in world frame) */
  az_world -= GRAVITY;

  /* Integrate acceleration to get velocity */
  state->vx += ax_world * dt;
  state->vy += ay_world * dt;
  state->vz += az_world * dt;

  /* Apply velocity threshold to reduce drift */
  if (fabsf(state->vx) < VELOCITY_THRESHOLD) state->vx = 0.0f;
  if (fabsf(state->vy) < VELOCITY_THRESHOLD) state->vy = 0.0f;
  if (fabsf(state->vz) < VELOCITY_THRESHOLD) state->vz = 0.0f;

  /* Integrate velocity to get position */
  state->x += state->vx * dt;
  state->y += state->vy * dt;
  state->z += state->vz * dt;
}

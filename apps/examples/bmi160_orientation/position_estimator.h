/****************************************************************************
 * bmi160_orientation/position_estimator.h
 *
 * Position estimation from accelerometer data
 *
 ****************************************************************************/

#ifndef __POSITION_ESTIMATOR_H
#define __POSITION_ESTIMATOR_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <MadgwickAHRS.h>

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct position_state_s
{
  /* Position (m) */
  float x;
  float y;
  float z;

  /* Velocity (m/s) */
  float vx;
  float vy;
  float vz;

  /* Bias correction for accelerometer */
  float bias_x;
  float bias_y;
  float bias_z;

  /* Calibration flag */
  int calibrated;
  int calib_samples;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: position_estimator_init
 *
 * Description:
 *   Initialize position estimator state
 *
 ****************************************************************************/

void position_estimator_init(struct position_state_s *state);

/****************************************************************************
 * Name: position_estimator_update
 *
 * Description:
 *   Update position estimation using accelerometer data
 *
 * Input Parameters:
 *   state - Position estimator state
 *   ahrs  - AHRS output (for coordinate transformation)
 *   ax    - Accelerometer X (sensor frame, m/s^2)
 *   ay    - Accelerometer Y (sensor frame, m/s^2)
 *   az    - Accelerometer Z (sensor frame, m/s^2)
 *   dt    - Time delta (seconds)
 *
 ****************************************************************************/

void position_estimator_update(struct position_state_s *state,
                               struct ahrs_out_s *ahrs,
                               float ax, float ay, float az,
                               float dt);

/****************************************************************************
 * Name: position_estimator_reset
 *
 * Description:
 *   Reset position and velocity to zero
 *
 ****************************************************************************/

void position_estimator_reset(struct position_state_s *state);

#endif /* __POSITION_ESTIMATOR_H */

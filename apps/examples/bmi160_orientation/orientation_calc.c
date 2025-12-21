/****************************************************************************
 * bmi160_orientation/orientation_calc.c
 *
 * Orientation calculation utilities
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <math.h>
#include <MadgwickAHRS.h>
#include "orientation_calc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RAD_TO_DEG  (180.0f / M_PI)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: orientation_get_euler
 ****************************************************************************/

void orientation_get_euler(struct ahrs_out_s *ahrs,
                          float *roll, float *pitch, float *yaw)
{
  float euler[3];

  /* Convert quaternion to Euler angles using Madgwick library */
  quaternion2euler(ahrs->q, euler);

  /* Convert from radians to degrees */
  *roll  = euler[0] * RAD_TO_DEG;
  *pitch = euler[1] * RAD_TO_DEG;
  *yaw   = euler[2] * RAD_TO_DEG;
}

/****************************************************************************
 * Name: orientation_quaternion_to_dcm
 ****************************************************************************/

void orientation_quaternion_to_dcm(const float q[4], float dcm[3][3])
{
  float q0 = q[0];
  float q1 = q[1];
  float q2 = q[2];
  float q3 = q[3];

  /* Calculate DCM elements from quaternion
   * Reference: "Quaternion to DCM" conversion
   */

  /* First row */
  dcm[0][0] = 1.0f - 2.0f * (q2 * q2 + q3 * q3);
  dcm[0][1] = 2.0f * (q1 * q2 - q0 * q3);
  dcm[0][2] = 2.0f * (q1 * q3 + q0 * q2);

  /* Second row */
  dcm[1][0] = 2.0f * (q1 * q2 + q0 * q3);
  dcm[1][1] = 1.0f - 2.0f * (q1 * q1 + q3 * q3);
  dcm[1][2] = 2.0f * (q2 * q3 - q0 * q1);

  /* Third row */
  dcm[2][0] = 2.0f * (q1 * q3 - q0 * q2);
  dcm[2][1] = 2.0f * (q2 * q3 + q0 * q1);
  dcm[2][2] = 1.0f - 2.0f * (q1 * q1 + q2 * q2);
}

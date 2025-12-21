/****************************************************************************
 * bmi160_orientation/orientation_calc.h
 *
 * Orientation calculation utilities
 *
 ****************************************************************************/

#ifndef __ORIENTATION_CALC_H
#define __ORIENTATION_CALC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <MadgwickAHRS.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: orientation_get_euler
 *
 * Description:
 *   Convert quaternion to Euler angles (Roll, Pitch, Yaw) in degrees
 *
 * Input Parameters:
 *   ahrs  - AHRS output structure containing quaternion
 *   roll  - Output: Roll angle in degrees
 *   pitch - Output: Pitch angle in degrees
 *   yaw   - Output: Yaw angle in degrees
 *
 ****************************************************************************/

void orientation_get_euler(struct ahrs_out_s *ahrs,
                          float *roll, float *pitch, float *yaw);

/****************************************************************************
 * Name: orientation_quaternion_to_dcm
 *
 * Description:
 *   Convert quaternion to Direction Cosine Matrix (DCM)
 *   DCM is used for coordinate transformation
 *
 * Input Parameters:
 *   q   - Quaternion [q0, q1, q2, q3]
 *   dcm - Output: 3x3 DCM matrix (row-major)
 *
 ****************************************************************************/

void orientation_quaternion_to_dcm(const float q[4], float dcm[3][3]);

#endif /* __ORIENTATION_CALC_H */

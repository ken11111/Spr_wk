/****************************************************************************
 * bmi160_orientation/bmi160_orientation_main.c
 *
 * BMI160 sensor orientation (Roll/Pitch/Yaw) and position estimation
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#include <nuttx/sensors/bmi160.h>
#include <MadgwickAHRS.h>

#include "orientation_calc.h"
#include "position_estimator.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BMI160_DEVPATH      "/dev/accel0"
#define SAMPLE_RATE_HZ      100.0f    /* 100Hz sampling */
#define MADGWICK_BETA       0.1f      /* Filter parameter */
#define RAD_TO_DEG          (180.0f / M_PI)

/* Sensor scale factors (need to be adjusted based on sensor configuration) */
#define GYRO_SCALE          (2000.0f / 32768.0f * M_PI / 180.0f)  /* rad/s */
#define ACCEL_SCALE         (16.0f / 32768.0f)                     /* g */
#define GRAVITY             9.80665f                               /* m/s^2 */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct ahrs_out_s g_ahrs;
static struct position_state_s g_pos_state;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * convert_sensor_data
 ****************************************************************************/

static void convert_sensor_data(struct accel_gyro_st_s *raw,
                                float *gx, float *gy, float *gz,
                                float *ax, float *ay, float *az)
{
  /* Convert gyroscope data to rad/s */
  *gx = (float)raw->gyro.x * GYRO_SCALE;
  *gy = (float)raw->gyro.y * GYRO_SCALE;
  *gz = (float)raw->gyro.z * GYRO_SCALE;

  /* Convert accelerometer data to m/s^2 */
  *ax = (float)raw->accel.x * ACCEL_SCALE * GRAVITY;
  *ay = (float)raw->accel.y * ACCEL_SCALE * GRAVITY;
  *az = (float)raw->accel.z * ACCEL_SCALE * GRAVITY;
}

/****************************************************************************
 * print_orientation_position
 ****************************************************************************/

static void print_orientation_position(uint32_t timestamp,
                                       float roll, float pitch, float yaw,
                                       float x, float y, float z)
{
  printf("[%10u] ", timestamp);
  printf("Roll:%7.2f° Pitch:%7.2f° Yaw:%7.2f° | ",
         roll, pitch, yaw);
  printf("X:%7.3fm Y:%7.3fm Z:%7.3fm\n",
         x, y, z);
  fflush(stdout);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * main
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int fd;
  struct accel_gyro_st_s raw_data;
  uint32_t prev_time = 0;
  float dt = 1.0f / SAMPLE_RATE_HZ;

  float gx, gy, gz;  /* Gyroscope: rad/s */
  float ax, ay, az;  /* Accelerometer: m/s^2 */
  float roll, pitch, yaw;  /* Orientation: degrees */

  printf("BMI160 Orientation and Position Estimation\n");
  printf("==========================================\n\n");

  /* Initialize AHRS */
  INIT_AHRS(&g_ahrs, MADGWICK_BETA, SAMPLE_RATE_HZ);
  printf("Madgwick AHRS initialized (beta=%.2f, rate=%.0fHz)\n",
         MADGWICK_BETA, SAMPLE_RATE_HZ);

  /* Initialize position estimator */
  position_estimator_init(&g_pos_state);
  printf("Position estimator initialized\n\n");

  /* Open BMI160 sensor */
  fd = open(BMI160_DEVPATH, O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr, "ERROR: Failed to open %s: %d\n",
              BMI160_DEVPATH, errno);
      return -1;
    }

  printf("BMI160 sensor opened successfully\n");
  printf("Starting data acquisition...\n\n");
  printf("Time(ms)      Roll    Pitch      Yaw   |     X       Y       Z\n");
  printf("                [deg]   [deg]    [deg]  |    [m]     [m]     [m]\n");
  printf("---------------------------------------------------------------\n");

  /* Main loop */
  for (;;)
    {
      int ret;

      /* Read sensor data */
      ret = read(fd, &raw_data, sizeof(struct accel_gyro_st_s));
      if (ret != sizeof(struct accel_gyro_st_s))
        {
          fprintf(stderr, "ERROR: Read failed: %d\n", ret);
          break;
        }

      /* Process only when timestamp changes */
      if (prev_time != raw_data.sensor_time)
        {
          /* Convert raw sensor data */
          convert_sensor_data(&raw_data, &gx, &gy, &gz, &ax, &ay, &az);

          /* Update orientation using Madgwick AHRS */
          MadgwickAHRSupdateIMU(&g_ahrs, gx, gy, gz, ax, ay, az, dt);

          /* Get Euler angles (Roll, Pitch, Yaw) */
          orientation_get_euler(&g_ahrs, &roll, &pitch, &yaw);

          /* Update position estimation */
          position_estimator_update(&g_pos_state, &g_ahrs,
                                    ax, ay, az, dt);

          /* Print results every 10 samples (10Hz output) */
          static int sample_count = 0;
          if (++sample_count >= 10)
            {
              print_orientation_position(raw_data.sensor_time,
                                        roll, pitch, yaw,
                                        g_pos_state.x,
                                        g_pos_state.y,
                                        g_pos_state.z);
              sample_count = 0;
            }

          prev_time = raw_data.sensor_time;
        }

      /* Small delay to prevent CPU overload */
      usleep(1000);  /* 1ms */
    }

  close(fd);
  return 0;
}

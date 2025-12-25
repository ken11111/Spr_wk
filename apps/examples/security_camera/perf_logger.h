/****************************************************************************
 * security_camera/perf_logger.h
 *
 * Performance Logging for Bandwidth and Latency Analysis
 *
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_SECURITY_CAMERA_PERF_LOGGER_H
#define __APPS_EXAMPLES_SECURITY_CAMERA_PERF_LOGGER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Performance logging enable/disable */

#define PERF_LOGGING_ENABLED         1

/* Performance log interval (frames) */

#define PERF_LOG_INTERVAL_FRAMES     30    /* Log every 30 frames (1 sec @ 30fps) */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Performance metrics per frame */

typedef struct perf_frame_metrics_s
{
  uint32_t frame_num;                /* Frame number */

  /* Timestamps (microseconds) */

  uint64_t ts_frame_start;           /* Frame processing start */
  uint64_t ts_camera_poll_start;     /* Camera poll() start */
  uint64_t ts_camera_poll_end;       /* Camera poll() end */
  uint64_t ts_camera_dqbuf_start;    /* VIDIOC_DQBUF start */
  uint64_t ts_camera_dqbuf_end;      /* VIDIOC_DQBUF end */
  uint64_t ts_pack_start;            /* mjpeg_pack_frame() start */
  uint64_t ts_pack_end;              /* mjpeg_pack_frame() end */
  uint64_t ts_usb_write_start;       /* USB write() start */
  uint64_t ts_usb_write_end;         /* USB write() end */
  uint64_t ts_frame_end;             /* Frame processing end */

  /* Sizes (bytes) */

  uint32_t jpeg_size;                /* JPEG frame size */
  uint32_t packet_size;              /* MJPEG packet size (with header+CRC) */
  uint32_t usb_written;              /* Actual USB bytes written */

  /* Latencies (microseconds) */

  uint32_t latency_camera_poll;      /* poll() wait time */
  uint32_t latency_camera_dqbuf;     /* DQBUF ioctl time */
  uint32_t latency_pack;             /* Packet creation time */
  uint32_t latency_usb_write;        /* USB write time */
  uint32_t latency_total;            /* Total frame processing time */

  /* Inter-frame interval */

  uint32_t interval_us;              /* Time since last frame (us) */

  /* USB retry count */

  uint8_t usb_retry_count;           /* Number of USB write retries */
} perf_frame_metrics_t;

/* Aggregated performance statistics */

typedef struct perf_stats_s
{
  /* Time window */

  uint64_t window_start_us;          /* Statistics window start time */
  uint32_t frames_in_window;         /* Frames processed in window */

  /* Throughput */

  uint64_t total_jpeg_bytes;         /* Total JPEG bytes */
  uint64_t total_packet_bytes;       /* Total packet bytes (with overhead) */
  uint64_t total_usb_bytes;          /* Total USB bytes sent */

  /* Average latencies (microseconds) */

  uint32_t avg_latency_camera;       /* Average camera capture time */
  uint32_t avg_latency_pack;         /* Average packet creation time */
  uint32_t avg_latency_usb;          /* Average USB write time */
  uint32_t avg_latency_total;        /* Average total frame time */
  uint32_t avg_interval;             /* Average inter-frame interval */

  /* Min/Max values */

  uint32_t min_jpeg_size;            /* Minimum JPEG size */
  uint32_t max_jpeg_size;            /* Maximum JPEG size */
  uint32_t min_interval;             /* Minimum frame interval */
  uint32_t max_interval;             /* Maximum frame interval */

  /* Error counts */

  uint32_t total_usb_retries;        /* Total USB write retries */
  uint32_t camera_timeouts;          /* Camera poll timeouts */
  uint32_t dropped_frames;           /* Frames dropped due to errors */
} perf_stats_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: perf_logger_init
 *
 * Description:
 *   Initialize performance logger
 *
 ****************************************************************************/

void perf_logger_init(void);

/****************************************************************************
 * Name: perf_logger_get_timestamp_us
 *
 * Description:
 *   Get current timestamp in microseconds (monotonic clock)
 *
 * Returns:
 *   Timestamp in microseconds
 *
 ****************************************************************************/

uint64_t perf_logger_get_timestamp_us(void);

/****************************************************************************
 * Name: perf_logger_record_frame
 *
 * Description:
 *   Record frame performance metrics
 *
 * Parameters:
 *   metrics - Pointer to frame metrics structure
 *
 ****************************************************************************/

void perf_logger_record_frame(const perf_frame_metrics_t *metrics);

/****************************************************************************
 * Name: perf_logger_print_stats
 *
 * Description:
 *   Print aggregated performance statistics (called periodically)
 *
 * Parameters:
 *   force - Force print even if interval not reached
 *
 ****************************************************************************/

void perf_logger_print_stats(bool force);

/****************************************************************************
 * Name: perf_logger_cleanup
 *
 * Description:
 *   Cleanup performance logger and print final statistics
 *
 ****************************************************************************/

void perf_logger_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_PERF_LOGGER_H */

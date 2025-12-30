/****************************************************************************
 * security_camera/perf_logger.c
 *
 * Performance Logging Implementation
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#include "perf_logger.h"
#include "config.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static perf_stats_t g_perf_stats;
static uint64_t g_last_frame_timestamp = 0;
static uint32_t g_total_frames = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: reset_stats
 *
 * Description:
 *   Reset performance statistics for new window
 *
 ****************************************************************************/

static void reset_stats(void)
{
  memset(&g_perf_stats, 0, sizeof(perf_stats_t));
  g_perf_stats.window_start_us = perf_logger_get_timestamp_us();
  g_perf_stats.min_jpeg_size = UINT32_MAX;
  g_perf_stats.max_jpeg_size = 0;
  g_perf_stats.min_interval = UINT32_MAX;
  g_perf_stats.max_interval = 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: perf_logger_init
 *
 * Description:
 *   Initialize performance logger
 *
 ****************************************************************************/

void perf_logger_init(void)
{
#if PERF_LOGGING_ENABLED
  reset_stats();
  g_last_frame_timestamp = 0;
  g_total_frames = 0;

  LOG_INFO("Performance logging initialized (interval=%d frames)",
           PERF_LOG_INTERVAL_FRAMES);
#endif
}

/****************************************************************************
 * Name: perf_logger_get_timestamp_us
 *
 * Description:
 *   Get current timestamp in microseconds (monotonic clock)
 *
 ****************************************************************************/

uint64_t perf_logger_get_timestamp_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/****************************************************************************
 * Name: perf_logger_record_frame
 *
 * Description:
 *   Record frame performance metrics
 *
 ****************************************************************************/

void perf_logger_record_frame(const perf_frame_metrics_t *metrics)
{
#if PERF_LOGGING_ENABLED
  if (metrics == NULL)
    {
      return;
    }

  g_total_frames++;

  /* Update throughput counters */

  g_perf_stats.frames_in_window++;
  g_perf_stats.total_jpeg_bytes += metrics->jpeg_size;
  g_perf_stats.total_packet_bytes += metrics->packet_size;
  g_perf_stats.total_usb_bytes += metrics->usb_written;

  /* Accumulate latencies for averaging */

  g_perf_stats.avg_latency_camera += (metrics->latency_camera_poll +
                                       metrics->latency_camera_dqbuf);
  g_perf_stats.avg_latency_pack += metrics->latency_pack;
  g_perf_stats.avg_latency_usb += metrics->latency_usb_write;
  g_perf_stats.avg_latency_total += metrics->latency_total;
  g_perf_stats.avg_interval += metrics->interval_us;

  /* Update min/max values */

  if (metrics->jpeg_size < g_perf_stats.min_jpeg_size)
    {
      g_perf_stats.min_jpeg_size = metrics->jpeg_size;
    }

  if (metrics->jpeg_size > g_perf_stats.max_jpeg_size)
    {
      g_perf_stats.max_jpeg_size = metrics->jpeg_size;
    }

  if (metrics->interval_us < g_perf_stats.min_interval &&
      metrics->interval_us > 0)
    {
      g_perf_stats.min_interval = metrics->interval_us;
    }

  if (metrics->interval_us > g_perf_stats.max_interval)
    {
      g_perf_stats.max_interval = metrics->interval_us;
    }

  /* Update error counts */

  g_perf_stats.total_usb_retries += metrics->usb_retry_count;

  /* Log detailed per-frame metrics (every Nth frame) */

  if (g_total_frames == 1 || g_total_frames % PERF_LOG_INTERVAL_FRAMES == 0)
    {
      LOG_INFO("[PERF] Frame %u: JPEG=%u B, Pkt=%u B, "
               "Lat(cam/pkt/usb/tot)=%u/%u/%u/%u us, Interval=%u us",
               metrics->frame_num,
               metrics->jpeg_size,
               metrics->packet_size,
               metrics->latency_camera_poll + metrics->latency_camera_dqbuf,
               metrics->latency_pack,
               metrics->latency_usb_write,
               metrics->latency_total,
               metrics->interval_us);
    }

  /* Print aggregated statistics every N frames */

  if (g_perf_stats.frames_in_window >= PERF_LOG_INTERVAL_FRAMES)
    {
      perf_logger_print_stats(false);
    }
#endif
}

/****************************************************************************
 * Name: perf_logger_print_stats
 *
 * Description:
 *   Print aggregated performance statistics
 *
 ****************************************************************************/

void perf_logger_print_stats(bool force)
{
#if PERF_LOGGING_ENABLED
  uint64_t window_duration_us;
  uint32_t frames;
  float fps;
  float avg_jpeg_kb;
  float avg_packet_kb;
  float throughput_mbps_jpeg;
  float throughput_mbps_usb;
  float usb_utilization;

  frames = g_perf_stats.frames_in_window;

  if (frames == 0 || (!force && frames < PERF_LOG_INTERVAL_FRAMES))
    {
      return;
    }

  /* Calculate window duration */

  window_duration_us = perf_logger_get_timestamp_us() -
                       g_perf_stats.window_start_us;

  if (window_duration_us == 0)
    {
      return;  /* Avoid division by zero */
    }

  /* Calculate averages */

  g_perf_stats.avg_latency_camera /= frames;
  g_perf_stats.avg_latency_pack /= frames;
  g_perf_stats.avg_latency_usb /= frames;
  g_perf_stats.avg_latency_total /= frames;
  g_perf_stats.avg_interval /= frames;

  /* Calculate throughput */

  fps = (float)frames * 1000000.0f / (float)window_duration_us;
  avg_jpeg_kb = (float)g_perf_stats.total_jpeg_bytes / (float)frames / 1024.0f;
  avg_packet_kb = (float)g_perf_stats.total_packet_bytes / (float)frames / 1024.0f;

  /* Throughput in Mbps */

  throughput_mbps_jpeg = (float)g_perf_stats.total_jpeg_bytes * 8.0f /
                         (float)window_duration_us;
  throughput_mbps_usb = (float)g_perf_stats.total_usb_bytes * 8.0f /
                        (float)window_duration_us;

  /* USB Full Speed utilization (12 Mbps = theoretical max) */

  usb_utilization = (throughput_mbps_usb / 12.0f) * 100.0f;

  /* Print comprehensive statistics */

  LOG_INFO("========================================================");
  LOG_INFO("[PERF STATS] Window: %u frames in %.2f sec (%.2f fps)",
           frames,
           (float)window_duration_us / 1000000.0f,
           fps);
  LOG_INFO("--------------------------------------------------------");
  LOG_INFO("[SIZE] JPEG: avg=%.2f KB, min=%u B, max=%u B",
           avg_jpeg_kb,
           g_perf_stats.min_jpeg_size,
           g_perf_stats.max_jpeg_size);
  LOG_INFO("[SIZE] Packet (w/overhead): avg=%.2f KB",
           avg_packet_kb);
  LOG_INFO("--------------------------------------------------------");
  LOG_INFO("[THROUGHPUT] JPEG data: %.2f Mbps",
           throughput_mbps_jpeg);
  LOG_INFO("[THROUGHPUT] USB (w/overhead): %.2f Mbps",
           throughput_mbps_usb);
  LOG_INFO("[USB] Utilization: %.1f%% of 12 Mbps Full Speed",
           usb_utilization);

  if (usb_utilization > 100.0f)
    {
      LOG_WARN("[USB] ⚠️  BANDWIDTH EXCEEDED! Target: <100%%, Actual: %.1f%%",
               usb_utilization);
      LOG_WARN("[USB] Recommend: Reduce FPS or JPEG quality");
    }
  else if (usb_utilization > 80.0f)
    {
      LOG_WARN("[USB] ⚠️  High bandwidth usage: %.1f%% (>80%%)",
               usb_utilization);
    }

  LOG_INFO("--------------------------------------------------------");
  LOG_INFO("[LATENCY] Camera: avg=%u us",
           g_perf_stats.avg_latency_camera);
  LOG_INFO("[LATENCY] Pack: avg=%u us",
           g_perf_stats.avg_latency_pack);
  LOG_INFO("[LATENCY] USB Write: avg=%u us",
           g_perf_stats.avg_latency_usb);
  LOG_INFO("[LATENCY] Total: avg=%u us",
           g_perf_stats.avg_latency_total);
  LOG_INFO("--------------------------------------------------------");
  LOG_INFO("[INTERVAL] Frame: avg=%u us, min=%u us, max=%u us",
           g_perf_stats.avg_interval,
           g_perf_stats.min_interval,
           g_perf_stats.max_interval);
  LOG_INFO("[INTERVAL] Target: 33333 us (30 fps)");

  if (g_perf_stats.avg_interval > 35000)
    {
      LOG_WARN("[INTERVAL] ⚠️  Slower than target (%.1f fps actual)",
               1000000.0f / (float)g_perf_stats.avg_interval);
    }

  LOG_INFO("--------------------------------------------------------");
  LOG_INFO("[ERRORS] USB retries: %u (%.2f per frame)",
           g_perf_stats.total_usb_retries,
           (float)g_perf_stats.total_usb_retries / (float)frames);
  LOG_INFO("[ERRORS] Camera timeouts: %u",
           g_perf_stats.camera_timeouts);
  LOG_INFO("[ERRORS] Dropped frames: %u",
           g_perf_stats.dropped_frames);
  LOG_INFO("========================================================");

  /* Reset statistics for next window */

  reset_stats();
#endif
}

/****************************************************************************
 * Name: perf_logger_cleanup
 *
 * Description:
 *   Cleanup performance logger and print final statistics
 *
 ****************************************************************************/

void perf_logger_cleanup(void)
{
#if PERF_LOGGING_ENABLED
  LOG_INFO("Performance logging cleanup");
  perf_logger_print_stats(true);  /* Print final statistics */

  LOG_INFO("Total frames processed: %u", g_total_frames);
#endif
}

/****************************************************************************
 * security_camera/camera_threads.c
 *
 *   Copyright 2025 Security Camera Project
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
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sched.h>
#include <time.h>

#include "camera_threads.h"
#include "frame_queue.h"
#include "camera_manager.h"
#include "mjpeg_protocol.h"
#include "usb_transport.h"
#include "perf_logger.h"
#include "fps_controller.h"
#include "config.h"

/* Phase 11: Enhanced adaptive control */
#ifdef CONFIG_PHASE11_ENABLE
#include "frame_statistics.h"
#include "enhanced_control.h"
#endif

/* Phase 7: WiFi/TCP transport support */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
#include "tcp_server.h"
#endif

/* Phase 10: Control engineering implementation */

/****************************************************************************
 * Performance Optimization Strategy (Step 5)
 ****************************************************************************/

/* Thread Priorities:
 *   Camera: 110 (HIGH)  - Must not miss frames from V4L2 driver
 *   USB:    100 (LOWER) - Can tolerate preemption
 *
 * Queue Depth: 3 buffers
 *   Matches V4L2 triple buffering
 *   Absorbs ~90ms timing variance
 *   Total memory: ~300KB (acceptable)
 *
 * Synchronization:
 *   Priority inheritance mutex (PTHREAD_PRIO_INHERIT)
 *   Single condition variable for bidirectional signaling
 *   NO blocking I/O inside mutex (poll/write outside)
 *
 * Buffer Management:
 *   32-byte aligned for DMA optimization
 *   Action queue: Filled frames (camera → USB)
 *   Empty queue: Recycled buffers (USB → camera)
 *
 * Performance Monitoring:
 *   Queue depth logged every 30 frames (~1 sec @ 30fps)
 *   USB throughput calculated and logged
 *   Thread exit statistics (frame count, bytes sent)
 */

/****************************************************************************
 * Public Data
 ****************************************************************************/

pthread_t g_camera_thread;
pthread_t g_usb_thread;
pthread_t g_control_thread;  /* Phase 10: PID control thread */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static thread_context_t *g_thread_ctx = NULL;

/* Phase 4.1: Metrics tracking variables */

static uint32_t g_metrics_sequence = 0;
static uint32_t g_total_camera_frames = 0;
static uint32_t g_total_usb_packets = 0;
static uint64_t g_total_packet_bytes = 0;
static uint32_t g_total_errors = 0;
static struct timespec g_start_time;
static struct timespec g_last_metrics_time;

#define METRICS_INTERVAL_MS 1000  /* Send metrics every 1 second */

/* Phase 10: PID Controller */

static fps_controller_t g_fps_controller;

/* Phase 11: Frame Statistics */

#ifdef CONFIG_PHASE11_ENABLE
static frame_statistics_t g_frame_statistics;
static bool g_frame_statistics_initialized = false;
#endif

/* Phase 7.3.3c: Unified frame drop logic with OR condition
 *
 * Frame drop strategy (OR condition):
 *   Drop frames if: (slow_send_count >= 3) OR (queue_depth >= 6)
 *
 * Rationale:
 * - Phase 7.3.3b had separate time-based and queue-based checks
 * - Problem: Independent checks caused counter inconsistency
 *   - Queue drop didn't reset slow_send_count
 *   - Time drop didn't prevent subsequent queue saturation
 * - Solution: Unified check with OR condition
 *   - Single drop logic triggered by either condition
 *   - Both counters reset after drop
 *   - Clear logging of trigger reason
 *
 * Drop conditions:
 * 1. Time-based: TCP send > 250ms for 3 consecutive frames
 * 2. Queue-based: Queue depth >= 6 (approaching max 7)
 *
 * Drop action:
 * - Pull 3 oldest frames from action queue
 * - Push to empty queue for recycling
 * - Reset slow_send_count
 * - Log drop reason (time vs queue)
 */

static uint32_t g_dropped_frames = 0;       /* Total dropped frames */
static uint32_t g_drop_events = 0;          /* Number of drop events */

/* Drop condition parameters */
#define SLOW_SEND_THRESHOLD_MS     250  /* Slow send threshold (ms) */
#define SLOW_SEND_COUNT_MAX        3    /* Consecutive slow sends to trigger drop */
#define QUEUE_SATURATION_THRESHOLD 6    /* Drop when queue depth >= 6 (max is 7) */
#define DROP_FRAME_COUNT           3    /* Number of frames to drop per event */

/* Phase 7.2a: Multi-frame batching support */

static uint32_t g_batch_sequence = 0;

typedef struct frame_batch_s
{
  frame_buffer_t *frames[MJPEG_BATCH_SIZE];
  uint32_t frame_sequences[MJPEG_BATCH_SIZE];
  int frame_count;
  uint32_t total_jpeg_size;
} frame_batch_t;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_uptime_ms
 *
 * Description:
 *   Get elapsed time since start in milliseconds
 *
 ****************************************************************************/

static uint32_t get_uptime_ms(void)
{
  struct timespec now;
  uint64_t start_ms;
  uint64_t now_ms;

  clock_gettime(CLOCK_MONOTONIC, &now);

  /* Bug fix: Convert to milliseconds first to avoid negative nsec_diff */

  start_ms = (uint64_t)g_start_time.tv_sec * 1000ULL +
             (uint64_t)g_start_time.tv_nsec / 1000000ULL;
  now_ms = (uint64_t)now.tv_sec * 1000ULL +
           (uint64_t)now.tv_nsec / 1000000ULL;

  return (uint32_t)(now_ms - start_ms);
}

/****************************************************************************
 * Name: get_timestamp_us
 *
 * Description:
 *   Get current timestamp in microseconds (Phase 10: Safety fallback)
 *
 ****************************************************************************/

static uint64_t get_timestamp_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/****************************************************************************
 * Name: send_metrics_packet
 *
 * Description:
 *   Queue metrics packet for USB thread to send (avoids race condition)
 *
 ****************************************************************************/

static int send_metrics_packet(int usb_fd)
{
  uint8_t metrics_buffer[METRICS_PACKET_SIZE];
  uint32_t uptime_ms;
  uint32_t avg_packet_size;
  uint32_t action_q_depth;
  uint32_t tcp_avg_send_us = 0;
  uint32_t tcp_max_send_us = 0;
  uint32_t tcp_health_moving_avg = 0;
  uint32_t tcp_health_total_spikes = 0;
  int ret;
  frame_buffer_t *buf;

  /* Get current metrics */

  uptime_ms = get_uptime_ms();
  avg_packet_size = (g_total_usb_packets > 0)
                  ? (uint32_t)(g_total_packet_bytes / g_total_usb_packets)
                  : 0;
  action_q_depth = frame_queue_depth(g_action_queue);

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
  /* Get TCP send statistics (Phase 7) */
  tcp_server_get_stats(&tcp_avg_send_us, &tcp_max_send_us);

  /* Phase 9.2: Get TCP health metrics */
  tcp_health_get_metrics(&tcp_health_moving_avg, NULL, &tcp_health_total_spikes, NULL);
#endif

  /* Pack metrics into packet */

  ret = mjpeg_pack_metrics(uptime_ms,
                           g_total_camera_frames,
                           g_total_usb_packets,
                           action_q_depth,
                           avg_packet_size,
                           g_total_errors,
                           tcp_avg_send_us,
                           tcp_max_send_us,
                           g_dropped_frames,          /* Phase 7.3.3 */
                           g_drop_events,             /* Phase 7.3.3 */
                           tcp_health_moving_avg,     /* Phase 9.2 */
                           tcp_health_total_spikes,   /* Phase 9.2 */
                           &g_metrics_sequence,
                           metrics_buffer);

  if (ret < 0)
    {
      LOG_ERROR("Failed to pack metrics: %d", ret);
      return ret;
    }

  /* Queue metrics packet for USB thread to send (avoid race condition) */

  buf = frame_queue_pull(&g_empty_queue);
  if (buf == NULL)
    {
      LOG_WARN("No empty buffer for metrics packet");
      return -ENOMEM;
    }

  /* Copy metrics packet to buffer */

  memcpy(buf->data, metrics_buffer, METRICS_PACKET_SIZE);
  buf->used = METRICS_PACKET_SIZE;

  /* Push to action queue for USB thread to send */

  frame_queue_push(&g_action_queue, buf);

  LOG_INFO("Metrics queued: seq=%lu, cam_frames=%lu, usb_pkts=%lu, q_depth=%lu",
           (unsigned long)(g_metrics_sequence - 1),
           (unsigned long)g_total_camera_frames,
           (unsigned long)g_total_usb_packets,
           (unsigned long)action_q_depth);

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: camera_thread_func
 *
 * Description:
 *   Camera thread function (producer)
 *   Step 2: Capture frames and pack into buffers
 *
 ****************************************************************************/

void *camera_thread_func(void *arg)
{
  thread_context_t *ctx = (thread_context_t *)arg;
  camera_frame_t frame;
  frame_buffer_t *buffer;
  int ret;
  int packet_size;
  uint32_t error_count = 0;

  /* Step 5: Performance statistics */

  uint32_t frame_count = 0;
  uint32_t stats_interval = 30;  /* Log every 30 frames (~1 sec @ 30fps) */
  uint32_t jpeg_validation_error_count = 0;  /* Phase 4.1.1 */
  uint32_t consecutive_jpeg_errors = 0;      /* Phase 4.1.1 */
  uint64_t total_jpeg_bytes = 0;             /* For average JPEG size */

  LOG_INFO("== Camera thread started (Step 2: active) ==");
  LOG_INFO("Camera thread priority: %d", CAMERA_THREAD_PRIORITY);

  while (!g_shutdown_requested)
    {
      /* Step 1: Pull empty buffer from queue (blocking if none available) */

      pthread_mutex_lock(&g_queue_mutex);
      while (frame_queue_is_empty(g_empty_queue) && !g_shutdown_requested)
        {
          pthread_cond_wait(&g_queue_cond, &g_queue_mutex);
        }

      if (g_shutdown_requested)
        {
          pthread_mutex_unlock(&g_queue_mutex);
          break;
        }

      buffer = frame_queue_pull(&g_empty_queue);
      pthread_mutex_unlock(&g_queue_mutex);

      if (buffer == NULL)
        {
          /* Should not happen, but handle gracefully */

          LOG_WARN("Camera thread: Failed to pull empty buffer");
          usleep(10000);  /* 10ms */
          continue;
        }

      /* Step 2: Get JPEG frame from camera (outside mutex - blocking I/O) */

      ret = camera_get_frame(&frame);
      if (ret < 0)
        {
          /* Step 4: Enhanced camera error detection */

          if (ret == ERR_TIMEOUT)
            {
              LOG_WARN("Camera thread: Frame timeout (may be transient)");

              /* Timeout is less critical - don't increment error count */
              /* Just return buffer and retry */
            }
          else
            {
              LOG_ERROR("Camera thread: Failed to get frame: %d", ret);
              error_count++;
              g_total_errors++;  /* Phase 4.1: Track total errors */

              if (error_count >= 3)
                {
                  LOG_ERROR("Camera thread: Too many errors (%lu consecutive), "
                            "shutting down", (unsigned long)error_count);
                  pthread_mutex_lock(&g_queue_mutex);
                  g_shutdown_requested = true;
                  pthread_cond_broadcast(&g_queue_cond);
                  pthread_mutex_unlock(&g_queue_mutex);

                  /* Return buffer before exiting */

                  pthread_mutex_lock(&g_queue_mutex);
                  frame_queue_push(&g_empty_queue, buffer);
                  pthread_mutex_unlock(&g_queue_mutex);
                  break;
                }
            }

          /* Return buffer to empty queue and retry */

          pthread_mutex_lock(&g_queue_mutex);
          frame_queue_push(&g_empty_queue, buffer);
          pthread_mutex_unlock(&g_queue_mutex);
          usleep(100000);  /* 100ms delay before retry */
          continue;
        }

      error_count = 0;  /* Reset error count on success */

      /* Step 3: Pack JPEG into MJPEG protocol packet (outside mutex) */
      /* Phase 4.1.1: JPEG validation happens inside mjpeg_pack_frame() */

      packet_size = mjpeg_pack_frame(frame.buf, frame.size, ctx->sequence,
                                      (uint8_t *)buffer->data, buffer->length);
      if (packet_size < 0)
        {
          /* Phase 4.1.1: JPEG validation error detected */

          LOG_ERROR("Failed to pack frame (JPEG validation error): %d", packet_size);
          jpeg_validation_error_count++;
          consecutive_jpeg_errors++;
          g_total_errors++;  /* Phase 4.1: Track JPEG validation errors */

          /* Warning at 5 consecutive errors */

          if (consecutive_jpeg_errors >= 5)
            {
              LOG_WARN("Camera thread: %lu consecutive JPEG validation errors",
                       (unsigned long)consecutive_jpeg_errors);
            }

          /* Error threshold at 10 consecutive */

          if (consecutive_jpeg_errors >= 10)
            {
              LOG_ERROR("Camera thread: 10+ consecutive JPEG validation errors, "
                        "this may indicate ISX012 hardware issue");
            }

          /* Return buffer to empty queue and continue */

          pthread_mutex_lock(&g_queue_mutex);
          frame_queue_push(&g_empty_queue, buffer);
          pthread_mutex_unlock(&g_queue_mutex);
          continue;
        }

      /* JPEG validation successful - reset consecutive error counter */

      consecutive_jpeg_errors = 0;
      buffer->used = packet_size;
      total_jpeg_bytes += frame.size;  /* Accumulate JPEG size */

      /* Phase 11: Update frame statistics */
#ifdef CONFIG_PHASE11_ENABLE
      if (!g_frame_statistics_initialized)
        {
          frame_statistics_init(&g_frame_statistics);
          g_frame_statistics_initialized = true;
          LOG_INFO("Phase 11: Frame statistics initialized");
        }

      /* Update frame statistics with current frame size */
      frame_statistics_update(&g_frame_statistics, frame.size, 0);
#endif

      /* Phase 4.1: Track total frames for metrics */

      g_total_camera_frames++;

      /* Step 4: Push filled buffer to action queue */

      pthread_mutex_lock(&g_queue_mutex);
      frame_queue_push(&g_action_queue, buffer);
      pthread_cond_signal(&g_queue_cond);  /* Wake USB thread/main loop */

      /* Step 5: Collect queue statistics */

      frame_count++;
      if (frame_count % stats_interval == 0)
        {
          int action_depth = frame_queue_depth(g_action_queue);
          int empty_depth = frame_queue_depth(g_empty_queue);
          uint32_t avg_jpeg_kb = (total_jpeg_bytes / frame_count) / 1024;
          float jpeg_error_rate = (float)jpeg_validation_error_count / (float)frame_count * 100.0f;

          LOG_INFO("Camera stats: frame=%lu, action_q=%d, empty_q=%d, "
                   "avg_jpeg=%lu KB, jpeg_errors=%lu (%.2f%%), dropped=%lu (events=%lu)",
                   (unsigned long)frame_count, action_depth, empty_depth,
                   (unsigned long)avg_jpeg_kb,
                   (unsigned long)jpeg_validation_error_count, jpeg_error_rate,
                   (unsigned long)g_dropped_frames, (unsigned long)g_drop_events);

#ifdef CONFIG_PHASE11_ENABLE
          /* Phase 11: Frame statistics logging */
          if (g_frame_statistics_initialized)
            {
              LOG_INFO("Phase 11 frame stats: avg=%.1fKB, range=%u-%uKB, "
                       "variance=%.2f, complexity=%.3f (smoothed=%.3f)",
                       frame_statistics_get_avg_size_kb(&g_frame_statistics),
                       g_frame_statistics.min_size_bytes / 1024,
                       g_frame_statistics.max_size_bytes / 1024,
                       frame_statistics_get_normalized_variance(&g_frame_statistics),
                       g_frame_statistics.complexity_index,
                       g_frame_statistics.smoothed_complexity);
            }
#endif
        }

      pthread_mutex_unlock(&g_queue_mutex);

      /* Phase 4.1: Check if it's time to send metrics packet */

      struct timespec now;
      clock_gettime(CLOCK_MONOTONIC, &now);
      uint64_t elapsed_ms = (uint64_t)(now.tv_sec - g_last_metrics_time.tv_sec) * 1000ULL +
                            (uint64_t)(now.tv_nsec - g_last_metrics_time.tv_nsec) / 1000000ULL;

      if (elapsed_ms >= METRICS_INTERVAL_MS)
        {
          send_metrics_packet(ctx->usb_fd);
          g_last_metrics_time = now;
        }

      /* Step 2: Maintain frame rate (30 fps = 33333 us per frame) */

      usleep(33333);  /* ~30 fps */
    }

  /* Phase 4.1.1: Final statistics */

  LOG_INFO("== Camera thread exiting (processed %lu frames) ==",
           (unsigned long)frame_count);

  if (jpeg_validation_error_count > 0)
    {
      float error_rate = (float)jpeg_validation_error_count / (float)frame_count * 100.0f;
      LOG_WARN("Camera thread: JPEG validation errors: %lu / %lu (%.2f%%)",
               (unsigned long)jpeg_validation_error_count,
               (unsigned long)frame_count, error_rate);
    }
  else
    {
      LOG_INFO("Camera thread: JPEG validation errors: 0 (all frames valid)");
    }

  if (frame_count > 0)
    {
      uint32_t avg_jpeg_kb = (total_jpeg_bytes / frame_count) / 1024;
      LOG_INFO("Camera thread: Average JPEG size: %lu KB",
               (unsigned long)avg_jpeg_kb);
    }

  return NULL;
}

/****************************************************************************
 * Name: usb_thread_func
 *
 * Description:
 *   USB thread function (consumer)
 *   Step 3: Send packets via USB/TCP
 *   Phase 7.2a: Multi-frame batching for TCP efficiency
 *
 ****************************************************************************/

void *usb_thread_func(void *arg)
{
  frame_buffer_t *buffer;
  frame_batch_t batch;
  int ret;
  uint32_t error_count = 0;
  struct timespec timeout;
  int wait_ret;
  uint8_t *batch_packet = NULL;
  int i;

  /* Step 5: Performance statistics */

  uint32_t packet_count = 0;
  uint32_t frame_count = 0;
  uint32_t batch_count = 0;
  uint32_t total_bytes = 0;
  uint32_t stats_interval = 30;  /* Log every 30 frames (~1 sec @ 30fps) */

  /* Phase 7.3.3: Frame drop on slow TCP send */

  uint32_t slow_send_count = 0;
  struct timespec send_start, send_end;

  (void)arg;  /* Unused parameter */

  LOG_INFO("== USB thread started (Step 3: active) ==");
  LOG_INFO("USB thread priority: %d", USB_THREAD_PRIORITY);

#if MJPEG_BATCHING_ENABLED
  LOG_INFO("Phase 7.2a: Multi-frame batching enabled (batch_size=%d, timeout=%dms)",
           MJPEG_BATCH_SIZE, MJPEG_BATCH_TIMEOUT_MS);

  /* Allocate batch packet buffer (max ~185KB for 3 frames) */

  batch_packet = (uint8_t *)malloc(MJPEG_MAX_BATCH_PACKET);
  if (batch_packet == NULL)
    {
      LOG_ERROR("Failed to allocate batch packet buffer (%d bytes)",
                MJPEG_MAX_BATCH_PACKET);
      return NULL;
    }
#endif

  memset(&batch, 0, sizeof(batch));

  while (!g_shutdown_requested)
    {
#if MJPEG_BATCHING_ENABLED
      /* Phase 7.2a: Collect frames for batching */

      pthread_mutex_lock(&g_queue_mutex);

      /* Collect up to MJPEG_BATCH_SIZE frames */

      while (batch.frame_count < MJPEG_BATCH_SIZE && !g_shutdown_requested)
        {
          /* If queue is empty, wait with timeout */

          if (frame_queue_is_empty(g_action_queue))
            {
              /* Calculate timeout (100ms from now) */

              clock_gettime(CLOCK_REALTIME, &timeout);
              timeout.tv_nsec += MJPEG_BATCH_TIMEOUT_MS * 1000000LL;

              /* Handle nsec overflow */

              if (timeout.tv_nsec >= 1000000000LL)
                {
                  timeout.tv_sec += timeout.tv_nsec / 1000000000LL;
                  timeout.tv_nsec %= 1000000000LL;
                }

              wait_ret = pthread_cond_timedwait(&g_queue_cond, &g_queue_mutex,
                                                 &timeout);

              /* If timeout or shutdown, send partial batch */

              if (wait_ret == ETIMEDOUT || g_shutdown_requested)
                {
                  break;
                }

              continue;
            }

          /* Pull frame from action queue */

          buffer = frame_queue_pull(&g_action_queue);
          if (buffer != NULL)
            {
              batch.frames[batch.frame_count] = buffer;
              batch.frame_sequences[batch.frame_count] = buffer->id;
              batch.total_jpeg_size += buffer->used;
              batch.frame_count++;
            }
        }

      pthread_mutex_unlock(&g_queue_mutex);

      /* If no frames collected (shutdown), exit loop */

      if (batch.frame_count == 0)
        {
          break;
        }

      /* Pack batch of frames */

      const uint8_t *frame_data[MJPEG_BATCH_SIZE];
      uint32_t frame_sizes[MJPEG_BATCH_SIZE];

      for (i = 0; i < batch.frame_count; i++)
        {
          frame_data[i] = (const uint8_t *)batch.frames[i]->data;
          frame_sizes[i] = batch.frames[i]->used;
        }

      ret = mjpeg_pack_batch(frame_data, frame_sizes, batch.frame_sequences,
                            batch.frame_count, &g_batch_sequence,
                            batch_packet, MJPEG_MAX_BATCH_PACKET);

      if (ret < 0)
        {
          LOG_ERROR("Failed to pack batch: %d", ret);
          error_count++;

          /* Return buffers to empty queue */

          pthread_mutex_lock(&g_queue_mutex);
          for (i = 0; i < batch.frame_count; i++)
            {
              frame_queue_push(&g_empty_queue, batch.frames[i]);
            }

          pthread_cond_signal(&g_queue_cond);
          pthread_mutex_unlock(&g_queue_mutex);

          memset(&batch, 0, sizeof(batch));
          continue;
        }

      /* Send batch packet */

      int batch_size = ret;

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
      /* Phase 7: Send via TCP */

      tcp_server_t *tcp_srv = (tcp_server_t *)g_thread_ctx->tcp_server;

      /* Phase 7.2a: Check connection status before send */
      bool has_client = tcp_server_has_client(tcp_srv);
      LOG_INFO("Pre-send check: has_client=%d, batch_size=%d bytes", has_client, batch_size);

      if (!has_client)
        {
          LOG_ERROR("No client connected before send attempt!");
          ret = -ENOTCONN;
        }
      else
        {
          /* Phase 7.3.3: Start send time measurement */
          clock_gettime(CLOCK_MONOTONIC, &send_start);

          LOG_INFO("Sending batch via TCP: %d bytes", batch_size);

          /* Phase 9: Use auto-reconnect enabled send */
          ret = tcp_server_send_with_reconnect(tcp_srv, batch_packet, batch_size);

          /* Phase 7.3.3: End send time measurement */
          clock_gettime(CLOCK_MONOTONIC, &send_end);

          if (ret > 0)
            {
              LOG_INFO("TCP send successful: %d bytes", ret);

              /* Phase 7.3.3: Calculate send time and check for frame drop */
              uint64_t send_time_us = ((uint64_t)send_end.tv_sec * 1000000ULL +
                                       (uint64_t)send_end.tv_nsec / 1000ULL) -
                                      ((uint64_t)send_start.tv_sec * 1000000ULL +
                                       (uint64_t)send_start.tv_nsec / 1000ULL);
              uint64_t send_time_ms = send_time_us / 1000ULL;

              LOG_INFO("TCP send time: %lu ms", (unsigned long)send_time_ms);

              /* Check for slow send */
              if (send_time_ms > SLOW_SEND_THRESHOLD_MS)
                {
                  slow_send_count++;
                  LOG_WARN("Slow TCP send detected (%lu ms), count=%lu",
                           (unsigned long)send_time_ms,
                           (unsigned long)slow_send_count);

                  if (slow_send_count >= SLOW_SEND_COUNT_MAX)
                    {
                      LOG_WARN("TCP send is consistently slow, dropping old frames...");

                      /* Drop old frames from action queue */
                      int dropped = 0;
                      pthread_mutex_lock(&g_queue_mutex);

                      for (i = 0; i < DROP_FRAME_COUNT; i++)
                        {
                          frame_buffer_t *old_frame = frame_queue_pull(&g_action_queue);
                          if (old_frame != NULL)
                            {
                              frame_queue_push(&g_empty_queue, old_frame);
                              dropped++;
                              g_dropped_frames++;
                            }
                          else
                            {
                              break;  /* Queue is empty */
                            }
                        }

                      pthread_mutex_unlock(&g_queue_mutex);

                      if (dropped > 0)
                        {
                          g_drop_events++;
                          LOG_WARN("Dropped %d frames (total: %lu, events: %lu)",
                                   dropped,
                                   (unsigned long)g_dropped_frames,
                                   (unsigned long)g_drop_events);
                        }

                      slow_send_count = 0;  /* Reset counter */
                    }
                }
              else
                {
                  slow_send_count = 0;  /* Reset on normal send */
                }
            }
          else
            {
              LOG_ERROR("TCP send failed: ret=%d, errno=%d", ret, errno);
            }
        }
#else
      /* Send via USB */

      ret = usb_transport_send_bytes(batch_packet, batch_size);
#endif

#else  /* !MJPEG_BATCHING_ENABLED */
      /* Original single-frame mode */

      pthread_mutex_lock(&g_queue_mutex);
      while (frame_queue_is_empty(g_action_queue) && !g_shutdown_requested)
        {
          pthread_cond_wait(&g_queue_cond, &g_queue_mutex);
        }

      if (g_shutdown_requested)
        {
          pthread_mutex_unlock(&g_queue_mutex);
          break;
        }

      buffer = frame_queue_pull(&g_action_queue);
      pthread_mutex_unlock(&g_queue_mutex);

      if (buffer == NULL)
        {
          LOG_WARN("USB thread: Failed to pull buffer from action queue");
          usleep(10000);
          continue;
        }

      /* Send single frame */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
      tcp_server_t *tcp_srv = (tcp_server_t *)g_thread_ctx->tcp_server;

      /* Phase 9.2: Check for preventive reconnect before send */

      if (tcp_health_should_reconnect())
        {
          LOG_WARN("TCP health degraded - initiating preventive reconnect");

          /* Trigger preventive reconnect */

          if (tcp_server_handle_disconnect(tcp_srv) == 0)
            {
              if (tcp_server_wait_reconnect(tcp_srv) == 0)
                {
                  LOG_INFO("Preventive reconnect successful");
                  tcp_health_clear_reconnect_flag();
                  tcp_health_reset();  /* Reset health monitor */
                }
            }

          /* Return buffer and try next frame */

          pthread_mutex_lock(&g_queue_mutex);
          frame_queue_push(&g_empty_queue, buffer);
          pthread_cond_signal(&g_queue_cond);
          pthread_mutex_unlock(&g_queue_mutex);
          continue;
        }

      /* Phase 7.3.3: Start send time measurement */
      clock_gettime(CLOCK_MONOTONIC, &send_start);

      /* Phase 9: Use auto-reconnect enabled send */
      ret = tcp_server_send_with_reconnect(tcp_srv, buffer->data, buffer->used);

      /* Phase 7.3.3: End send time measurement */
      clock_gettime(CLOCK_MONOTONIC, &send_end);

      if (ret > 0)
        {
          /* Phase 7.3.3: Calculate send time and check for frame drop */
          uint64_t send_time_us = ((uint64_t)send_end.tv_sec * 1000000ULL +
                                   (uint64_t)send_end.tv_nsec / 1000ULL) -
                                  ((uint64_t)send_start.tv_sec * 1000000ULL +
                                   (uint64_t)send_start.tv_nsec / 1000ULL);
          uint64_t send_time_ms = send_time_us / 1000ULL;

          LOG_INFO("TCP send time: %lu ms", (unsigned long)send_time_ms);

          /* Phase 7.3.3c: Unified frame drop logic (OR condition)
           * Drop frames if:
           *   1. TCP send time > 250ms (slow send detected)
           *   2. Queue depth >= 6 (approaching saturation)
           */

          /* Check for slow send and update counter */
          if (send_time_ms > SLOW_SEND_THRESHOLD_MS)
            {
              slow_send_count++;
              LOG_WARN("Slow TCP send detected (%lu ms), count=%lu",
                       (unsigned long)send_time_ms,
                       (unsigned long)slow_send_count);
            }
          else
            {
              slow_send_count = 0;  /* Reset on normal send */
            }

          /* Get current queue depth for unified drop decision */
          pthread_mutex_lock(&g_queue_mutex);
          int current_depth = frame_queue_depth(g_action_queue);
          pthread_mutex_unlock(&g_queue_mutex);

          /* Unified drop condition: (slow send x3) OR (queue depth >= 6) */
          bool should_drop = (slow_send_count >= SLOW_SEND_COUNT_MAX) ||
                             (current_depth >= QUEUE_SATURATION_THRESHOLD);

          if (should_drop)
            {
              const char *reason = (slow_send_count >= SLOW_SEND_COUNT_MAX) ?
                                   "slow TCP send" : "queue saturation";
              LOG_WARN("Frame drop triggered by %s (send_count=%lu, depth=%d)",
                       reason,
                       (unsigned long)slow_send_count,
                       current_depth);

              /* Drop old frames from action queue */
              int dropped = 0;
              pthread_mutex_lock(&g_queue_mutex);

              for (int j = 0; j < DROP_FRAME_COUNT; j++)
                {
                  frame_buffer_t *old_frame = frame_queue_pull(&g_action_queue);
                  if (old_frame != NULL)
                    {
                      frame_queue_push(&g_empty_queue, old_frame);
                      dropped++;
                      g_dropped_frames++;
                    }
                  else
                    {
                      break;  /* Queue is empty */
                    }
                }

              pthread_mutex_unlock(&g_queue_mutex);

              if (dropped > 0)
                {
                  g_drop_events++;
                  LOG_WARN("Dropped %d frames (total: %lu, events: %lu)",
                           dropped,
                           (unsigned long)g_dropped_frames,
                           (unsigned long)g_drop_events);
                }

              /* Reset slow send counter after drop */
              slow_send_count = 0;
            }
        }
#else
      ret = usb_transport_send_bytes((uint8_t *)buffer->data, buffer->used);
#endif

#endif  /* MJPEG_BATCHING_ENABLED */

      /* Handle send errors */

      if (ret < 0)
        {
#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
          /* Phase 9: Handle reconnect success (-EAGAIN) */

          if (ret == -EAGAIN)
            {
              LOG_INFO("TCP reconnected successfully, skipping current frame");

              /* Phase 9.2: Reset health monitor after reconnect */

              tcp_health_reset();

              /* Return buffers and continue with next frame */

#if MJPEG_BATCHING_ENABLED
              pthread_mutex_lock(&g_queue_mutex);
              for (i = 0; i < batch.frame_count; i++)
                {
                  frame_queue_push(&g_empty_queue, batch.frames[i]);
                }

              pthread_cond_signal(&g_queue_cond);
              pthread_mutex_unlock(&g_queue_mutex);
              memset(&batch, 0, sizeof(batch));
#else
              pthread_mutex_lock(&g_queue_mutex);
              frame_queue_push(&g_empty_queue, buffer);
              pthread_cond_signal(&g_queue_cond);
              pthread_mutex_unlock(&g_queue_mutex);
#endif
              continue;  /* Continue with next frame */
            }

          /* Phase 9: Handle unrecoverable disconnect (reconnect failed) */

          if (ret == -ENOTCONN || ret == -ECONNRESET || ret == -EPIPE)
            {
              LOG_ERROR("TCP thread: Client disconnected and reconnect failed (error %d)", ret);

              pthread_mutex_lock(&g_queue_mutex);
              g_shutdown_requested = true;
              pthread_cond_broadcast(&g_queue_cond);
              pthread_mutex_unlock(&g_queue_mutex);

#if MJPEG_BATCHING_ENABLED
              pthread_mutex_lock(&g_queue_mutex);
              for (i = 0; i < batch.frame_count; i++)
                {
                  frame_queue_push(&g_empty_queue, batch.frames[i]);
                }

              pthread_mutex_unlock(&g_queue_mutex);
#else
              pthread_mutex_lock(&g_queue_mutex);
              frame_queue_push(&g_empty_queue, buffer);
              pthread_mutex_unlock(&g_queue_mutex);
#endif
              break;
            }

          LOG_ERROR("TCP thread: Failed to send packet: %d", ret);
#else
          if (ret == -ENXIO || ret == -EIO || ret == ERR_USB_DISCONNECTED)
            {
              LOG_ERROR("USB thread: USB device disconnected (error %d)", ret);

              pthread_mutex_lock(&g_queue_mutex);
              g_shutdown_requested = true;
              pthread_cond_broadcast(&g_queue_cond);
              pthread_mutex_unlock(&g_queue_mutex);

#if MJPEG_BATCHING_ENABLED
              pthread_mutex_lock(&g_queue_mutex);
              for (i = 0; i < batch.frame_count; i++)
                {
                  frame_queue_push(&g_empty_queue, batch.frames[i]);
                }

              pthread_mutex_unlock(&g_queue_mutex);
#else
              pthread_mutex_lock(&g_queue_mutex);
              frame_queue_push(&g_empty_queue, buffer);
              pthread_mutex_unlock(&g_queue_mutex);
#endif
              break;
            }

          LOG_ERROR("USB thread: Failed to send packet: %d", ret);
#endif
          error_count++;

          if (error_count >= 10)
            {
#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
              LOG_ERROR("Too many TCP errors (%lu consecutive), shutting down",
                        (unsigned long)error_count);
#else
              LOG_ERROR("Too many USB errors (%lu consecutive), shutting down",
                        (unsigned long)error_count);
#endif
              pthread_mutex_lock(&g_queue_mutex);
              g_shutdown_requested = true;
              pthread_cond_broadcast(&g_queue_cond);
              pthread_mutex_unlock(&g_queue_mutex);

#if MJPEG_BATCHING_ENABLED
              pthread_mutex_lock(&g_queue_mutex);
              for (i = 0; i < batch.frame_count; i++)
                {
                  frame_queue_push(&g_empty_queue, batch.frames[i]);
                }

              pthread_mutex_unlock(&g_queue_mutex);
#else
              pthread_mutex_lock(&g_queue_mutex);
              frame_queue_push(&g_empty_queue, buffer);
              pthread_mutex_unlock(&g_queue_mutex);
#endif
              break;
            }
        }
      else
        {
          error_count = 0;

#if MJPEG_BATCHING_ENABLED
          /* Update statistics for batching mode */

          packet_count++;
          batch_count++;
          frame_count += batch.frame_count;
          total_bytes += ret;

          g_total_usb_packets++;
          g_total_packet_bytes += ret;

          if (frame_count % stats_interval == 0)
            {
              uint32_t avg_packet_size = packet_count > 0 ? total_bytes / packet_count : 0;
              float avg_batch_size = batch_count > 0 ? (float)frame_count / (float)batch_count : 0.0f;

              LOG_INFO("Batch stats: frames=%lu, batches=%lu, avg_batch_size=%.2f, "
                       "avg_packet=%lu bytes",
                       (unsigned long)frame_count, (unsigned long)batch_count,
                       avg_batch_size,
                       (unsigned long)avg_packet_size);
            }
#else
          /* Update statistics for single-frame mode */

          packet_count++;
          total_bytes += ret;

          g_total_usb_packets++;
          g_total_packet_bytes += ret;

          if (packet_count % stats_interval == 0)
            {
              uint32_t avg_packet_size = total_bytes / packet_count;
              uint32_t throughput_kbps = (total_bytes * 8) / 1000;

              LOG_INFO("USB stats: packets=%lu, avg_size=%lu bytes, throughput~%lu kbps",
                       (unsigned long)packet_count, (unsigned long)avg_packet_size,
                       (unsigned long)throughput_kbps);
            }
#endif
        }

      /* Return buffers to empty queue */

      pthread_mutex_lock(&g_queue_mutex);

#if MJPEG_BATCHING_ENABLED
      for (i = 0; i < batch.frame_count; i++)
        {
          frame_queue_push(&g_empty_queue, batch.frames[i]);
        }
#else
      frame_queue_push(&g_empty_queue, buffer);
#endif

      pthread_cond_signal(&g_queue_cond);
      pthread_mutex_unlock(&g_queue_mutex);

#if MJPEG_BATCHING_ENABLED
      memset(&batch, 0, sizeof(batch));
#endif
    }

#if MJPEG_BATCHING_ENABLED
  if (batch_packet != NULL)
    {
      free(batch_packet);
    }

  LOG_INFO("== USB thread exiting (sent %lu batches, %lu frames, %lu bytes total) ==",
           (unsigned long)batch_count, (unsigned long)frame_count,
           (unsigned long)total_bytes);
#else
  LOG_INFO("== USB thread exiting (sent %lu packets, %lu bytes total) ==",
           (unsigned long)packet_count, (unsigned long)total_bytes);
#endif

  return NULL;
}

/****************************************************************************
 * Name: apply_priority_boost
 *
 * Description:
 *   Apply priority boost based on queue saturation (Phase 10)
 *
 ****************************************************************************/

static void apply_priority_boost(int queue_depth)
{
  static bool usb_boosted = false;
  int ret;

  /* Priority boost logic based on queue saturation */
  if (queue_depth >= 6 && !usb_boosted)
    {
      /* Boost USB thread priority when queue is saturated */
      ret = adjust_thread_priority(g_usb_thread, 105);
      if (ret == 0)
        {
          usb_boosted = true;
          LOG_INFO("USB thread priority boosted to 105 (queue depth: %d)", queue_depth);
        }
    }
  else if (queue_depth <= 2 && usb_boosted)
    {
      /* Reset USB thread priority when queue is light */
      ret = adjust_thread_priority(g_usb_thread, USB_THREAD_PRIORITY);
      if (ret == 0)
        {
          usb_boosted = false;
          LOG_INFO("USB thread priority reset to %d (queue depth: %d)",
                   USB_THREAD_PRIORITY, queue_depth);
        }
    }
}

/****************************************************************************
 * Name: detect_control_failure
 *
 * Description:
 *   Detect control system failures (Phase 10)
 *
 ****************************************************************************/

static bool detect_control_failure(void)
{
  static int failure_count = 0;
  static uint64_t last_check_us = 0;
  uint64_t current_time_us = get_timestamp_us();
  bool controller_unstable;
  bool allocation_failed;

  /* Check every 5 seconds to avoid false positives */
  if (current_time_us - last_check_us < 5000000ULL)
    {
      return false;
    }

  last_check_us = current_time_us;

  /* Check controller stability */
  controller_unstable = !fps_controller_is_stable(&g_fps_controller);

  /* Check for memory allocation failures */
  allocation_failed = !frame_queue_is_buffer_pool_healthy();

  /* Count failures */
  if (controller_unstable || allocation_failed)
    {
      failure_count++;
      LOG_WARN("Control failure detected (count: %d) - unstable: %s, alloc_fail: %s",
               failure_count,
               controller_unstable ? "yes" : "no",
               allocation_failed ? "yes" : "no");

      /* Trigger fallback after 3 consecutive failures */
      if (failure_count >= 3)
        {
          failure_count = 0; /* Reset counter */
          return true;
        }
    }
  else
    {
      /* Reset counter on successful operation */
      if (failure_count > 0)
        {
          LOG_INFO("Control system recovered, failure count reset");
        }
      failure_count = 0;
    }

  return false;
}

/****************************************************************************
 * Name: fallback_to_static_config
 *
 * Description:
 *   Fallback to static configuration on control failure (Phase 10)
 *
 ****************************************************************************/

void fallback_to_static_config(void)
{
  LOG_WARN("=== CONTROL SYSTEM FAILURE - ACTIVATING FALLBACK ===");

  /* Disable PID controller */
  fps_controller_enable(&g_fps_controller, false);

  /* Revert to static 30fps */
  camera_set_fps_runtime(30);
  LOG_INFO("FPS reverted to static 30fps");

  /* Reset thread priorities to baseline */
  adjust_thread_priority(g_camera_thread, CAMERA_THREAD_PRIORITY);
  adjust_thread_priority(g_usb_thread, USB_THREAD_PRIORITY);
  LOG_INFO("Thread priorities reset to baseline");

  /* Reset queue depth to default */
  g_current_queue_depth = CONFIG_QUEUE_DEPTH_DEFAULT;
  LOG_INFO("Queue depth reset to default: %d", CONFIG_QUEUE_DEPTH_DEFAULT);

  LOG_WARN("=== FALLBACK COMPLETE - SYSTEM RUNNING IN SAFE MODE ===");
}

/****************************************************************************
 * Name: control_thread_func
 *
 * Description:
 *   PID control thread for dynamic FPS adjustment (Phase 10)
 *
 ****************************************************************************/

void *control_thread_func(void *arg)
{
  thread_context_t *ctx = (thread_context_t *)arg;
  int queue_depth;
  float fps_output;
  int current_fps;

  LOG_INFO("== Control thread started (Phase 10 PID Controller) ==");

  /* Initialize PID controller */
  fps_controller_init(&g_fps_controller,
                     FPS_CONTROLLER_SETPOINT,
                     FPS_CONTROLLER_KP,
                     FPS_CONTROLLER_KI,
                     FPS_CONTROLLER_KD);

  /* Enable controller */
  fps_controller_enable(&g_fps_controller, true);

  /* Control loop - 10Hz (100ms period) */
  static uint32_t control_cycle_count = 0;
  static uint64_t last_metrics_log_us = 0;

  while (!g_shutdown_requested)
    {
      control_cycle_count++;

      /* Check for control failures */
      if (detect_control_failure())
        {
          fallback_to_static_config();
          break; /* Exit control loop, let static config take over */
        }

      /* Get current queue depth */
      pthread_mutex_lock(&g_queue_mutex);
      queue_depth = frame_queue_depth(g_action_queue);
      pthread_mutex_unlock(&g_queue_mutex);

      /* Update PID controller */
      fps_output = fps_controller_update(&g_fps_controller, (float)queue_depth);

      /* Apply FPS adjustment */
      current_fps = (int)(fps_output + 0.5f); /* Round to nearest integer */
      camera_set_fps_runtime(current_fps);

      /* Apply priority boost based on queue saturation */
      apply_priority_boost(queue_depth);

      LOG_DEBUG("Control: queue_depth=%d fps_output=%.1f current_fps=%d",
               queue_depth, fps_output, current_fps);

      /* Detailed control metrics every 5 seconds (50 cycles at 10Hz) */
      uint64_t current_time_us = get_timestamp_us();
      if (current_time_us - last_metrics_log_us >= 5000000ULL) /* 5 seconds */
        {
          control_metrics_t metrics;
          fps_controller_get_metrics(&g_fps_controller, &metrics);

          LOG_INFO("=== Phase 10 Control Metrics ===");
          LOG_INFO("Cycles: %lu, Queue: %.1f, Error: %.2f, FPS: %.1f",
                   (unsigned long)control_cycle_count, metrics.queue_depth,
                   metrics.error, metrics.control_output);
          LOG_INFO("PID Terms - P: %.3f, I: %.3f, Buffer Depth: %d",
                   metrics.proportional_term, metrics.integral_term, g_current_queue_depth);

          last_metrics_log_us = current_time_us;
        }

      /* Wait for next control period (100ms) */
      usleep(FPS_CONTROLLER_PERIOD_US);
    }

  LOG_INFO("== Control thread exiting ==");
  return NULL;
}

/****************************************************************************
 * Name: camera_threads_init
 *
 * Description:
 *   Initialize threading system and create threads
 *
 ****************************************************************************/

int camera_threads_init(thread_context_t *ctx)
{
  int ret;
  pthread_attr_t attr;
  struct sched_param sparam;

  if (ctx == NULL)
    {
      LOG_ERROR("Thread context is NULL");
      return -EINVAL;
    }

  g_thread_ctx = ctx;

  /* Phase 4.1: Initialize metrics start time */

  clock_gettime(CLOCK_MONOTONIC, &g_start_time);
  g_last_metrics_time = g_start_time;

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
  /* Phase 9.2: Initialize TCP health monitor */

  tcp_health_init();
#endif

  /* Initialize frame queue system */

  ret = frame_queue_init();
  if (ret < 0)
    {
      LOG_ERROR("Failed to initialize frame queue: %d", ret);
      return ret;
    }

  /* Initialize configurable queue depth (Phase 10) */
  g_current_queue_depth = CONFIG_QUEUE_DEPTH_DEFAULT;

  /* Allocate buffer pool (Step 2) */

  ret = frame_queue_allocate_buffers(ctx->packet_buffer_size, g_current_queue_depth);
  if (ret < 0)
    {
      LOG_ERROR("Failed to allocate buffer pool: %d", ret);
      frame_queue_cleanup();
      return ret;
    }

  /* Create camera thread with high priority */

  pthread_attr_init(&attr);
  sparam.sched_priority = CAMERA_THREAD_PRIORITY;
  pthread_attr_setschedparam(&attr, &sparam);
  pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

  ret = pthread_create(&g_camera_thread, &attr, camera_thread_func, ctx);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      LOG_ERROR("Failed to create camera thread: %d", ret);
      frame_queue_cleanup();
      return -ret;
    }

  LOG_INFO("Camera thread created (priority %d)", CAMERA_THREAD_PRIORITY);

  /* Create USB thread with lower priority */

  pthread_attr_init(&attr);
  sparam.sched_priority = USB_THREAD_PRIORITY;
  pthread_attr_setschedparam(&attr, &sparam);
  pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

  ret = pthread_create(&g_usb_thread, &attr, usb_thread_func, ctx);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      LOG_ERROR("Failed to create USB thread: %d", ret);

      /* Cleanup camera thread */

      pthread_mutex_lock(&g_queue_mutex);
      g_shutdown_requested = true;
      pthread_cond_broadcast(&g_queue_cond);
      pthread_mutex_unlock(&g_queue_mutex);

      pthread_join(g_camera_thread, NULL);
      frame_queue_cleanup();
      return -ret;
    }

  LOG_INFO("USB thread created (priority %d)", USB_THREAD_PRIORITY);

  /* Phase 10: Create control thread with lowest priority */

  pthread_attr_init(&attr);
  sparam.sched_priority = CONTROL_THREAD_PRIORITY;
  pthread_attr_setschedparam(&attr, &sparam);
  pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

  ret = pthread_create(&g_control_thread, &attr, control_thread_func, ctx);
  pthread_attr_destroy(&attr);

  if (ret != 0)
    {
      LOG_ERROR("Failed to create control thread: %d", ret);

      /* Cleanup existing threads */

      pthread_mutex_lock(&g_queue_mutex);
      g_shutdown_requested = true;
      pthread_cond_broadcast(&g_queue_cond);
      pthread_mutex_unlock(&g_queue_mutex);

      pthread_join(g_usb_thread, NULL);
      pthread_join(g_camera_thread, NULL);
      frame_queue_cleanup();
      return -ret;
    }

  LOG_INFO("Control thread created (priority %d)", CONTROL_THREAD_PRIORITY);
  LOG_INFO("Threading system initialized with Phase 10 PID control");

  return 0;
}

/****************************************************************************
 * Name: adjust_thread_priority
 *
 * Description:
 *   Adjust thread priority at runtime (Phase 10)
 *
 ****************************************************************************/

int adjust_thread_priority(pthread_t thread, int new_priority)
{
  struct sched_param param;
  int ret;

  if (new_priority < 1 || new_priority > 255)
    {
      LOG_ERROR("Invalid priority %d (valid range: 1-255)", new_priority);
      return -EINVAL;
    }

  param.sched_priority = new_priority;
  ret = pthread_setschedparam(thread, SCHED_RR, &param);
  if (ret != 0)
    {
      LOG_ERROR("Failed to set thread priority to %d: %d", new_priority, ret);
      return -ret;
    }

  LOG_DEBUG("Thread priority adjusted to %d", new_priority);
  return 0;
}


/****************************************************************************
 * Name: camera_threads_cleanup
 *
 * Description:
 *   Cleanup threading system
 *
 ****************************************************************************/

void camera_threads_cleanup(void)
{
  int ret;

  LOG_INFO("Cleaning up threading system...");

  /* Step 4: Enhanced shutdown - signal threads to exit */

  pthread_mutex_lock(&g_queue_mutex);
  if (!g_shutdown_requested)
    {
      g_shutdown_requested = true;
      LOG_INFO("Setting shutdown flag for threads");
    }

  pthread_cond_broadcast(&g_queue_cond);  /* Wake all waiting threads */
  pthread_mutex_unlock(&g_queue_mutex);

  /* Give threads a moment to process shutdown signal */

  usleep(50000);  /* 50ms */

  /* Join camera thread */

  LOG_INFO("Waiting for camera thread to exit...");
  ret = pthread_join(g_camera_thread, NULL);
  if (ret != 0)
    {
      LOG_ERROR("Camera thread join failed: %d", ret);
    }
  else
    {
      LOG_INFO("Camera thread joined successfully");
    }

  /* Join USB thread */

  LOG_INFO("Waiting for USB thread to exit...");
  ret = pthread_join(g_usb_thread, NULL);
  if (ret != 0)
    {
      LOG_ERROR("USB thread join failed: %d", ret);
    }
  else
    {
      LOG_INFO("USB thread joined successfully");
    }

  /* Phase 7.1b: Log final metrics to console (safe after thread join)
   *
   * This approach avoids race conditions by logging metrics AFTER threads
   * have joined. Metrics are captured in minicom/serial logs even when
   * TCP connection fails. No TCP sending involved - just console logging.
   */

  if (g_thread_ctx != NULL && g_total_camera_frames > 0)
    {
      uint32_t uptime_ms;
      uint32_t avg_packet_size;
      uint32_t action_q_depth;
      uint32_t tcp_avg_send_us = 0;
      uint32_t tcp_max_send_us = 0;

      uptime_ms = get_uptime_ms();
      avg_packet_size = (g_total_usb_packets > 0)
                      ? (uint32_t)(g_total_packet_bytes / g_total_usb_packets)
                      : 0;

      /* Safe to read queue depth now - threads are stopped */

      action_q_depth = frame_queue_depth(g_action_queue);

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
      tcp_server_get_stats(&tcp_avg_send_us, &tcp_max_send_us);
#endif

      /* Log metrics to console for capture via minicom */

      LOG_INFO("=================================================");
      LOG_INFO("FINAL METRICS (Shutdown/Error Detection)");
      LOG_INFO("=================================================");
      LOG_INFO("Uptime: %lu ms", (unsigned long)uptime_ms);
      LOG_INFO("Camera Frames: %lu", (unsigned long)g_total_camera_frames);
      LOG_INFO("USB/TCP Packets: %lu", (unsigned long)g_total_usb_packets);
      LOG_INFO("Action Queue Depth: %lu", (unsigned long)action_q_depth);
      LOG_INFO("Average Packet Size: %lu bytes", (unsigned long)avg_packet_size);
      LOG_INFO("Total Errors: %lu", (unsigned long)g_total_errors);
#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
      LOG_INFO("TCP Avg Send Time: %lu us", (unsigned long)tcp_avg_send_us);
      LOG_INFO("TCP Max Send Time: %lu us", (unsigned long)tcp_max_send_us);

      /* Phase 9.2: Log health metrics */

      uint32_t health_moving_avg_ms;
      uint8_t health_consecutive_spikes;
      uint32_t health_total_spikes;
      bool health_degradation_alert;

      tcp_health_get_metrics(&health_moving_avg_ms,
                             &health_consecutive_spikes,
                             &health_total_spikes,
                             &health_degradation_alert);

      LOG_INFO("TCP Health Moving Avg: %lu ms", (unsigned long)health_moving_avg_ms);
      LOG_INFO("TCP Health Total Spikes: %lu", (unsigned long)health_total_spikes);
      LOG_INFO("TCP Health Degradation Alert: %s",
               health_degradation_alert ? "YES" : "NO");
#endif
      LOG_INFO("Dropped Frames: %lu", (unsigned long)g_dropped_frames);
      LOG_INFO("Drop Events: %lu", (unsigned long)g_drop_events);
      LOG_INFO("=================================================");
    }

  /* Cleanup frame queue system */

  frame_queue_cleanup();

  g_thread_ctx = NULL;

  LOG_INFO("Threading system cleaned up successfully");
}

#ifdef CONFIG_PHASE11_ENABLE
/****************************************************************************
 * Name: get_frame_statistics
 *
 * Description:
 *   Get current frame statistics for enhanced control system
 *
 ****************************************************************************/

const frame_statistics_t* get_frame_statistics(void)
{
  if (!g_frame_statistics_initialized)
    {
      return NULL;
    }

  return &g_frame_statistics;
}

/****************************************************************************
 * Name: reset_frame_statistics
 *
 * Description:
 *   Reset frame statistics (for testing or on control failure)
 *
 ****************************************************************************/

int reset_frame_statistics(void)
{
  if (!g_frame_statistics_initialized)
    {
      return -1;
    }

  frame_statistics_reset(&g_frame_statistics);
  LOG_INFO("Phase 11: Frame statistics reset");
  return 0;
}
#endif

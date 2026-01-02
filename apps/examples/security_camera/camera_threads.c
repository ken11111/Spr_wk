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
#include "config.h"

/* Phase 7: WiFi/TCP transport support */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
#include "tcp_server.h"
#endif

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
  uint64_t elapsed_ms;

  clock_gettime(CLOCK_MONOTONIC, &now);

  elapsed_ms = (uint64_t)(now.tv_sec - g_start_time.tv_sec) * 1000ULL +
               (uint64_t)(now.tv_nsec - g_start_time.tv_nsec) / 1000000ULL;

  return (uint32_t)elapsed_ms;
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
  int ret;
  frame_buffer_t *buf;

  /* Get current metrics */

  uptime_ms = get_uptime_ms();
  avg_packet_size = (g_total_usb_packets > 0)
                  ? (uint32_t)(g_total_packet_bytes / g_total_usb_packets)
                  : 0;
  action_q_depth = frame_queue_depth(g_action_queue);

  /* Pack metrics into packet */

  ret = mjpeg_pack_metrics(uptime_ms,
                           g_total_camera_frames,
                           g_total_usb_packets,
                           action_q_depth,
                           avg_packet_size,
                           g_total_errors,
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
                   "avg_jpeg=%lu KB, jpeg_errors=%lu (%.2f%%)",
                   (unsigned long)frame_count, action_depth, empty_depth,
                   (unsigned long)avg_jpeg_kb,
                   (unsigned long)jpeg_validation_error_count, jpeg_error_rate);
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
 *   Step 3: Send packets via USB
 *
 ****************************************************************************/

void *usb_thread_func(void *arg)
{
  frame_buffer_t *buffer;
  int ret;
  uint32_t error_count = 0;

  /* Step 5: Performance statistics */

  uint32_t packet_count = 0;
  uint32_t total_bytes = 0;
  uint32_t stats_interval = 30;  /* Log every 30 packets (~1 sec @ 30fps) */

  (void)arg;  /* Unused parameter */

  LOG_INFO("== USB thread started (Step 3: active) ==");
  LOG_INFO("USB thread priority: %d", USB_THREAD_PRIORITY);

  while (!g_shutdown_requested)
    {
      /* Step 1: Pull buffer from action queue (blocking if none available) */

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
          /* Should not happen, but handle gracefully */

          LOG_WARN("USB thread: Failed to pull buffer from action queue");
          usleep(10000);  /* 10ms */
          continue;
        }

      /* Step 2: Send packet via transport (USB or TCP) */

#ifdef CONFIG_EXAMPLES_SECURITY_CAMERA_WIFI
      /* Phase 7: Send via TCP */

      tcp_server_t *tcp_srv = (tcp_server_t *)g_thread_ctx->tcp_server;
      ret = tcp_server_send(tcp_srv, buffer->data, buffer->used);
#else
      /* Send via USB */

      ret = usb_transport_send_bytes((uint8_t *)buffer->data, buffer->used);
#endif

      if (ret < 0)
        {
          /* Step 4: Enhanced USB error detection */

          if (ret == -ENXIO || ret == -EIO || ret == ERR_USB_DISCONNECTED)
            {
              LOG_ERROR("USB thread: USB device disconnected (error %d)", ret);

              /* Immediate shutdown on USB disconnect */

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

          LOG_ERROR("USB thread: Failed to send packet: %d", ret);
          error_count++;

          if (error_count >= 10)
            {
              LOG_ERROR("Too many USB errors (%lu consecutive), shutting down",
                        (unsigned long)error_count);
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
      else
        {
          error_count = 0;  /* Reset error count on success */

          /* Phase 4.1: Update global metrics */

          g_total_usb_packets++;
          g_total_packet_bytes += buffer->used;

          /* Step 5: Collect transmission statistics */

          packet_count++;
          total_bytes += buffer->used;

          if (packet_count % stats_interval == 0)
            {
              uint32_t avg_packet_size = total_bytes / packet_count;
              uint32_t throughput_kbps = (total_bytes * 8) / 1000;  /* Approx kbps */

              LOG_INFO("USB stats: packets=%lu, avg_size=%lu bytes, "
                       "throughput~%lu kbps",
                       (unsigned long)packet_count, (unsigned long)avg_packet_size,
                       (unsigned long)throughput_kbps);
            }
        }

      /* Step 3: Return buffer to empty queue for camera thread to reuse */

      pthread_mutex_lock(&g_queue_mutex);
      frame_queue_push(&g_empty_queue, buffer);
      pthread_cond_signal(&g_queue_cond);  /* Wake camera thread */
      pthread_mutex_unlock(&g_queue_mutex);
    }

  LOG_INFO("== USB thread exiting (sent %lu packets, %lu bytes total) ==",
           (unsigned long)packet_count, (unsigned long)total_bytes);
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

  /* Initialize frame queue system */

  ret = frame_queue_init();
  if (ret < 0)
    {
      LOG_ERROR("Failed to initialize frame queue: %d", ret);
      return ret;
    }

  /* Allocate buffer pool (Step 2) */

  ret = frame_queue_allocate_buffers(ctx->packet_buffer_size, MAX_QUEUE_DEPTH);
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
  LOG_INFO("Threading system initialized (Step 1: stub threads)");

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

  /* Cleanup frame queue system */

  frame_queue_cleanup();

  g_thread_ctx = NULL;

  LOG_INFO("Threading system cleaned up successfully");
}

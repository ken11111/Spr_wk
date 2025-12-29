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

#include "camera_threads.h"
#include "frame_queue.h"
#include "camera_manager.h"
#include "mjpeg_protocol.h"
#include "usb_transport.h"
#include "perf_logger.h"
#include "config.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

pthread_t g_camera_thread;
pthread_t g_usb_thread;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static thread_context_t *g_thread_ctx = NULL;

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

              if (error_count >= 3)
                {
                  LOG_ERROR("Camera thread: Too many errors (%u consecutive), "
                            "shutting down", error_count);
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

      packet_size = mjpeg_pack_frame(frame.buf, frame.size, ctx->sequence,
                                      (uint8_t *)buffer->data, buffer->length);
      if (packet_size < 0)
        {
          LOG_ERROR("Failed to pack frame: %d", packet_size);

          /* Return buffer to empty queue and continue */

          pthread_mutex_lock(&g_queue_mutex);
          frame_queue_push(&g_empty_queue, buffer);
          pthread_mutex_unlock(&g_queue_mutex);
          continue;
        }

      buffer->used = packet_size;

      /* Step 4: Push filled buffer to action queue */

      pthread_mutex_lock(&g_queue_mutex);
      frame_queue_push(&g_action_queue, buffer);
      pthread_cond_signal(&g_queue_cond);  /* Wake USB thread/main loop */
      pthread_mutex_unlock(&g_queue_mutex);

      /* Step 2: Maintain frame rate (30 fps = 33333 us per frame) */

      usleep(33333);  /* ~30 fps */
    }

  LOG_INFO("== Camera thread exiting ==");
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

      /* Step 2: Send packet via USB (outside mutex - blocking I/O) */

      ret = usb_transport_send_bytes((uint8_t *)buffer->data, buffer->used);
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
              LOG_ERROR("Too many USB errors (%u consecutive), shutting down",
                        error_count);
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
        }

      /* Step 3: Return buffer to empty queue for camera thread to reuse */

      pthread_mutex_lock(&g_queue_mutex);
      frame_queue_push(&g_empty_queue, buffer);
      pthread_cond_signal(&g_queue_cond);  /* Wake camera thread */
      pthread_mutex_unlock(&g_queue_mutex);
    }

  LOG_INFO("== USB thread exiting ==");
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

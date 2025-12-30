/****************************************************************************
 * security_camera/camera_app_main.c
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
 *    the distribution.
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
#include <signal.h>
#include <syslog.h>

#include "camera_manager.h"
/* encoder_manager.h removed - using MJPEG from camera */
#include "protocol_handler.h"
#include "usb_transport.h"
#include "mjpeg_protocol.h"
#include "perf_logger.h"
#include "config.h"
#include "camera_threads.h"  /* Step 1: Threading support */
#include "frame_queue.h"     /* Step 1: Frame queue */

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MAX_NAL_UNITS_PER_FRAME  4
#define MAX_PACKETS_PER_NAL      2
#define FRAME_INTERVAL_US        (1000000 / CONFIG_CAMERA_FPS)  /* 33333us for 30fps */

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile bool g_running = true;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: signal_handler
 *
 * Description:
 *   Handle signals for graceful shutdown (Step 4: Enhanced)
 *
 ****************************************************************************/

static void signal_handler(int signo)
{
  if (signo == SIGINT || signo == SIGTERM)
    {
      LOG_INFO("Received signal %d, shutting down...", signo);
      g_running = false;

      /* Step 4: Signal threads to shutdown if threading is enabled */

      pthread_mutex_lock(&g_queue_mutex);
      g_shutdown_requested = true;
      pthread_cond_broadcast(&g_queue_cond);  /* Wake all waiting threads */
      pthread_mutex_unlock(&g_queue_mutex);
    }
}

/* cleanup_nal_units removed - not needed for MJPEG */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: camera_app_main
 *
 * Description:
 *   Security camera application main entry point
 *
 * Architecture Overview (Step 1-5 Complete):
 *
 *   ┌───────────────────────────────────────────────────────────┐
 *   │                    Main Application                       │
 *   │  - Signal handling (SIGINT/SIGTERM)                       │
 *   │  - Thread lifecycle management                            │
 *   │  - Monitor mode (3 sec runtime)                           │
 *   └───────────────────────────────────────────────────────────┘
 *                        │            │
 *              ┌─────────┴───┐    ┌──┴──────────┐
 *              │             │    │             │
 *         ┌────▼────┐   ┌────▼────▼────┐   ┌───▼─────┐
 *         │ Camera  │   │ Frame Queues │   │  USB    │
 *         │ Thread  ├──→│ (depth 3)    │──→│ Thread  │
 *         │ (P:110) │   │ - Action Q   │   │ (P:100) │
 *         └────┬────┘   │ - Empty Q    │   └───┬─────┘
 *              │        └──────┬───────┘       │
 *              └───────────────┴───────────────┘
 *                        (Buffer recycle)
 *
 * Performance:
 *   Baseline:  11.0 fps (sequential)
 *   Target:    12.5-13.2 fps (pipelined, +14-20%)
 *   Minimum:   12.0 fps (acceptance)
 *
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;
  camera_config_t camera_config;
  camera_frame_t frame;  /* Used in sequential mode fallback */
  uint32_t frame_count = 0;
  uint32_t error_count = 0;
  uint32_t sequence = 0;
  uint8_t *packet_buffer;
  int packet_size;
  perf_frame_metrics_t perf_metrics;
  uint64_t last_frame_ts = 0;
  thread_context_t thread_ctx;  /* Step 1: Thread context */
  bool use_threading = true;    /* Step 2: Enable threading */

  LOG_INFO("=================================================");
  LOG_INFO("Security Camera Application Starting (MJPEG)");
  LOG_INFO("=================================================");

  /* Setup signal handlers */

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  /* Initialize camera configuration */

  memset(&camera_config, 0, sizeof(camera_config_t));
  camera_config.width = CONFIG_CAMERA_WIDTH;
  camera_config.height = CONFIG_CAMERA_HEIGHT;
  camera_config.fps = CONFIG_CAMERA_FPS;
  camera_config.format = CONFIG_CAMERA_FORMAT;
  camera_config.hdr_enable = CONFIG_CAMERA_HDR_ENABLE;

  LOG_INFO("Camera config: %dx%d @ %d fps, Format=JPEG, HDR=%d",
           camera_config.width, camera_config.height,
           camera_config.fps, camera_config.hdr_enable);

  /* Initialize camera manager */

  ret = camera_manager_init(&camera_config);
  if (ret < 0)
    {
      LOG_ERROR("Failed to initialize camera manager: %d", ret);
      return ret;
    }

  /* Allocate packet buffer for MJPEG protocol */

  packet_buffer = (uint8_t *)malloc(MJPEG_MAX_PACKET_SIZE);
  if (packet_buffer == NULL)
    {
      LOG_ERROR("Failed to allocate packet buffer");
      camera_manager_cleanup();
      return -ENOMEM;
    }

  LOG_INFO("Packet buffer allocated: %d bytes", MJPEG_MAX_PACKET_SIZE);

  /* Initialize USB transport */

  ret = usb_transport_init();
  if (ret < 0)
    {
      LOG_ERROR("Failed to initialize USB transport: %d", ret);
      free(packet_buffer);
      camera_manager_cleanup();
      return ret;
    }

  LOG_INFO("USB transport initialized (/dev/ttyACM0)");

  /* Initialize performance logger */

  perf_logger_init();

  /* Step 1: Initialize threading (disabled for Step 1) */

  if (use_threading)
    {
      memset(&thread_ctx, 0, sizeof(thread_context_t));
      thread_ctx.packet_buffer = packet_buffer;
      thread_ctx.packet_buffer_size = MJPEG_MAX_PACKET_SIZE;
      thread_ctx.sequence = &sequence;

      ret = camera_threads_init(&thread_ctx);
      if (ret < 0)
        {
          LOG_ERROR("Failed to initialize threading: %d", ret);
          LOG_INFO("Falling back to sequential mode");
          use_threading = false;
        }
      else
        {
          LOG_INFO("Threading initialized (stub threads running)");
        }
    }

  LOG_INFO("Starting main loop (will capture 90 frames for testing)...");
  LOG_INFO("=================================================");

  /* Main loop - Step 3: Monitor mode (threads handle everything) */

  if (use_threading)
    {
      /* Step 3: Fully threaded mode - threads handle capture and USB */

      LOG_INFO("Fully threaded mode: Camera and USB threads active");
      LOG_INFO("Main thread will wait for 3 seconds (90 frames @ 30fps)...");

      /* Run for 3 seconds to capture approximately 90 frames at 30fps */

      int elapsed_ms = 0;
      int target_ms = 3000;  /* 3 seconds */

      while (g_running && elapsed_ms < target_ms)
        {
          usleep(100000);  /* 100ms */
          elapsed_ms += 100;

          /* Step 4: Check for shutdown conditions */

          if (g_shutdown_requested)
            {
              LOG_INFO("Shutdown requested by threads, exiting main loop");
              g_running = false;
              break;
            }

          if (!g_running)
            {
              LOG_INFO("Signal received, exiting main loop");
              break;
            }
        }

      LOG_INFO("Main loop completed after %d ms", elapsed_ms);
      LOG_INFO("Threads processed frames in parallel (camera + USB)");

      /* Step 4: Ensure clean shutdown */

      if (elapsed_ms >= target_ms)
        {
          LOG_INFO("Target duration reached, signaling shutdown");
          pthread_mutex_lock(&g_queue_mutex);
          g_shutdown_requested = true;
          pthread_cond_broadcast(&g_queue_cond);
          pthread_mutex_unlock(&g_queue_mutex);
        }
    }
  else
    {
      /* Sequential mode (fallback) - original implementation */

      while (g_running && frame_count < 90)
        {
          /* Initialize performance metrics for this frame */

          memset(&perf_metrics, 0, sizeof(perf_frame_metrics_t));
          perf_metrics.ts_frame_start = perf_logger_get_timestamp_us();
          perf_metrics.frame_num = frame_count + 1;

          /* Calculate inter-frame interval */

          if (last_frame_ts > 0)
            {
              perf_metrics.interval_us =
                perf_metrics.ts_frame_start - last_frame_ts;
            }

          last_frame_ts = perf_metrics.ts_frame_start;

          /* Get JPEG frame from camera */

          perf_metrics.ts_camera_poll_start = perf_logger_get_timestamp_us();
          ret = camera_get_frame(&frame);
          perf_metrics.ts_camera_dqbuf_end = perf_logger_get_timestamp_us();
          if (ret < 0)
            {
              if (ret == ERR_TIMEOUT)
                {
                  LOG_WARN("Camera frame timeout");
                  continue;
                }

              LOG_ERROR("Failed to get camera frame: %d", ret);
              error_count++;

              if (error_count >= 10)
                {
                  LOG_ERROR("Too many camera errors, exiting");
                  break;
                }

              usleep(100000);  /* 100ms delay */
              continue;
            }

          error_count = 0;  /* Reset error count on success */

          /* We now have a JPEG frame directly from camera */

          frame_count++;

          /* Calculate camera latency */

          perf_metrics.latency_camera_poll =
            perf_metrics.ts_camera_dqbuf_end - perf_metrics.ts_camera_poll_start;
          perf_metrics.latency_camera_dqbuf = perf_metrics.latency_camera_poll;
          perf_metrics.jpeg_size = frame.size;

          /* Pack JPEG frame using MJPEG protocol */

          perf_metrics.ts_pack_start = perf_logger_get_timestamp_us();
          packet_size = mjpeg_pack_frame(frame.buf, frame.size, &sequence,
                                          packet_buffer, MJPEG_MAX_PACKET_SIZE);
          perf_metrics.ts_pack_end = perf_logger_get_timestamp_us();

          if (packet_size < 0)
            {
              LOG_ERROR("Failed to pack frame: %d", packet_size);
              continue;
            }

          perf_metrics.latency_pack =
            perf_metrics.ts_pack_end - perf_metrics.ts_pack_start;
          perf_metrics.packet_size = packet_size;

          /* Send packet via USB CDC */

          perf_metrics.ts_usb_write_start = perf_logger_get_timestamp_us();
          ret = usb_transport_send_bytes(packet_buffer, packet_size);
          perf_metrics.ts_usb_write_end = perf_logger_get_timestamp_us();
          if (ret < 0)
            {
              LOG_ERROR("Failed to send packet via USB: %d", ret);
              error_count++;

              if (error_count >= 10)
                {
                  LOG_ERROR("Too many USB errors, exiting");
                  break;
                }
            }
          else
            {
              error_count = 0;  /* Reset error count on success */
              perf_metrics.usb_written = ret;
            }

          /* Calculate final metrics */

          perf_metrics.latency_usb_write =
            perf_metrics.ts_usb_write_end - perf_metrics.ts_usb_write_start;
          perf_metrics.ts_frame_end = perf_logger_get_timestamp_us();
          perf_metrics.latency_total =
            perf_metrics.ts_frame_end - perf_metrics.ts_frame_start;

          /* Record performance metrics */

          perf_logger_record_frame(&perf_metrics);

          /* Maintain frame rate (30 fps = 33333 us per frame) */

          usleep(FRAME_INTERVAL_US);
        }
    }

  LOG_INFO("=================================================");
  if (use_threading)
    {
      LOG_INFO("Main loop ended (threaded mode: ~90 frames expected @ 30fps)");
    }
  else
    {
      LOG_INFO("Main loop ended, total frames: %lu",
               (unsigned long)frame_count);
    }

  /* Cleanup */

  LOG_INFO("Cleaning up...");

  /* Step 1: Cleanup threading */

  if (use_threading)
    {
      camera_threads_cleanup();
    }

  perf_logger_cleanup();

  if (packet_buffer != NULL)
    {
      free(packet_buffer);
      packet_buffer = NULL;
    }

  usb_transport_cleanup();
  camera_manager_cleanup();

  LOG_INFO("=================================================");
  LOG_INFO("Security Camera Application Stopped");
  LOG_INFO("=================================================");

  return ERR_OK;
}

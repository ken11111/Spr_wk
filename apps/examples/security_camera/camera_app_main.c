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
#include "config.h"

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
 *   Handle signals for graceful shutdown
 *
 ****************************************************************************/

static void signal_handler(int signo)
{
  if (signo == SIGINT || signo == SIGTERM)
    {
      LOG_INFO("Received signal %d, shutting down...", signo);
      g_running = false;
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
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  int ret;
  camera_config_t camera_config;
  camera_frame_t frame;
  packet_t handshake_packet;
  uint32_t frame_count = 0;
  uint32_t error_count = 0;
  uint32_t sequence = 0;
  uint8_t *packet_buffer;
  int packet_size;

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

  LOG_INFO("Starting main loop (will capture 90 frames for testing)...");
  LOG_INFO("=================================================");

  /* Main loop - capture 90 frames (3 seconds at 30fps) for testing */

  while (g_running && frame_count < 90)
    {
      /* Get JPEG frame from camera */

      ret = camera_get_frame(&frame);
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

      /* Pack JPEG frame using MJPEG protocol */

      packet_size = mjpeg_pack_frame(frame.buf, frame.size, &sequence,
                                      packet_buffer, MJPEG_MAX_PACKET_SIZE);

      if (packet_size < 0)
        {
          LOG_ERROR("Failed to pack frame: %d", packet_size);
          continue;
        }

      /* Send packet via USB CDC */

      ret = usb_transport_send_bytes(packet_buffer, packet_size);
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
        }

      if (frame_count == 1 || frame_count % 30 == 0)  /* Log first and every second */
        {
          LOG_INFO("Frame %u: JPEG=%u bytes, Packet=%d bytes, USB sent=%d, Seq=%u",
                   frame_count, frame.size, packet_size, ret, sequence - 1);
        }

      /* Maintain frame rate (30 fps = 33333 us per frame) */

      usleep(FRAME_INTERVAL_US);
    }

  LOG_INFO("=================================================");
  LOG_INFO("Main loop ended, total frames: %u", frame_count);

cleanup:

  /* Cleanup */

  LOG_INFO("Cleaning up...");

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

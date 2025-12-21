/****************************************************************************
 * security_camera/usb_transport.c
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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>

#include "usb_transport.h"
#include "config.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static usb_transport_t g_usb_transport;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: usb_transport_init
 *
 * Description:
 *   Initialize USB transport
 *
 ****************************************************************************/

int usb_transport_init(void)
{
  int retry;

  memset(&g_usb_transport, 0, sizeof(usb_transport_t));

  /* Open USB CDC device */

  LOG_INFO("Opening USB CDC device: %s", CONFIG_USB_DEVICE_PATH);

  for (retry = 0; retry < 10; retry++)
    {
      g_usb_transport.fd = open(CONFIG_USB_DEVICE_PATH, O_WRONLY);
      if (g_usb_transport.fd >= 0)
        {
          break;
        }

      LOG_WARN("Failed to open USB device, retry %d/10: %d", retry + 1, errno);
      sleep(1);
    }

  if (g_usb_transport.fd < 0)
    {
      LOG_ERROR("Failed to open USB CDC device after 10 retries");
      return ERR_USB_OPEN;
    }

  g_usb_transport.connected = true;
  g_usb_transport.bytes_sent = 0;
  g_usb_transport.current_buffer = 0;

  LOG_INFO("USB transport initialized successfully");

  return ERR_OK;
}

/****************************************************************************
 * Name: usb_transport_wait_connection
 *
 * Description:
 *   Wait for USB connection (already connected in init)
 *
 ****************************************************************************/

int usb_transport_wait_connection(void)
{
  if (g_usb_transport.fd < 0)
    {
      LOG_ERROR("USB not initialized");
      return ERR_USB_INIT;
    }

  /* In Spresense, USB CDC is automatically connected when opened */

  LOG_INFO("USB connected");
  return ERR_OK;
}

/****************************************************************************
 * Name: usb_transport_send
 *
 * Description:
 *   Send packet via USB
 *
 ****************************************************************************/

int usb_transport_send(const packet_t *packet)
{
  ssize_t written;
  size_t total_size;
  int retry;

  if (g_usb_transport.fd < 0)
    {
      LOG_ERROR("USB not initialized");
      return ERR_USB_INIT;
    }

  if (packet == NULL)
    {
      LOG_ERROR("Invalid packet");
      return ERR_USB_WRITE;
    }

  /* Calculate total packet size */

  total_size = sizeof(packet_header_t) + packet->header.payload_size;

  /* Send packet with retry */

  for (retry = 0; retry < CONFIG_MAX_RECONNECT_RETRY; retry++)
    {
      written = write(g_usb_transport.fd, packet, total_size);

      if (written == (ssize_t)total_size)
        {
          g_usb_transport.bytes_sent += written;

          LOG_DEBUG("USB sent: %zd bytes (seq=%u, type=0x%02X)",
                    written, packet->header.sequence, packet->header.type);

          return written;
        }
      else if (written < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              /* Temporary error, retry */

              LOG_WARN("USB write would block, retry %d/%d",
                       retry + 1, CONFIG_MAX_RECONNECT_RETRY);
              usleep(10000);  /* 10ms delay */
              continue;
            }
          else
            {
              LOG_ERROR("USB write error: %d", errno);
              return ERR_USB_WRITE;
            }
        }
      else
        {
          /* Partial write */

          LOG_WARN("USB partial write: %zd/%zu bytes", written, total_size);
          return ERR_USB_WRITE;
        }
    }

  LOG_ERROR("USB write failed after %d retries", CONFIG_MAX_RECONNECT_RETRY);
  g_usb_transport.connected = false;

  return ERR_USB_DISCONNECTED;
}

/****************************************************************************
 * Name: usb_transport_send_bytes
 *
 * Description:
 *   Send raw bytes via USB (for MJPEG packets)
 *
 ****************************************************************************/

int usb_transport_send_bytes(const uint8_t *data, size_t size)
{
  ssize_t written;
  ssize_t total_written = 0;
  int retry;

  if (g_usb_transport.fd < 0)
    {
      LOG_ERROR("USB not initialized");
      return ERR_USB_INIT;
    }

  if (data == NULL || size == 0)
    {
      LOG_ERROR("Invalid data or size");
      return ERR_USB_WRITE;
    }

  /* Send data with retry */

  for (retry = 0; retry < CONFIG_MAX_RECONNECT_RETRY; retry++)
    {
      written = write(g_usb_transport.fd, data + total_written,
                      size - total_written);

      if (written > 0)
        {
          total_written += written;
          g_usb_transport.bytes_sent += written;

          if (total_written == (ssize_t)size)
            {
              /* Complete write */

              LOG_DEBUG("USB sent: %zd bytes", total_written);
              return total_written;
            }

          /* Partial write, continue */
        }
      else if (written < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              /* Temporary error, retry */

              LOG_WARN("USB write would block, retry %d/%d",
                       retry + 1, CONFIG_MAX_RECONNECT_RETRY);
              usleep(10000);  /* 10ms delay */
              continue;
            }
          else
            {
              LOG_ERROR("USB write error: %d", errno);
              g_usb_transport.connected = false;
              return ERR_USB_WRITE;
            }
        }
    }

  LOG_ERROR("USB write failed after %d retries (sent %zd/%zu bytes)",
            CONFIG_MAX_RECONNECT_RETRY, total_written, size);
  g_usb_transport.connected = false;

  return ERR_USB_DISCONNECTED;
}

/****************************************************************************
 * Name: usb_transport_is_connected
 *
 * Description:
 *   Check if USB is connected
 *
 ****************************************************************************/

bool usb_transport_is_connected(void)
{
  return g_usb_transport.connected && (g_usb_transport.fd >= 0);
}

/****************************************************************************
 * Name: usb_transport_cleanup
 *
 * Description:
 *   Cleanup USB transport
 *
 ****************************************************************************/

int usb_transport_cleanup(void)
{
  if (g_usb_transport.fd >= 0)
    {
      close(g_usb_transport.fd);
      g_usb_transport.fd = -1;
    }

  g_usb_transport.connected = false;

  LOG_INFO("USB transport cleaned up (total sent: %u bytes)",
           g_usb_transport.bytes_sent);

  return ERR_OK;
}

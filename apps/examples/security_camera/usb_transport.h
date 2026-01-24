/****************************************************************************
 * security_camera/usb_transport.h
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

#ifndef __SECURITY_CAMERA_USB_TRANSPORT_H
#define __SECURITY_CAMERA_USB_TRANSPORT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include "protocol_handler.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define USB_TX_BUFFER_COUNT   4        /* Transmission buffer count */
#define USB_TX_BUFFER_SIZE    8192     /* 8KB - Optimal (confirmed by testing) */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* USB transmission buffer */

typedef struct usb_tx_buffer_s
{
  uint8_t  data[USB_TX_BUFFER_SIZE];
  uint32_t size;
  bool     in_use;
} usb_tx_buffer_t;

/* USB transport structure */

typedef struct usb_transport_s
{
  int fd;                          /* USB CDC device file descriptor */
  usb_tx_buffer_t buffers[USB_TX_BUFFER_COUNT];
  uint32_t current_buffer;         /* Current buffer index */
  uint32_t bytes_sent;             /* Total bytes sent */
  bool     connected;              /* Connection status */
} usb_transport_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Initialize USB transport
 * @return 0: success, <0: error
 */

int usb_transport_init(void);

/**
 * @brief Wait for USB connection
 * @return 0: success, <0: error
 */

int usb_transport_wait_connection(void);

/**
 * @brief Send packet via USB
 * @param packet Packet to send
 * @return Bytes sent, <0: error
 */

int usb_transport_send(const packet_t *packet);

/**
 * @brief Send raw bytes via USB (for MJPEG packets)
 * @param data Pointer to data buffer
 * @param size Size of data in bytes
 * @return Bytes sent, <0: error
 */

int usb_transport_send_bytes(const uint8_t *data, size_t size);

/**
 * @brief Check if USB is connected
 * @return true: connected, false: disconnected
 */

bool usb_transport_is_connected(void);

/**
 * @brief Cleanup USB transport
 * @return 0: success, <0: error
 */

int usb_transport_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_USB_TRANSPORT_H */

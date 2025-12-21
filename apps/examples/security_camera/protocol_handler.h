/****************************************************************************
 * security_camera/protocol_handler.h
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

#ifndef __SECURITY_CAMERA_PROTOCOL_HANDLER_H
#define __SECURITY_CAMERA_PROTOCOL_HANDLER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include "encoder_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PACKET_MAGIC      0x5350  /* 'SP' */
#define PACKET_VERSION    0x01
#define MAX_PAYLOAD_SIZE  4096    /* 4KB */

/* Packet type definitions */

#define PKT_TYPE_HANDSHAKE    0x01  /* Handshake */
#define PKT_TYPE_VIDEO_SPS    0x10  /* H.264 SPS */
#define PKT_TYPE_VIDEO_PPS    0x11  /* H.264 PPS */
#define PKT_TYPE_VIDEO_IDR    0x12  /* H.264 I-frame */
#define PKT_TYPE_VIDEO_SLICE  0x13  /* H.264 P-frame */
#define PKT_TYPE_HEARTBEAT    0x20  /* Heartbeat */
#define PKT_TYPE_ERROR        0xFF  /* Error notification */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* Packet header structure */

typedef struct packet_header_s
{
  uint16_t magic;              /* Magic number (0x5350) */
  uint8_t  version;            /* Protocol version (0x01) */
  uint8_t  type;               /* Packet type */
  uint32_t sequence;           /* Sequence number */
  uint64_t timestamp_us;       /* Timestamp (microseconds) */
  uint32_t payload_size;       /* Payload size */
  uint16_t checksum;           /* Checksum (CRC16) */
} __attribute__((packed)) packet_header_t;

/* Packet structure */

typedef struct packet_s
{
  packet_header_t header;
  uint8_t payload[MAX_PAYLOAD_SIZE];
} packet_t;

/* Handshake payload structure */

typedef struct handshake_payload_s
{
  uint16_t video_width;     /* 1280 */
  uint16_t video_height;    /* 720 */
  uint8_t  fps;             /* 30 */
  uint8_t  codec;           /* 0x01 = H.264 */
  uint32_t bitrate;         /* 2000000 */
} __attribute__((packed)) handshake_payload_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Pack NAL Unit into packet(s)
 *        If NAL Unit is large, it will be fragmented
 * @param nal NAL Unit
 * @param packets Output packet array
 * @param max_packets Maximum packet count
 * @return Number of packets created, <0: error
 */

int protocol_pack_nal_unit(const h264_nal_unit_t *nal,
                           packet_t *packets,
                           int max_packets);

/**
 * @brief Send handshake packet
 * @param packet Output handshake packet
 * @return 0: success, <0: error
 */

int protocol_create_handshake(packet_t *packet);

/**
 * @brief Calculate CRC16 checksum
 * @param data Data buffer
 * @param len Data length
 * @return CRC16 checksum
 */

uint16_t protocol_crc16(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SECURITY_CAMERA_PROTOCOL_HANDLER_H */

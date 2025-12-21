/****************************************************************************
 * security_camera/protocol_handler.c
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
#include <syslog.h>

#include "protocol_handler.h"
#include "config.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t g_sequence_number = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_packet_type_from_nal
 *
 * Description:
 *   Get packet type from NAL unit type
 *
 ****************************************************************************/

static uint8_t get_packet_type_from_nal(uint8_t nal_type)
{
  switch (nal_type)
    {
      case NAL_TYPE_SPS:
        return PKT_TYPE_VIDEO_SPS;
      case NAL_TYPE_PPS:
        return PKT_TYPE_VIDEO_PPS;
      case NAL_TYPE_IDR:
        return PKT_TYPE_VIDEO_IDR;
      case NAL_TYPE_SLICE:
        return PKT_TYPE_VIDEO_SLICE;
      default:
        return PKT_TYPE_VIDEO_SLICE;  /* Default to slice */
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: protocol_crc16
 *
 * Description:
 *   Calculate CRC16 checksum (CRC-16-IBM-SDLC)
 *
 ****************************************************************************/

uint16_t protocol_crc16(const uint8_t *data, uint32_t len)
{
  uint16_t crc = 0xFFFF;
  uint32_t i;
  uint8_t j;

  for (i = 0; i < len; i++)
    {
      crc ^= (uint16_t)data[i];

      for (j = 0; j < 8; j++)
        {
          if (crc & 0x0001)
            {
              crc = (crc >> 1) ^ 0x8408;  /* Polynomial: 0x8408 (reversed 0x1021) */
            }
          else
            {
              crc = crc >> 1;
            }
        }
    }

  return crc ^ 0xFFFF;
}

/****************************************************************************
 * Name: protocol_pack_nal_unit
 *
 * Description:
 *   Pack NAL Unit into packet(s)
 *
 ****************************************************************************/

int protocol_pack_nal_unit(const h264_nal_unit_t *nal,
                           packet_t *packets,
                           int max_packets)
{
  uint32_t remaining_size;
  uint32_t offset;
  int packet_count = 0;
  uint8_t packet_type;

  if (nal == NULL || packets == NULL || max_packets < 1)
    {
      LOG_ERROR("Invalid parameters");
      return ERR_PROTOCOL_INVALID;
    }

  packet_type = get_packet_type_from_nal(nal->type);
  remaining_size = nal->size;
  offset = 0;

  /* Pack NAL unit into packets */

  while (remaining_size > 0 && packet_count < max_packets)
    {
      uint32_t payload_size = remaining_size;
      if (payload_size > MAX_PAYLOAD_SIZE)
        {
          payload_size = MAX_PAYLOAD_SIZE;
        }

      /* Fill packet header */

      packets[packet_count].header.magic = PACKET_MAGIC;
      packets[packet_count].header.version = PACKET_VERSION;
      packets[packet_count].header.type = packet_type;
      packets[packet_count].header.sequence = g_sequence_number++;
      packets[packet_count].header.timestamp_us = nal->timestamp_us;
      packets[packet_count].header.payload_size = payload_size;

      /* Copy payload */

      memcpy(packets[packet_count].payload, nal->data + offset, payload_size);

      /* Calculate checksum */

      packets[packet_count].header.checksum =
        protocol_crc16(packets[packet_count].payload, payload_size);

      offset += payload_size;
      remaining_size -= payload_size;
      packet_count++;

      LOG_DEBUG("Packed packet: seq=%u, type=0x%02X, size=%u",
                packets[packet_count-1].header.sequence,
                packet_type, payload_size);
    }

  if (remaining_size > 0)
    {
      LOG_WARN("NAL unit too large, truncated: %u bytes remaining",
               remaining_size);
    }

  return packet_count;
}

/****************************************************************************
 * Name: protocol_create_handshake
 *
 * Description:
 *   Create handshake packet
 *
 ****************************************************************************/

int protocol_create_handshake(packet_t *packet)
{
  handshake_payload_t handshake;

  if (packet == NULL)
    {
      LOG_ERROR("Invalid packet pointer");
      return ERR_PROTOCOL_INVALID;
    }

  /* Fill handshake payload */

  handshake.video_width = CONFIG_CAMERA_WIDTH;
  handshake.video_height = CONFIG_CAMERA_HEIGHT;
  handshake.fps = CONFIG_CAMERA_FPS;
  handshake.codec = 0x01;  /* H.264 */
  handshake.bitrate = CONFIG_ENCODER_BITRATE;

  /* Fill packet header */

  packet->header.magic = PACKET_MAGIC;
  packet->header.version = PACKET_VERSION;
  packet->header.type = PKT_TYPE_HANDSHAKE;
  packet->header.sequence = g_sequence_number++;
  packet->header.timestamp_us = 0;
  packet->header.payload_size = sizeof(handshake_payload_t);

  /* Copy payload */

  memcpy(packet->payload, &handshake, sizeof(handshake_payload_t));

  /* Calculate checksum */

  packet->header.checksum =
    protocol_crc16(packet->payload, sizeof(handshake_payload_t));

  LOG_INFO("Created handshake packet: %dx%d @ %d fps, %d bps",
           handshake.video_width, handshake.video_height,
           handshake.fps, handshake.bitrate);

  return ERR_OK;
}

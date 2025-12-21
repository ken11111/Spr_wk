/****************************************************************************
 * security_camera/mjpeg_protocol.c
 *
 * MJPEG Streaming Protocol Implementation
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "mjpeg_protocol.h"
#include "config.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mjpeg_crc16_ccitt
 *
 * Description:
 *   Calculate CRC-16-CCITT checksum (polynomial 0x1021, init 0xFFFF)
 *
 ****************************************************************************/

uint16_t mjpeg_crc16_ccitt(const uint8_t *data, size_t len)
{
  uint16_t crc = 0xFFFF;
  size_t i;
  int j;

  for (i = 0; i < len; i++)
    {
      crc ^= (uint16_t)data[i] << 8;

      for (j = 0; j < 8; j++)
        {
          if (crc & 0x8000)
            {
              crc = (crc << 1) ^ 0x1021;
            }
          else
            {
              crc = crc << 1;
            }
        }
    }

  return crc;
}

/****************************************************************************
 * Name: mjpeg_pack_frame
 *
 * Description:
 *   Pack JPEG frame into MJPEG protocol packet
 *
 ****************************************************************************/

int mjpeg_pack_frame(const uint8_t *jpeg_data,
                     uint32_t jpeg_size,
                     uint32_t *sequence,
                     uint8_t *packet,
                     size_t packet_max_size)
{
  mjpeg_packet_t *pkt;
  uint16_t crc;
  size_t total_size;

  /* Validate inputs */

  if (jpeg_data == NULL || sequence == NULL || packet == NULL)
    {
      LOG_ERROR("Invalid parameters");
      return -EINVAL;
    }

  if (jpeg_size == 0 || jpeg_size > MJPEG_MAX_JPEG_SIZE)
    {
      LOG_ERROR("Invalid JPEG size: %u", jpeg_size);
      return -EINVAL;
    }

  /* Calculate total packet size */

  total_size = MJPEG_HEADER_SIZE + jpeg_size + MJPEG_CRC_SIZE;

  if (total_size > packet_max_size)
    {
      LOG_ERROR("Packet buffer too small: need %zu, have %zu",
                total_size, packet_max_size);
      return -ENOMEM;
    }

  /* Build packet header */

  pkt = (mjpeg_packet_t *)packet;
  pkt->header.sync_word = MJPEG_SYNC_WORD;
  pkt->header.sequence = *sequence;
  pkt->header.size = jpeg_size;

  /* Copy JPEG data */

  memcpy(pkt->data, jpeg_data, jpeg_size);

  /* Calculate CRC over header + JPEG data */

  crc = mjpeg_crc16_ccitt(packet, MJPEG_HEADER_SIZE + jpeg_size);

  /* Append CRC */

  memcpy(pkt->data + jpeg_size, &crc, MJPEG_CRC_SIZE);

  /* Increment sequence number */

  (*sequence)++;

  LOG_DEBUG("Packed frame: seq=%u, size=%u, crc=0x%04X, total=%zu",
            pkt->header.sequence, jpeg_size, crc, total_size);

  return total_size;
}

/****************************************************************************
 * Name: mjpeg_validate_header
 *
 * Description:
 *   Validate MJPEG packet header
 *
 ****************************************************************************/

int mjpeg_validate_header(const mjpeg_header_t *header)
{
  if (header == NULL)
    {
      return -EINVAL;
    }

  /* Check sync word */

  if (header->sync_word != MJPEG_SYNC_WORD)
    {
      LOG_ERROR("Invalid sync word: 0x%08X (expected 0x%08X)",
                header->sync_word, MJPEG_SYNC_WORD);
      return -EBADMSG;
    }

  /* Check JPEG size */

  if (header->size == 0 || header->size > MJPEG_MAX_JPEG_SIZE)
    {
      LOG_ERROR("Invalid JPEG size: %u", header->size);
      return -EINVAL;
    }

  return 0;
}

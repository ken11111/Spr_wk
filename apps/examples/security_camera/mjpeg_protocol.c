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
 * Name: mjpeg_validate_jpeg_data
 *
 * Description:
 *   Validate JPEG data format (SOI/EOI markers) and find actual JPEG size
 *   by searching for EOI marker, removing trailing padding.
 *
 * Input Parameters:
 *   jpeg_data      - Pointer to JPEG data
 *   jpeg_size      - Size of JPEG data buffer (including padding)
 *   actual_size    - Output: actual JPEG size (up to EOI marker)
 *
 * Returned Value:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

static int mjpeg_validate_jpeg_data(const uint8_t *jpeg_data,
                                     uint32_t jpeg_size,
                                     uint32_t *actual_size)
{
  int32_t i;
  uint32_t eoi_pos = 0;

  /* Size validation */

  if (jpeg_size < 4 || jpeg_size > MJPEG_MAX_JPEG_SIZE)
    {
      LOG_ERROR("Invalid JPEG size: %lu bytes",
                (unsigned long)jpeg_size);
      return -EINVAL;
    }

  /* SOI marker validation (0xFF 0xD8) */

  if (jpeg_data[0] != 0xFF || jpeg_data[1] != 0xD8)
    {
      LOG_ERROR("Missing JPEG SOI marker: [0]=%02X [1]=%02X (expected FF D8)",
                jpeg_data[0], jpeg_data[1]);
      return -EBADMSG;
    }

  /* Phase 4.1.1: Search for EOI marker (0xFF 0xD9) from the end
   * to handle ISX012 camera padding */

  for (i = (int32_t)jpeg_size - 2; i >= 0; i--)
    {
      if (jpeg_data[i] == 0xFF && jpeg_data[i + 1] == 0xD9)
        {
          eoi_pos = i + 2;  /* Position after EOI marker */
          break;
        }
    }

  if (eoi_pos == 0)
    {
      LOG_ERROR("Missing JPEG EOI marker: searched %lu bytes, not found",
                (unsigned long)jpeg_size);
      LOG_ERROR("JPEG header: %02X %02X %02X %02X",
                jpeg_data[0], jpeg_data[1], jpeg_data[2], jpeg_data[3]);
      LOG_ERROR("JPEG footer: %02X %02X %02X %02X",
                jpeg_data[jpeg_size - 4], jpeg_data[jpeg_size - 3],
                jpeg_data[jpeg_size - 2], jpeg_data[jpeg_size - 1]);
      return -EBADMSG;
    }

  /* Set actual JPEG size (excluding padding) */

  *actual_size = eoi_pos;

  /* Log padding removal if detected */

  if (eoi_pos < jpeg_size)
    {
      uint32_t padding_bytes = jpeg_size - eoi_pos;
      LOG_DEBUG("JPEG padding removed: %lu bytes (size: %lu -> %lu)",
                (unsigned long)padding_bytes,
                (unsigned long)jpeg_size,
                (unsigned long)eoi_pos);
    }

  /* Validation successful */

  return 0;
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
  int ret;

  /* Validate inputs */

  if (jpeg_data == NULL || sequence == NULL || packet == NULL)
    {
      LOG_ERROR("Invalid parameters");
      return -EINVAL;
    }

  if (jpeg_size == 0 || jpeg_size > MJPEG_MAX_JPEG_SIZE)
    {
      LOG_ERROR("Invalid JPEG size: %lu", (unsigned long)jpeg_size);
      return -EINVAL;
    }

  /* ============================================
   * Phase 4.1.1: JPEG format validation and padding removal
   * ============================================ */

  uint32_t actual_jpeg_size = jpeg_size;

  ret = mjpeg_validate_jpeg_data(jpeg_data, jpeg_size, &actual_jpeg_size);
  if (ret < 0)
    {
      LOG_ERROR("JPEG validation failed (seq=%lu, size=%lu)",
                (unsigned long)*sequence, (unsigned long)jpeg_size);
      return ret;  /* Return error to caller */
    }

  /* Use actual JPEG size (padding removed) */

  jpeg_size = actual_jpeg_size;

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

  LOG_DEBUG("Packed frame: seq=%lu, size=%lu, crc=0x%04X, total=%zu",
            (unsigned long)pkt->header.sequence, (unsigned long)jpeg_size,
            crc, total_size);

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
      LOG_ERROR("Invalid sync word: 0x%08lX (expected 0x%08lX)",
                (unsigned long)header->sync_word,
                (unsigned long)MJPEG_SYNC_WORD);
      return -EBADMSG;
    }

  /* Check JPEG size */

  if (header->size == 0 || header->size > MJPEG_MAX_JPEG_SIZE)
    {
      LOG_ERROR("Invalid JPEG size: %lu", (unsigned long)header->size);
      return -EINVAL;
    }

  return 0;
}

/****************************************************************************
 * Name: mjpeg_pack_metrics
 *
 * Description:
 *   Pack metrics data into metrics protocol packet (Phase 4.1 extension)
 *
 ****************************************************************************/

int mjpeg_pack_metrics(uint32_t timestamp_ms,
                       uint32_t camera_frames,
                       uint32_t usb_packets,
                       uint32_t action_q_depth,
                       uint32_t avg_packet_size,
                       uint32_t errors,
                       uint32_t tcp_avg_send_us,
                       uint32_t tcp_max_send_us,
                       uint32_t dropped_frames,
                       uint32_t drop_events,
                       uint32_t tcp_health_moving_avg,
                       uint32_t tcp_health_total_spikes,
                       uint32_t *sequence,
                       uint8_t *packet)
{
  metrics_packet_t *metrics;
  uint16_t crc;

  /* Validate inputs */

  if (sequence == NULL || packet == NULL)
    {
      LOG_ERROR("Invalid parameters for metrics packet");
      return -EINVAL;
    }

  /* Build metrics packet */

  metrics = (metrics_packet_t *)packet;
  metrics->sync_word = METRICS_SYNC_WORD;
  metrics->sequence = *sequence;
  metrics->timestamp_ms = timestamp_ms;
  metrics->camera_frames = camera_frames;
  metrics->usb_packets = usb_packets;
  metrics->action_q_depth = action_q_depth;
  metrics->avg_packet_size = avg_packet_size;
  metrics->errors = errors;
  metrics->tcp_avg_send_us = tcp_avg_send_us;
  metrics->tcp_max_send_us = tcp_max_send_us;
  metrics->dropped_frames = dropped_frames;                   /* Phase 7.3.3 */
  metrics->drop_events = drop_events;                         /* Phase 7.3.3 */
  metrics->tcp_health_moving_avg_ms = tcp_health_moving_avg;  /* Phase 9.2 */
  metrics->tcp_health_total_spikes = tcp_health_total_spikes; /* Phase 9.2 */

  /* Calculate CRC over all fields except crc16 itself (56 bytes) */

  crc = mjpeg_crc16_ccitt(packet, METRICS_PACKET_SIZE - sizeof(uint16_t));
  metrics->crc16 = crc;

  /* Increment sequence number */

  (*sequence)++;

  LOG_DEBUG("Packed metrics: seq=%lu, cam_frames=%lu, usb_pkts=%lu, "
            "q_depth=%lu, avg_size=%lu, errors=%lu, dropped=%lu, drop_events=%lu, "
            "health_avg=%lu ms, health_spikes=%lu, crc=0x%04X",
            (unsigned long)metrics->sequence,
            (unsigned long)camera_frames,
            (unsigned long)usb_packets,
            (unsigned long)action_q_depth,
            (unsigned long)avg_packet_size,
            (unsigned long)errors,
            (unsigned long)dropped_frames,
            (unsigned long)drop_events,
            (unsigned long)tcp_health_moving_avg,
            (unsigned long)tcp_health_total_spikes,
            crc);

  return METRICS_PACKET_SIZE;
}

/****************************************************************************
 * Name: mjpeg_pack_batch
 *
 * Description:
 *   Pack multiple JPEG frames into a batch packet (Phase 7.2a)
 *
 ****************************************************************************/

int mjpeg_pack_batch(const uint8_t **frames,
                     const uint32_t *frame_sizes,
                     const uint32_t *frame_sequences,
                     uint32_t frame_count,
                     uint32_t *batch_sequence,
                     uint8_t *packet,
                     size_t packet_max_size)
{
  mjpeg_batch_header_t *header;
  mjpeg_frame_meta_t *meta;
  uint8_t *ptr;
  uint32_t total_size = 0;
  uint32_t i;
  uint16_t crc;
  size_t required_size;

  /* Validate input parameters */

  if (frames == NULL || frame_sizes == NULL || frame_sequences == NULL ||
      frame_count == 0 || frame_count > MJPEG_BATCH_SIZE ||
      batch_sequence == NULL || packet == NULL)
    {
      return -EINVAL;
    }

  /* Calculate total JPEG data size */

  for (i = 0; i < frame_count; i++)
    {
      if (frames[i] == NULL || frame_sizes[i] == 0 || frame_sizes[i] > MJPEG_MAX_JPEG_SIZE)
        {
          LOG_ERROR("Invalid frame %lu: size=%lu", (unsigned long)i, (unsigned long)frame_sizes[i]);
          return -EINVAL;
        }
      total_size += frame_sizes[i];
    }

  /* Calculate required packet size */

  required_size = MJPEG_BATCH_HEADER_SIZE +
                  (MJPEG_FRAME_META_SIZE + 0) * frame_count +  /* metadata */
                  total_size +                                  /* JPEG data */
                  MJPEG_CRC_SIZE;

  /* Add actual frame sizes to required_size */

  for (i = 0; i < frame_count; i++)
    {
      required_size += frame_sizes[i];
    }

  /* Recalculate properly */

  required_size = MJPEG_BATCH_HEADER_SIZE +
                  MJPEG_FRAME_META_SIZE * frame_count +
                  total_size +
                  MJPEG_CRC_SIZE;

  if (required_size > packet_max_size)
    {
      LOG_ERROR("Batch packet too large: required=%lu, max=%lu",
                (unsigned long)required_size, (unsigned long)packet_max_size);
      return -ENOMEM;
    }

  /* Fill batch header */

  header = (mjpeg_batch_header_t *)packet;
  header->sync_word = MJPEG_BATCH_SYNC_WORD;
  header->batch_sequence = *batch_sequence;
  header->frame_count = frame_count;
  header->total_size = total_size;

  ptr = packet + MJPEG_BATCH_HEADER_SIZE;

  /* Pack each frame */

  for (i = 0; i < frame_count; i++)
    {
      /* Frame metadata */

      meta = (mjpeg_frame_meta_t *)ptr;
      meta->frame_sequence = frame_sequences[i];
      meta->frame_size = frame_sizes[i];
      ptr += MJPEG_FRAME_META_SIZE;

      /* JPEG data */

      memcpy(ptr, frames[i], frame_sizes[i]);
      ptr += frame_sizes[i];
    }

  /* Calculate and append CRC */

  crc = mjpeg_crc16_ccitt(packet, ptr - packet);
  memcpy(ptr, &crc, sizeof(crc));
  ptr += sizeof(crc);

  /* Increment batch sequence */

  (*batch_sequence)++;

  LOG_DEBUG("Packed batch: seq=%lu, frames=%lu, total_size=%lu, "
            "packet_size=%ld, crc=0x%04X",
            (unsigned long)header->batch_sequence,
            (unsigned long)frame_count,
            (unsigned long)total_size,
            (long)(ptr - packet),
            crc);

  return ptr - packet;  /* Total packet size */
}

/****************************************************************************
 * security_camera/mjpeg_protocol.h
 *
 * MJPEG Streaming Protocol Definition
 *
 * Protocol Format:
 *   [SYNC_WORD:4] [SEQUENCE:4] [SIZE:4] [JPEG_DATA:N] [CRC16:2]
 *
 ****************************************************************************/

#ifndef __APPS_EXAMPLES_SECURITY_CAMERA_MJPEG_PROTOCOL_H
#define __APPS_EXAMPLES_SECURITY_CAMERA_MJPEG_PROTOCOL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>
#include <stddef.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Protocol constants */

#define MJPEG_SYNC_WORD          0xCAFEBABE
#define MJPEG_HEADER_SIZE        12           /* sync + seq + size */
#define MJPEG_CRC_SIZE           2
#define MJPEG_OVERHEAD_SIZE      (MJPEG_HEADER_SIZE + MJPEG_CRC_SIZE)
#define MJPEG_MAX_JPEG_SIZE      61440        /* 60 KB (Phase 7.2: memory optimization, 18% safety margin over 52KB observed max) */
#define MJPEG_MAX_PACKET_SIZE    (MJPEG_HEADER_SIZE + MJPEG_MAX_JPEG_SIZE + MJPEG_CRC_SIZE)

/* Metrics packet constants (Phase 4.1 extension) */

#define METRICS_SYNC_WORD        0xCAFEBEEF
#define METRICS_PACKET_SIZE      58           /* Total size including CRC (Phase 9.2: +8 bytes for health metrics) */

/* Phase 7.2a: Multi-frame batching constants */

#define MJPEG_BATCH_SYNC_WORD    0xCAFEBABF  /* Batched frames (末尾0xBF) */
#define MJPEG_BATCHING_ENABLED   0           /* 0: disabled, 1: enabled - Phase 7.2a: Temporarily disabled for debugging */
#define MJPEG_BATCH_SIZE         2           /* Number of frames per batch (1-3) - Phase 7.2a: Reduced to 2 for reliable TCP transmission (<100KB) */
#define MJPEG_BATCH_TIMEOUT_MS   100         /* Timeout for partial batch (ms) */
#define MJPEG_BATCH_HEADER_SIZE  16          /* batch header size */
#define MJPEG_FRAME_META_SIZE    8           /* per-frame metadata size */
#define MJPEG_MAX_BATCH_PACKET   (MJPEG_BATCH_HEADER_SIZE + \
                                  (MJPEG_FRAME_META_SIZE + MJPEG_MAX_JPEG_SIZE) * MJPEG_BATCH_SIZE + \
                                  MJPEG_CRC_SIZE)  /* Max: ~185 KB */

/****************************************************************************
 * Public Types
 ****************************************************************************/

/* MJPEG packet header */

typedef struct mjpeg_header_s
{
  uint32_t sync_word;                         /* Magic number: 0xCAFEBABE */
  uint32_t sequence;                          /* Frame sequence number */
  uint32_t size;                              /* JPEG data size in bytes */
} __attribute__((packed)) mjpeg_header_t;

/* Complete MJPEG packet structure */

typedef struct mjpeg_packet_s
{
  mjpeg_header_t header;                      /* Packet header */
  uint8_t data[];                             /* Flexible array: JPEG data + CRC */
} __attribute__((packed)) mjpeg_packet_t;

/* Metrics packet structure (Phase 4.1 extension, Phase 7 TCP stats, Phase 7.3.3 drop stats, Phase 9.2 health) */

typedef struct metrics_packet_s
{
  uint32_t sync_word;                         /* Magic number: 0xCAFEBEEF */
  uint32_t sequence;                          /* Metrics packet sequence number */
  uint32_t timestamp_ms;                      /* Spresense uptime in milliseconds */
  uint32_t camera_frames;                     /* Total camera frames captured */
  uint32_t usb_packets;                       /* Total USB packets sent */
  uint32_t action_q_depth;                    /* Current action queue depth (0-3) */
  uint32_t avg_packet_size;                   /* Average MJPEG packet size (bytes) */
  uint32_t errors;                            /* Total error count */
  uint32_t tcp_avg_send_us;                   /* Average TCP send time (microseconds, Phase 7) */
  uint32_t tcp_max_send_us;                   /* Maximum TCP send time (microseconds, Phase 7) */
  uint32_t dropped_frames;                    /* Total dropped frames (Phase 7.3.3) */
  uint32_t drop_events;                       /* Number of drop events (Phase 7.3.3) */
  uint32_t tcp_health_moving_avg_ms;          /* TCP health: moving average send time (Phase 9.2) */
  uint32_t tcp_health_total_spikes;           /* TCP health: total spike count (Phase 9.2) */
  uint16_t crc16;                             /* CRC-16-CCITT checksum */
} __attribute__((packed)) metrics_packet_t;

/* Phase 7.2a: Multi-frame batching structures */

/* Batch packet header */

typedef struct mjpeg_batch_header_s
{
  uint32_t sync_word;                         /* Magic number: 0xCAFEBABF */
  uint32_t batch_sequence;                    /* Batch sequence number */
  uint32_t frame_count;                       /* Number of frames in this batch (1-3) */
  uint32_t total_size;                        /* Total size of all frame data (sum of JPEG sizes) */
} __attribute__((packed)) mjpeg_batch_header_t;

/* Frame metadata within batch */

typedef struct mjpeg_frame_meta_s
{
  uint32_t frame_sequence;                    /* Individual frame sequence number */
  uint32_t frame_size;                        /* JPEG data size for this frame */
} __attribute__((packed)) mjpeg_frame_meta_t;

/* Complete batch packet structure */

typedef struct mjpeg_batch_packet_s
{
  mjpeg_batch_header_t header;                /* Batch header */
  uint8_t data[];                             /* Flexible array: [meta1][jpeg1][meta2][jpeg2]...[crc] */
} __attribute__((packed)) mjpeg_batch_packet_t;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Name: mjpeg_crc16_ccitt
 *
 * Description:
 *   Calculate CRC-16-CCITT checksum
 *
 * Parameters:
 *   data - Pointer to data buffer
 *   len  - Length of data in bytes
 *
 * Returns:
 *   16-bit CRC value
 *
 ****************************************************************************/

uint16_t mjpeg_crc16_ccitt(const uint8_t *data, size_t len);

/****************************************************************************
 * Name: mjpeg_pack_frame
 *
 * Description:
 *   Pack JPEG frame into MJPEG protocol packet
 *
 * Parameters:
 *   jpeg_data - Pointer to JPEG image data
 *   jpeg_size - Size of JPEG data in bytes
 *   sequence  - Pointer to sequence number (will be incremented)
 *   packet    - Output buffer for packed packet
 *   packet_max_size - Maximum size of packet buffer
 *
 * Returns:
 *   Total packet size on success, negative errno on failure
 *
 ****************************************************************************/

int mjpeg_pack_frame(const uint8_t *jpeg_data,
                     uint32_t jpeg_size,
                     uint32_t *sequence,
                     uint8_t *packet,
                     size_t packet_max_size);

/****************************************************************************
 * Name: mjpeg_validate_header
 *
 * Description:
 *   Validate MJPEG packet header
 *
 * Parameters:
 *   header - Pointer to header structure
 *
 * Returns:
 *   0 on success, negative errno on failure
 *
 ****************************************************************************/

int mjpeg_validate_header(const mjpeg_header_t *header);

/****************************************************************************
 * Name: mjpeg_pack_metrics
 *
 * Description:
 *   Pack metrics data into metrics protocol packet (Phase 4.1 extension)
 *
 * Parameters:
 *   timestamp_ms            - Spresense uptime in milliseconds
 *   camera_frames           - Total camera frames captured
 *   usb_packets             - Total USB packets sent
 *   action_q_depth          - Current action queue depth (0-3)
 *   avg_packet_size         - Average MJPEG packet size
 *   errors                  - Total error count
 *   tcp_avg_send_us         - Average TCP send time (microseconds, Phase 7)
 *   tcp_max_send_us         - Maximum TCP send time (microseconds, Phase 7)
 *   dropped_frames          - Total dropped frames (Phase 7.3.3)
 *   drop_events             - Number of drop events (Phase 7.3.3)
 *   tcp_health_moving_avg   - TCP health moving average send time (Phase 9.2)
 *   tcp_health_total_spikes - TCP health total spike count (Phase 9.2)
 *   sequence                - Pointer to sequence number (will be incremented)
 *   packet                  - Output buffer for packed packet
 *
 * Returns:
 *   Packet size (METRICS_PACKET_SIZE) on success, negative errno on failure
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
                       uint8_t *packet);

/****************************************************************************
 * Name: mjpeg_pack_batch
 *
 * Description:
 *   Pack multiple JPEG frames into a batch packet (Phase 7.2a)
 *
 * Parameters:
 *   frames          - Array of frame data pointers
 *   frame_sizes     - Array of frame sizes
 *   frame_sequences - Array of frame sequence numbers
 *   frame_count     - Number of frames in batch (1-3)
 *   batch_sequence  - Pointer to batch sequence number (will be incremented)
 *   packet          - Output buffer for packed batch packet
 *   packet_max_size - Maximum size of packet buffer
 *
 * Returns:
 *   Total packet size on success, negative errno on failure
 *
 ****************************************************************************/

int mjpeg_pack_batch(const uint8_t **frames,
                     const uint32_t *frame_sizes,
                     const uint32_t *frame_sequences,
                     uint32_t frame_count,
                     uint32_t *batch_sequence,
                     uint8_t *packet,
                     size_t packet_max_size);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_MJPEG_PROTOCOL_H */

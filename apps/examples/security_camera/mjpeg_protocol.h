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
#define MJPEG_MAX_JPEG_SIZE      98304        /* 96 KB (Phase 1.5: 47% safety margin) */
#define MJPEG_MAX_PACKET_SIZE    (MJPEG_HEADER_SIZE + MJPEG_MAX_JPEG_SIZE + MJPEG_CRC_SIZE)

/* Metrics packet constants (Phase 4.1 extension) */

#define METRICS_SYNC_WORD        0xCAFEBEEF
#define METRICS_PACKET_SIZE      38           /* Total size including CRC */

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

/* Metrics packet structure (Phase 4.1 extension) */

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
  uint32_t reserved;                          /* Reserved for future use (0) */
  uint16_t crc16;                             /* CRC-16-CCITT checksum */
} __attribute__((packed)) metrics_packet_t;

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
 *   timestamp_ms     - Spresense uptime in milliseconds
 *   camera_frames    - Total camera frames captured
 *   usb_packets      - Total USB packets sent
 *   action_q_depth   - Current action queue depth (0-3)
 *   avg_packet_size  - Average MJPEG packet size
 *   errors           - Total error count
 *   sequence         - Pointer to sequence number (will be incremented)
 *   packet           - Output buffer for packed packet
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
                       uint32_t *sequence,
                       uint8_t *packet);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_MJPEG_PROTOCOL_H */

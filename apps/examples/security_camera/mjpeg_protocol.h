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
#define MJPEG_MAX_JPEG_SIZE      131072       /* 128 KB (Phase 1.5: VGA support) */
#define MJPEG_MAX_PACKET_SIZE    (MJPEG_HEADER_SIZE + MJPEG_MAX_JPEG_SIZE + MJPEG_CRC_SIZE)

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

#ifdef __cplusplus
}
#endif

#endif /* __APPS_EXAMPLES_SECURITY_CAMERA_MJPEG_PROTOCOL_H */

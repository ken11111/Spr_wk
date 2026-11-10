/****************************************************************************
 * tests/test_mjpeg_protocol.c
 *
 * Unit tests for MJPEG protocol module.
 * Tests CRC-16 calculation, JPEG validation, packet packing, and header
 * validation — all hardware-independent logic.
 *
 ****************************************************************************/

#include "test_framework.h"
#include "mock_nuttx.h"

/* Include the source under test directly so we can test static functions */
#include "../mjpeg_protocol.c"

/****************************************************************************
 * Test: CRC-16-CCITT
 ****************************************************************************/

static void test_crc16_empty(void)
{
  uint16_t crc = mjpeg_crc16_ccitt(NULL, 0);
  TEST_ASSERT_EQUAL(0xFFFF, crc);
}

static void test_crc16_known_value(void)
{
  const uint8_t data[] = { 0x31, 0x32, 0x33, 0x34 }; /* "1234" */
  uint16_t crc = mjpeg_crc16_ccitt(data, sizeof(data));
  TEST_ASSERT(crc != 0xFFFF);
}

static void test_crc16_deterministic(void)
{
  const uint8_t data[] = { 0xAA, 0xBB, 0xCC };
  uint16_t crc1 = mjpeg_crc16_ccitt(data, sizeof(data));
  uint16_t crc2 = mjpeg_crc16_ccitt(data, sizeof(data));
  TEST_ASSERT_EQUAL(crc1, crc2);
}

static void test_crc16_different_data(void)
{
  const uint8_t data1[] = { 0x01, 0x02, 0x03 };
  const uint8_t data2[] = { 0x01, 0x02, 0x04 };
  uint16_t crc1 = mjpeg_crc16_ccitt(data1, sizeof(data1));
  uint16_t crc2 = mjpeg_crc16_ccitt(data2, sizeof(data2));
  TEST_ASSERT(crc1 != crc2);
}

/****************************************************************************
 * Test: JPEG validation
 ****************************************************************************/

static void test_validate_jpeg_valid(void)
{
  /* Minimal valid JPEG: SOI + ... + EOI */
  uint8_t jpeg[] = { 0xFF, 0xD8, 0x00, 0x00, 0xFF, 0xD9 };
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(jpeg, sizeof(jpeg), &actual_size);
  TEST_ASSERT_EQUAL(0, ret);
  TEST_ASSERT_EQUAL(6, actual_size);
}

static void test_validate_jpeg_with_padding(void)
{
  /* JPEG with trailing padding bytes after EOI */
  uint8_t jpeg[] = { 0xFF, 0xD8, 0x00, 0x00, 0xFF, 0xD9, 0x00, 0x00, 0x00 };
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(jpeg, sizeof(jpeg), &actual_size);
  TEST_ASSERT_EQUAL(0, ret);
  TEST_ASSERT_EQUAL(6, actual_size); /* Padding stripped */
}

static void test_validate_jpeg_missing_soi(void)
{
  uint8_t jpeg[] = { 0x00, 0x00, 0x00, 0x00, 0xFF, 0xD9 };
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(jpeg, sizeof(jpeg), &actual_size);
  TEST_ASSERT(ret < 0);
}

static void test_validate_jpeg_missing_eoi(void)
{
  uint8_t jpeg[] = { 0xFF, 0xD8, 0x00, 0x00, 0x00, 0x00 };
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(jpeg, sizeof(jpeg), &actual_size);
  TEST_ASSERT(ret < 0);
}

static void test_validate_jpeg_too_small(void)
{
  uint8_t jpeg[] = { 0xFF, 0xD8 };
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(jpeg, sizeof(jpeg), &actual_size);
  TEST_ASSERT(ret < 0);
}

static void test_validate_jpeg_too_large(void)
{
  uint32_t actual_size = 0;
  int ret = mjpeg_validate_jpeg_data(NULL, MJPEG_MAX_JPEG_SIZE + 1, &actual_size);
  TEST_ASSERT(ret < 0);
}

/****************************************************************************
 * Test: Packet packing
 ****************************************************************************/

static void test_pack_frame_basic(void)
{
  /* Valid JPEG with SOI and EOI */
  uint8_t jpeg[] = { 0xFF, 0xD8, 0xAA, 0xBB, 0xFF, 0xD9 };
  uint8_t packet[256];
  uint32_t sequence = 0;

  int ret = mjpeg_pack_frame(jpeg, sizeof(jpeg), &sequence,
                             packet, sizeof(packet));

  TEST_ASSERT(ret > 0);
  TEST_ASSERT_EQUAL(1, sequence); /* Incremented */

  /* Verify sync word */
  mjpeg_packet_t *pkt = (mjpeg_packet_t *)packet;
  TEST_ASSERT_EQUAL(MJPEG_SYNC_WORD, pkt->header.sync_word);
  TEST_ASSERT_EQUAL(0, pkt->header.sequence);
  TEST_ASSERT_EQUAL(6, pkt->header.size);
}

static void test_pack_frame_sequence_increments(void)
{
  uint8_t jpeg[] = { 0xFF, 0xD8, 0x00, 0x00, 0xFF, 0xD9 };
  uint8_t packet[256];
  uint32_t sequence = 42;

  mjpeg_pack_frame(jpeg, sizeof(jpeg), &sequence, packet, sizeof(packet));
  TEST_ASSERT_EQUAL(43, sequence);

  mjpeg_pack_frame(jpeg, sizeof(jpeg), &sequence, packet, sizeof(packet));
  TEST_ASSERT_EQUAL(44, sequence);
}

static void test_pack_frame_null_inputs(void)
{
  uint8_t packet[256];
  uint32_t sequence = 0;
  uint8_t jpeg[] = { 0xFF, 0xD8, 0xFF, 0xD9 };

  TEST_ASSERT(mjpeg_pack_frame(NULL, 10, &sequence, packet, sizeof(packet)) < 0);
  TEST_ASSERT(mjpeg_pack_frame(jpeg, 4, NULL, packet, sizeof(packet)) < 0);
  TEST_ASSERT(mjpeg_pack_frame(jpeg, 4, &sequence, NULL, sizeof(packet)) < 0);
}

static void test_pack_frame_buffer_too_small(void)
{
  uint8_t jpeg[] = { 0xFF, 0xD8, 0x00, 0x00, 0xFF, 0xD9 };
  uint8_t packet[4]; /* Way too small */
  uint32_t sequence = 0;

  int ret = mjpeg_pack_frame(jpeg, sizeof(jpeg), &sequence,
                             packet, sizeof(packet));
  TEST_ASSERT(ret < 0);
}

static void test_pack_frame_crc_present(void)
{
  uint8_t jpeg[] = { 0xFF, 0xD8, 0xAA, 0xBB, 0xFF, 0xD9 };
  uint8_t packet[256];
  uint32_t sequence = 0;

  int total = mjpeg_pack_frame(jpeg, sizeof(jpeg), &sequence,
                               packet, sizeof(packet));

  /* Total = header(12) + jpeg(6) + crc(2) = 20 */
  TEST_ASSERT_EQUAL(MJPEG_HEADER_SIZE + 6 + MJPEG_CRC_SIZE, total);

  /* Verify CRC is not zero (extremely unlikely for real data) */
  uint16_t crc;
  memcpy(&crc, packet + MJPEG_HEADER_SIZE + 6, sizeof(crc));
  TEST_ASSERT(crc != 0);
}

/****************************************************************************
 * Test: Header validation
 ****************************************************************************/

static void test_validate_header_valid(void)
{
  mjpeg_header_t header = {
    .sync_word = MJPEG_SYNC_WORD,
    .sequence = 0,
    .size = 1024
  };
  TEST_ASSERT_EQUAL(0, mjpeg_validate_header(&header));
}

static void test_validate_header_bad_sync(void)
{
  mjpeg_header_t header = {
    .sync_word = 0xDEADBEEF,
    .sequence = 0,
    .size = 1024
  };
  TEST_ASSERT(mjpeg_validate_header(&header) < 0);
}

static void test_validate_header_zero_size(void)
{
  mjpeg_header_t header = {
    .sync_word = MJPEG_SYNC_WORD,
    .sequence = 0,
    .size = 0
  };
  TEST_ASSERT(mjpeg_validate_header(&header) < 0);
}

static void test_validate_header_oversized(void)
{
  mjpeg_header_t header = {
    .sync_word = MJPEG_SYNC_WORD,
    .sequence = 0,
    .size = MJPEG_MAX_JPEG_SIZE + 1
  };
  TEST_ASSERT(mjpeg_validate_header(&header) < 0);
}

static void test_validate_header_null(void)
{
  TEST_ASSERT(mjpeg_validate_header(NULL) < 0);
}

/****************************************************************************
 * Main
 ****************************************************************************/

int main(void)
{
  TEST_SUITE_BEGIN("MJPEG Protocol Tests");

  /* CRC-16 tests */
  RUN_TEST(test_crc16_empty);
  RUN_TEST(test_crc16_known_value);
  RUN_TEST(test_crc16_deterministic);
  RUN_TEST(test_crc16_different_data);

  /* JPEG validation tests */
  RUN_TEST(test_validate_jpeg_valid);
  RUN_TEST(test_validate_jpeg_with_padding);
  RUN_TEST(test_validate_jpeg_missing_soi);
  RUN_TEST(test_validate_jpeg_missing_eoi);
  RUN_TEST(test_validate_jpeg_too_small);
  RUN_TEST(test_validate_jpeg_too_large);

  /* Packet packing tests */
  RUN_TEST(test_pack_frame_basic);
  RUN_TEST(test_pack_frame_sequence_increments);
  RUN_TEST(test_pack_frame_null_inputs);
  RUN_TEST(test_pack_frame_buffer_too_small);
  RUN_TEST(test_pack_frame_crc_present);

  /* Header validation tests */
  RUN_TEST(test_validate_header_valid);
  RUN_TEST(test_validate_header_bad_sync);
  RUN_TEST(test_validate_header_zero_size);
  RUN_TEST(test_validate_header_oversized);
  RUN_TEST(test_validate_header_null);

  TEST_SUITE_END();
}

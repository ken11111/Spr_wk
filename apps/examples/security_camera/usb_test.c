/****************************************************************************
 * USB CDC Test - Send sync word
 ****************************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

int main(void)
{
  int fd;
  uint32_t sync_word = 0xCAFEBABE;
  uint8_t test_data[16] = {
    0xBE, 0xBA, 0xFE, 0xCA,  /* Sync word (little-endian) */
    0x01, 0x00, 0x00, 0x00,  /* Sequence = 1 */
    0x04, 0x00, 0x00, 0x00,  /* Size = 4 */
    0xAA, 0xBB, 0xCC, 0xDD   /* Test data */
  };

  printf("Opening /dev/ttyACM0...\n");
  fd = open("/dev/ttyACM0", O_WRONLY);

  if (fd < 0)
    {
      printf("Failed to open /dev/ttyACM0\n");
      return -1;
    }

  printf("Sending test data (16 bytes)...\n");
  write(fd, test_data, 16);

  printf("Sending sync word...\n");
  write(fd, &sync_word, 4);
  write(fd, &sync_word, 4);
  write(fd, &sync_word, 4);

  printf("Done!\n");
  close(fd);

  return 0;
}

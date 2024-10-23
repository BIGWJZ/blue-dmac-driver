#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/bdma_c2h_0"
#define MR_SIZE (8192)

enum control_code { BDMA_READ, BDMA_WRITE, BDMA_MR };

struct sw_desc {
  unsigned long addr;
  size_t length;
  unsigned int control;
};

char *test_mr(int fd) {
  int rv = 0;
  struct sw_desc desc = {0, 0, 0};

  char *alloc_mem = malloc(MR_SIZE);
  if (!alloc_mem) {
    perror("Failed to allocate memory");
  }
  printf("Allocate memory @ %p, size %u\n", alloc_mem, MR_SIZE);

  desc.addr = (unsigned long)alloc_mem;
  desc.length = MR_SIZE + 1;
  desc.control = BDMA_MR;
  rv = write(fd, &desc, sizeof(desc));
  if (rv < 0) {
    perror("Write to engine failed");
    free(alloc_mem);
  }

  printf("Memor Register Done!\n");
  return alloc_mem;
}

int test_transfer(int fd, char *addr, size_t size, enum control_code dir) {
  struct sw_desc desc = {0, 0, 0};
  int rv = 0;

  if (dir > 1) {
    printf("Wrong control code!\n");
    return -EINVAL;
  }

  desc.addr = (unsigned long long)addr;
  desc.length = size;
  desc.control = dir;
  rv = write(fd, &desc, sizeof(desc));
  if (rv < 0) {
    perror("Write to engine failed");
  }

  return rv;
}

int main(int argc, char *argv[]) {

  int fd;
  char *mr_ptr, *src_ptr, *dest_ptr;

  int rv = 0;

  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open file");
    return errno;
  }

  mr_ptr = test_mr(fd);
  src_ptr = mr_ptr;
  dest_ptr = src_ptr + (MR_SIZE / 2);

  memset(src_ptr, 'a', MR_SIZE / 2);
  memset(dest_ptr, 'b', MR_SIZE / 2);

  printf("Dma Read @:%p, data:%c \n", src_ptr, *src_ptr);
  rv = test_transfer(fd, src_ptr, 8, BDMA_READ);

  printf("Dma Write @:%p, origin data:%c \n", dest_ptr, *dest_ptr);
  rv = test_transfer(fd, dest_ptr, 8, BDMA_WRITE);

  // Just for testing
  sleep(5);

  printf("After Dma @:%p, data:%c \n", dest_ptr, *dest_ptr);

  close(fd);
  free(mr_ptr);
  return 0;
}

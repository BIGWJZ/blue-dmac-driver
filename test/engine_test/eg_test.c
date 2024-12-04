#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define DEVICE_PATH "/dev/bdma_c2h_0"
#define MR_SIZE (8192)
#define TEST_SIZE (128)

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
    return NULL;
  }
  printf("Allocate memory @ %p, size %u\n", alloc_mem, MR_SIZE);

  desc.addr = (unsigned long)alloc_mem;
  desc.length = MR_SIZE;
  desc.control = BDMA_MR;
  rv = write(fd, &desc, sizeof(desc));
  if (rv < 0) {
    perror("Write to engine failed");
    free(alloc_mem);
    return NULL;
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

double get_time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char *argv[]) {

  int fd;
  char *mr_ptr, *src_ptr, *dest_ptr, *last_ptr;
  struct timespec start, end;
  int rv = 0;
  // Do not trans larger than 512, will be fix in next verison
  size_t trans_len = 1;

  if (trans_len > TEST_SIZE) {
    perror("trans_len should be small than TEST_SIZE");
    return -EINVAL;
  }

  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open file");
    return errno;
  }

  mr_ptr = test_mr(fd);
  if (!mr_ptr) {
    close(fd);
    return -ENOMEM;
  }

  src_ptr = mr_ptr;
  dest_ptr = src_ptr + TEST_SIZE;
  last_ptr = dest_ptr + trans_len - 1;

  memset(src_ptr, 'a', TEST_SIZE);
  memset(dest_ptr, 'b', TEST_SIZE);

  printf("Dma Read [ %p : %p], data:%c \n", src_ptr, src_ptr + trans_len , *src_ptr);
  rv = test_transfer(fd, src_ptr, trans_len, BDMA_READ);

  printf("Dma Write [ %p : %p], origin data:%c \n", dest_ptr, last_ptr + 1, *dest_ptr);
  rv = test_transfer(fd, dest_ptr, trans_len, BDMA_WRITE);

  clock_gettime(CLOCK_MONOTONIC, &start);

  clock_gettime(CLOCK_MONOTONIC, &end);
  // Just for testing, done flag is not implemented now
  while ((*last_ptr != 'a') && get_time_diff(start, end) < 5) {
    clock_gettime(CLOCK_MONOTONIC, &end);
  }

  if (*last_ptr != *src_ptr)
    printf("Test failed!\n");
  else
    printf("Test Pass!\n");

  printf("After Dma: start:%p, data:%c, end:%p, data:%c, run %ld bytes taking %.6fs\n", 
        dest_ptr, *dest_ptr, last_ptr, *last_ptr, trans_len, get_time_diff(start, end));

  close(fd);
  free(mr_ptr);
  return 0;
}

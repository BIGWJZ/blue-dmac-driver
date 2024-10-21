#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_PATH "/dev/bdma_control0"
#define REGISTER_ALIGN (4)

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <data> <offset>\n", argv[0]);
    return 1;
  }
  uint32_t write_data = (uint32_t)strtoul(argv[1], NULL, 0);
  off_t offset = strtol(argv[2], NULL, 0);
  int fd;

  uint32_t read_data = 0;

  fd = open(DEVICE_PATH, O_RDWR);
  if (fd < 0) {
    perror("open");
    return 1;
  }

  if (lseek(fd, offset * REGISTER_ALIGN, SEEK_SET) < 0) {
    perror("lseek");
    close(fd);
    return 1;
  }

  ssize_t bytes_written = write(fd, &write_data, sizeof(write_data));
  if (bytes_written != sizeof(write_data)) {
    perror("write");
    close(fd);
    return 1;
  }
  printf("Written 0x%x to offset 0x%lx\n", write_data, offset);

  // 3. 移动到目标寄存器偏移再次读取
  if (lseek(fd, offset * REGISTER_ALIGN, SEEK_SET) < 0) {
    perror("lseek");
    close(fd);
    return 1;
  }

  // 4. 从寄存器读取数据
  ssize_t bytes_read = read(fd, &read_data, sizeof(read_data));
  if (bytes_read != sizeof(read_data)) {
    perror("read");
    close(fd);
    return 1;
  }
  printf("Read 0x%x from offset 0x%lx\n", read_data, offset);

  // 5. 验证读回的数据是否与写入的数据一致
  if (write_data == read_data) {
    printf("Success: Data read matches data written.\n");
  } else {
    printf("Error: Data mismatch (written: 0x%x, read: 0x%x)\n", write_data,
           read_data);
  }

  // 关闭设备文件
  close(fd);
  return 0;
}

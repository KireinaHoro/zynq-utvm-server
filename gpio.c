#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "gpio.h"

ssize_t try_export(int pin) {
  char path_buf[PATH_MAX];
  snprintf(path_buf, PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);
  int fd = open(path_buf, O_RDWR);
  if (fd < 0 && errno == ENOENT) {
    // try to export the pin
    int export_fd = open("/sys/class/gpio/export", O_WRONLY);
    if (export_fd < 0) {
      perror("open gpio export");
      return -1;
    }
    char export_buf[16];
    int bytes = snprintf(export_buf, sizeof(export_buf), "%d", pin);
    write(export_fd, export_buf, bytes);
    close(export_fd);
    fd = open(path_buf, O_RDWR);
    if (fd < 0) {
      perror("open pin after export");
      return -1;
    }
  } else if (fd < 0) {
    // other error
    perror("try to open pin");
    return -1;
  }
  close(fd);
  return 0;
}

ssize_t write_pin(int pin, int value) {
  static const char s_values_str[] = "01";
  char path_buf[PATH_MAX];
  snprintf(path_buf, PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);
  int fd = open(path_buf, O_RDWR);
  if (fd < 0) {
    perror("try to open pin");
    return -1;
  }
  if (write(fd, &s_values_str[value == 0 ? 0 : 1], 1) != 1) {
    // something happened while trying to write pin
    perror("write pin");
    return -1;
  }
  close(fd);
  return 0;
}

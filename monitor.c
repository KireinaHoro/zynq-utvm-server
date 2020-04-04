#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "daemon.h"
#include "gpio.h"
#include "monitor.h"

/* load monitor software to RISC-V and read initialization magic
 * return monitor fd on success, -1 on failure
 */
int initialize(void *mem) {
  printf("initializing monitor with " MONITOR_FILENAME "\n");
  if (try_export(RESET_GPIO) < 0) {
    return -1;
  }
  int monitor_fd = open(MONITOR_FILENAME, O_RDONLY);
  if (monitor_fd < 0) {
    perror("open " MONITOR_FILENAME);
    return -1;
  }
  // stat data file to get size
  struct stat statbuf;
  if (fstat(monitor_fd, &statbuf)) {
    perror("stat" MONITOR_FILENAME);
    return -1;
  }
  int dat_size = statbuf.st_size;

  uint8_t *dat_ptr = mmap(0, dat_size, PROT_READ, MAP_PRIVATE, monitor_fd, 0);
  if (dat_ptr == MAP_FAILED) {
    perror("mmap data" MONITOR_FILENAME);
    return -1;
  }

  // copy data to start
  memcpy(mem, dat_ptr, dat_size);

  // reset RISC-V over GPIO
  write_pin(RESET_GPIO, 1);
  usleep(100 * 1000);
  write_pin(RESET_GPIO, 0);

  // wait for INIT_MAGIC over tty

  munmap(dat_ptr, dat_size);
  close(monitor_fd);
  return 0;
}

/* instruct monitor to execute at func_addr and return at stop_addr
 * will poll on client_fd to check if client disconnected
 * return 0 on successful finish, -1 on client_fd close
 */
ssize_t execute(uint64_t func_addr, uint64_t stop_addr, int tty_fd,
                int client_fd) {
  return -1;
}

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
#include "serial.h"
#include "socket.h"

#define MAXLINE 128

/* load monitor software to RISC-V and read initialization magic
 * return monitor tty fd on success, -1 on failure
 */
int initialize(void *mem) {
  printf("initializing monitor with " MONITOR_FILENAME "\n");
  if (try_export(RESET_GPIO) < 0) {
    goto fail;
  }
  int monitor_fd = open(MONITOR_FILENAME, O_RDONLY);
  if (monitor_fd < 0) {
    perror("open " MONITOR_FILENAME);
    goto fail;
  }
  // stat data file to get size
  struct stat statbuf;
  if (fstat(monitor_fd, &statbuf)) {
    perror("stat" MONITOR_FILENAME);
    goto fail;
  }
  int dat_size = statbuf.st_size;

  uint8_t *dat_ptr = mmap(0, dat_size, PROT_READ, MAP_PRIVATE, monitor_fd, 0);
  if (dat_ptr == MAP_FAILED) {
    perror("mmap data" MONITOR_FILENAME);
    goto fail;
  }

  // copy data to start
  memcpy(mem, dat_ptr, dat_size);
  munmap(dat_ptr, dat_size);
  close(monitor_fd);

  // initialize tty
  int tty_fd = init_tty("/dev/ttyS1");
  if (tty_fd < 0) {
    goto fail;
  }

  // reset RISC-V over GPIO
  write_pin(RESET_GPIO, 1);
  usleep(100 * 1000);
  write_pin(RESET_GPIO, 0);

  // wait for INIT_MAGIC over tty
  char buf[MAXLINE];
  SOCK_READ(tty_fd, buf, strlen(INIT_MAGIC), fail)
  return tty_fd;
fail:
  printf("failed to initialize monitor\n");
  return -1;
}

/* instruct monitor to execute at func_addr and return at stop_addr
 * will poll on client_fd to check if client disconnected
 * return 0 on successful finish, -1 on client_fd close
 */
ssize_t execute(uint64_t func_addr, uint64_t stop_addr, int tty_fd,
                int client_fd) {
  char buf[MAXLINE];
  memset(buf, 0, MAXLINE);
  snprintf(buf, MAXLINE - 1, "%ld %ld\n", func_addr, stop_addr);
  SOCK_WRITE(tty_fd, buf, strlen(buf), fail)
  SOCK_READ(tty_fd, buf, strlen(EXIT_MAGIC), fail)
  return 0;
fail:
  return -1;
}

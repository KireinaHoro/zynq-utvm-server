#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "daemon.h"
#include "gpio.h"
#include "monitor.h"
#include "serial.h"
#include "socket.h"
#include "util.h"

#define MAXLINE 128

/* load monitor software to RISC-V and read initialization magic
 * return monitor tty fd on success, -1 on failure
 */
int initialize(void *mem) {
  // printf("initializing monitor with " MONITOR_FILENAME "\n");
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

  // put RISC-V into reset - active low
  write_pin(RESET_GPIO, 0);

  // copy data to start
  memcpy(mem, dat_ptr, dat_size);
  munmap(dat_ptr, dat_size);
  close(monitor_fd);

  // initialize tty
  int tty_fd = init_tty(MONITOR_TTY);
  if (tty_fd < 0) {
    goto fail;
  }

  // take RISC-V out of reset
  usleep(100);
  write_pin(RESET_GPIO, 1);

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
 * return RISC-V sepc on successful finish, -1 on client_fd close
 */
ssize_t execute(uint64_t func_addr, uint64_t stop_addr, int tty_fd,
                int client_fd) {
  char buf[MAXLINE], sepc[19];
  sepc[18] = '\0';
  memset(buf, 0, MAXLINE);
  snprintf(buf, MAXLINE - 1, "%ld %ld\n", func_addr, stop_addr);
  SOCK_WRITE(tty_fd, buf, strlen(buf), fail)

  // select between tty_fd and client_fd to handle client close in case
  // we are stuck
  fd_set rfds;
  int nfds = max(tty_fd, client_fd) + 1;
  struct timeval tv;
  int retval;
  FD_ZERO(&rfds);
  FD_SET(client_fd, &rfds);
  FD_SET(tty_fd, &rfds);

  tv.tv_sec = EXECUTE_TIMEOUT;
  tv.tv_usec = 0;

  retval = select(nfds, &rfds, NULL, NULL, &tv);
  if (retval > 0) {
    if (FD_ISSET(tty_fd, &rfds)) {
      // 0xXXXXXXXXXXXXXXXX, 18 chars
      SOCK_READ(tty_fd, sepc, 18, fail)
      uint64_t ret;
      sscanf(sepc, "0x%lx", &ret);
      SOCK_READ(tty_fd, buf, strlen(EXIT_MAGIC), fail)
      return ret;
    }
    // client has closed or timeout has reached, fail the execute operation
  }
  printf("timeout waiting for execute\n");
fail:
  return -1;
}

/* only meaningful when called after execute() (the monitor is in trap()
 * already). flushes the cache in the region of [addr, addr+len) before read out
 * from ARM
 */
void flush(uint64_t addr, uint64_t len, int tty_fd) {
  char buf[MAXLINE];
  snprintf(buf, MAXLINE - 1, "%ld %ld\n", addr, len);
  SOCK_WRITE(tty_fd, buf, strlen(buf), fail)
  SOCK_READ(tty_fd, buf, strlen(FLUSH_MAGIC), fail);
fail:
  return;
}

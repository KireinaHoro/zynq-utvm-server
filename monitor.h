#ifndef _MONITOR_H_
#define _MONITOR_H_

#include <stdint.h>

#define INIT_MAGIC "monitor-initialize\n"
#define EXIT_MAGIC "monitor-exit\n"

#define MONITOR_FILENAME "fw_payload.bin"
#define MONITOR_TTY "/dev/ttyS1"
#define RESET_GPIO 511

/* load monitor software to RISC-V and read initialization magic
 * return monitor fd on success, -1 on failure
 */
int initialize(void *mem);

/* instruct monitor to execute at func_addr and return at stop_addr
 * will poll on client_fd to check if client disconnected
 * return 0 on successful finish, -1 on client_fd close
 */
ssize_t execute(uint64_t func_addr, uint64_t stop_addr, int tty_fd,
                int client_fd);

#endif // _MONITOR_H_

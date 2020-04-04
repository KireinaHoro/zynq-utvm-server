#ifndef _DAEMON_H_
#define _DAEMON_H_

#include <stddef.h>
#include <stdint.h>

#define RISCV_OFFSET 0x800000000L
#define RISCV_SIZE 0x80000000L

ssize_t start(int port);

#endif // _DAEMON_H_

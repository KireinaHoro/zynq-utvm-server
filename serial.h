#ifndef _SERIAL_H_
#define _SERIAL_H_

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// implementation from https://stackoverflow.com/a/6947758/5520728

int set_interface_attribs(int fd, int speed, int parity);

void set_blocking(int fd, int should_block);

int init_tty(const char *tty_file);

#endif // _SERIAL_H_

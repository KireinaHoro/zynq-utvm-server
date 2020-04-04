#ifndef _GPIO_H_
#define _GPIO_H_

#include <stdint.h>

ssize_t try_export(int pin);

ssize_t write_pin(int pin, int value);

#endif // _GPIO_H_

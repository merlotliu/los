#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H

#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

/* init console */
void console_init(void);

/* get console */
void console_acquire(void);

/* release consle */
void console_release(void);

/* terminator print string */
void console_put_str(char* str);

/* terminator print character */
void console_put_char(uint8_t char_asci);

/* terminator print integer */
void console_put_int (uint32_t num);

#endif
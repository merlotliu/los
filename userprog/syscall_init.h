#ifndef __USERPROG_SYSCALL_INIT_H
#define __USERPROG_SYSCALL_INIT_H

#include "stdint.h"
#include "file.h"

/* get process pid */
uint32_t sys_getpid(void);

/* print single character */
int sys_putchar(int c);

/* init system call */
void syscall_init(void);

#endif
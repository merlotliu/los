#ifndef __USERPROG_SYSCALL_INIT_H
#define __USERPROG_SYSCALL_INIT_H

#include "stdint.h"

/* get process pid */
uint32_t sys_getpid(void);

/* init system call */
void syscall_init(void);

#endif
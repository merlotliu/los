#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_WRITE,
};

/* get current process id */
uint32_t getpid(void);

/* write */
uint32_t write(char* str);

#endif /* __LIB_USER_SYSCALL_H */
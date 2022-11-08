#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID = 0
};

/* get current process id */
uint32_t getpid(void);

#endif /* __LIB_USER_SYSCALL_H */
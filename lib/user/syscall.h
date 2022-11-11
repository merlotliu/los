#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE
};

/* get current process id */
uint32_t getpid(void);

/* write */
uint32_t write(char* str);

/* memory allocate */
void *malloc(uint32_t size);

/* free memory that ptr points to */
void free(void* ptr);

#endif /* __LIB_USER_SYSCALL_H */
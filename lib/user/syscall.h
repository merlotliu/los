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

/* write() writes  up  to  count  bytes  from  the buffer pointed buf to the file referred to by the file descriptor fd. */
ssize_t write(int fd, const void* buf, size_t count);

/* memory allocate */
void *malloc(uint32_t size);

/* free memory that ptr points to */
void free(void* ptr);

#endif /* __LIB_USER_SYSCALL_H */
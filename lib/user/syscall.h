#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "global.h"
#include "thread.h"

enum SYSCALL_NR {
    SYS_GETPID = 0,
    SYS_MALLOC,
    SYS_FREE,
    SYS_WRITE,
    SYS_READ,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_FORK
};

/* get current process id */
uint32_t getpid(void);

/* memory allocate */
void *malloc(uint32_t size);

/* free memory that ptr points to */
void free(void* ptr);

/* write() writes  up  to  count  bytes  from  the buffer pointed buf to the file referred to by the file descriptor fd. */
ssize_t write(int fd, const void* buf, size_t count);

/* read() attempts to read up to count bytes from file descriptor fd into the buffer starting at buf. */
ssize_t read(int fd, void* buf, size_t count);

/* print single character */
int putchar(int c);

/* clear screen */
void clear(void);

/* process copy */
pid_t fork(void);

#endif /* __LIB_USER_SYSCALL_H */
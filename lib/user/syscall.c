#include "syscall.h"

#define _syscall0(NUMBER) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER)\
        : "memory"\
    );\
    retval;\
})

#define _syscall1(NUMBER, ARG1) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1)\
        : "memory"\
    );\
    retval;\
})

#define _syscall2(NUMBER, ARG1, ARG2) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2)\
        : "memory"\
    );\
    retval;\
})

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({\
    int retval;\
    asm volatile (\
        "int $0x80"\
        : "=a" (retval)\
        : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)\
        : "memory"\
    );\
    retval;\
})

/* get current process id */
uint32_t getpid(void) {
    return _syscall0(SYS_GETPID);
}

/* write() writes  up  to  count  bytes  from  the buffer pointed buf to the file referred to by the file descriptor fd. */
ssize_t write(int fd, const void* buf, size_t count) {
    return _syscall3(SYS_WRITE, fd, buf, count);
}

/* memory allocate */
void* malloc(uint32_t size) {
    return (void*)_syscall1(SYS_MALLOC, size);
}

/* free memory that ptr points to */
void free(void* ptr) {
    _syscall1(SYS_FREE, ptr);
}

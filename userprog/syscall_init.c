#include "syscall_init.h"
#include "syscall.h"
#include "stdint.h"
#include "thread.h"
#include "print.h"
#include "console.h"
#include "stdio_kernel.h"
#include "fs.h"
#include "fork.h"

#define syscall_nr 32
typedef void* syscall;

syscall syscall_table[syscall_nr];

/* get process pid */
uint32_t sys_getpid(void) {
    return thread_running()->pid;
}

/* print single character */
int sys_putchar(int c) {
    console_put_char(c);
    return 0;
}

/* init system call */
void syscall_init(void) {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_PUTCHAR] = sys_putchar;
    syscall_table[SYS_CLEAR] = cls_screen;
    syscall_table[SYS_FORK] = sys_fork;
    put_str("syscall_init done\n");
}
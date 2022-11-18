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
    syscall_table[SYS_GETCWD] = sys_getcwd;
    // syscall_table[SYS_OPEN];
    // syscall_table[SYS_CLOSE];
    // syscall_table[SYS_LSEEK];
    // syscall_table[SYS_UNLINK];
    syscall_table[SYS_MKDIR] = sys_mkdir;
    // syscall_table[SYS_OPENDIR];
    // syscall_table[SYS_CLOSEDIR];
    syscall_table[SYS_CHDIR] = sys_chdir;
    // syscall_table[SYS_RMDIR];
    // syscall_table[SYS_READDIR];
    // syscall_table[SYS_REWINDDIR];
    // syscall_table[SYS_STAT];
    syscall_table[SYS_PS] = sys_ps;
    put_str("syscall_init done\n");
}
#include "syscall_init.h"
#include "syscall.h"
#include "stdint.h"
#include "thread.h"
#include "print.h"
#include "console.h"

#define syscall_nr 32
typedef void* syscall;

syscall syscall_table[syscall_nr];

/* get process pid */
uint32_t sys_getpid(void) {
    return thread_running()->pid;
}

/* write to memory */
uint32_t sys_write(char* str) {
    console_put_str(str);
    return strlen(str);
}

/* init system call */
void syscall_init(void) {
    put_str("syscall_init start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    put_str("syscall_init done\n");
}
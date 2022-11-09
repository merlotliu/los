#include "print.h"
#include "init.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall_init.h"
#include "stdio.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int prog_a_pid = 0, prog_b_pid = 0;

int main(void) {
    put_str("I am kernel\n");
    init_all();

    /* thread */
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    intr_enable(); /* open interrupt */
    ASSERT(intr_status_get() == INTR_ON);

    console_put_str("    main_pid:0x");
    console_put_int(sys_getpid());
    console_put_str("\n");
    thread_start("k_thread_a", 31, k_thread_a, "    KThrdA");
    thread_start("k_thread_b", 31, k_thread_b, "    KThrdB");

    while(1) {
        // console_put_str("Main ");
    }
    return 0;
}

void k_thread_a(void* arg) {
    char* param = arg;
    console_put_str(param);
    console_put_str(" PID:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
    // console_put_str("   prog_a_pid:0x");
    // console_put_int(prog_a_pid);
    // console_put_char('\n');
    while(1);
}

void k_thread_b(void* arg) {
    char* param = arg;
    console_put_str(param);
    console_put_str(" PID:0x");
    console_put_int(sys_getpid());
    console_put_char('\n');
    // console_put_str("   prog_b_pid:0x");
    // console_put_int(prog_b_pid);
    // console_put_char('\n');
    while(1);
}

void u_prog_a(void) {
    char* name = "UProcA";
    printf("    %s PID:%d%c", name, getpid(), '\n');
    while(1);
}

void u_prog_b(void) {
    char* name = "UProcB";
    printf("    %s PID:%d%c", name, getpid(), '\n');
    while(1);
}
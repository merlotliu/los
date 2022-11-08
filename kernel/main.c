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

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int test_var_a = 0, test_var_b = 0;

int main(void) {
    put_str("I am kernel\n");
    init_all();

    /* thread */
    thread_start("k_thread_a", 31, k_thread_a, " argA");
    thread_start("k_thread_b", 31, k_thread_b, " argB");
    process_execute(u_prog_a, "user_prog_a");
    process_execute(u_prog_b, "user_prog_b");

    intr_enable(); /* open interrupt */
    ASSERT(intr_status_get() == INTR_ON);

    while(1) {
        // console_put_str("Main ");
    }
    return 0;
}

void k_thread_a(void* arg) {
    char* param = arg;
    while(1) {
        console_put_str(param);
        console_put_str(":0x");
        console_put_int(test_var_a);
    }
}

void k_thread_b(void* arg) {
    char* param = arg;
    while(1) {
        console_put_str(param);
        console_put_str(":0x");
        console_put_int(test_var_b);
    }
}

void u_prog_a(void) {
    while(1) {
        test_var_a++;
    }
}

void u_prog_b(void) {
    while(1) {
        test_var_b++;
    }
}
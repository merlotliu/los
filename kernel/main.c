#include "print.h"
#include "init.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"
#include "console.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void) {
    put_str("I am kernel\n");
    init_all();

    /* thread */
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 8, k_thread_b, "argB ");

    intr_enable(); /* open interrupt */
    ASSERT(intr_status_get() == INTR_ON);

    int i = 1000000;
    while(i--) {
        console_put_str("Main ");
    }
    while(1);
    return 0;
}

void k_thread_a(void* arg) {
    char* param = (char*)arg;
    int i = 1000000;
    while(i--) {
        console_put_str(param);
    }
    while(1);
}

void k_thread_b(void* arg) {
    char* param = (char*)arg;
    int i = 1000000;
    while(i--) {
        console_put_str(param);
    }
    while(1);
}
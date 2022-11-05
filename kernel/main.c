#include "print.h"
#include "init.h"
#include "memory.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

void k_thread_a(void*);
void k_thread_b(void*);

int main(void) {
    put_str("I am kernel\n");
    init_all();
    
    /* thread */
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 8, k_thread_b, "argB ");

    intr_enable();
    ASSERT(intr_status_get() == INTR_ON);

    while(1) {
        put_str("Main ");
    }
    return 0;
}

void k_thread_a(void* arg) {
    char* param = (char*)arg;
    while(1) {
        put_str(param);
    }
}

void k_thread_b(void* arg) {
    char* param = (char*)arg;
    while(1) {
        put_str(param);
    }
}
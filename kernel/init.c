#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"

/* init all of content */
void init_all(void) {
    put_str("init_all\n");
    idt_init(); /* init idt */
    timer_init(); /* init PIT */
    mem_init(); /* init memory pool */
    thread_env_init(); /* init thread environment */
}
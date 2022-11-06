#include "init.h"

/* init all of content */
void init_all(void) {
    put_str("init_all\n");
    idt_init(); /* init idt */
    mem_init(); /* init memory pool */
    thread_env_init(); /* init thread environment */
    timer_init(); /* init PIT */
    console_init(); /* init console before open interrupt */
}
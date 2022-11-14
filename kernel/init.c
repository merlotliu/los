#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall_init.h"
#include "ide.h"
#include "fs.h"

/* init all of content */
void init_all(void) {
    put_str("init_all\n");
    idt_init(); /* init idt */
    mem_init(); /* init memory pool */
    thread_env_init(); /* init thread environment */
    timer_init(); /* init PIT */
    console_init(); /* init console before open interrupt */
    keyboard_init(); /* init keyboard input */
    tss_init(); /* init TSS */
    syscall_init(); /* init syscall */
    ide_init(); /* init hd */
    filesys_init(); /* init file system */
}
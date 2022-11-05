#include "timer.h"
#include "io.h"
#include "print.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY      100

#define INPUT_FREQUENCY     1193180
#define COUNTER0_VALUE      INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT       0x40
#define COUNTER1_PORT       0x41
#define COUNTER2_PORT       0x42
#define COUNTER0_NO         0
#define COUNTER1_NO         1
#define COUNTER2_NO         2
#define READ_WRITE_LATCH    3
#define COUNTER_MODE_2      2
#define PIT_BCD_0           0
#define PIT_BCD_1           1
#define PIT_CONTROL_PORT    0x43

/* ticks内核开中断以来总共的嘀嗒数，类似系统时长 */
uint32_t ticks = 0; 

/* write control word to port to set work mode */
static void set_ctl_mode(uint8_t ctl_port, 
                        uint8_t counter_no,
                        uint8_t rwl,
                        uint8_t counter_mode,
                        uint8_t bcd) {
    /* control word :
        [6-7](counter no) 
        [4-5](read write attribute) 
        [1-3](counter work mode) 
        [0](binary-coded decimal) */ 
    uint8_t cw = (counter_no << 6) | (rwl << 4) | (counter_mode << 1) | bcd; 
    outb(ctl_port, cw);
}

/* set counter primary value */
static void set_frequency(uint8_t counter_port, 
                        uint16_t counter_value) {
    outb(counter_port, (uint8_t)counter_value); /* low 8 bit */
    outb(counter_port, (uint8_t)(counter_value >> 8)); /* high 8 bit */
}

/* timer interrupt handler */
static void timer_intr_handler(void) {
    struct task_ctl_blk* cur_thread = thread_running();
    
    /* 判断栈是否溢出 */
    ASSERT(0x19870916 == cur_thread->stack_magic);
    
    cur_thread->elapsed_ticks++;
    ticks++;
    /* 时间片用完则开始新的调度 */
    if(cur_thread->ticks == 0) {
        schedule();
    } else {
        cur_thread->ticks--;
    }
}

void timer_init(void) {
    put_str("timer_init start\n");
    set_ctl_mode(PIT_CONTROL_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE_2, PIT_BCD_0);
    set_frequency(COUNTER0_PORT, COUNTER0_VALUE);
    intr_handler_register(0x20, timer_intr_handler);
    put_str("timer_init done\n");
}
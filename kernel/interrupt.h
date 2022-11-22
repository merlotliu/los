#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H

#include "stdint.h"

typedef void* intr_handler;
/* init interrupt discriptor table */
void idt_init(void);

/* interrupt status : turn on | turn off */
enum intr_status {
    INTR_OFF, /* turn off interrupt */
    INTR_ON /* turn on interrupt */
};

#define IDT_DESC_CNT 0x81 /* curent interrput handler count */
#define PIC_M_CTRL 0x20 /* master control port */
#define PIC_M_DATA 0x21 /* master data port */
#define PIC_S_CTRL 0xa0 /* slave control port */
#define PIC_S_DATA 0xa1 /* slave data port */

#define EFLAGS_IF 0x00000200
/* pushfl will push eflags register. popl%0 & '=g' will pop eflags's value to EFLAG_VAR */
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount; 

    uint8_t attribute;
    uint16_t func_offset_high_word;
};

/* intr_entry_table just a entry */
extern intr_handler intr_entry_table[IDT_DESC_CNT]; 
/* syscall handler */
extern uint32_t syscall_handler(void);

/* get current interrupt status */
enum intr_status intr_status_get(void);
/* turn on interrupt & return last interrupt status */
enum intr_status intr_enable(void);
/* turn off interrupt & return last interrupt status */
enum intr_status intr_disable(void);
/* set interrupt status & return last interrupt status */
enum intr_status intr_status_set(enum intr_status);

/* register interrupt handler in idt table */
void intr_handler_register(uint8_t vec_no, intr_handler func);

#endif
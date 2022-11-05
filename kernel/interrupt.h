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
#ifndef _KERNEL_INIT_H
#define _KERNEL_INIT_H

#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"

/* init all of resource */
void init_all(void);

#endif
#ifndef __USERPROG_TSS_H
#define __USERPROG_TSS_H

#include "global.h"
#include "thread.h"
#include "string.h"

/* update esp0 of tss to pthread's stack(level 0) */
void tss_update_esp(struct task_struct* pthread);

/* create tss in GDT & reload gdt */
void tss_init(void);

#endif /* __USERPROG_TSS_H */
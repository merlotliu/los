#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define IO_QUEUE_BUFFER_SIZE 64

/* cycle queue */
typedef struct {
    locker_t locker;
    /* producer & consumer */
    struct task_ctl_blk *producer;
    struct task_ctl_blk *consumer;

    char buf[IO_QUEUE_BUFFER_SIZE];
    int32_t head; /* index of buffer */
    int32_t tail;
}ioqueue_t;

/* init io queue ioq */
void ioq_init(ioqueue_t *ioq);

/* ioq is or not full : 1 full 0 not */
bool ioq_full(ioqueue_t *ioq);

/* ioq is or not empty : 1 empty 0 not */
bool ioq_empty(ioqueue_t *ioq);

/* produce a byte in ioq */
void ioq_putchar(ioqueue_t *ioq, char byte);

/* consume a byte in ioq */
char ioq_getchar(ioqueue_t *ioq);

#endif
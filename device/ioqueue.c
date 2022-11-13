#include "ioqueue.h"
#include "debug.h"
#include "interrupt.h"

/* init io queue ioq */
void ioq_init(ioqueue_t *ioq) {
    ASSERT(ioq != NULL);
    locker_init(&ioq->locker);
    ioq->producer = NULL;
    ioq->consumer = NULL;
    ioq->head = 0;
    ioq->tail = 0;
}

/* get next position in buffer */
static int32_t ioq_next_pos(int32_t pos) {
    return ((pos + 1) % IO_QUEUE_BUFFER_SIZE);
}

/* ioq is or not full : 1 full 0 not */
bool ioq_full(ioqueue_t *ioq) {
    ASSERT(ioq != NULL);
    ASSERT(intr_status_get() == INTR_OFF);
    return (ioq_next_pos(ioq->head) == ioq->tail ? true : false);
}

/* ioq is or not empty : 1 empty 0 not */
bool ioq_empty(ioqueue_t *ioq) {
    ASSERT(ioq != NULL);
    ASSERT(intr_status_get() == INTR_OFF);
    return (ioq->head == ioq->tail ? true : false);
}

/* 使当前生产者或消费者在此缓冲区等待，并记录该等待者 */
static void ioq_wait(struct task_struct **waiter) {
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = thread_running();
    thread_block(TASK_BLOCKED);
}

/* wakeup waiter */
static void ioq_wakeup(struct task_struct **waiter) {
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

/* produce a byte in ioq */
void ioq_putchar(ioqueue_t *ioq, char byte) {
    ASSERT(ioq != NULL);
    ASSERT(intr_status_get() == INTR_OFF);

    while(ioq_full(ioq)) {
        locker_lock(&ioq->locker);
        ioq_wait(&ioq->producer);
        locker_unlock(&ioq->locker);
    }

    /* ioq is not full */
    ioq->buf[ioq->head] = byte;
    ioq->head = ioq_next_pos(ioq->head);

    if(ioq->consumer != NULL) {
        /* wakeup consumer */
        ioq_wakeup(&ioq->consumer);
    }
}

/* consume a byte in ioq */
char ioq_getchar(ioqueue_t *ioq) {
    ASSERT(ioq != NULL);
    ASSERT(intr_status_get() == INTR_OFF);

    while(ioq_empty(ioq)) {
        locker_lock(&ioq->locker);
        ioq_wait(&ioq->consumer);
        locker_unlock(&ioq->locker);
    }

    /* ioq is not empty */
    char byte = ioq->buf[ioq->tail];
    ioq->tail = ioq_next_pos(ioq->tail);

    if(ioq->producer != NULL) {
        /* wake up produce */
        ioq_wakeup(&ioq->producer);
    }
    return byte;
}
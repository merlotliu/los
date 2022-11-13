#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "list.h"
#include "stdint.h"
#include "thread.h"

/******************* semaphore ******************
 ************************************************/
/* semaphore struct */
typedef struct {
    struct list waiters;
    uint8_t value;
}sem_t;

/* semaphore operation */
/* sem_wait init semaphore pointed to by psem to val  */
void sem_init(sem_t *psem, uint8_t val);
/* sem_wait decrements the semaphore pointed to by psem */
void sem_wait(sem_t *psem);
/* sem_post increments the semaphore pointed to by psem. 
    If the semaphore's value consequently becomes greater than zero, 
    then another process or thread blocked in a sem_wait call will
    be woken up and proceed to lock the semaphore. */
void sem_post(sem_t *psem);

/********************* locker *******************
 ************************************************/
/* locker struct */
typedef struct {
    struct task_struct *holder; /* locker holder */
    uint32_t holder_repeat_nr; /* holder repeat apply count */
    sem_t sem; /* binary semaphore */
}locker_t;

/* locker operation */
/* locker_init init locker pointed to by plocker */
void locker_init(locker_t *plocker);
/* locker_lock get locker pointed to by plocker. If locker already is holded by others, then blocked. */
void locker_lock(locker_t *plocker);
/* locker_lock release locker pointed to by plocker. */
void locker_unlock(locker_t *plocker);

#endif /* __THREAD_SYNC_H */
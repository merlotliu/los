#include "sync.h"
#include "debug.h"
#include "interrupt.h"

/* semaphore operation */
/* sem_wait init semaphore pointed to by psem to val  */
void sem_init(sem_t *psem, uint8_t val) {
    psem->value = val;
    list_init(&psem->waiters);
}

/* sem_wait decrements the semaphore pointed to by psem */
void sem_wait(sem_t *psem) {
    enum intr_status old_stat = intr_disable();

    while(psem->value == 0) { /* 信号量不足，阻塞 */
        ASSERT(!elem_find(&psem->waiters, &thread_running()->general_tag));
        if(elem_find(&psem->waiters, &thread_running()->general_tag)) {
           PANIC("sem_wait : thread blocked has been in waiters list\n");
        }
        list_push_back(&psem->waiters, &thread_running()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    psem->value--;
    
    intr_status_set(old_stat);
}

/* sem_post increments the semaphore pointed to by psem. 
    If the semaphore's value consequently becomes greater than zero, 
    then another process or thread blocked in a sem_wait call will
    be woken up and proceed to lock the semaphore. */
void sem_post(sem_t *psem) {
    enum intr_status old_stat = intr_disable();
    
    if(!list_empty(&psem->waiters)) {

        struct task_struct* thread_blocked = 
            elem2entry(struct task_struct, general_tag, list_pop(&psem->waiters));
        thread_unblock(thread_blocked);    
    }
    psem->value++;
    ASSERT(psem->value > 0);

    intr_status_set(old_stat);
}


/* locker operation */
/* locker_init init locker pointed to by plocker */
void locker_init(locker_t *plocker) {
    plocker->holder = NULL;
    plocker->holder_repeat_nr = 0;
    sem_init(&plocker->sem, 1);
}

/* locker_lock get locker pointed to by plocker. If locker already is holded by others, then blocked. */
void locker_lock(locker_t *plocker) {
    /* avoid repeat apply locker */
    if(plocker->holder != thread_running()) {
        sem_wait(&plocker->sem); /* P */
        ASSERT(plocker->sem.value == 0);

        plocker->holder = thread_running();
    
        ASSERT(plocker->holder_repeat_nr == 0);
        plocker->holder_repeat_nr = 1;
    } else {
        plocker->holder_repeat_nr++;
    }
}

/* locker_lock release locker pointed to by plocker. */
void locker_unlock(locker_t *plocker) {
    ASSERT(plocker->holder == thread_running());
    if(plocker->holder_repeat_nr > 1) {
        plocker->holder_repeat_nr--;
        return;
    }
    ASSERT(plocker->holder_repeat_nr == 1);

    plocker->holder = NULL;
    plocker->holder_repeat_nr = 0;
    sem_post(&plocker->sem); /* V */
}
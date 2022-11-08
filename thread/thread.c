#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "list.h"
#include "process.h"

struct task_ctl_blk* main_thread; /* main thread PCB */
struct list thread_ready_list; /* ready tasks queue */
struct list thread_all_list; /* all tasks queue */
static struct list_elem* thread_tag; /* thread node of queue */

extern void switch_to(struct task_ctl_blk* cur, struct task_ctl_blk* next);

/* 获取当前线程 PCB 指针 */
struct task_ctl_blk* thread_running(void) {
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));
    /* 高 20 位即为 PCB 地址 */
    return (struct task_ctl_blk*)(esp & 0xfffff000);
}

/* kernel_thread call func(func_arg) */
static void kernel_thread(thread_func* func, void* func_arg) {
    intr_enable(); /* 避免因中断关闭而无法调度其他线程 */
    func(func_arg);
}

/* 初始化线程栈 thread_stack，将待执行的参数放在对应位置 */
void thread_create(struct task_ctl_blk* pthread, thread_func func, void* func_arg) { 
    /* 预留出中断栈和线程栈的空间 */
    pthread->self_kstack -= sizeof(struct intr_stack);
    pthread->self_kstack -= sizeof(struct thread_stack);
    struct thread_stack* kthread_stack =\
        (struct thread_stack*)pthread->self_kstack;

    kthread_stack->eip = kernel_thread;
    kthread_stack->func = func;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp =\
        kthread_stack->ebx =\
        kthread_stack->esi =\
        kthread_stack->edi = 0;
}

/* 初始化线程基本信息 */
void thread_attr_init(struct task_ctl_blk* pthread, char* name, int priority) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if(pthread == main_thread) {
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }
    /* 栈从本页的最高地址 + 1 开始向下生长 */
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = priority;
    pthread->ticks = priority;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->stack_magic = 0x19990926; /* 定义的魔数，如果该值被覆盖，说明溢出 */
}

/* 创建优先级为 prio 名为 name 的线程，执行函数为 func(func_arg) */
struct task_ctl_blk* thread_start(char* name, int priority, thread_func* func, void* func_arg) {
    /* pcb 都位于内核空间，包括用户进程的 pcb 也是在内核空间 */
    struct task_ctl_blk* thread = get_kernel_pages(1);
    
    thread_attr_init(thread, name, priority);
    thread_create(thread, func, func_arg);

    /* ASSERT 保证元素不被重复添加进队列 */
    ASSERT(!(elem_find(&thread_ready_list, &thread->general_tag)));
    list_push_back(&thread_ready_list, &thread->general_tag);

    ASSERT(!(elem_find(&thread_all_list, &thread->all_list_tag)));
    list_push_back(&thread_all_list, &thread->all_list_tag);

    return thread;
}

/* thread block */
void thread_block(enum task_status stat) {
    ASSERT((TASK_BLOCKED == stat) || (TASK_WAITING == stat) || (TASK_HANGING == stat));
    enum intr_status old_stat = intr_disable();

    struct task_ctl_blk* cur_tcb = thread_running();
    cur_tcb->status = stat;
    schedule();

    intr_status_set(old_stat);
}

/* wakeup blocked thread */
void thread_unblock(struct task_ctl_blk* pthread) {
    ASSERT((TASK_BLOCKED == pthread->status) || (TASK_WAITING == pthread->status) || (TASK_HANGING == pthread->status));
    enum intr_status old_stat = intr_disable();

    if(TASK_READY != pthread->status) {
        ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
        if(elem_find(&thread_ready_list, &pthread->general_tag)) {
            PANIC("thread_unblock: blocked thread in ready_list\n") ;
        }
        list_push_front(&thread_ready_list, &pthread->general_tag);
        pthread->status = TASK_READY;
    }

    intr_status_set(old_stat);
}

/* 将 kernel 中的 main 函数完善为主线程 */
static void make_main_thread(void) {
    /* main 线程早已经运行，
    在 loader.S 中进入内核时的 mov esp, 
    0xc009f000 即为预留的 PCB，故 
    PCB 地址为 0xc009e000，不需要另外申请页面 */
    main_thread = thread_running();
    thread_attr_init(main_thread, "main", MAIN_THREAD_PRIORITY);

    /* main 函数为当前线程，不在就绪队列中，仅在 thread_all_list 中 */
    ASSERT(!(elem_find(&thread_all_list, &main_thread->all_list_tag)));
    list_push_back(&thread_all_list, &main_thread->all_list_tag);
}

/* task schedule */
void schedule(void) {
    ASSERT(intr_status_get() == INTR_OFF);
    /* 主要任务：将当前线程下处理器，并在就绪队列中找到下一个可运行的执行流，换上处理器 */
    struct task_ctl_blk* cur_tcb = thread_running();
    if(cur_tcb->status == TASK_RUNNING) {
        /* 时间片到，加入就绪队列尾部 */

        ASSERT(!elem_find(&thread_ready_list, &cur_tcb->general_tag));

        list_push_back(&thread_ready_list, &cur_tcb->general_tag);
        cur_tcb->ticks = cur_tcb->priority; /* 重置时间片为 priority */
        cur_tcb->status = TASK_READY;
    } else {
        /* 不是时间片到下 CPU，不加入就绪队列，此时没有进行任何操作 */
    }
    
    /* 暂时用该语句保证就绪队列不为空 */
    ASSERT(!list_empty(&thread_ready_list));
    
    /* 从就绪队列获取一个 tcb，但因为其中存的是 list_elem 需要转化为 tcb 的地址 */
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    /* elem to tcb */
    struct task_ctl_blk* next_tcb =\
        elem2entry(struct task_ctl_blk, general_tag, thread_tag);
    next_tcb->status = TASK_RUNNING;

    /* activate task page table ... */
    process_activate(next_tcb);

    // struct list_elem* elem = &thread_ready_list.head;
    // while(elem != &thread_ready_list.tail) {
    //     put_int((uint32_t)elem2entry(struct task_ctl_blk, general_tag, elem));
    //     elem = elem->next;
    // }

    switch_to(cur_tcb, next_tcb);
}

/* init thread environment */
void thread_env_init(void) {
    put_str("thread_init start\n");
    
    list_init(&thread_ready_list);
    list_init(&thread_all_list);

    make_main_thread();
    put_str("thread_init done\n");
}

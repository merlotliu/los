#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "list.h"
#include "process.h"
#include "sync.h"
#include "file.h"
#include "stdio.h"
#include "fs.h"

struct task_struct* main_thread; /* main thread PCB */
struct task_struct* idle_thread; /* idle thread PCB */
struct list __thread_ready_list; /* ready tasks queue */
struct list __thread_all_list; /* all tasks queue */
static struct list_elem* thread_tag; /* thread node of queue */
locker_t pid_locker; /* pid locker */

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

/* 获取当前线程 PCB 指针 */
struct task_struct* thread_running(void) {
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp));
    /* 高 20 位即为 PCB 地址 */
    return (struct task_struct*)(esp & 0xfffff000);
}

/* kernel_thread call func(func_arg) */
static void kernel_thread(thread_func* func, void* func_arg) {
    intr_enable(); /* 避免因中断关闭而无法调度其他线程 */
    func(func_arg);
}

/* allocate pid */
static pid_t allocate_pid(void) {
    static pid_t next_pid = 0;
    locker_lock(&pid_locker);
    next_pid++;
    locker_unlock(&pid_locker);
    return next_pid;
}

/* allocate pid for fork */
pid_t fork_pid(void) {
    return allocate_pid();
}

/* 初始化线程栈 thread_stack，将待执行的参数放在对应位置 */
void thread_create(struct task_struct* pthread, thread_func func, void* func_arg) { 
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
void thread_attr_init(struct task_struct* pthread, char* name, int priority) {
    memset(pthread, 0, sizeof(*pthread));
    pthread->pid = allocate_pid();
    pthread->ppid = -1;
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
    
    /* 文件描述符表 */
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;
    int fd_idx = 3;
    for(; fd_idx < MAX_FILES_OPEN_PER_PROC; fd_idx++) {
        pthread->fd_table[fd_idx] = -1;
    }
    
    pthread->pgdir = NULL;
    pthread->cwd_inode_nr = 0; /* 根目录为默认工作路径 */
    pthread->stack_magic = 0x19990926; /* 定义的魔数，如果该值被覆盖，说明溢出 */
}

/* 创建优先级为 prio 名为 name 的线程，执行函数为 func(func_arg) */
struct task_struct* thread_start(char* name, int priority, thread_func* func, void* func_arg) {
    /* pcb 都位于内核空间，包括用户进程的 pcb 也是在内核空间 */
    struct task_struct* thread = get_kernel_pages(1);
    
    thread_attr_init(thread, name, priority);
    thread_create(thread, func, func_arg);

    /* ASSERT 保证元素不被重复添加进队列 */
    ASSERT(!(elem_find(&__thread_ready_list, &thread->general_tag)));
    list_push_back(&__thread_ready_list, &thread->general_tag);

    ASSERT(!(elem_find(&__thread_all_list, &thread->all_list_tag)));
    list_push_back(&__thread_all_list, &thread->all_list_tag);

    return thread;
}

/* thread block */
void thread_block(enum task_status stat) {
    ASSERT((TASK_BLOCKED == stat) || (TASK_WAITING == stat) || (TASK_HANGING == stat));
    enum intr_status old_stat = intr_disable();

    struct task_struct* cur_tcb = thread_running();
    cur_tcb->status = stat;
    schedule();

    intr_status_set(old_stat);
}

/* wakeup blocked thread */
void thread_unblock(struct task_struct* pthread) {
    ASSERT((TASK_BLOCKED == pthread->status) || (TASK_WAITING == pthread->status) || (TASK_HANGING == pthread->status));
    enum intr_status old_stat = intr_disable();

    if(TASK_READY != pthread->status) {
        ASSERT(!elem_find(&__thread_ready_list, &pthread->general_tag));
        if(elem_find(&__thread_ready_list, &pthread->general_tag)) {
            PANIC("thread_unblock: blocked thread in ready_list\n") ;
        }
        list_push_front(&__thread_ready_list, &pthread->general_tag);
        pthread->status = TASK_READY;
    }

    intr_status_set(old_stat);
}

/* 主动让出 CPU 切换其他线程 */
void thread_yield(void) {
    enum intr_status old_stat = intr_disable();

    struct task_struct* cur_thread = thread_running();
    ASSERT(!elem_find(&__thread_ready_list, &cur_thread->general_tag));
    list_push_back(&__thread_ready_list, &cur_thread->general_tag);
    cur_thread->status = TASK_READY;
    schedule();
    
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

    /* main 函数为当前线程，不在就绪队列中，仅在 __thread_all_list 中 */
    ASSERT(!(elem_find(&__thread_all_list, &main_thread->all_list_tag)));
    list_push_back(&__thread_all_list, &main_thread->all_list_tag);
}

/* 系统空闲时运行的线程 */
static void idle(void* arg UNUSED) {
    while(1) {
        thread_block(TASK_BLOCKED);
        /* 开中断执行 hlt */
        asm volatile("sti; hlt" : : : "memory");
    }
}

/* task schedule */
void schedule(void) {
    ASSERT(intr_status_get() == INTR_OFF);
    /* 主要任务：将当前线程下处理器，并在就绪队列中找到下一个可运行的执行流，换上处理器 */
    struct task_struct* cur_tcb = thread_running();
    if(cur_tcb->status == TASK_RUNNING) {
        /* 时间片到，加入就绪队列尾部 */

        ASSERT(!elem_find(&__thread_ready_list, &cur_tcb->general_tag));

        list_push_back(&__thread_ready_list, &cur_tcb->general_tag);
        cur_tcb->ticks = cur_tcb->priority; /* 重置时间片为 priority */
        cur_tcb->status = TASK_READY;
    } else {
        /* 不是时间片到下 CPU，不加入就绪队列，此时没有进行任何操作 */
    }
    
    // /* 暂时用该语句保证就绪队列不为空 */
    // ASSERT(!list_empty(&__thread_ready_list));
    /* 没有可运行的任务，则执行 idle */
    if(list_empty(&__thread_ready_list)) {
        thread_unblock(idle_thread);
    }

    /* 从就绪队列获取一个 tcb，但因为其中存的是 list_elem 需要转化为 tcb 的地址 */
    thread_tag = NULL;
    thread_tag = list_pop(&__thread_ready_list);
    /* elem to tcb */
    struct task_struct* next_tcb =\
        elem2entry(struct task_struct, general_tag, thread_tag);
    next_tcb->status = TASK_RUNNING;

    /* activate task page table ... */
    process_activate(next_tcb);

    // struct list_elem* elem = &__thread_ready_list.head;
    // while(elem != &__thread_ready_list.tail) {
    //     put_int((uint32_t)elem2entry(struct task_struct, general_tag, elem));
    //     elem = elem->next;
    // }

    switch_to(cur_tcb, next_tcb);
}

/* init thread environment */
void thread_env_init(void) {
    put_str("thread_env_init start\n");
    
    list_init(&__thread_ready_list);
    list_init(&__thread_all_list);
    
    locker_init(&pid_locker);
    /* 创建第一个用户进程：放在第一个初始化，init 进程pid就会为 1 */
    process_execute(init, "init");
    /* 当前 main 函数设置为主线程 */
    make_main_thread();
    /* 创建 idle 线程 */
    idle_thread = thread_start("idle", IDLE_THREAD_PRIORITY, idle, NULL);
    
    put_str("thread_env_init done\n");
}

/* 输出对齐 */
static void pad_print(char* buf, int buf_len, void* ptr, char format) {
    bzero(buf, buf_len);
    int idx = 0;
    switch (format) {
    case 's': {
        idx = sprintf(buf, "%s", ptr);
        break;
    }
    case 'd': {
        idx = sprintf(buf, "%d", *((uint16_t*)ptr));
        break;
    }
    case 'x': {
        idx = sprintf(buf, "%d", *((uint32_t*)ptr));
        break;
    }
    default:
        break;
    }
    while(idx < buf_len - 1) {
        buf[idx++] = ' ';
    }
    sys_write(stdout_no, buf, buf_len - 1);
}

static bool elem2threadinfo(struct list_elem* elem, void* arg UNUSED) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, elem);
    char buf[16] = {0};
    pad_print(buf, 16, &pthread->pid, 'd');
    
    if(-1 == pthread->ppid) {
        pad_print(buf, 16, "NULL", 's');
    } else {
        pad_print(buf, 16, &pthread->ppid, 'd');
    }
    switch (pthread->status) {
        case TASK_RUNNING : { /* running */
            pad_print(buf, 16, "TASK_RUNNING", 's');
            break;
        }
        case TASK_READY : { /* reading */
            pad_print(buf, 16, "TASK_READY", 's');
            break;
        }
        case TASK_BLOCKED : { /* blocked */
            pad_print(buf, 16, "TASK_BLOCKED", 's');
            break;
        }
        case TASK_WAITING : { /* waiting */
            pad_print(buf, 16, "TASK_WAITING", 's');
            break;
        }
        case TASK_HANGING : { /* hanging */
            pad_print(buf, 16, "TASK_HANGING", 's');
            break;
        }
        case TASK_DIED : {
            pad_print(buf, 16, "TASK_DIED", 's');
            break;
        }
        default: {
            break;
        }
    }
    pad_print(buf, 16, &pthread->elapsed_ticks, 'x');

    bzero(buf, 16);
    ASSERT(strlen(pthread->name) < 17);
    sprintf(buf, "%s\n", pthread->name);
    sys_write(stdout_no, buf, strlen(buf));
    return false;
}

/* print process info */
void sys_ps(void) {
    char buf[16] = {0};
    pad_print(buf, 16, "PID", 's');
    pad_print(buf, 16, "PPID", 's');
    pad_print(buf, 16, "STAT", 's');
    pad_print(buf, 16, "TICKS", 's');
    pad_print(buf, 16, "COMMAD", 's');
    printf("\n");
    list_traversal(&__thread_all_list, elem2threadinfo, NULL);
}

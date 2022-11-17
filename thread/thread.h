#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "list.h"
#include "string.h"
#include "global.h"
#include "memory.h"

#define THREAD_PRIORITY_DEFAULT 31
#define IDLE_THREAD_PRIORITY 	10
#define MAIN_THREAD_PRIORITY 	31

#define MAX_FILES_OPEN_PER_PROC 8 /* 每个进程最多能打开的文件描述符个数 */

/* general thread function type */
typedef void thread_func(void*);

typedef int16_t pid_t;

/* process & thread status */
enum task_status {
	TASK_RUNNING, /* running */
	TASK_READY, /* reading */
	TASK_BLOCKED, /* blocked */
	TASK_WAITING, /* waiting */
	TASK_HANGING, /* hanging */
	TASK_DIED
};

/* 中断栈
 * 用于再中断发生时保护（进程或线程）的上下文环境：中断发生时，按照此结构压入上下文，寄存器
 * intr_exit 中的出栈操作与压栈顺序相反
 * 该结构在线程自己的内核栈中位置固定，所在页的最顶端
 */
struct intr_stack {
	uint32_t vec_no; /* interrupt vector number */
	//[1] pushad & popad
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp_dummy; /* popad ignore */
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	//![1]

	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	
	/* low level -> high level */
	uint32_t err_code; /* after eip in stack */
	void (*eip)(void);
	uint32_t cs;
	uint32_t eflags;
	void* esp;
	uint32_t ss; /* 栈段 */
};

/************* Thread Stack *************
 * 存储线程中待执行的函数，在线程内核栈位置不固定
 * 用于线程切换时保存线程环境，实际位置屈取决于实际运行情况
 ****************************************/
struct thread_stack {
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;
	
	/* 线程第一次执行时，eip指向待调用的函数 kernel_thread，其他时候指向 switch_to 的返回地址  */
	void (*eip)(thread_func* func, void* func_arg);	

	/* CPU 第一次调用 */
	void (*unused_retaddr);
	thread_func* func; /* kernel_thread 调用的函数名*/
	void* func_arg; /* kernel_thread 调用函数所需参数  */
};

/* 进程或线程的 PCB */
struct task_struct {
	uint32_t* self_kstack; /* 各内核线程都用自己的内核栈 */
	pid_t pid;
	pid_t ppid; /* parent pid */
	enum task_status status; 
	char name[16];
	uint8_t priority; /* 线程优先级，优先级越高，时间片越长 */
	uint8_t ticks; /* 每次运行的时间片，每次时钟中断则--，减为0则下cpu */
	
	/* 任务运行在 cpu 的总时间数，从开始到结束，该值不断递增 */
	uint32_t elapsed_ticks;
	int32_t fd_table[MAX_FILES_OPEN_PER_PROC]; /* 文件描述符数组 */
	/* 标识线程，加入到线程就绪队列 */
	struct list_elem general_tag;
	/* 标识线程，加入到全部线程队列 */
	struct list_elem all_list_tag;
	uint32_t* pgdir; /* 进程自己页表的虚拟地址，线程为 NULL */
	struct vaddr_mem_pool userprog_vaddr_mem_pool; /* 用户进程虚拟地址 */
	mem_bck_desc_t u_bck_descs[MEM_DESC_CNT];
	
	uint32_t cwd_inode_nr; /* 进程所在工作目录的 inode 编号 */
	uint32_t stack_magic; /* 定义的魔数，如果该值被覆盖，说明溢出 */
};
/* 获取当前线程 PCB 指针 */
struct task_struct* thread_running(void);

/* allocate pid for fork */
pid_t fork_pid(void);

/* 初始化线程栈 thread_stack，将待执行的参数放在对应位置 */
void thread_create(struct task_struct* pthread, thread_func func, void* func_arg);

/* 初始化线程基本信息 */
void thread_attr_init(struct task_struct* pthread, char* name, int priority);

/* 创建优先级为 prio 名为 name 的线程，执行函数为 func(func_arg) */
struct task_struct* thread_start(char* name, int priority, thread_func* func, void* func_arg);

/* thread block */
void thread_block(enum task_status stat);

/* wakeup blocked thread */
void thread_unblock(struct task_struct* pthread);

/* 主动让出 CPU 切换其他线程 */
void thread_yield(void);

/* task schedule */
void schedule(void);

/* init thread environment */
void thread_env_init(void);

/* print process info */
void sys_ps(void);

#endif /* __THREAD_THREAD_H */

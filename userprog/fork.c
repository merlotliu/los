#include "fork.h"
#include "interrupt.h"
#include "debug.h"
#include "list.h"
#include "process.h"
#include "file.h"
#include "thread.h"
#include "stdio_kernel.h"

extern struct list __thread_ready_list; /* ready tasks queue */
extern struct list __thread_all_list; /* all tasks queue */

extern struct file __file_table[MAX_FILE_OPEN]; /* 文件表 */

extern void intr_exit(void); /* 中断返回地址 */

/* 将父进程 pcb 拷贝给子进程 */
static int32_t pcb_vbtmp_stk0_copy(struct task_struct* child_thread, struct task_struct* parent_thread) {
    /* 1 复制pcb所在的整个页面（pcb信息，0级栈以及返回地址） */
    memcpy(child_thread, parent_thread, PG_SIZE);
    
    /* 修改对应信息 */
    child_thread->pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority;
    child_thread->ppid = parent_thread->pid;
    child_thread->general_tag.prev = NULL;
    child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = NULL;
    child_thread->all_list_tag.next = NULL;
    bck_desc_init(child_thread->u_bck_descs);

    /* 复制父进程虚拟地址池的位图 */
    uint32_t btmp_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    void* vaddr_btmp = get_kernel_pages(btmp_pg_cnt);
    memcpy(vaddr_btmp, child_thread->userprog_vaddr_mem_pool.vaddr_bitmap.bits, btmp_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr_mem_pool.vaddr_bitmap.bits = vaddr_btmp;
    
    ASSERT(strlen(child_thread->name) < 11); /* 防止名字越界 */
    strcat(child_thread->name, "_fork");

    return 0;
}

/* 复制父进程进程体（代码和数据）及用户栈 */
static void procbody_stk3_copy(struct task_struct* child_thread, struct task_struct* parent_thread, void* buf_pg) {
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddr_mem_pool.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr_mem_pool.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start = parent_thread->userprog_vaddr_mem_pool.vaddr_start;
    uint32_t byte_idx = 0;
    uint32_t bit_idx = 0;
    uint32_t prog_vaddr = 0;
    while (byte_idx < btmp_bytes_len) {
        if(vaddr_btmp[byte_idx]) {
            bit_idx = 0;
            while(bit_idx < 8) {
                if((BITMAP_MASK << bit_idx) & vaddr_btmp[byte_idx]) {
                    prog_vaddr = vaddr_start + PG_SIZE * (byte_idx * 8 + bit_idx);
                    /* 将父进程数据复制到内核缓冲 */
                    memcpy(buf_pg, (void*)prog_vaddr, PG_SIZE);
                    /* 页表切换到子进程，避免后续申请内存函数将pte和pde安装在父进程的页表中 */
                    page_dir_activate(child_thread);
                    /* 申请虚拟地址 */
                    get_a_page2(MPF_USER, prog_vaddr);
                    /* 从内核缓冲区复制数据到子进程 */
                    memcpy((void*)prog_vaddr, buf_pg, PG_SIZE);
                    /* 恢复父进程页表 */
                    page_dir_activate(parent_thread);
                }
                bit_idx++;
            }
        }
        byte_idx++;
    }
}

/* 为子进程构建 thread_stack 和修改返回值 */
static int32_t child_stk_build(struct task_struct* child_thread) {
    /* 1 栈顶置为 0，让子进程获取到的返回值为 0 */
    struct intr_stack* intr_stk0 = (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
    intr_stk0->eax = 0;

    /* 2 switch_to 构建 thread_stack */
    uint32_t* thread_stk_eip = (uint32_t*)intr_stk0 - 1; /* 返回地址 */
	uint32_t* thread_stk_esi = (uint32_t*)intr_stk0 - 2;
	uint32_t* thread_stk_edi = (uint32_t*)intr_stk0 - 3;
    uint32_t* thread_stk_ebx = (uint32_t*)intr_stk0 - 4;
    uint32_t* thread_stk_ebp = (uint32_t*)intr_stk0 - 5;

    /* 返回地址更新为 intr_exit，直接从中断返回 */
    *thread_stk_eip = (uint32_t)intr_exit;
    /* 可以不需要清空 */
    *thread_stk_ebp = 0;
    *thread_stk_ebx = 0;
    *thread_stk_edi = 0;
    *thread_stk_esi = 0;

    /* 将构建的 thread_stack 作为子进程的内核栈顶 */
    child_thread->self_kstack = thread_stk_ebp;
    return 0;
}

/* 更新 inode 引用计数 */
static void inode_ref_update(struct task_struct* thread) {
    int32_t lfd = 3;
    int32_t gfd = 0;
    while(lfd < MAX_FILES_OPEN_PER_PROC) {
        gfd = thread->fd_table[lfd];
        ASSERT(gfd < MAX_FILE_OPEN);
        if(gfd != -1) {
            __file_table[gfd].fd_inode->i_count++;
        }
        lfd++;
    }
}

/* 将 parent_thread 进程资源，拷贝给 child_thread */
static int32_t process_copy(struct task_struct* child_thread, struct task_struct* parent_thread) {
    /* 1 复制父进程 pcb，虚拟地址位图、内核栈到子进程 */
    if(pcb_vbtmp_stk0_copy(child_thread, parent_thread) == -1) {
        return -1;
    }
    /* 2 为子进程创建页表，仅包含内核空间 */
    child_thread->pgdir = page_dir_create();
    if(child_thread->pgdir == NULL) {
        return -1;
    }
    /* 3 复制父进程体及用户栈给子进程 */
    /* 内核缓冲区：作为数据的中转 */
    void* buf_pg = get_kernel_pages(1);
    if(buf_pg == NULL) {
        return -1;
    }
    procbody_stk3_copy(child_thread, parent_thread, buf_pg);
    /* 4 构建子进程 thread_stack 和修改返回值 pid */
    child_stk_build(child_thread);
    /* 5 更新文件 inode 的引用数 */
    inode_ref_update(child_thread);

    mfree_page(MPF_KERNEL, buf_pg, 1);
    return 0;
}

/* fork 子进程 内核不可直接调用 */
pid_t sys_fork(void) {
    struct task_struct* parent_thread = thread_running();
    struct task_struct* child_thread = get_kernel_pages(1);
    if(child_thread == NULL) {
        return -1;
    }
    ASSERT(INTR_OFF == intr_status_get() && parent_thread->pgdir != NULL);
    if(process_copy(child_thread, parent_thread) == -1) {
        return -1;
    }

    ASSERT(!(elem_find(&__thread_ready_list, &child_thread->general_tag)));
    list_push_back(&__thread_ready_list, &child_thread->general_tag);
    ASSERT(!(elem_find(&__thread_all_list, &child_thread->all_list_tag)));
    list_push_back(&__thread_all_list, &child_thread->all_list_tag);

    // printf("%s 0x%x\n", child_thread->name, child_thread);
    
    return child_thread->pid; /* child pid */
}

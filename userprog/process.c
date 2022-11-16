#include "process.h"
#include "userprog.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "debug.h"
#include "tss.h"
#include "console.h"

extern void intr_exit(void);
extern struct list __thread_ready_list; /* ready tasks queue */
extern struct list __thread_all_list; /* all tasks queue */

/* 构建用户进程初始上下文信息 */
void process_start(void *filename_) {
    struct task_struct* cur_thread = thread_running();

    cur_thread->self_kstack += sizeof(struct thread_stack);
    struct intr_stack* proc_stack = (struct intr_stack*)(cur_thread->self_kstack);

    proc_stack->edi = 0;
    proc_stack->esi = 0;
    proc_stack->ebp = 0;
    proc_stack->esp_dummy = 0;
    
    proc_stack->ebx = 0;
    proc_stack->edx = 0;
    proc_stack->ecx = 0;
    proc_stack->eax = 0;
    
    proc_stack->gs = 0; /* 用户态用不上 */
    proc_stack->ds = SELECTOR_U_DATA;
    proc_stack->es = SELECTOR_U_DATA;
    proc_stack->fs = SELECTOR_U_DATA;
    
    proc_stack->eip = (void*)(filename_); /* 待执行的用户程序地址 */
    proc_stack->cs = SELECTOR_U_CODE;
    
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(MPF_USER, USER_STACK3_VADDR) + PG_SIZE);

    proc_stack->ss = SELECTOR_U_DATA;

    // put_int((uint32_t)cur_thread);
    // put_str("\n");

    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}   

/* 为进程重新加载页目录表，以激活进程页表 */
void page_dir_activate(struct task_struct* pthread) {
    /* 无论是线程还是进程都需要重新安装页表，防止线程使用进程的页表 */
    /* 默认为内核页目录表（物理地址 0x100000） */
    uint32_t phy_page_dir_addr = 0x100000;
    if(pthread->pgdir != NULL) {
        /* 进程自己的页目录表 */
        phy_page_dir_addr = addr_v2p((uint32_t)pthread->pgdir);
    }
    /* 更新页目录寄存器 cr3 使新页表生效 */
    asm volatile("movl %0, %%cr3" : : "r" (phy_page_dir_addr) : "memory");
}

/* 重新加载页目录项，激活页表，更新 tss 中的 esp0 为进程的特权级 0 的栈 */
void process_activate(struct task_struct* pthread) {
    ASSERT(pthread != NULL);
    page_dir_activate(pthread);
    /* 内核线程不需要更新，用户进程才需要更新 */
    if(pthread->pgdir) {
        tss_update_esp(pthread);
    }
}

/* 创建页目录表，将当前页表的表示内核空间的 pde 赋值，成功则返回页目录的虚拟地址，否则返回NULL */
uint32_t* page_dir_create(void) {
    /* 用户进程页表不能让用户直接访问，需要在内核空间申请 */
    uint32_t* page_dir_vaddr = get_kernel_pages(1);  
    if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get kernel__page failed !");
        return NULL;
    }
    /* 1. 复制页表 */
    /* 复制内核目录768~1023页表项（共256） */
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300 * 4), (uint32_t*)(0xfffff000 + 0x300*4), 256 * 4);

    /* 2. 更新页目录地址 */
    uint32_t new_phy_page_dir_addr = addr_v2p((uint32_t)page_dir_vaddr);
    /* 页目录项地址存在页目录的最后一项，更新为新的页目录物理地址 */
    page_dir_vaddr[1023] = new_phy_page_dir_addr | PG_US_U | PG_RW_W | PG_P_1;
    
    return page_dir_vaddr;
}

/* 创建用户进程虚拟地址位图 */
void user_vaddr_bitmap_create(struct task_struct* user_prog) {
    user_prog->userprog_vaddr_mem_pool.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    user_prog->userprog_vaddr_mem_pool.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr_mem_pool.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr_mem_pool.vaddr_bitmap);
}

/* 创建用户进程 */
void process_execute(void* filename, char* name) {
    struct task_struct* pthread = get_kernel_pages(1);
    thread_attr_init(pthread, name, THREAD_PRIORITY_DEFAULT);
    user_vaddr_bitmap_create(pthread);
    thread_create(pthread, process_start, filename);
    pthread->pgdir = page_dir_create();
    bck_desc_init(pthread->u_bck_descs);

    enum intr_status old_stat = intr_disable();

    ASSERT(!elem_find(&__thread_ready_list, &pthread->general_tag));
    list_push_back(&__thread_ready_list, &pthread->general_tag);

    ASSERT(!elem_find(&__thread_all_list, &pthread->all_list_tag));
    list_push_back(&__thread_all_list, &pthread->all_list_tag);

    intr_status_set(old_stat);
}

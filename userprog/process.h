#ifndef __USERPROG_PROCESS_H
#define __USERPROG_PROCESS_H

#define USER_VADDR_START 0x8048000

#include "process.h"
#include "userprog.h"
#include "thread.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"

extern void intr_exit(void);
extern struct list thread_ready_list; /* ready tasks queue */
extern struct list thread_all_list; /* all tasks queue */

/* 构建用户进程初始上下文信息 */
void process_start(void *filename_);

/* 为进程重新加载页目录表，以激活进程页表 */
void page_dir_activate(struct task_struct* pthread);

/* 重新加载页目录项，激活页表，更新 tss 中的 esp0 为进程的特权级 0 的栈 */
void process_activate(struct task_struct* pthread);

/* 创建页目录表，将当前页表的表示内核空间的 pde 赋值，成功则返回页目录的虚拟地址，否则返回NULL */
uint32_t* page_dir_create(void);

/* 创建用户进程虚拟地址位图 */
void user_vaddr_bitmap_create(struct task_struct* user_prog);

/* 创建用户进程 */
void process_execute(void* filename, char* name);

#endif /* __USERPROG_PROCESS_H */

#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "bitmap.h"

#define PG_SIZE 4096

enum mem_pool_flags {
    MPF_KERNEL = 1, /* kernel memory pool */
    MPF_USER = 2 /* user memory pool */
};

#define PG_P_0  0   /* present attribute of page or page directory */
#define PG_P_1  1
#define PG_RW_R 0   /* R/W : read & execute */
#define PG_RW_W 2   /* R/W : read & write & execute */
#define PG_US_S 0   /* U/S : system */
#define PG_US_U 4   /* U/S : user */

/* virtual address memory pool */
struct vaddr_mem_pool {
    struct bitmap vaddr_bitmap;
    uint32_t vaddr_start; /* virtual address start */
};

extern struct pool kernel_pool, user_pool;

/*
 * @brief: entry of init memory manager
 */
void mem_init(void);

/*
 * @brief: get virtual address vaddr's pde pointer (virtual address)
 */
uint32_t* pde_ptr(uint32_t vaddr);

/*
 * @brief: get virtual address vaddr's pte pointer (virtual address)
 */
uint32_t* pte_ptr(uint32_t vaddr);

/*
 * @brief: 分配 pg_cnt 个页的内存空间
 *      1. 通过 vaddr_get 在虚拟内存池中申请虚拟地址；
 *      2. 通过 palloc 在物理内存池中申请物理页；
 *      3. 通过page_table_add 将以上得到的虚拟地址和物理地址在页表中完成映射；
 * @return: 成功返回起始虚拟地址，失败返回 NULL
 */
void* malloc_page(enum mem_pool_flags mpf, uint32_t pg_cnt);
/*
 * @brief: 申请 pg_cnt 个页的内核物理地址
 * @return: 成功返回内存首地址，失败返回 NULL
 */
void* get_kernel_pages(uint32_t pg_cnt);

#endif /* __KERNEL_MEMORY_H */
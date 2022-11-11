#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "global.h"
#include "bitmap.h"
#include "list.h"

typedef struct list_elem list_elem_t;
typedef struct list list_t;

/* memory pool flags(kernel or user) */
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

/* memory block */
typedef struct {
    list_elem_t free_elem;
}mem_bck_t;

/* memory block descriptor */
typedef struct {
    uint32_t bck_size; /* per block size */
    uint32_t bcks_per_arena; /* total blocks of current arena */
    list_t free_list;
} mem_bck_desc_t;

/* memory block descriptor stardard : 16 32 64 128 256 512 1024 */
#define MEM_DESC_CNT 7 

// extern struct paddr_mem_pool kernel_phy_pool, user_phy_pool;

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

/*
 * @brief: 申请 pg_cnt 个页的内核物理地址
 * @return: 成功返回内存首地址，失败返回 NULL
 */
void* get_user_pages(uint32_t pg_cnt);

/*
 * @brief: 将虚拟地址 vaddr 与 mpf 池中物理地址相关联
 *  1. 获取物理内存池根据 mpf；
 *  2. 将当前虚拟地址位图置 1；
 *  3. 用户进程申请修改用户虚拟地址位图，内核进程申请则修改内核虚拟地址位图；
 *  4. 申请在 mpf 物理内存池申请一页物理内存；
 *  5. 将虚拟页映射到申请到的物理内存；
 */
void* get_a_page(enum mem_pool_flags mpf, uint32_t vaddr);

/* get physical address which virtual address mapped */
uint32_t addr_v2p(uint32_t vaddr);

/* 堆中申请size字节的内存 */
void* sys_malloc(uint32_t size);

/* 初始化所有规格的内存块 */
void bck_desc_init(mem_bck_desc_t* desc_array);

/* 将物理地址会受到物理内存池 */
void pfree(uint32_t pg_phy_addr);

/* 释放以虚拟地址 vaddr 起始的 cnt 个物理页框 */
void mfree_page(enum mem_pool_flags mpf, void* _vaddr, uint32_t pg_cnt);

/* 回收内存 ptr */
void sys_free(void* ptr);

/*
 * @brief: entry of init memory manager
 */
void mem_init(void);

#endif /* __KERNEL_MEMORY_H */
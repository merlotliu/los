#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "debug.h"
#include "string.h"
#include "sync.h"
#include "interrupt.h"

#define MEMORY_TOTAL_SIZE *((uint32_t*)(0xb00))

/*******************位图地址*******************
 * 0xc009f000 是内核主线程栈顶
 * 0xc009e000 位内核主线程 pcb
 * 一个页框大小的位图可表示 128 MB内存
 * （1 位表示 1 个KB，共 8 * 4 KB）
 * 位图位于 0xc009a000 的情况下
 * 可支持 4 个页框的位图，即 512 MB
 **********************************************/
#define MEM_BITMAP_BASE 0xc009a000

/* 
 * 堆的起始虚拟地址
 * 0xc0000000 为内核的起始虚拟地址 
 * 0xc0100000 表示跨过低端 1 MB 内存，使虚拟地址在逻辑上连续
 */
#define K_HEAP_START 0xc0100000

/* 
 * PDE_IDX can get address high 10 bits (page directory entry table index)
 * PTE_IDX can get address mid 10 bits (page table entry table index)
 */
#define PDE_MASK 0xffc00000
#define PTE_MASK 0x003ff000
#define PDE_IDX(addr) ((addr & PDE_MASK) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 物理页内存池结构 */
struct paddr_mem_pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start; /* 该内存池管理的物理内存起始地址 */
    uint32_t pool_size; /* 本内存池的字节容量 */
    locker_t locker; /* muetx locker */
}kernel_phy_pool, user_phy_pool;
/* kernel pool & user pool */

/* 使用该结构为内存分类虚拟地址 */
struct vaddr_mem_pool kernel_vir_pool; 

/* 小内存管理结构 */
typedef struct {
    mem_bck_desc_t* desc;
    uint32_t cnt;
    bool large;
}arena_t;
/* 小内存块描述符数组 */
mem_bck_desc_t k_bck_descs[MEM_DESC_CNT];

/*
 * @brief: 初始化内存池
 */
static void mem_pool_init(uint32_t all_mem) {
    put_str("   mem_pool_init start\n");
    /* 页表大小 = 1页页目录表 + 0 & 768 指向的同一个页表 + 769~1022 共 254 个页表 = 256 个页 */
    uint32_t page_table_size = PG_SIZE * 256;

    /* 低端 1 MB + 页表占用 */
    uint32_t used_mem = page_table_size + 0x100000;
    uint32_t free_mem = all_mem - used_mem;
    /* 不足一页的部分不考虑 */
    uint32_t all_free_pages = free_mem / PG_SIZE;
    /* kernel free physical pages */
    uint16_t kernel_free_pages = all_free_pages / 2;
    /* user free physical pages */
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    
    /* 忽略余数：虽然会丢失部分内存，但方便内存管理且不用越界检查 */
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;

    /* memory pool start address */
    uint32_t kp_start = used_mem;
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;

    kernel_phy_pool.phy_addr_start = kp_start;
    user_phy_pool.phy_addr_start = up_start;

    kernel_phy_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_phy_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_phy_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_phy_pool.pool_bitmap.btmp_bytes_len = ubm_length;

    /********* 内核内存池和用户内存池位图 ********************
     * 位图是全局的数据，长度不固定。
     * 全局或静态的数组需要在编译时知道其长度，
     * 而我们需要根据总内存大小算出需要多少字节，
     * 所以改为指定一块内存来生成位图。
     *******************************************************/
    kernel_phy_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_phy_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    /* print memory pool information */
    put_str("       kernel_phy_pool_bitmap_start:");
    put_int((int)kernel_phy_pool.pool_bitmap.bits);
    put_str("\n");
    put_str("       kernel_phy_pool_phy_addr_start :");
    put_int(kernel_phy_pool.phy_addr_start);
    put_str("\n");
    
    put_str("       user_phy_pool bi tmap_start :");
    put_int((int) user_phy_pool .pool_bitmap .bits);
    put_str("\n");
    put_str("       user_phy_pool_phy_addr_start: ");
    put_int(user_phy_pool.phy_addr_start);
    put_str("\n");

    /* clear bitmap */
    bitmap_init(&kernel_phy_pool.pool_bitmap);
    bitmap_init(&user_phy_pool.pool_bitmap);
    
    /* init kernel virtual address bitmap, according real physical memory size */
    kernel_vir_pool.vaddr_bitmap.btmp_bytes_len = kbm_length;
    kernel_vir_pool.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

    kernel_vir_pool.vaddr_start = K_HEAP_START;
    bitmap_init(&kernel_vir_pool.vaddr_bitmap);

    locker_init(&user_phy_pool.locker);
    locker_init(&kernel_phy_pool.locker);

    put_str("   mem_pool_init done\n");
}

/*
 * @brief: 在 pf 表示的虚拟内存池中申请 pg_cnt 个虚拟页
 * @return: 成功返回虚拟页起始地址，失败返回 NULL
 */
static void* vaddr_get(enum mem_pool_flags mpf, uint32_t pg_cnt) {
    int vaddr_start = 0;
    int bit_idx_start = -1;
    uint32_t cnt = 0;
    if(MPF_KERNEL == mpf) {
        bit_idx_start = bitmap_scan(&kernel_vir_pool.vaddr_bitmap, pg_cnt); 
        if(bit_idx_start == -1) {
            return NULL;
        }
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vir_pool.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = kernel_vir_pool.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        /* 用户内存池
         * 1. 获取当前线程 pcb；
         * 2. 扫描用户的虚拟地址池的位图，申请 pg_cnt 页内存；
         * 3. 将申请到的位图标记为已占用（1）；
         * 4. 返回申请到的内存的首地址（虚拟地址）；
         */
        struct task_struct* cur_thread = thread_running();
        struct bitmap* vaddr_bitmap = &cur_thread->userprog_vaddr_mem_pool.vaddr_bitmap;
        bit_idx_start = bitmap_scan(vaddr_bitmap, pg_cnt);
        if(bit_idx_start == -1) {
            return NULL;
        }
        while(cnt < pg_cnt) {
            bitmap_set(vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = cur_thread->userprog_vaddr_mem_pool.vaddr_start + bit_idx_start * PG_SIZE;
        /* (0xc0000000 - PG_SIZE ）作为用户 3 级栈已经在 start_process 被分配 */
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
    }
    return (void*)vaddr_start;
}

/*
 * @brief: 在 mem_pool 指向的内存池分配 1 个物理页
 * @return: 成功返回页框的物理地址，失败返回 NULL
 */
static void* palloc(struct paddr_mem_pool* mem_pool) {
    /* 扫描和设置位图需要保证原子性 */
    int bit_idx = bitmap_scan(&mem_pool->pool_bitmap, 1);
    if(bit_idx == -1) {
        return NULL;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 1); /* mark page used */
    uint32_t page_paddr = ((bit_idx * PG_SIZE) + mem_pool->phy_addr_start);
    return (void*)page_paddr;
}

/*
 * @brief: 获取虚拟地址 vaddr 对应的 pde 指针 (虚拟地址)
 */
uint32_t* pde_ptr(uint32_t vaddr) {
    /* 由于最后一个页目录存储的是页目录表的物理地址所以 0xfffff000 就是页目录表的物理地址 */
    /* 就是需要让高 10 位和中间 10 位找到的都是页目录表的物理地址，最后加上偏移就会得到页表的指针 */
    uint32_t* pde = (uint32_t*)(0xfffff000 + PDE_IDX(vaddr) * 4);
    return pde;
}

/*
 * @brief: 获取虚拟地址 vaddr 对应的 pte 指针 (虚拟地址)
 */
uint32_t* pte_ptr(uint32_t vaddr) {
    /* 
     * 1. 0xffc00000（高 10 位全为 1） 能够找到页目录表的最后一个表项得到页表项的物理地址；
     * 2. 中间 10 位赋值为 原来 vaddr 的高 10，就会在页目录表中检索到原来的 页表的物理地址；
     * 3. 再用原来的中间 10 位 * 4 作为低 12 位即可；
     */
    uint32_t* pte = (uint32_t*)(0xffc00000 | ((vaddr & PDE_MASK) >> 10) | PTE_IDX(vaddr) * 4);
    return pte;
}

/*
 * @brief: 页表中添加虚拟地址 _vir_addr 和物理地址 _phy_addr 的映射
 */
static void page_table_map(void* _vir_addr, void* _phy_addr) {
    uint32_t vir_addr = (uint32_t)_vir_addr;
    uint32_t phy_addr = (uint32_t)_phy_addr;
    uint32_t* pde = pde_ptr(vir_addr);
    uint32_t* pte = pte_ptr(vir_addr);

    /* 先在页目录页中判断 P 位确定页表是否存在 */
    if(PG_P_1 & *pde) {
        ASSERT(!(PG_P_1 & *pte));
        if(!(PG_P_1 & *pte)) {
            /* 创建页表时都应该不存在 */
            *pte = (phy_addr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
            *pte = (phy_addr | PG_US_U | PG_RW_W | PG_P_1);
        }
    } else {
        /* 先创建页表，再创建页表项 */
        uint32_t pde_phy_addr = (uint32_t)palloc(&kernel_phy_pool);
        *pde = (pde_phy_addr | PG_US_U | PG_RW_W | PG_P_1);

        /* 
         * 清空页表的数据，避免旧数据指向页表或页目录导致混乱；
         * 当前页面的页表的物理地址就是 pte（指向 pt 中的 pte） 高 20 位虚拟地址转化之后的结果
         */ 
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
        
        ASSERT(!(PG_P_1 & *pte));
        *pte = (phy_addr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

/*
 * @brief: 分配 pg_cnt 个页的内存空间
 *      1. 通过 vaddr_get 在虚拟内存池中申请虚拟地址；
 *      2. 通过 palloc 在物理内存池中申请物理页；
 *      3. 通过page_table_add 将以上得到的虚拟地址和物理地址在页表中完成映射；
 * @return: 成功返回起始虚拟地址，失败返回 NULL
 */
void* malloc_page(enum mem_pool_flags mpf, uint32_t pg_cnt) {
    /* 内核和用户空间各约 16 MB，保守起见使用 15 MB约束 */
    ASSERT(pg_cnt > 0 && pg_cnt < (15 * 1024 * 1024 / 4096));

    void* vaddr_start = vaddr_get(mpf, pg_cnt);
    if(vaddr_start == NULL) {
        return NULL;
    }

    uint32_t vaddr = (uint32_t)vaddr_start;
    uint32_t cnt = pg_cnt;
    struct paddr_mem_pool* mem_pool = mpf & MPF_KERNEL ? &kernel_phy_pool : &user_phy_pool;

    /* 逐一映射物理内存 */
    while(cnt--) {
        void* page_paddr = palloc(mem_pool);
        if(page_paddr == NULL) {
            put_str("!!! malloc_page error !!!\n");
            /* 
                需要将已经申请的虚拟地址和物理地址回滚 
                待实现
                ... ...    
            */

            return NULL;
        }
        /* virtual page map to physical */
        page_table_map((void*)vaddr, page_paddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

/*
 * @brief: 申请 pg_cnt 个页的内核物理地址
 * @return: 成功返回内存首地址，失败返回 NULL
 */
void* get_kernel_pages(uint32_t pg_cnt) {
    void* vaddr = malloc_page(MPF_KERNEL, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    return vaddr;   
}

/*
 * @brief: 申请 pg_cnt 个页的内核物理地址
 * @return: 成功返回内存首地址，失败返回 NULL
 */
void* get_user_pages(uint32_t pg_cnt) {
    locker_lock(&user_phy_pool.locker);
    void* vaddr = malloc_page(MPF_USER, pg_cnt);
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    locker_lock(&user_phy_pool.locker);
    return vaddr;   
}

/*
 * @brief: 将虚拟地址 vaddr 与 mpf 池中物理地址相关联
 *  1. 获取物理内存池根据 mpf；
 *  2. 将当前虚拟地址位图置 1；
 *  3. 用户进程申请修改用户虚拟地址位图，内核进程申请则修改内核虚拟地址位图；
 *  4. 申请在 mpf 物理内存池申请一页物理内存；
 *  5. 将虚拟页映射到申请到的物理内存；
 */
void* get_a_page(enum mem_pool_flags mpf, uint32_t vaddr) {
    struct paddr_mem_pool* mem_pool = mpf & MPF_KERNEL ? &kernel_phy_pool : &user_phy_pool;
    locker_lock(&mem_pool->locker);
    
    struct task_struct* cur_thread = thread_running();
    int32_t bit_idx = -1;

    if(cur_thread->pgdir != NULL && mpf == MPF_USER) {
        /* user process */
        bit_idx = (vaddr - cur_thread->userprog_vaddr_mem_pool.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur_thread->userprog_vaddr_mem_pool.vaddr_bitmap, bit_idx, 1);
    } else if(cur_thread->pgdir == NULL && mpf == MPF_KERNEL) {
        /* kernel thread */
        bit_idx = (vaddr - kernel_vir_pool.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vir_pool.vaddr_bitmap, bit_idx, 1);
    } else {
        /* error */
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }

    /* 申请一页物理内存 */
    void* phy_page = palloc(mem_pool);
    if(phy_page == NULL) {
        return NULL;
    }
    page_table_map((void*)vaddr, phy_page);
    
    locker_unlock(&mem_pool->locker);
    return ((void*)vaddr);
}

/* get physical address which virtual address mapped */
uint32_t addr_v2p(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    return (uint32_t)((*pte & (PDE_MASK | PTE_MASK)) + (vaddr & 0x00000fff));
}

/* 返回 arena 中第 idx 个内存块的地址 */
static mem_bck_t* arena2bck(arena_t* arena, uint32_t idx) {
    return (mem_bck_t*)((uint32_t)arena + sizeof(arena_t) + idx * arena->desc->bck_size);
}

/* 返回内存块 b 所在的 arena 地址 */
static arena_t* bck2arena(mem_bck_t* bck) {
    return (arena_t*)((uint32_t)bck & 0xfffff000);
}

/* 堆中申请size字节的内存 */
void* sys_malloc(uint32_t size) {
    enum mem_pool_flags mpf;
    struct paddr_mem_pool* mem_pool;
    uint32_t pool_size;
    mem_bck_desc_t* descs;
    struct task_struct* cur_thread = thread_running();

    /* 判断用那个内存池 */
    if(cur_thread->pgdir == NULL) { /* kernel */
        mpf = MPF_KERNEL;
        pool_size = kernel_phy_pool.pool_size;
        mem_pool = &kernel_phy_pool;
        descs = k_bck_descs;
    } else {
        mpf = MPF_USER;
        pool_size = user_phy_pool.pool_size;
        mem_pool = &user_phy_pool;
        descs = cur_thread->u_bck_descs;
    }
    if(!(size > 0 && size < pool_size)) {
        return NULL;
    }
    arena_t* arena;
    mem_bck_t* bck;

    locker_lock(&mem_pool->locker);
    
    /* 大于 1024 分配页框 */
    if(size > 1024) {
        /*  1. 计算需要的页框数，向上取整；
            2. 申请页面并清 0；
            3. 成功返回内存首地址，失败返回 NULL；
        */
        uint32_t pg_cnt = DIV_ROUND_UP(size + sizeof(arena_t), PG_SIZE);
        
        arena = malloc_page(mpf, pg_cnt);
        if(arena != NULL) {
            memset(arena, 0, pg_cnt * PG_SIZE);
            arena->desc = NULL;
            arena->cnt = pg_cnt;
            arena->large = true;

            locker_unlock(&mem_pool->locker);
            return (void*)(arena + 1);
        } else {
            locker_unlock(&mem_pool->locker);
            return NULL;
        }
    } else { 
        /* 小于等于 1024 ：
            1. 找到合适规格的内存块；
            2. 该规格空闲链表为空，则创建新的 arena，申请页框，划分为当前规格内存并添加到空闲链表；
            3. 开始分配内存块，从空闲链表中弹出元素，找到其起始地址，清空，获得其arena地址，设置相关属性（可用内存块数-1），返回内存块地址；
        */
        uint8_t desc_idx;
        for(desc_idx = 0; desc_idx < MEM_DESC_CNT; desc_idx++) {
            if(size <= descs[desc_idx].bck_size) {
                break;
            }
        }
        if(list_empty(&(descs[desc_idx].free_list))) {
            /*  1. 申请1个页框并清空；
                2. 设置申请的内存的内存描述符属性；
                3. 拆分成当前规格的块，并添加到空闲链表中；
            */
            arena = malloc_page(mpf, 1);
            if(arena == NULL) {
                locker_unlock(&mem_pool->locker);
                return NULL;
            }
            memset(arena, 0, PG_SIZE);

            arena->desc = &descs[desc_idx];
            arena->large = false;
            arena->cnt = descs[desc_idx].bcks_per_arena;
            
            enum intr_status old_stat = intr_disable();
            uint32_t bck_idx;
            for(bck_idx = 0; bck_idx < descs[desc_idx].bcks_per_arena; bck_idx++) {
                bck = arena2bck(arena, bck_idx);
                ASSERT(!elem_find(&arena->desc->free_list, &bck->free_elem));
                list_push_back(&arena->desc->free_list, &bck->free_elem);
            }
            intr_status_set(old_stat);
        }

        bck = elem2entry(mem_bck_t, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(bck, 0, descs[desc_idx].bck_size);

        arena = bck2arena(bck);
        arena->cnt--;
        locker_unlock(&mem_pool->locker);
        return (void*)bck;
    }
}

/* 初始化所有规格的内存块 */
void bck_desc_init(mem_bck_desc_t* desc_array) {
    uint16_t desc_idx;
    uint16_t bck_size = 16;

    for(desc_idx = 0; desc_idx < MEM_DESC_CNT; desc_idx++) {
        desc_array[desc_idx].bck_size = bck_size;
        desc_array[desc_idx].bcks_per_arena = (PG_SIZE - sizeof(arena_t)) / bck_size;
        list_init(&desc_array[desc_idx].free_list);

        bck_size <<= 1;
    }
}

/* 将物理地址回收到物理内存池 */
void pfree(uint32_t pg_phy_addr) {
    /* 找到该物理地址对应的内存池和索引，并将对应索引下的位图置为0 */
    struct paddr_mem_pool* mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr >= user_phy_pool.phy_addr_start) {
        /* user physical pool */
        mem_pool = &user_phy_pool;
        bit_idx = (pg_phy_addr - user_phy_pool.phy_addr_start) / PG_SIZE;
    } else {
        /* kernel physical pool */
        mem_pool = &kernel_phy_pool;
        bit_idx = (pg_phy_addr - kernel_phy_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

/* 去掉页表中虚拟地址 vaddr 的映射，将 vaddr 的 pte 的 P 置为 0 */
static void page_table_unmap(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;
    /* 刷新快表 */
    asm volatile("invlpg %0" : : "m" (vaddr) : "memory");
}

/* 从虚拟地址池释放 _vaddr 起始的连续 pg_cnt 个虚拟页 */
static void vaddr_remove(enum mem_pool_flags mpf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0;
    uint32_t vaddr = (uint32_t)_vaddr;
    uint32_t cnt = 0;

    if(mpf == MPF_KERNEL) {
        bit_idx_start = (vaddr - kernel_vir_pool.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vir_pool.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    } else {
        struct task_struct* cur_thread = thread_running();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr_mem_pool.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt) {
            bitmap_set(&cur_thread->userprog_vaddr_mem_pool.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
}

/* 释放以虚拟地址 vaddr 起始的 cnt 个物理页框 */
void mfree_page(enum mem_pool_flags mpf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t pg_phy_addr;
    uint32_t vaddr = ((int32_t)_vaddr);
    uint32_t page_cnt = 0;
    ASSERT(pg_cnt >= 1 && (vaddr % PG_SIZE) == 0);

    /* 虚拟地址转化为物理地址 */
    pg_phy_addr = addr_v2p(vaddr);
    /* 确保该地址在 1MB+1KB（页目录）+1KB（页表）的地址外 */
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

    if(pg_phy_addr >= user_phy_pool.phy_addr_start) {
        /* 找到对应的虚拟地址的物理地址释放，最后将虚拟地址连续的页释放 */
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_phy_pool.phy_addr_start);
            pfree(pg_phy_addr);
            page_table_unmap(vaddr);
            page_cnt++;
        }
        vaddr_remove(mpf, _vaddr, pg_cnt);
    } else {
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= kernel_phy_pool.phy_addr_start && pg_phy_addr < user_phy_pool.phy_addr_start);
            pfree(pg_phy_addr);
            page_table_unmap(vaddr);
            page_cnt++;
        }
        vaddr_remove(mpf, _vaddr, pg_cnt);
    }
}

/* 回收内存 ptr */
void sys_free(void* ptr) {
    ASSERT(ptr != NULL);
    if(ptr == NULL) return;

    enum mem_pool_flags mpf;
    struct paddr_mem_pool* mem_pool;
    if(thread_running()->pgdir == NULL) {
        ASSERT((uint32_t)ptr >= K_HEAP_START);
        mpf = MPF_KERNEL;
        mem_pool = &kernel_phy_pool;
    } else {
        mpf = MPF_USER;
        mem_pool = &user_phy_pool;
    }
    /* 以下操作需要上锁 */
    /* 大于 1025 的内存使用 mfree_pag 释放 */
    /* 小内存释放：
        1. 将内存块添加回空闲链表；
        2. 如果 arena 中所有内存都为空闲则全部释放 */
    locker_lock(&mem_pool->locker);

    mem_bck_t* bck = ptr;
    arena_t* arena = bck2arena(bck);
    ASSERT(arena->large == 0 || arena->large == 1);
    if(arena->desc == NULL && arena->large == true) {
        mfree_page(mpf, arena, arena->cnt);
    } else {
        list_push_back(&arena->desc->free_list, &bck->free_elem);
        arena->cnt++;

        if(arena->cnt == arena->desc->bcks_per_arena) {
            uint32_t bck_idx;
            for (bck_idx = 0; bck_idx < arena->cnt; bck_idx++) {
                bck = arena2bck(arena, bck_idx);
                ASSERT(elem_find(&arena->desc->free_list, &bck->free_elem));
                list_remove(&bck->free_elem);
            }
            mfree_page(mpf, arena, 1);
        }
    }
    locker_unlock(&mem_pool->locker);
}

/*
 * @brief: 内存管理初始化入口
 */
void mem_init(void) {
    put_str("mem_init start\n");
    /* 0xb03 开始 32 位存放了内存的总容量 */
    mem_pool_init(MEMORY_TOTAL_SIZE); /* init memory pool */
    bck_desc_init(k_bck_descs);
    put_str("mem_init done\n");
}

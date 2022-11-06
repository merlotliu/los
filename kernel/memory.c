#include "memory.h"
#include "stdint.h"
#include "print.h"
#include "debug.h"
#include "string.h"

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

struct paddr_mem_pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start; /* 该内存池管理的物理内存起始地址 */
    uint32_t pool_size; /* 本内存池的字节容量 */
}kernel_phy_pool, user_phy_pool;
/* kernel pool & user pool */

/* 使用该结构为内存分类虚拟地址 */
struct vaddr_mem_pool kernel_vir_pool; 

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

    put_str("   mem_pool_init done\n");
}

/*
 * @brief: 内存管理初始化入口
 */
void mem_init(void) {
    put_str("mem_init start\n");
    /* 0xb03 开始 32 位存放了内存的总容量 */
    uint32_t mem_bytes_total = *((uint32_t*)(0xb03));
    mem_pool_init(mem_bytes_total); /* init memory pool */
    put_str("mem_init done\n");
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
        /* 
            用户部分
            待实现
            ... ...
        */
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
    uint32_t* pte = pte_ptr (vir_addr);

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
    if(vaddr == NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;   
}
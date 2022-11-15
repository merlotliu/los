#include "inode.h"
#include "global.h"
#include "ide.h"
#include "fs.h"
#include "interrupt.h"
#include "stdio_kernel.h"
#include "debug.h"

/* 用于存储 inode 位置 */
struct inode_position {
    uint32_t sec_lba_base; /* inode 所在的扇区号 */
    uint8_t sec_cnt; /* 该 inode 占用扇区数，正常来说不是 1 就是 2 */
    uint32_t off_size; /* inode 在扇区内的字节偏移 */
};

/* 获取 inode 所在的扇区和山区内的偏移量 */
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
    ASSERT(inode_no < 4096);

    uint32_t inode_table_lba = part->sb->inode_table_lba_base;
    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no * inode_size; /* 相对于表头的字节 */
    uint32_t off_sec = off_size / SECTOR_SIZE; /* 相对于表头的扇区偏移 */
    uint32_t off_size_in_sec = off_size % SECTOR_SIZE; /* inode 所在扇区的字节偏移 */

    inode_pos->sec_cnt = 1 + ((inode_size + off_size_in_sec) > SECTOR_SIZE);
    inode_pos->sec_lba_base = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}

/* 将 inode 写入到分区 part */
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);
    
    ASSERT(inode_pos.sec_lba_base <= (part->lba_start + part->sec_cnt));

    struct inode pure_inode;
    memcpy(&pure_inode, inode, sizeof(struct inode));
    pure_inode.i_count = 0;
    pure_inode.i_write = false;
    pure_inode.i_tag.prev = pure_inode.i_tag.next = NULL;

    char* inode_buf = (char*)io_buf;
    /* 读写扇区是以扇区为单位，写入数据不足一扇区时，需将原先的内容全部读出，加入新内容后在一并写入 */
    uint8_t sec_cnt = inode_pos.sec_cnt;
    ide_read(part->my_disk, inode_pos.sec_lba_base, inode_buf, sec_cnt);
    memcpy((inode_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba_base, inode_buf, sec_cnt);
}

/* 找到符合条件的 inode 节点 */
static bool inode_find(struct list_elem* elem, void* arg) {
    return ((uint32_t)arg == (elem2entry(struct inode, i_tag, elem))->i_no);
}

/* 根据 inode 节点号返回对应的 inode 节点地址 */
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
    /* 首先在 inode 缓存队列中寻找 inode，没有找到在从磁盘读入 */
    struct inode* inode;
    struct list_elem* elem = list_traversal(&part->open_inodes, inode_find, (void*)inode_no);
    if(elem != NULL) {
        inode = elem2entry(struct inode, i_tag, elem);
        inode->i_count++;
    } else {
    
        /* 从磁盘读取 inode */
        struct inode_position inode_pos;
        inode_locate(part, inode_no, &inode_pos);

        /* 为了达到 inode 被多任务共享，所以该 inode 加载到内存需要在内核申请内存，
            因为在实现 sys_malloc 时使用 pgdir 属性来判断是否在内核申请内存，故需
            先置为 NULL 再申请内存 */
        struct task_struct* cur_thread = thread_running();
        uint32_t* cur_pgdir_bak = cur_thread->pgdir;
        cur_thread->pgdir = NULL;
        inode = (struct inode*)sys_malloc(sizeof(struct inode));
        cur_thread->pgdir = cur_pgdir_bak;

        /* 从扇区读取到缓冲区，再将缓冲区数据写入 inode 节点 */
        char* inode_buf;
        inode_buf = (char*)sys_malloc(SECTOR_SIZE * inode_pos.sec_cnt);
        ide_read(part->my_disk, inode_pos.sec_lba_base, inode_buf, inode_pos.sec_cnt);
        memcpy(inode, inode_buf + inode_pos.off_size, sizeof(struct inode));
        sys_free(inode_buf);

        list_push_back(&part->open_inodes, &inode->i_tag);
        inode->i_count = 1;
    }
    return inode;
}

/* 减少 inode 引用计数 */
void inode_close(struct inode* inode) {
    ASSERT(inode->i_count != 0);

    enum intr_status old_stat = intr_disable();
    if(--inode->i_count == 0) {
        /* 引用计数为 0 则从 inode 缓冲队列移除 */

        list_remove(&inode->i_tag);
        
        /* inode 申请的是内核的内存，也应该释放内核的内存 */
        struct task_struct* cur_thread = thread_running();
        uint32_t* cur_pgdir_bak = cur_thread->pgdir;
        cur_thread->pgdir = NULL;
        sys_free(inode);
        
        cur_thread->pgdir = cur_pgdir_bak;
    }
    intr_status_set(old_stat);
}

/* 初始化新的 inode 节点 */
void inode_init(uint32_t inode_no, struct inode* new_inode) {
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_count = 0;
    new_inode->i_write = false;

    uint8_t sec_idx = 0;
    while(sec_idx < 13) {
        new_inode->i_sectors[sec_idx++] = 0;
    }
}

#include "file.h"
#include "thread.h"
#include "ide.h"
#include "stdio_kernel.h"
#include "inode.h"
#include "interrupt.h"
#include "debug.h"
#include "fs.h"

extern struct partition* __cur_part; /* 当前操作分区（全局变量） */

struct file __file_table[MAX_FILE_OPEN]; /* 文件表 */

/* 从文件表 file_table 中获取一个空闲位，成功返回下表，失败返回 -1 */
int32_t get_free_slot_in_global(void) {
    uint32_t fd_idx = 3;
    while(fd_idx < MAX_FILE_OPEN) {
        if(__file_table[fd_idx].fd_inode == NULL) {
            return (fd_idx - 1);
        }
        fd_idx++;
    }
    if(fd_idx == MAX_FILE_OPEN) {
        printk("exceed max open files\n");
    }
    return -1;
}

/* 将全局描述符下标安装到线程或进程自己的全局文件描述符数组 */
int32_t pcb_fd_install(int32_t global_fd_idx) {
    struct task_struct* cur_thread = thread_running();
    uint8_t local_fd_idx = 3;
    while(local_fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if(cur_thread->fd_table[local_fd_idx] == -1) {
            cur_thread->fd_table[local_fd_idx] = global_fd_idx;
            return local_fd_idx;
        }
        local_fd_idx++;
    }
    if(local_fd_idx == MAX_FILE_OPEN) {
        printk("exceed max open files_per_proc\n");
    }
    return -1;
}

/* 分配一个 inode 节点， 并返回 inode 号 */
int32_t inode_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->inode_btmp, 1);
    if(bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_btmp, bit_idx, 1);
    return bit_idx;
}

/* 分配一个扇区， 并返回扇区 lba 绝对地址 */
int32_t block_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->bck_btmp, 1);
    if(bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->bck_btmp, bit_idx, 1);
    return (bit_idx + part->sb->data_lba_start);
}

/* 将内存中 bitmap 第 bit_idx 位所在的 512 字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {
    ASSERT(bit_idx != 0);

    uint32_t off_sec = bit_idx / 4096; /* 扇区偏移 */
    uint32_t off_size = off_sec % 4096; /* 扇区内字节偏移 */
    uint32_t sec_lba;
    uint8_t* btmp_off;
    
    /* inode 位图和 block 位图需要同步 inode表 每个进程独享 */
    switch (btmp) {
        case INODE_BITMAP: {
            sec_lba = part->sb->inode_btmp_lba_base + off_sec;
            btmp_off = part->inode_btmp.bits + off_size;
            break;
        }
        case BLOCK_BITMAP: {
            sec_lba = part->sb->bck_btmp_lba_base + off_sec;
            btmp_off = part->bck_btmp.bits + off_size;
            break;
        }
        default: {
            break;
        }
    }
    ide_write(part->my_disk, sec_lba, btmp_off, 1);
}

/* 打开编号为 inode_no 的 inode 对应的文件 */
int32_t file_open(uint32_t inode_no, uint8_t flag) {
    /*  1. 在 __file_table 中获取空位下表，并初始化文件表项；
        2. 判断文件打开标志，和写标志（i_write）; */
    int fd_idx = get_free_slot_in_global();
    if(fd_idx == -1) {
        printk("exceed max open files\n");
        return -1;
    }

    __file_table[fd_idx].fd_inode = inode_open(__cur_part, inode_no);
    __file_table[fd_idx].fd_offset = 0; /* 每次打开让偏移置0 */
    __file_table[fd_idx].fd_flag = flag;

    bool* write_deny = &__file_table[fd_idx].fd_inode->i_write;

    if(O_WRONLY & flag || O_RDWR & flag) {
        enum intr_status old_stat = intr_disable();
        if(*write_deny) { /* 其他进程正在写 */
            intr_status_set(old_stat);
            printk("file can't be write now, try again later\n") ;
            return -1;
        } else { /* 没有其他进程写文件的情况 */
            *write_deny = true;
            intr_status_set(old_stat);
        }
    }
    return pcb_fd_install(fd_idx);
}

/* 关闭文件 */
int32_t file_close(struct file* file) {
    if(file == NULL) {
        return -1;
    }
    file->fd_inode->i_write = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

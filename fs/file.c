#include "file.h"
#include "thread.h"
#include "ide.h"
#include "stdio_kernel.h"

/* 文件表 */
struct file __file_table[MAX_FILE_OPEN];

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
    int32_t bit_idx = bitmap_scan(&part->inode_btp, 1);
    if(bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->inode_btp, bit_idx, 1);
    return bit_idx;
}

/* 分配一个扇区， 并返回扇区 lba 绝对地址 */
int32_t block_bitmap_alloc(struct partition* part) {
    int32_t bit_idx = bitmap_scan(&part->bck_btp, 1);
    if(bit_idx == -1) {
        return -1;
    }
    bitmap_set(&part->bck_btp, bit_idx, 1);
    return (bit_idx + part->sb->data_lba_start);
}

/* 将内存中 bitmap 第 bit_idx 位所在的 512 字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp) {
    uint32_t off_sec = bit_idx / 4096; /* 扇区偏移 */
    uint32_t off_size = off_sec % 4096; /* 扇区内字节偏移 */
    uint32_t sec_lba;
    uint8_t* btmp_off;
    
    /* inode 位图和 block 位图需要同步 inode表 每个进程独享 */
    switch (btmp) {
        case INODE_BITMAP: {
            sec_lba = part->sb->inode_btmp_lba_base + off_sec;
            btmp_off = part->inode_btp.bits + off_size;
            break;
        }
        case BLOCK_BITMAP: {
            sec_lba = part->sb->bck_btmp_lba_base + off_sec;
            btmp_off = part->bck_btp.bits + off_size;
            break;
        }
        default: {
            break;
        }
    }
    ide_write(part->my_disk, sec_lba, btmp_off, 1);
}

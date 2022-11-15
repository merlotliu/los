#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "global.h"
#include "ide.h"

/* file struct */
struct file {
    uint32_t fd_offset; /* 文件内偏移 0 - (filesize-1) */
    uint32_t fd_flag;
    struct inode* fd_inode;
};

enum std_fd {
    stdin_no, /* 0 标准输入 */
    stdout_no, /* 1 标准输出 */
    stderr_no /* 2 标准错误 */
};

/* 位图类型 */
enum bitmap_type {
    INODE_BITMAP, /* inode bitmap */ 
    BLOCK_BITMAP /* block bitmap */
};

#define MAX_FILE_OPEN 32 /* 系统可打开的最大文件数 */

/* 从文件表 file_table 中获取一个空闲位，成功返回下表，失败返回 -1 */
int32_t get_free_slot_in_global(void);

/* 将全局描述符下标安装到线程或进程自己的全局文件描述符数组 */
int32_t pcb_fd_install(int32_t global_fd_idx);

/* 分配一个 inode 节点， 并返回 inode 号 */
int32_t inode_bitmap_alloc(struct partition* part);

/* 分配一个扇区， 并返回扇区 lba 绝对地址 */
int32_t block_bitmap_alloc(struct partition* part);

/* 将内存中 bitmap 第 bit_idx 位所在的 512 字节同步到硬盘 */
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp);

/* 打开编号为 inode_no 的 inode 对应的文件 */
int32_t file_open(uint32_t inode_no, uint8_t flag);

/* 关闭文件 */
int32_t file_close(struct file* file);

#endif /* __FS_FILE_H */

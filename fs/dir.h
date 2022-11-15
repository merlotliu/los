#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "fs.h"
#include "ide.h"
#include "inode.h"
#include "file.h"
#include "debug.h"
#include "stdio_kernel.h"
#include "super_block.h"

#define MAX_FILE_NAME_LEN 16 /* max file name length */

/* directory struct */
struct dir {
    struct inode* d_inode;
    uint32_t d_offset; /* 记录在目录中的偏移 */
    uint8_t d_buf[512]; /* 目录的数据缓存 */
};

/* directory entry struct */
struct dentry {
    uint32_t d_inode;
    enum file_type d_ftype; /* 文件类型 */
    char d_filename[MAX_FILE_NAME_LEN];
};

/* 打开根目录 */
void root_dir_open(struct partition* part);

/* 在分区 part 上打开 inode 节点为 inode_no 的目录并返回目录指针 */ 
struct dir* dir_open(struct partition* part, uint32_t inode_no) ;

/* 在 part 分区的 pdir 目录内寻找 name 的文件或目录，找到则返回 true 并将目录项存入 dir_entry 反之返回false */
bool dentry_search(struct partition* part, struct dir* pdir, const char* name, struct dentry* dir_entry) ;

/* 关闭目录 */
void dir_close(struct dir* dir);

/* 在内存中初始化目录项 dentry */
void dentry_create(char* filename, uint32_t inode_no, uint8_t ftype, struct dentry* dentry);

/* 将目录项 dentry 写入父目录 parent_dir 中，io_buf 由主调函数提供 */
bool dentry_sync(struct dir* parent_dir, struct dentry* dentry,void* io_buf);

#endif /* __FS_DIR_H */
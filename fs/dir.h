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
    uint8_t d_buf[SECTOR_SIZE]; /* 目录的数据缓存 */
};

/* directory entry struct */
struct dentry {
    uint32_t d_inode_no;
    enum file_type d_ftype; /* 文件类型 */
    char d_filename[MAX_FILE_NAME_LEN];
};

/* 打开根目录 */
void root_dir_open(struct partition* part);

/* 是否为根目录 */
bool is_root_dir(const char* pathname);

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

/* 读取一个目录项，成功返回 1 个目录项，失败返回 NULL */
struct dentry* dir_read(struct dir* dir);

/* 删除 parent_dir 中的 inode_no 对应的目录项 */
bool dentry_delete(struct partition* part, struct dir* parent_dir, uint32_t inode_no, void* io_buf);

/* 判断目录是否为空(1 空 0 非空)： 仅含有 . 和 .. 两个目录（那么inode所指向的数据大小应该是两个目录项的大小） */
int dir_is_empty(struct dir* dir);

/* 在父目录 parent_dir 中删除 child_dir */
int dir_remove(struct dir* parent_dir, struct dir* child_dir);

#endif /* __FS_DIR_H */
#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "fs.h"
#include "super_block.h"

#define MAX_FILE_NAME_LEN 16 /* max file name length */

/* directory struct */
struct dir {
    struct inode* d_inode;
    uint32_t d_pos; /* 记录在目录中的偏移 */
    uint8_t d_buf[512]; /* 目录的数据缓存 */
};

/* directory entry struct */
struct dentry {
    uint32_t d_inode;
    enum file_type d_ftype; /* 文件类型 */
    char d_filename[MAX_FILE_NAME_LEN];
};

#endif /* __FS_DIR_H */
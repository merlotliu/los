#ifndef __FS_FS_H
#define __FS_FS_H

#include "global.h"

#define MAX_FILES_PER_PART  4096        /* 每个分区最大的文件数 */
#define BITS_PER_SECTOR     4096        /* 每扇区的位数 512*8 */
#define SECTOR_SIZE         512         /* 扇区字节大小 */
#define BLOCK_SIZE          SECTOR_SIZE /* 块字节大小 */

#define MAX_PATH_LEN        512         /* 路径最大长度 */

/* file type */
enum file_type {
    FT_UNKNOWN = 0, /* unknown file type */
    FT_REGULAR, /* regular file */
    FT_DIRECTORY /* directory */
};

/* 打开文件选项 */
enum oflags {
    O_RDONLY, /* 只读 */
    O_WRONLY, /* 只写 */
    O_RDWR, /* 读写 */
    O_CREAT = 4 /* 创建 */
};

/* 查找文件的路径 */
struct path_search_record {
    char searched_path[MAX_PATH_LEN]; /* 父路径 */
    struct dir* parent_dir; /* 文件或目录的直接父目录 */
    enum file_type p_ftype; /* 找到的文件类型 */
};

/* 返回路径深度，如 '/a/b/c' 和 '///a/b//c' 返回的都是 3 */
uint32_t path_depth(char* pathname);

/* 创建文件，成功则返回文件描述符，失败返回-1 */
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flags);

/* 成功打开或创建文件后，返回文件描述符，否则返回 -1 */
int32_t sys_open(const char* pathname, uint8_t flags);

/* 在磁盘上搜索文件系统，若没有则格式化分区创建文件系统 */
void filesys_init(void);

#endif /* __FS_FS_H */